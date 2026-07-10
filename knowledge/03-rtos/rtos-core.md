# RTOS 核心机制

本章以 FreeRTOS 思路讲 RTOS 基础，概念同样适用于 RT-Thread、Zephyr 等系统。

## 本章学习方式

RTOS 不是“多写几个任务”，而是把 CPU 时间、内存、事件和共享资源显式管理起来。学习时建议始终围绕四个问题：

- 哪些代码必须及时运行，最坏延迟是多少？
- 哪些代码可以等待，等待什么事件？
- 哪些资源会被多个上下文访问，谁拥有锁？
- 系统卡住时，如何知道卡在哪里？

如果一个 RTOS 设计没有任务表、通信图、资源表和故障策略，它通常还只是 demo。

## 任务

任务是可被调度的执行单元。每个任务通常包含：

- 独立栈。
- 任务控制块 TCB。
- 优先级。
- 状态：就绪、运行、阻塞、挂起。

任务函数：

```c
void sensor_task(void *arg)
{
    (void)arg;

    for (;;) {
        sensor_sample();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

任务必须避免直接返回。若要结束任务，应调用删除 API 或进入安全状态。

任务设计要避免两个极端：

- 把所有功能塞进一个大任务，结果无法表达实时性和阻塞边界。
- 把每个小函数都拆成任务，结果调度、栈和通信开销失控。

一个任务应该对应稳定的执行节奏、清晰的事件来源和可描述的资源所有权。

## 调度

常见调度规则：

- 高优先级任务优先运行。
- 高优先级任务就绪时可抢占低优先级任务。
- 同优先级任务可时间片轮转。
- 阻塞任务不占 CPU。

任务状态转换：

```text
创建 → 就绪 → 运行
          ↑     ↓
          阻塞 ← 等待队列/延时/信号量
```

### 调度推理

判断一个任务能否按时运行，不能只看优先级表，还要看阻塞链。

```text
control_task 等 I2C mutex
  ↓
sensor_task 持有 I2C mutex
  ↓
sensor_task 等日志队列空间
  ↓
logger_task 优先级低且输出阻塞
```

表面上 `control_task` 优先级最高，实际却被低优先级日志路径间接拖住。RTOS 调试的很多问题都来自这种隐藏依赖。

## 延时

`vTaskDelay()` 是相对延时，适合“等待一段时间”。

```c
vTaskDelay(pdMS_TO_TICKS(100));
```

`vTaskDelayUntil()` 是周期调度，适合固定频率任务。

```c
void control_task(void *arg)
{
    TickType_t last = xTaskGetTickCount();

    for (;;) {
        control_loop();
        vTaskDelayUntil(&last, pdMS_TO_TICKS(10));
    }
}
```

控制、采样、心跳类任务优先使用绝对周期。

`vTaskDelay(10ms)` 表示“从当前时刻起阻塞 10ms”，任务执行时间会累积成周期漂移。`vTaskDelayUntil()` 锚定上一次唤醒时间，更适合固定频率循环。做控制或采样时，要记录实际周期和抖动，而不是只看代码里的延时值。

## 队列

队列用于任务间传递数据，线程安全，可阻塞。

```c
typedef struct {
    uint32_t timestamp;
    float temperature;
    float humidity;
} sensor_msg_t;

QueueHandle_t sensor_queue;

void sensor_task(void *arg)
{
    sensor_msg_t msg;

    for (;;) {
        msg.timestamp = xTaskGetTickCount();
        sensor_read(&msg.temperature, &msg.humidity);
        xQueueSend(sensor_queue, &msg, pdMS_TO_TICKS(10));
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void network_task(void *arg)
{
    sensor_msg_t msg;

    for (;;) {
        if (xQueueReceive(sensor_queue, &msg, portMAX_DELAY) == pdTRUE) {
            mqtt_publish_sensor(&msg);
        }
    }
}
```

队列传大对象会拷贝数据。大 buffer 推荐传指针，但要明确所有权。

### 队列满不是小概率事件

队列满时必须有明确策略：

| 策略 | 行为 | 适用 |
|---|---|---|
| 阻塞等待 | 生产者暂停 | 普通任务，允许背压 |
| 丢弃新数据 | 保留旧数据 | 命令、事件不能乱序 |
| 丢弃旧数据 | 保留最新状态 | 传感器状态、UI 状态 |
| 写入离线缓存 | 延后处理 | 网络上报、关键告警 |
| 触发故障 | 进入降级或复位 | 安全关键数据 |

不要在 ISR 中等待队列空间。ISR 投递失败时应增加计数，并让任务决定恢复策略。

## 信号量

二值信号量常用于 ISR 通知任务：

```c
void DMA_IRQHandler(void)
{
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(dma_done_sem, &woken);
    portYIELD_FROM_ISR(woken);
}

void worker_task(void *arg)
{
    for (;;) {
        if (xSemaphoreTake(dma_done_sem, portMAX_DELAY) == pdTRUE) {
            process_dma_buffer();
        }
    }
}
```

注意：ISR 中必须使用 `FromISR` 版本 API。

## 互斥锁

互斥锁保护共享资源，如 I2C 总线、日志输出、文件系统。

```c
xSemaphoreTake(i2c_mutex, portMAX_DELAY);
sensor_read_reg(...);
xSemaphoreGive(i2c_mutex);
```

互斥锁和二值信号量的区别：

- 互斥锁有所有者。
- 互斥锁通常支持优先级继承。
- 互斥锁用于资源保护，信号量用于同步通知。

互斥锁保护的是“资源不被并发破坏”，不是“让流程按顺序发生”。如果你只是想通知另一个任务某件事完成，优先考虑信号量、队列、任务通知或事件组。

## 优先级反转

场景：

1. 低优先级任务 L 拿到 I2C 锁。
2. 高优先级任务 H 等待 I2C 锁。
3. 中优先级任务 M 抢占 L。
4. H 被间接阻塞，实时性受损。

解决：

- 使用支持优先级继承的互斥锁。
- 缩短临界区。
- 避免低优先级任务长期持有关键资源。
- 对关键资源设计专用服务任务。

优先级继承只能缓解“锁持有者被中优先级任务抢占”的情况，不能解决所有设计问题。如果低优先级任务持锁期间做了阻塞 I/O、等待网络、打印日志，继承也无法让高优先级任务立刻拿到资源。

## 事件组

事件组适合等待多个 bit 条件：

```c
#define EVT_WIFI_READY   (1u << 0)
#define EVT_TIME_SYNCED  (1u << 1)
#define EVT_MQTT_READY   (1u << 2)

xEventGroupWaitBits(
    sys_events,
    EVT_WIFI_READY | EVT_TIME_SYNCED,
    pdFALSE,
    pdTRUE,
    portMAX_DELAY
);
```

适用场景：

- 网络状态组合。
- 多模块初始化完成。
- 低功耗唤醒条件。

## 软件定时器

软件定时器运行在定时器服务任务上下文中，不应执行耗时操作。

适合：

- 心跳触发。
- 超时检测。
- 周期状态检查。

不适合：

- 阻塞网络请求。
- 大量计算。
- 长时间持锁。

## 栈和内存

RTOS 常见问题：

- 任务栈太小。
- `printf` 栈占用大。
- 局部数组过大。
- 动态创建对象失败。
- heap 碎片化。

建议：

- 开启栈溢出检测。
- 周期性查看 high water mark。
- 优先使用静态创建接口。
- 每个任务记录最大栈使用量。

### 栈预算方法

任务栈不要靠猜。建议从三类证据确定：

- 静态估计：函数调用深度、局部数组、库函数使用。
- 运行测量：high water mark、栈哨兵、压力路径。
- 故障注入：打开详细日志、触发错误路径、模拟最大消息。

很多系统在正常路径不溢出，但在错误路径里因为 `printf`、JSON、证书解析或递归解析突然栈爆。

## 中断与任务协作

设计原则：

```text
ISR:
  清中断标志
  读取必要状态
  投递信号/队列
  请求调度

Task:
  等待事件
  处理数据
  执行业务逻辑
```

不要在 ISR 中：

- 打印大量日志。
- 等待互斥锁。
- 调用阻塞 API。
- 做复杂协议解析。

## 任务划分示例

环境监测节点：

| 任务 | 优先级 | 周期/触发 | 职责 |
|---|---:|---|---|
| sensor_task | 中 | 1s | 采集传感器 |
| network_task | 中低 | 队列触发 | MQTT 上传 |
| control_task | 高 | 10ms | 控制闭环 |
| logger_task | 低 | 队列触发 | 异步日志 |
| watchdog_task | 高 | 500ms | 健康检查和喂狗 |

### 任务设计评审表

| 项 | 要回答的问题 |
|---|---|
| 触发源 | 周期、队列、中断通知、事件组还是状态机？ |
| 最坏执行时间 | 是否测过最大耗时和抖动？ |
| 阻塞点 | 会等锁、队列、Flash、网络或日志吗？ |
| 栈大小 | 依据是什么，high water mark 多少？ |
| 共享资源 | 访问哪些总线、buffer、文件或全局状态？ |
| 故障行为 | 卡住、队列满、内存不足时谁发现？ |
| 可观测性 | 有心跳、错误计数、任务状态和 trace 吗？ |

## 故障注入

RTOS 学习必须主动制造故障：

- 让一个低优先级任务持锁后长时间 sleep，观察高优先级任务延迟。
- 故意把队列长度设小，记录队列满时系统行为。
- 把某个任务栈减半，触发栈溢出检测。
- 在 ISR 中制造高频事件，观察任务是否处理不过来。
- 让日志输出阻塞，检查是否影响关键任务。

每个实验都要记录任务状态、队列深度、栈 high water mark 和复位原因。

## 深入思考

1. 哪些功能不应该拆成独立任务，而应该作为状态机运行在同一任务内？
2. 如果一个高优先级任务需要访问 I2C 总线，应该直接拿锁，还是通过 I2C 服务任务？
3. 看门狗任务优先级应该很高还是很低？不同选择分别能发现什么问题，又会掩盖什么问题？
4. 任务通知、队列、事件组、信号量都能“通知”，为什么实际工程中不能随便混用？

## 练习

1. 把裸机温湿度采集程序拆成采集任务、显示任务、上传任务。
2. 制造一个优先级反转场景，然后用互斥锁验证优先级继承。
3. 记录每个任务的栈 high water mark，并调整栈大小。
4. 画出你的 RTOS 项目阻塞链，标出每个任务可能等待的锁、队列和外设。
