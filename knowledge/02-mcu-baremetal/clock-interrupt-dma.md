# 时钟、中断与 DMA

时钟、中断和 DMA 是 MCU 裸机开发的三根主线。GPIO 点灯可以绕开它们，但只要系统涉及稳定通信、实时响应和高吞吐数据，就必须理解这三者如何协作。

## 时钟树

时钟决定 CPU 和外设运行速度，也决定 UART 波特率、定时器周期、ADC 采样速度、USB 精度和功耗。

典型时钟链路：

```text
HSE/HSI/PLL
  -> SYSCLK
  -> AHB
  -> APB1/APB2
  -> Timer/UART/SPI/I2C/ADC/USB
```

配置时钟时要确认：

- 时钟源：内部 RC、外部晶振、PLL。
- PLL 输入和倍频是否在规格范围内。
- AHB/APB 分频是否影响外设时钟。
- Flash wait states 是否匹配主频。
- USB、SDIO、ADC 等专用时钟是否满足精度。
- 低功耗唤醒后是否需要重新配置 PLL。

## 时钟问题特征

| 现象 | 可能原因 |
|---|---|
| UART 波特率偏差 | 系统时钟或 APB 时钟配置错误 |
| I2C/SPI 速度不对 | 外设时钟源或预分频错误 |
| 高主频下随机异常 | Flash wait states、供电或温度不满足 |
| USB 枚举失败 | 48 MHz 时钟精度不够 |
| 低功耗唤醒后外设异常 | 唤醒后时钟树未恢复 |

## 中断模型

中断让外设事件打断主程序，由 ISR 快速处理。

中断链路：

```text
外设事件
  -> 外设状态位和中断使能
  -> NVIC pending
  -> CPU 入栈
  -> ISR
  -> 清除标志
  -> 退出中断
```

必须同时配置：

- 外设内部中断使能。
- NVIC 中断使能。
- 优先级。
- 中断服务函数名称和向量表。
- 正确清除中断标志。

## 中断设计规则

- ISR 尽量短，只做清标志、搬少量数据、投递事件。
- 不在 ISR 里做阻塞等待。
- 不在 ISR 里调用不可重入函数。
- ISR 和主循环共享变量要使用 `volatile`，必要时加临界区。
- RTOS 下只调用 FromISR 版本 API。
- 统计 ISR 触发次数和最大执行时间。

## 优先级

Cortex-M 中断优先级要理解两个概念：

- 抢占优先级：决定一个中断能否打断另一个中断。
- 子优先级：决定同级 pending 时谁先执行。

常见策略：

| 中断类型 | 建议 |
|---|---|
| 硬实时保护 | 最高优先级，逻辑极短 |
| 通信接收 | 高优先级，快速入队 |
| 系统节拍 | 稳定优先级，不要被长 ISR 干扰 |
| 普通传感器 | 中低优先级 |
| 调试和日志 | 低优先级 |

RTOS 项目还要遵守内核对“可调用系统 API 的最高中断优先级”的限制。

## DMA 模型

DMA 用来减少 CPU 搬运数据：

```text
外设数据寄存器 <-> DMA <-> 内存 buffer
```

适合场景：

- UART 高速接收。
- ADC 多通道连续采样。
- SPI 显示屏刷新。
- I2S/SAI 音频流。
- 大块内存拷贝。

核心配置：

| 项 | 说明 |
|---|---|
| 方向 | 外设到内存、内存到外设、内存到内存 |
| 地址递增 | 外设地址通常固定，内存地址通常递增 |
| 数据宽度 | byte、halfword、word 必须匹配 |
| 长度 | 搬运元素个数，不一定是字节数 |
| 模式 | normal、circular、double buffer |
| 中断 | half complete、complete、error |

## DMA 与 Cache

带 D-Cache 的 MCU 或 MPU/MPU-like SoC 上，DMA 和 CPU 看到的内存可能不一致。

典型问题：

- CPU 改了发送 buffer，但数据还在 cache，DMA 读到旧数据。
- DMA 写了接收 buffer，但 CPU cache 里仍是旧数据。

处理方式：

- 发送前 clean cache。
- 接收后 invalidate cache。
- 把 DMA buffer 放到 non-cacheable 区域。
- 对齐 cache line。

## UART DMA 接收

常用方案是 circular DMA + idle line 中断：

```text
DMA 持续写入环形 buffer
UART 检测到 idle
  -> 计算新增数据长度
  -> 投递到协议解析任务
  -> 继续接收
```

注意事项：

- idle 中断要清除正确。
- 计算 DMA 写指针时处理回绕。
- 协议解析不要在 ISR 中完成。
- buffer 满要统计丢包。

## 调试检查清单

- [ ] 外设时钟已打开。
- [ ] GPIO 复用已配置。
- [ ] 外设状态位符合预期。
- [ ] NVIC 已使能。
- [ ] ISR 名称和启动文件一致。
- [ ] 中断标志已清除。
- [ ] DMA 请求源正确。
- [ ] DMA buffer 地址和对齐正确。
- [ ] 数据宽度和长度单位正确。
- [ ] cache 处理正确。

## 练习

1. 改变系统时钟，观察 UART 实际波特率变化。
2. 写一个按键 EXTI 中断，只在 ISR 中投递事件，主循环处理消抖。
3. 用 ADC + DMA circular 模式采样 3 个通道，并统计半传输和完成中断次数。
