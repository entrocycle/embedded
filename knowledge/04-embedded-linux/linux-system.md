# 嵌入式 Linux 全链路

嵌入式 Linux 的核心是“硬件描述、内核驱动、用户空间”三者协作。

## 本章学习方式

Linux 系统比 MCU 裸机复杂，但主线仍然清楚：启动链路把控制权一层层交出去，设备树描述硬件，内核驱动管理资源，用户态实现策略。学习时不要把命令背成清单，而要能画出这条证据链：

```text
BootROM/SPL/U-Boot 日志
  ↓
kernel cmdline 和设备树
  ↓
驱动 probe 日志
  ↓
/proc、/sys、/dev 节点
  ↓
用户态服务日志
```

启动失败、驱动不匹配、RootFS 挂载失败、应用无法访问设备，都可以沿这条链路定位。

## 系统组成

| 组件 | 职责 |
|---|---|
| BootROM | 芯片固化启动代码，加载一级启动程序 |
| SPL | 初始化 DDR，加载 U-Boot |
| U-Boot | 加载内核、设备树、RootFS，设置启动参数 |
| Kernel | 调度、内存、驱动、网络、文件系统 |
| Device Tree | 描述板级硬件资源 |
| RootFS | 用户空间程序、库、配置和服务 |
| App | 业务应用 |

### 边界意识

| 问题 | 应该优先看哪里 | 不应先做什么 |
|---|---|---|
| 串口完全无输出 | 电源、Boot 模式、串口、SPL | 直接改应用代码 |
| 内核找不到设备 | DTS、compatible、驱动配置 | 在用户态硬轮询 |
| `/dev` 没有节点 | 驱动注册、udev/mdev、class | 反复重启应用 |
| 服务启动失败 | systemd 日志、依赖、权限 | 先怀疑内核 |
| 偶发文件损坏 | 掉电、文件系统、sync 策略 | 只加应用重试 |

## U-Boot

U-Boot 常用命令：

```bash
printenv
setenv bootargs console=ttyS0,115200 root=/dev/mmcblk0p2 rw
saveenv
tftp 0x82000000 zImage
tftp 0x83000000 board.dtb
bootz 0x82000000 - 0x83000000
```

排查启动问题：

- 没有任何串口输出：电源、串口、Boot 引脚、BootROM 阶段。
- SPL 后卡住：DDR 初始化、时钟、PMIC。
- U-Boot 能进但内核不启动：内核地址、DTB 地址、bootargs。
- 内核启动但挂 RootFS 失败：root 参数、分区、文件系统驱动、设备节点。

U-Boot 阶段要保留两类信息：启动参数和实际加载地址。很多问题不是镜像坏了，而是 `bootargs`、DTB 地址、内核地址或分区号不一致。

## 设备树

设备树用来描述硬件，而不是写驱动逻辑。

示例：

```dts
&i2c1 {
    status = "okay";
    clock-frequency = <100000>;

    temp_sensor@48 {
        compatible = "vendor,temp-sensor";
        reg = <0x48>;
    };
};
```

驱动通过 `compatible` 匹配设备：

```c
static const struct of_device_id temp_of_match[] = {
    { .compatible = "vendor,temp-sensor" },
    { }
};
MODULE_DEVICE_TABLE(of, temp_of_match);
```

设备树检查：

```bash
dtc -I dts -O dtb -o board.dtb board.dts
dtc -I dtb -O dts -o dump.dts board.dtb
```

设备树常见错误：

- `compatible` 和驱动匹配表不一致。
- `reg` 地址或长度写错。
- pinctrl 状态缺失，导致引脚仍是默认功能。
- clock/reset/gpio 属性缺失，probe 中获取资源失败。
- `status = "disabled"` 没改成 `"okay"`。
- 修改了源码 dts，但实际启动加载的是旧 dtb。

排查时要确认“内核运行时看到的设备树”，而不是只看源码文件。

## 字符设备

字符设备适合顺序读写类设备，如传感器、按键、简单控制接口。

最小模型：

```c
static int dev_open(struct inode *inode, struct file *file)
{
    return 0;
}

static ssize_t dev_read(struct file *file, char __user *buf,
                        size_t count, loff_t *ppos)
{
    return 0;
}

static const struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = dev_open,
    .read = dev_read,
};
```

工程中更常使用：

- `alloc_chrdev_region`
- `cdev_init` / `cdev_add`
- `class_create` / `device_create`
- `copy_to_user` / `copy_from_user`

## Platform 驱动

SoC 内部外设常用 platform 驱动模型。

```text
Device Tree node
      ↓ compatible match
platform_device
      ↓
platform_driver.probe()
```

probe 中通常做：

1. 获取寄存器资源 `platform_get_resource`。
2. 映射寄存器 `devm_ioremap_resource`。
3. 获取中断号。
4. 获取 clock/reset/gpio。
5. 注册字符设备、input、netdev 或其他子系统接口。

`probe()` 不是“驱动入口函数”这么简单，它是内核确认设备存在、资源可用、驱动可以接管硬件的地方。`probe()` 失败要返回明确错误码，让 `dmesg` 能解释原因，而不是静默失败。

## I2C/SPI 驱动

外部芯片通常挂在 I2C 或 SPI 总线上。

I2C 驱动核心：

```c
static int temp_probe(struct i2c_client *client)
{
    dev_info(&client->dev, "temp sensor probed\n");
    return 0;
}

static const struct i2c_device_id temp_ids[] = {
    { "temp_sensor", 0 },
    { }
};

static struct i2c_driver temp_driver = {
    .driver = {
        .name = "temp_sensor",
        .of_match_table = temp_of_match,
    },
    .probe = temp_probe,
    .id_table = temp_ids,
};
module_i2c_driver(temp_driver);
```

## RootFS

常见构建方式：

- BusyBox：适合极简系统。
- Buildroot：快速构建完整嵌入式系统。
- Yocto：适合复杂产品和长期维护。
- Debian/Ubuntu 根文件系统：适合资源较大的板卡。

RootFS 必备目录：

```text
/bin /sbin /lib /etc /dev /proc /sys /tmp /usr /var
```

启动后常用挂载：

```bash
mount -t proc none /proc
mount -t sysfs none /sys
mount -t devtmpfs none /dev
```

RootFS 设计也有取舍：

| 方案 | 优点 | 代价 | 适用 |
|---|---|---|---|
| initramfs | 启动简单，和内核绑定 | 更新不灵活，容量受限 | 早期 bring-up、救援系统 |
| squashfs + overlay | 只读可靠，便于恢复 | 写入区需要设计 | 产品固件 |
| ext4 | 通用，工具多 | 掉电策略要做好 | eMMC/SSD |
| UBIFS | 适合 raw NAND | 构建和调试复杂 | NAND 产品 |

不要只问“能不能启动”，还要问掉电、升级、日志写入和恢复策略。

## 用户态调试

常用命令：

```bash
dmesg -w
cat /proc/cmdline
cat /proc/interrupts
cat /proc/iomem
ls /sys/bus/platform/devices
strace ./app
gdbserver :2345 ./app
```

驱动调试：

- `dev_info/dev_err` 打日志。
- `dynamic_debug` 动态打开日志。
- `debugfs` 暴露内部状态。
- `ftrace` 跟踪函数调用。
- `perf` 分析性能。

## 应用和驱动边界

不要把复杂业务塞进内核驱动。驱动负责：

- 硬件访问。
- 中断处理。
- 数据搬运。
- 统一抽象接口。

用户态负责：

- 策略和业务。
- 网络通信。
- 配置管理。
- 日志上报。
- 升级流程。

边界判断可以用一句话：内核负责安全、通用、低延迟的硬件抽象；用户态负责可变、复杂、可升级的策略。把业务策略塞进内核，会增加崩溃影响面和升级成本。

## 故障注入

| 故障 | 验证点 | 证据 |
|---|---|---|
| 启动参数 root 写错 | 是否进入 panic 或 emergency shell | 串口日志、`/proc/cmdline` |
| DTS compatible 写错 | 驱动是否不 probe | `dmesg`、`/sys/bus/*/devices` |
| RootFS 缺少库 | 应用启动失败是否可诊断 | `ldd`、systemd 日志 |
| 服务进程崩溃 | 是否自动重启并限制重启风暴 | `systemctl status` |
| 掉电写配置 | 文件是否损坏，是否有恢复策略 | 校验、版本、备份文件 |

## 深入思考

1. 为什么设备树描述硬件而不描述业务逻辑？如果把业务参数写进 DTS 会有什么后果？
2. 一个传感器数据处理流程，哪些部分适合内核驱动，哪些部分适合用户态服务？
3. Buildroot 和 Yocto 的差异不只是命令不同，它们分别适合什么团队和产品生命周期？
4. 如果现场设备只能通过网络远程访问，Linux 系统必须预留哪些诊断入口？

## 练习

1. 修改设备树启用一个 I2C 控制器，并在系统中确认节点。
2. 写一个返回固定字符串的字符设备驱动。
3. 用 Buildroot 构建一个最小 RootFS，并启动到 shell。
4. 故意让驱动 `compatible` 匹配失败，记录从源码 DTS 到运行时 sysfs 的排查过程。
