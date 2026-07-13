# MCU 启动流程与最小系统

理解启动流程后，很多“烧进去不运行”“全局变量初值不对”“一进中断就飞”等问题会变得可定位。

## 本章任务与边界

完成本章后，你应能从上电、启动脚和向量表追到 `Reset_Handler`、C 运行时和 `main()`，检查初始 SP、PC、VTOR、`.data/.bss` 和时钟切换，并保存可符号化的 fault 上下文。

本章以 Cortex-M 为主要模型。不同内核、TrustZone、双核 MCU、厂商 BootROM 和低功耗唤醒路径会增加额外状态；示意向量表和 HardFault 框架必须按目标 CMSIS 与芯片手册实现。

完成证据：调试器停在复位入口和 `main()` 的寄存器快照、与 map 对应的启动地址，以及一次故意错误链接/向量配置后的故障定位。串口出现日志只证明已走到日志点，不证明启动上下文全部正确。

## 最小硬件系统

典型 MCU 最小系统包含：

- 电源：稳定的 VDD/VDDA，去耦电容。
- 复位：NRST 上拉、复位按键或电源监控。
- 时钟：内部 RC 或外部晶振。
- 启动配置：BOOT0/BOOT1 或 option bytes。
- 下载调试：SWDIO、SWCLK、GND、RESET。

## 启动来源

常见启动来源：

| 启动源 | 用途 |
|---|---|
| Main Flash | 正常应用启动 |
| System Memory/BootROM | 串口/USB/DFU ISP 下载 |
| SRAM | 调试或特殊启动 |

启动地址必须和链接脚本、向量表位置一致。

## 向量表

Cortex-M 向量表的第 0 项是初始栈顶，第 1 项是复位处理函数，后面是异常和中断入口。

```c
extern unsigned long _estack;
void Reset_Handler(void);
void Default_Handler(void);

__attribute__((section(".isr_vector")))
void (* const vector_table[])(void) = {
    (void (*)(void))(&_estack),
    Reset_Handler,
    Default_Handler,   // NMI
    Default_Handler,   // HardFault
};
```

## Reset_Handler

Reset_Handler 是 C 运行环境的入口，不是业务逻辑入口。它负责把 RAM 准备好。

```text
Reset_Handler
  ├── 拷贝 .data 初值：Flash -> RAM
  ├── 清零 .bss：RAM = 0
  ├── SystemInit：时钟、FPU、向量表偏移
  ├── __libc_init_array：C++ 构造函数等
  └── main
```

## 时钟树

时钟配置影响所有外设：

- CPU 运行频率。
- AHB/APB 总线频率。
- UART 波特率。
- ADC 采样时钟。
- Timer 计数频率。
- USB、SDIO、以太网等特殊时钟。

调试时钟问题的顺序：

1. 确认晶振是否起振。
2. 确认 PLL 配置是否满足数据手册限制。
3. 确认 Flash wait states 是否匹配主频。
4. 确认 APB 分频是否影响外设。
5. 用 MCO 或定时器输出测频。

## 中断基本流程

1. 外设产生中断请求。
2. NVIC 判断中断使能和优先级。
3. CPU 自动压栈部分寄存器。
4. 跳转到中断服务函数。
5. ISR 清除中断标志并处理最小逻辑。
6. CPU 弹栈返回被打断的位置。

ISR 原则：

- 快速。
- 不做长时间阻塞。
- 不调用不可重入函数。
- 只做采样、清标志、投递事件。
- 复杂处理交给主循环或任务。

## HardFault 定位

常见原因：

- 空指针/野指针。
- 栈溢出。
- 访问非法地址。
- 未对齐访问。
- 中断向量错误。
- 外设时钟未使能就访问寄存器。

建议保留一个 HardFault 框架，能打印或保存：

- PC、LR、SP。
- xPSR。
- CFSR/HFSR/MMFAR/BFAR。
- 当前任务名或模块状态。

## 最小主循环

```c
int main(void)
{
    system_clock_init();
    board_init();
    uart_log_init();

    log_info("boot ok\r\n");

    while (1) {
        app_poll();
    }
}
```

裸机主循环的关键是“非阻塞”。如果某个函数内部长时间等待，会拖慢整个系统。

## 练习

1. 画出你的开发板从上电到 `main()` 的流程图。
2. 改错启动地址，观察程序表现，再恢复。
3. 禁用某个外设时钟后访问它，记录现象并解释原因。

## 深入思考

启动流程是理解 MCU 的主线：CPU 不是“自动运行 C 程序”，而是从固定地址取初始栈指针和复位向量，执行汇编启动代码，再建立 C 运行环境。理解这条链路后，很多 Bootloader、异常向量、低功耗唤醒和 HardFault 问题都会变得可解释。

启动阶段要特别关注不变量：

- 初始 SP 必须落在有效 RAM 范围内并满足对齐要求。
- Reset_Handler 地址必须落在可执行区域，且 Thumb 位正确。
- `.data` 必须从 Flash 拷贝到 RAM，`.bss` 必须清零。
- 系统时钟切换前后，Flash wait state 和外设时钟假设必须匹配。
- 开中断前，向量表、外设标志位和中断优先级必须处于已知状态。

开放问题：

1. Bootloader 跳转 App 前为什么要关闭或清理中断、SysTick 和外设状态？
2. 如果某个全局变量上电后不是 0，你会检查启动文件、链接脚本、段属性还是编译选项？
3. 为什么“能进入 main”不等于启动正确？还需要验证哪些时钟、栈、向量表和运行时条件？
