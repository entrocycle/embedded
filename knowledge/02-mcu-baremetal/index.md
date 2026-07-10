# MCU 裸机开发

MCU 裸机开发训练的是对硬件的直接控制能力。即使后续使用 RTOS、HAL 或 Linux，裸机阶段建立的启动、时钟、中断、寄存器和外设模型仍然是底层调试的基础。

## 章节

- [MCU 启动流程与最小系统](mcu-startup.md)
- [时钟、中断与 DMA](clock-interrupt-dma.md)
- [寄存器与外设驱动方法](peripheral-driver.md)
- [传感器与执行器](sensor-actuator.md)
- [Bootloader 与 OTA 基础](bootloader-ota.md)
- [低功耗设计](low-power.md)

## MCU 系统组成

```text
┌──────────────────────────────┐
│ Cortex-M Core                │
│  NVIC / SysTick / MPU / FPU  │
├──────────────────────────────┤
│ Bus Matrix / AHB / APB       │
├──────────────────────────────┤
│ Flash / SRAM / DMA           │
├──────────────────────────────┤
│ GPIO UART SPI I2C ADC TIM    │
└──────────────────────────────┘
```

## 裸机主线

1. 上电复位。
2. BootROM 或 Flash 启动。
3. CPU 从向量表取栈顶和复位入口。
4. 启动代码初始化数据段。
5. 配置时钟和基础外设。
6. 进入 `main()`。
7. 主循环和中断共同驱动系统。

## 必备外设顺序

建议按以下顺序学习：

1. GPIO：理解时钟、模式、输入输出。
2. UART：建立日志和调试通道。
3. SysTick/Timer：建立时间基准。
4. EXTI/NVIC：理解中断。
5. SPI/I2C：连接常见传感器和显示屏。
6. ADC/PWM：模拟采集和控制输出。
7. DMA：理解高吞吐和低 CPU 占用。
8. Watchdog/RTC/低功耗：走向工程化。
9. Bootloader/OTA：走向可维护产品。

补充主线：

- 通过 [时钟、中断与 DMA](clock-interrupt-dma.md) 把时间基准、异步事件和数据搬运串起来。
- 通过 [传感器与执行器](sensor-actuator.md) 把外设 API 落到真实采集和控制链路。

## 深挖问题

- 上电后 CPU 为什么能找到第一条指令？向量表、栈顶和链接地址如何配合？
- 一个外设“初始化成功”的证据是什么：寄存器、日志、波形还是数据？
- 中断、DMA 和主循环同时访问数据时，所有权如何划分？
- 低功耗不是调用 sleep API 就结束，哪些 GPIO、外设和传感器会让电流下不去？

## 阶段产出

- 一个从启动到 `main()` 的最小工程，能解释启动文件和链接脚本。
- 至少一个 UART/I2C/SPI 驱动实验，包含错误码、超时和波形记录。
- 一份低功耗测量表，按状态记录电流、停留时间和异常原因。
