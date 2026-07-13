# Linux 驱动模型与调试

Linux 驱动开发的重点不是注册一个设备节点，而是理解内核已有子系统，尽量把设备接入合适的框架。

## 本章任务与边界

完成本章后，你应能为设备选择标准子系统，解释 DT/ACPI/总线匹配到 `probe()` 的链路，设计资源获取和逆序清理，并处理 IRQ、DMA、work、用户 fd、remove 与 suspend 的并发生命周期。

Linux 内核 API 和子系统惯例随版本演进。示例只表达机制，实际代码必须以目标内核文档、现有同类驱动、binding schema 和源码为准。`devm_` 管理资源释放时机，不自动停止硬件与异步活动。

完成证据：binding/schema 检查、运行时匹配/probe 证据、一次 probe defer 或中途失败测试，以及 remove/suspend 与异步活动的故障序列。创建 `/dev` 节点不是驱动完成的充分条件。

## 驱动选择

| 设备类型 | 推荐子系统 |
|---|---|
| 简单字节流设备 | misc/char device |
| GPIO 按键 | input subsystem |
| LED | LED subsystem |
| 温湿度/ADC/IMU | IIO subsystem |
| 摄像头 | V4L2 |
| 音频 | ALSA/ASoC |
| 网络设备 | netdev |
| 块存储 | block layer |
| 电源/电池 | power supply |
| PWM | PWM subsystem |
| RTC | RTC subsystem |

能接入标准子系统，就不要自定义私有 ioctl。

## 内核驱动职责边界

适合放内核：

- 直接访问寄存器。
- 中断处理。
- DMA。
- 时序严格的硬件控制。
- 接入内核标准子系统。

不适合放内核：

- 复杂业务策略。
- JSON/MQTT/HTTP。
- 大量字符串处理。
- UI。
- 可放用户态的配置逻辑。

原则：内核提供机制，用户态决定策略。

## probe/remove 模型

platform 驱动典型生命周期：

```text
设备树节点解析
  ↓
匹配 compatible
  ↓
probe()
  ├── 获取资源
  ├── 映射寄存器
  ├── 获取 clock/reset/gpio/irq
  ├── 初始化硬件
  └── 注册子系统接口
remove()
  ├── 阻止新请求
  ├── 停止 IRQ/work/timer/DMA
  ├── 等待在途访问结束
  └── 注销接口并释放资源
```

推荐使用 `devm_` 系列 API，减少错误路径释放遗漏。

`devm_` 只管理已注册资源的释放时机，不会自动停止你的硬件和异步执行。若 workqueue、timer、threaded IRQ 或 DMA 回调仍可能访问驱动私有数据，即使内存最后由 devm 释放也会发生 use-after-free。`remove`、`shutdown` 和 probe 失败路径必须先让设备静默，再取消/同步异步活动，最后让用户接口不可达。

### probe defer 与错误传播

clock、regulator、GPIO provider 等依赖尚未就绪时，资源获取可能返回 `-EPROBE_DEFER`。不要统一改写成 `-ENODEV`，否则驱动失去稍后重试机会。现代内核可用 `dev_err_probe()` 保留返回值并避免重复 deferred 日志：

```c
clk = devm_clk_get(dev, NULL);
if (IS_ERR(clk))
    return dev_err_probe(dev, PTR_ERR(clk), "failed to get clock\n");
```

判断 probe 是否成功不能只看是否创建了 `/dev` 节点。应同时检查 deferred probe 列表、driver/device 绑定、clock/reset/regulator 状态和子系统注册结果。

### 生命周期不变量

驱动至少维护这些不变量：

- 接口对用户可见时，底层资源已经初始化且硬件可访问。
- IRQ、DMA、timer 和 work 能运行时，其访问的数据与设备引用仍然有效。
- suspend/remove 开始后，不再接受无法完成的新请求。
- resume 失败时，设备保持不可用或安全降级，不向用户报告虚假成功。

把 probe 中的每个成功步骤与反向撤销动作配对，并分别推演 probe 中途失败、用户仍持有 fd 时 remove、IRQ 与 remove 并发、suspend 时 DMA 未完成等序列。

## 资源获取

常见 API：

```c
res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
base = devm_ioremap_resource(&pdev->dev, res);

irq = platform_get_irq(pdev, 0);
ret = devm_request_irq(&pdev->dev, irq, handler, 0, dev_name(&pdev->dev), data);

clk = devm_clk_get(&pdev->dev, NULL);
reset = devm_reset_control_get_optional(&pdev->dev, NULL);
```

设备树资源和驱动代码必须一致。

## sysfs、procfs、debugfs

| 接口 | 用途 |
|---|---|
| sysfs | 稳定的设备属性接口 |
| procfs | 进程和内核状态，现代驱动少放自定义接口 |
| debugfs | 调试信息，不保证稳定 ABI |

生产接口不要依赖 debugfs。

## ioctl 设计

如果必须使用 ioctl：

- 命令号用 `_IO/_IOR/_IOW/_IOWR`。
- 结构体字段固定宽度。
- 保留版本字段。
- 处理 32/64 位兼容。
- 严格检查用户指针和长度。

用户态数据拷贝：

```c
if (copy_from_user(&cfg, argp, sizeof(cfg)))
    return -EFAULT;
```

## 并发和锁

驱动可能被多个进程同时打开，也可能同时被中断和工作队列访问。

常见工具：

- spinlock：中断上下文和短临界区，不可睡眠。
- mutex：进程上下文，可睡眠。
- completion：等待一次性事件。
- wait queue：等待条件变化。
- workqueue：把中断下半部放到进程上下文。

不要在持有 spinlock 时调用可能睡眠的函数。

## 中断处理

中断上半部做最少工作，下半部处理耗时逻辑。

方式：

- threaded irq。
- tasklet。
- workqueue。

现代驱动中 threaded irq 和 workqueue 更常见。

```c
ret = devm_request_threaded_irq(dev, irq,
                                irq_handler,
                                irq_thread,
                                IRQF_ONESHOT,
                                "mydev", data);
```

## DMA 与 cache

DMA 常见坑：

- buffer 未按要求对齐。
- cache 未同步。
- 使用了不可 DMA 的内存。
- 设备地址和 CPU 地址混淆。

先确认设备 DMA 寻址能力：

```c
ret = dma_set_mask_and_coherent(dev, DMA_BIT_MASK(32));
if (ret)
    return dev_err_probe(dev, ret, "32-bit DMA unavailable\n");
```

优先使用 DMA API，不要用 `virt_to_phys()` 猜设备地址；IOMMU、highmem 和非连续内存下，CPU 虚拟地址、物理地址与 DMA 地址可能完全不同。

一致性分配适合长期共享的描述符或控制结构：

```c
buf = dma_alloc_coherent(dev, size, &dma_handle, GFP_KERNEL);
```

流式映射适合有明确 CPU/设备所有权切换的数据 buffer：

```c
dma_addr_t dma = dma_map_single(dev, buf, len, DMA_TO_DEVICE);
if (dma_mapping_error(dev, dma))
    return -EIO;

start_device(dma, len);
/* 等设备完成，CPU 在此期间不访问 buf。 */
dma_unmap_single(dev, dma, len, DMA_TO_DEVICE);
```

方向必须与真实数据流匹配。长期保持 streaming mapping 时，CPU 与设备每次交换所有权要使用 `dma_sync_single_for_cpu()` / `dma_sync_single_for_device()`；具体时机按 DMA API 文档和设备协议确定。coherent 只表示可见性模型由 API 保证，不代表访问没有顺序要求，也不代表 buffer 适合任意高频 CPU 访问。

DMA 审查时画出：buffer 分配者、CPU 拥有期、设备拥有期、完成信号、错误/超时后的终止动作和最终释放者。超时后直接释放 buffer 而没有先确认设备停止，是典型的异步 use-after-free。

## 设备树绑定文档

正式驱动应写 binding，描述属性：

```yaml
compatible:
  const: vendor,my-sensor
reg:
  maxItems: 1
interrupts:
  maxItems: 1
```

这样可以用 `dtbs_check` 检查 DTS。

## 调试方法

常用命令：

```bash
dmesg -w
lsmod
modinfo mydrv.ko
insmod mydrv.ko
rmmod mydrv
cat /proc/interrupts
cat /proc/iomem
ls /sys/bus/platform/devices
hexdump -C /dev/mydev
```

动态调试：

```bash
echo 'file drivers/misc/mydrv.c +p' > /sys/kernel/debug/dynamic_debug/control
```

函数跟踪：

```bash
echo function > /sys/kernel/debug/tracing/current_tracer
echo mydrv_probe > /sys/kernel/debug/tracing/set_ftrace_filter
cat /sys/kernel/debug/tracing/trace_pipe
```

## 驱动评审清单

- [ ] 是否接入了正确子系统？
- [ ] 是否有设备树 binding？
- [ ] probe 错误路径是否正确释放？
- [ ] 是否使用 devm API 简化资源管理？
- [ ] 是否保留 `-EPROBE_DEFER`，并能观察依赖未就绪？
- [ ] 用户输入是否做长度和权限检查？
- [ ] 中断处理是否足够短？
- [ ] 是否考虑并发 open/read/write/ioctl？
- [ ] remove/shutdown 是否先停止 IRQ、work、timer 和 DMA？
- [ ] DMA mask、方向、mapping error 和所有权切换是否明确？
- [ ] 是否正确处理 suspend/resume？
- [ ] 是否有清晰日志和 debugfs 状态？

## 练习

1. 为一个 GPIO LED 判断应写字符设备还是接入 LED 子系统，并说明原因。
2. 写一个 platform driver 的 probe 伪代码，包含 reg、irq、clock。
3. 用 `cat /proc/interrupts` 观察按键或网卡中断计数变化。
4. 为一个带 IRQ + delayed work + streaming DMA 的驱动画出 probe 失败、remove 和 suspend 的逆序清理流程。

## 深入思考

1. `devm_request_irq()` 已自动释放 IRQ，为什么 remove 中仍可能需要禁止设备中断并同步 handler？
2. probe 成功后创建字符节点，和接入 IIO/input 等标准子系统相比，用户态 ABI、并发和复用成本有什么差异？
3. 一个 DMA 超时是“设备没有完成”还是“完成中断丢失”？释放资源前如何用证据区分，最保守动作是什么？
4. suspend 与用户 ioctl 同时发生时，由谁阻止新事务，已经在途的事务如何收敛？

## 工程洞察

Linux 驱动开发的关键不是“能在内核里操作寄存器”，而是正确接入内核的设备模型、资源生命周期、并发规则和用户态 ABI。驱动一旦进入内核，就会受到 probe defer、热插拔、电源管理、并发访问、权限、安全和长期 ABI 兼容约束。

写驱动前先决定三件事：

- 这个设备是否已有标准子系统承载，例如 gpio、iio、input、netdev、tty、rtc、hwmon。
- 用户态需要的是稳定语义，还是临时调试接口；这决定 sysfs、debugfs、char device、netlink 或标准框架的选择。
- 资源生命周期如何逆序清理，尤其是 IRQ、workqueue、timer、DMA、clock、regulator 和 runtime PM。

驱动 bug 的危险之处在于它经常把硬件错误、并发错误和 ABI 设计错误混在一起。工程化驱动必须提供可观测证据：probe 日志、错误码、tracepoint、debugfs 状态、寄存器 dump 和用户态复现脚本。没有证据链的驱动，后续维护会被“偶发”和“怀疑硬件”拖垮。
