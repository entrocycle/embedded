# C 语言与内存模型

嵌入式 C 的核心不是语法数量，而是“类型、地址、生命周期、可见性、并发上下文”这五件事。

## 本章任务与边界

完成本章后，你应能为一段 C 代码中的每个对象标注存储区、长度、生命周期、所有权和访问者；能通过符号、段信息和边界测试验证判断；能解释指针、结构体布局、位操作和共享变量在何种条件下失效。

本章不系统覆盖 C 标准全部语法，也不把某一 MCU 的原子访问宽度当作通用保证。涉及 ISR、DMA、cache 和多核的结论，需要结合 [CPU、内存映射与并发](cpu-memory-concurrency.md) 以及目标架构手册验证。

完成证据至少包括：一份对象与内存段分析、仓库 `labs/ring-buffer` 的失败注入记录，以及一个真实驱动 buffer 的所有权契约。实验入口见 [实验地图](../00-learning-path/labs-map.md)。

## 本章学习方式

学 C 不要只问“这个语法怎么写”，而要追问三个更底层的问题：

- 这个对象在哪里：Flash、RAM、栈、堆、寄存器还是外设地址空间？
- 谁会访问它：主循环、ISR、DMA、另一个任务、编译器优化器还是硬件？
- 访问规则是什么：生命周期、对齐、原子性、可见性和所有权是否明确？

如果能把一段 C 代码翻译成“地址、长度、读写者、时序和副作用”，你才真正掌握了嵌入式 C。

## 变量和存储区

| 写法 | 常见存储位置 | 生命周期 | 典型用途 |
|---|---|---|---|
| 局部变量 | 栈 | 函数调用期间 | 临时计算 |
| 全局变量 | `.data` 或 `.bss` | 程序整个运行期 | 模块状态 |
| `static` 局部变量 | `.data` 或 `.bss` | 程序整个运行期 | 函数内部持久状态 |
| `const` 全局变量 | `.rodata`/Flash | 程序整个运行期 | 表、配置、字符串 |
| 动态分配 | heap | `malloc` 到 `free` | 可变长度对象 |

典型内存布局：

```text
Flash:
  .isr_vector  中断向量表
  .text        代码
  .rodata      常量
  .data load   已初始化变量的初值

RAM:
  .data        已初始化全局/静态变量
  .bss         未初始化全局/静态变量
  heap         动态分配区
  stack        主栈/任务栈
```

### 从 C 代码推导内存布局

看下面这段代码时，不要先想输出结果，先标注每个对象的存储区。

```c
const char fw_name[] = "env-node";
uint32_t boot_count;
static uint8_t rx_dma_buf[256];

void sample_once(void)
{
    uint16_t adc_raw[16];
    static uint32_t seq;

    seq++;
    boot_count++;
    (void)adc_raw;
}
```

推导结果：

| 对象 | 位置 | 原因 | 风险 |
|---|---|---|---|
| `fw_name` | `.rodata`/Flash | 全局 `const` 常量 | 若被强转写入会异常或无效 |
| `boot_count` | `.bss` | 未初始化全局变量 | 多上下文访问需保护 |
| `rx_dma_buf` | `.bss` | 静态数组 | DMA cache 一致性、对齐要求 |
| `adc_raw` | 栈 | 函数局部数组 | 栈过小会溢出 |
| `seq` | `.bss` | 静态局部变量 | 函数内部持久状态，非线程安全 |

这类推导要和 `.map` 文件互相验证。不要只相信直觉。

## 指针

指针保存地址。嵌入式里常见的指针用途：

- 访问内存映射寄存器。
- 传递缓冲区给驱动、DMA、协议栈。
- 描述数组、字符串、结构体。
- 实现回调函数和模块接口。

寄存器访问例子：

```c
#include <stdint.h>

#define GPIOA_BASE      0x40020000UL
#define GPIOA_MODER     (*(volatile uint32_t *)(GPIOA_BASE + 0x00))
#define GPIOA_ODR       (*(volatile uint32_t *)(GPIOA_BASE + 0x14))

void gpioa_pin5_output(void)
{
    GPIOA_MODER &= ~(0x3u << (5u * 2u));
    GPIOA_MODER |=  (0x1u << (5u * 2u));
}

void gpioa_pin5_set(int high)
{
    if (high) {
        GPIOA_ODR |= (1u << 5u);
    } else {
        GPIOA_ODR &= ~(1u << 5u);
    }
}
```

`volatile` 的作用是阻止编译器把访问优化掉，因为寄存器值可能由硬件改变。

### 指针的所有权

嵌入式代码里，指针错误经常不是“语法不懂”，而是所有权没说清楚。

| 场景 | 谁拥有 buffer | 常见错误 | 更稳妥的表达 |
|---|---|---|---|
| 驱动同步读取 | 调用者 | 长度不匹配、空指针 | `read(buf, len)` 返回实际长度和错误码 |
| DMA 接收 | DMA 与驱动共享 | CPU 提前读、cache 未失效 | 明确 buffer 对齐、区域和完成事件 |
| 协议解析 | 上层或协议栈 | 保存临时 buffer 指针 | 需要长期保存时复制数据 |
| 回调参数 | 调用方或框架 | 回调后继续使用失效地址 | 文档写清生命周期 |

接口设计时至少写清楚：谁分配、谁释放、什么时候可以读写、失败时内容是否有效。

## 指针常见错误

| 错误 | 现象 | 预防 |
|---|---|---|
| 空指针解引用 | HardFault、崩溃 | 初始化后检查 |
| 野指针 | 随机异常 | 指针释放后置空，限制作用域 |
| 越界访问 | 数据被破坏 | 明确长度，统一边界检查 |
| 栈上对象地址返回 | 后续访问异常 | 不返回局部变量地址 |
| 类型强转错误 | 对齐异常、数据错乱 | 用结构体和 `memcpy` 谨慎处理 |

## 位操作

寄存器通常由多个 bit 字段组成，位操作是驱动开发的基础。

```c
#define BIT(n)              (1u << (n))
#define SET_BITS(reg, m)    ((reg) |= (m))
#define CLR_BITS(reg, m)    ((reg) &= ~(m))
#define READ_BITS(reg, m)   ((reg) & (m))

static inline void modify_bits(volatile uint32_t *reg, uint32_t mask, uint32_t value)
{
    uint32_t tmp = *reg;
    tmp &= ~mask;
    tmp |= value & mask;
    *reg = tmp;
}
```

注意：读改写不是天然原子操作。如果寄存器也会在中断中修改，需要关中断、使用原子寄存器或明确访问上下文。

### 位操作背后的并发问题

下面的写法在单上下文里通常没问题：

```c
GPIOA_ODR |= (1u << 5u);
```

但它实际包含“读寄存器、修改临时值、写回寄存器”三个动作。如果主循环设置 bit5 的同时，中断修改 bit6，就可能发生覆盖。很多 MCU 提供 BSRR、SET/CLR、OUTSET/OUTCLR 这类原子置位/清零寄存器，就是为了解决这个问题。

判断一个寄存器写法是否安全时，问四件事：

- 有没有其他上下文也写同一个寄存器？
- 写 1 清零、写 0 保持、读清标志这类特殊语义是否存在？
- 是否有硬件自动修改位？
- 数据手册是否提供专门的 set/clear/toggle 寄存器？

## 结构体和对齐

结构体适合表达寄存器组、协议帧和模块状态。

```c
typedef struct {
    volatile uint32_t MODER;
    volatile uint32_t OTYPER;
    volatile uint32_t OSPEEDR;
    volatile uint32_t PUPDR;
    volatile uint32_t IDR;
    volatile uint32_t ODR;
} gpio_regs_t;

#define GPIOA ((gpio_regs_t *)0x40020000UL)
```

协议帧不要轻易直接强转结构体，原因是字节序、对齐和 padding 可能不同。跨平台协议更推荐手动编码/解码：

```c
uint16_t read_u16_be(const uint8_t *p)
{
    return ((uint16_t)p[0] << 8) | p[1];
}
```

### 对齐不是性能细节

在一些架构上，非对齐访问会变慢；在另一些架构上，可能直接触发 fault。DMA、cache line 和总线访问也常常有对齐要求。

工程上建议：

- 协议帧用字节数组编码，不依赖结构体 padding。
- 外设寄存器结构体只用于明确匹配手册布局的场景。
- DMA buffer 根据芯片要求做对齐，并放在 DMA 可访问内存区。
- 需要跨编译器或跨平台的数据结构，不把 `sizeof(struct)` 当作协议长度。

## `const`、`static`、`extern`

- `const` 表示只读，常用于查表、版本信息、固定配置。
- `static` 修饰全局函数/变量时，限制在当前文件可见。
- `static` 修饰局部变量时，让变量生命周期延长到整个程序运行期。
- `extern` 用于声明其他文件定义的变量或函数，但应避免滥用全局变量。

模块状态推荐封装在 `.c` 文件中：

```c
static int sensor_ready;

int sensor_init(void)
{
    sensor_ready = 1;
    return 0;
}

int sensor_is_ready(void)
{
    return sensor_ready;
}
```

## 栈和堆

裸机或 RTOS 中，栈空间通常很小。任务栈过小会导致随机异常。

栈使用风险：

- 大数组放在局部变量中。
- 函数调用层级过深。
- 中断嵌套过多。
- `printf` 等库函数栈占用超预期。

堆使用风险：

- 内存泄漏。
- 碎片化。
- 分配失败未处理。
- 多任务并发访问 allocator。

嵌入式工程中常见策略：

- 初始化阶段可以动态分配，运行阶段尽量不分配。
- 用静态内存池替代频繁 `malloc/free`。
- RTOS 中为队列、任务、定时器使用静态创建 API。

## 中断和共享变量

中断和主循环/任务之间共享变量时，至少要考虑：

- 变量是否需要 `volatile`。
- 读写是否原子。
- 是否可能出现读一半被打断。
- 是否需要临界区保护。

例子：

```c
static volatile uint32_t tick_count;

void SysTick_Handler(void)
{
    tick_count++;
}

uint32_t get_tick(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    uint32_t value = tick_count;

    __set_PRIMASK(primask);
    return value;
}
```

不能在临界区末尾无条件调用 `__enable_irq()`：若调用者进入函数前已经屏蔽中断，这会意外破坏外层临界区。上例保存并恢复 `PRIMASK`，适用于 Cortex-M 的示意；实际项目优先使用芯片 SDK 或 RTOS 提供的可嵌套临界区 API。

在 32 位 MCU 上，对齐的 32 位单次读写通常是原子的，但必须由目标架构保证。读取多个相关变量、执行读改写或与 DMA/多核共享时，单次访问原子仍不足以保证快照一致。若这里仅有单个 `tick_count` 的单写者/单读者，可在确认架构原子性后省去临界区；示例保留临界区是为了演示正确的状态恢复，而不是要求每次读都关中断。

### `volatile` 不能解决什么

`volatile` 只能约束编译器访问这个对象的方式，它不能：

- 保证多字节读写一定原子。
- 保证多个变量之间的一致性。
- 替代互斥锁、临界区或内存屏障。
- 解决 cache 与 DMA 的一致性。
- 让复杂表达式变成线程安全。

如果 ISR 更新 `head`，主循环更新 `tail`，环形缓冲的正确性来自单生产者/单消费者设计、索引宽度、临界区和边界规则，而不是只靠 `volatile`。

## 深入思考

1. 为什么 `.data` 的初值放在 Flash，而运行时变量又在 RAM？启动代码必须做什么？
2. 如果 DMA 正在写 `rx_dma_buf`，CPU 同时解析这个 buffer，可能出现哪些不一致？
3. `volatile uint32_t *p`、`uint32_t * volatile p`、`volatile uint32_t * volatile p` 分别限制了什么？
4. 一个驱动接口接收 `uint8_t *buf` 和 `const uint8_t *buf` 时，表达的所有权有什么不同？

## 实验与复盘

建议做一个小实验仓库，至少包含：

- 一个程序定义全局变量、静态变量、常量字符串、局部大数组，编译后查看 `.map` 文件。
- 一个故意栈溢出的例子，记录现象、PC/LR/SP 和修复方式。
- 一个主循环与 ISR 共享计数器的例子，对比加/不加临界区的行为。
- 一份复盘：列出每个对象的位置、访问者、生命周期和风险。

## 练习

1. 写一个 `ring_buffer_t`，支持中断写入、主循环读取。
2. 写一个 `set_field(reg, mask, shift, value)` 宏，并用它配置一个假想寄存器。
3. 编译一个包含全局变量、静态变量、常量字符串的程序，查看 `.map` 文件中它们的位置。
4. 为一个 DMA 接收接口写注释，说明 buffer 对齐、所有权、完成事件和 cache 处理要求。

## 工程洞察

C 语言在嵌入式中强大，是因为它让程序员能接近内存、寄存器和 ABI；C 语言危险，也是因为它默认相信程序员已经定义好生命周期、所有权、边界和并发关系。很多嵌入式故障并不是“不懂 C 语法”，而是没有把 C 对象放进系统运行环境中理解。

写每一个接口时，建议显式回答：

- 谁分配这段内存，谁释放或复用它？
- 调用者能否在函数返回后继续访问传入指针？
- 这个对象会被 ISR、DMA、另一个任务或硬件同时访问吗？
- 长度、对齐、空指针、溢出和截断如何处理？
- 结构体布局是否会跨编译器、CPU 字节序和协议版本保持稳定？

当你能从一段 C 代码中画出对象位置、生命周期、访问者和失效模式，就已经超越了“写语法”的层次，开始具备设计可靠固件接口的能力。

开放问题：

1. 为什么把通信协议直接映射成 C 结构体很诱人，也很危险？
2. 一个函数返回局部数组地址为什么错误？如果这个错误偶尔能跑通，说明了什么？
3. 在资源受限系统中完全不用堆是否总是更好？静态分配、内存池和动态分配分别把复杂度转移到哪里？
