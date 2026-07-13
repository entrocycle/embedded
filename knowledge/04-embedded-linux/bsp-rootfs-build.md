# BSP、RootFS 与系统构建

嵌入式 Linux 项目不是只编译一个内核。可交付系统通常由 Bootloader、Kernel、Device Tree、RootFS、应用、配置、升级包和生产脚本组成。BSP 的价值是把这些内容稳定地绑定到具体硬件平台。

## 本章任务与边界

完成本章后，你应能定义 BSP 组成、选择 Buildroot/Yocto/厂商 SDK 等构建路径，区分只读系统、持久数据和设备身份，产出带配置、依赖、license/manifest、hash 和升级说明的可追溯镜像。

示例命令用于说明流程，不锁定某个 Buildroot/Yocto 版本。生产系统还需结合供应链、安全响应、许可证、可重复构建能力和厂商 BSP 维护策略；“一次构建成功”不证明未来能从干净环境重现。

完成证据：两次干净构建的产物比较、运行版本清单、RootFS 分区/数据边界，以及一个包或配置变更的影响追踪。

## BSP 包含什么

```text
BSP
├── bootloader/     U-Boot、SPL、启动配置
├── kernel/         内核源码、defconfig、补丁
├── device-tree/    板级 DTS/DTSI
├── rootfs/         用户空间、服务、配置
├── drivers/        板级外设驱动
├── tools/          烧录、打包、签名、调试脚本
├── docs/           bring-up、接口、版本说明
└── release/        镜像和升级包
```

BSP 不是“能启动一次”的临时目录，而是硬件版本、软件版本、构建参数和发布产物的可追溯入口。

## 构建方式选择

| 方式 | 适合场景 | 优点 | 代价 |
|---|---|---|---|
| 手工交叉编译 | 学习、验证驱动 | 透明、容易理解 | 难复现 |
| Buildroot | 中小型产品、网关、快速交付 | 简洁、构建快 | 包生态和版本治理较弱 |
| Yocto | 长周期产品、复杂依赖、多硬件 | 可定制、可追溯 | 学习和维护成本高 |
| Debian/Ubuntu RootFS | 高性能板卡、研发工具丰富 | 包管理方便 | 体积大、裁剪和只读化要额外处理 |

学习阶段建议先手工理解链路，再用 Buildroot 做完整镜像；产品阶段根据团队能力和生命周期选择 Buildroot 或 Yocto。

## 启动产物

常见产物：

| 文件 | 作用 |
|---|---|
| `u-boot-spl.bin` | DDR 初始化和加载 U-Boot |
| `u-boot.bin` | 启动内核、设置环境变量 |
| `Image` / `zImage` | Linux 内核 |
| `*.dtb` | 设备树二进制 |
| `rootfs.ext4` | 根文件系统镜像 |
| `boot.scr` / `extlinux.conf` | 启动脚本 |
| `update.img` | 厂商或项目定义的升级包 |

发布时要记录每个产物的 git commit、配置、构建时间、工具链版本和 hash。

## Device Tree 管理

DTS 管理建议：

- 芯片公共资源放 SoC `.dtsi`。
- 板卡公共资源放 board-family `.dtsi`。
- 具体硬件版本放 board-rev `.dts`。
- 不同 SKU 用 overlay 或独立 dts 管理。
- 所有外设节点都要标明供电、时钟、中断、GPIO 和 pinctrl。

设备树变更要配套验证：

```bash
dtc -I dts -O dtb -o board.dtb board.dts
dtc -I dtb -O dts -o dump.dts board.dtb
grep -R . /proc/device-tree/model
ls /sys/firmware/devicetree/base
```

## RootFS 内容

最小 RootFS 必须有：

```text
/bin /sbin /lib /usr /etc /dev /proc /sys /tmp /var
```

产品 RootFS 通常还需要：

- init/systemd 或 busybox init。
- 网络配置。
- 日志配置。
- 应用服务。
- OTA 客户端。
- 诊断工具。
- 证书和密钥索引。
- 只读根文件系统或恢复分区。

## 文件系统选择

| 文件系统 | 适用 |
|---|---|
| initramfs | 极简启动、恢复系统 |
| ext4 | eMMC/SD 卡、通用场景 |
| squashfs | 只读系统，适合 A/B 升级 |
| ubifs | raw NAND |
| jffs2 | 小容量 raw NOR/NAND |
| overlayfs | 只读系统上叠加可写配置 |

如果设备可能断电，必须明确文件系统一致性策略。不要把频繁写入的日志直接落到脆弱分区。

## Buildroot 最小流程

```bash
make menuconfig
make linux-menuconfig
make busybox-menuconfig
make
```

建议纳入版本管理：

- `configs/<board>_defconfig`
- Linux defconfig。
- U-Boot defconfig。
- 自定义 package。
- board post-build 和 post-image 脚本。

不要把 `output/` 整体提交，提交可复现构建所需配置和脚本。

## Yocto 基本概念

| 概念 | 说明 |
|---|---|
| layer | 一组配方和配置 |
| recipe | 构建一个包的规则 |
| image | 最终镜像定义 |
| machine | 硬件平台配置 |
| distro | 发行版策略 |
| bbappend | 对已有 recipe 做增量修改 |

Yocto 更适合产品长期维护，但团队要接受较高的构建系统复杂度。

## 发布检查

- [ ] Bootloader、Kernel、RootFS 均可从干净环境构建。
- [ ] DTS 与硬件版本匹配。
- [ ] 镜像 hash 已记录。
- [ ] 版本号能在设备上查询。
- [ ] SSH、串口、调试口策略明确。
- [ ] 日志不会无限写满分区。
- [ ] 断电恢复测试通过。
- [ ] OTA 包和烧录包都验证过。

## 练习

1. 用 Buildroot 生成一个能启动到 shell 的 RootFS。
2. 修改 DTS 启用一个 GPIO LED，并在 `/sys/class/leds` 中验证。
3. 写一个 post-image 脚本，把内核、DTB、RootFS 打成发布目录。

## 工程洞察

BSP 和 RootFS 构建不是“把系统编出来”，而是定义一台设备从 BootROM 到用户服务的可追溯供应链。U-Boot、Kernel、DTB、RootFS、模块、固件、配置和启动脚本必须来自一致的版本和配置，否则系统可能能启动，却无法长期维护。

构建系统要回答：

- 每个启动产物来自哪个源码 commit、配置文件和工具链版本？
- DTB 与内核驱动是否匹配，运行时设备树是否真的是你编译的那份？
- RootFS 中哪些文件是只读系统内容，哪些是运行时数据，哪些需要 OTA 或恢复出厂设置保留？
- kernel module、firmware blob 和用户态 ABI 是否随内核升级同步验证？
- 发布包是否包含 hash、版本、符号、map、配置、烧录脚本和回滚路径？

Buildroot 强调可控和简洁，Yocto 强调层次化和大规模产品维护。选择工具不是看谁“更高级”，而是看团队规模、产品线复用、定制复杂度、构建时间和长期维护成本。
