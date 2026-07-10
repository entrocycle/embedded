# RTOS 移植与配置

会创建任务不等于会用 RTOS。工程项目里更关键的是：内核如何和 MCU 启动、中断、SysTick、堆栈、临界区、低功耗和调试工具接起来。

## 移植边界

RTOS 移植通常包含：

- CPU 架构相关汇编。
- 上下文保存和恢复。
- 系统节拍。
- 临界区和中断屏蔽。
- 任务栈初始化。
- heap 管理。
- 中断优先级规则。
- 异常处理钩子。

使用官方移植包时，也要知道这些边界，否则出现 HardFault、任务不调度、队列异常时很难定位。

## 启动顺序

典型流程：

```text
Reset_Handler
  -> runtime init
  -> SystemInit
  -> main
  -> board_init
  -> create tasks
  -> start scheduler
  -> first task
```

检查点：

- 调度器启动前不能依赖任务 API。
- 中断优先级要在启动调度前配置好。
- 系统时钟变更后要确认 tick 配置。
- 任务栈大小不能按“能跑一次”估算。

## Tick 与时间

RTOS 时间基准通常来自 SysTick 或硬件定时器。

关键配置：

| 配置 | 影响 |
|---|---|
| tick rate | 延迟精度、调度频率、功耗 |
| time slicing | 同优先级任务是否轮转 |
| tickless idle | 低功耗空闲能力 |
| software timer | 回调执行上下文和延迟 |

tick rate 不是越高越好。过高会增加调度开销和功耗，过低会影响延迟精度。

## 中断优先级规则

RTOS 下中断分两类：

- 可以调用 RTOS FromISR API 的中断。
- 不允许调用 RTOS API 的高优先级中断。

常见错误：

- 中断优先级数值和实际优先级方向搞反。
- 在过高优先级 ISR 里调用队列/信号量 API。
- ISR 中忘记请求任务切换。
- 任务和 ISR 共享数据没有同步。

建议写一份项目中断表：

| IRQ | 用途 | 优先级 | 是否调用 RTOS API | 最大耗时 |
|---|---|---:|---|---:|
| SysTick | 系统节拍 | 低 | 是 | 短 |
| UART DMA idle | 串口接收 | 中 | 是 | 短 |
| Motor fault | 保护 | 高 | 否 | 极短 |

## 内存管理

RTOS 项目有三类内存：

- 编译期静态内存。
- 任务栈。
- RTOS heap 或 C heap。

建议：

- 核心任务优先静态创建。
- 任务栈初期留足，稳定后根据 high watermark 收敛。
- 运行期少用频繁 malloc/free。
- 明确第三方库是否使用 C heap。
- 把队列、事件组、定时器数量纳入 RAM 预算。

## FreeRTOS 常用配置

必须重点理解：

| 配置 | 说明 |
|---|---|
| `configCPU_CLOCK_HZ` | CPU 时钟 |
| `configTICK_RATE_HZ` | tick 频率 |
| `configMAX_PRIORITIES` | 任务优先级数量 |
| `configMINIMAL_STACK_SIZE` | 最小任务栈 |
| `configTOTAL_HEAP_SIZE` | RTOS heap |
| `configCHECK_FOR_STACK_OVERFLOW` | 栈溢出检查 |
| `configUSE_MUTEXES` | 互斥锁 |
| `configUSE_TIMERS` | 软件定时器 |
| `configUSE_TRACE_FACILITY` | trace 支持 |

建议打开：

- 栈溢出 hook。
- malloc failed hook。
- 断言。
- 运行时间统计或 trace。

## 调试钩子

```c
void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    fault_record("stack_overflow", name);
    board_reboot();
}

void vApplicationMallocFailedHook(void)
{
    fault_record("malloc_failed", NULL);
    board_reboot();
}
```

钩子里不要做复杂操作，重点是记录关键线索并进入安全状态。

## Tickless 与低功耗

tickless idle 的基本思路：

1. 内核判断短时间内没有任务需要运行。
2. 关闭周期 tick。
3. 配置低功耗唤醒源。
4. 进入 sleep/stop。
5. 唤醒后补偿系统 tick。

需要确认：

- 哪些外设在低功耗下继续运行。
- 唤醒源是否可靠。
- 低功耗进出是否影响时钟和外设。
- 调试器连接时是否禁用低功耗。

## 移植检查清单

- [ ] 启动文件和向量表正确。
- [ ] SysTick 或替代 tick 定时器正常。
- [ ] PendSV/SVC 优先级符合要求。
- [ ] 中断优先级分组正确。
- [ ] heap 和任务栈有统计。
- [ ] 栈溢出和 malloc failed hook 已启用。
- [ ] 断言能输出文件和行号。
- [ ] 低功耗路径不会破坏 tick。
- [ ] trace 或任务列表可用。

## 练习

1. 新建一个最小 FreeRTOS 工程，记录启动到第一个任务运行的调用链。
2. 故意把任务栈调小，验证栈溢出 hook 是否触发。
3. 建立项目 IRQ 表，标注哪些中断允许调用 RTOS API。
