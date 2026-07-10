# Linux 驱动模型与调试

Linux 驱动开发的重点不是注册一个设备节点，而是理解内核已有子系统，尽量把设备接入合适的框架。

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
  └── 释放或由 devm 自动管理
```

推荐使用 `devm_` 系列 API，减少错误路径释放遗漏。

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

优先使用 DMA API：

```c
buf = dma_alloc_coherent(dev, size, &dma_handle, GFP_KERNEL);
```

一致性 DMA 简单但可能性能低；流式 DMA 需要 map/unmap。

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
- [ ] 用户输入是否做长度和权限检查？
- [ ] 中断处理是否足够短？
- [ ] 是否考虑并发 open/read/write/ioctl？
- [ ] 是否正确处理 suspend/resume？
- [ ] 是否有清晰日志和 debugfs 状态？

## 练习

1. 为一个 GPIO LED 判断应写字符设备还是接入 LED 子系统，并说明原因。
2. 写一个 platform driver 的 probe 伪代码，包含 reg、irq、clock。
3. 用 `cat /proc/interrupts` 观察按键或网卡中断计数变化。
