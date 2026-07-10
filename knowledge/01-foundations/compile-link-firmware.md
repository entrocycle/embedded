# 编译、链接与固件镜像

嵌入式工程必须理解“源码如何变成可以烧录的固件”。这条链路出了问题，会表现为无法启动、HardFault、变量初值错误、Flash/RAM 超限、下载后运行异常。

## 从源码到固件

```text
.c/.s
  ↓ 预处理
.i
  ↓ 编译
.s
  ↓ 汇编
.o
  ↓ 链接
.elf
  ↓ objcopy
.bin/.hex
```

| 文件 | 含义 |
|---|---|
| `.o` | 目标文件，包含机器码片段和符号 |
| `.a` | 静态库，本质是一组 `.o` |
| `.elf` | 可执行文件，包含段、符号、调试信息 |
| `.bin` | 纯二进制镜像，适合直接烧录到指定地址 |
| `.hex` | Intel HEX 文本格式，带地址信息 |
| `.map` | 链接器输出的内存布局和符号表 |

## 编译选项

Cortex-M 常见编译选项：

```makefile
CFLAGS += -mcpu=cortex-m4 -mthumb
CFLAGS += -ffunction-sections -fdata-sections
CFLAGS += -Wall -Wextra -Werror
CFLAGS += -Og -g3
LDFLAGS += -Wl,--gc-sections
LDFLAGS += -T board.ld
```

说明：

- `-mcpu` 和 `-mthumb` 决定目标指令集。
- `-ffunction-sections -fdata-sections` 把每个函数/数据放入独立 section。
- `--gc-sections` 删除未引用代码，减小固件。
- `-Og -g3` 适合调试，发布版本常用 `-Os` 或 `-O2`。
- `-T board.ld` 指定链接脚本。

## 链接脚本

链接脚本决定每个段放到哪里。

```ld
MEMORY
{
  FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 512K
  RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 128K
}

_estack = ORIGIN(RAM) + LENGTH(RAM);

SECTIONS
{
  .isr_vector :
  {
    KEEP(*(.isr_vector))
  } > FLASH

  .text :
  {
    *(.text*)
    *(.rodata*)
  } > FLASH

  .data :
  {
    _sdata = .;
    *(.data*)
    _edata = .;
  } > RAM AT > FLASH

  _sidata = LOADADDR(.data);

  .bss :
  {
    _sbss = .;
    *(.bss*)
    *(COMMON)
    _ebss = .;
  } > RAM
}
```

关键点：

- `.isr_vector` 必须放在启动地址，通常是 Flash 起始位置。
- `.text` 和 `.rodata` 放 Flash。
- `.data` 运行在 RAM，但初值存放在 Flash，启动时要拷贝。
- `.bss` 运行在 RAM，启动时要清零。
- `_estack` 提供初始栈顶。

## 启动代码做了什么

复位后 CPU 从向量表读取：

1. 初始 MSP 栈顶。
2. Reset_Handler 地址。

Reset_Handler 通常执行：

1. 设置运行环境。
2. 将 `.data` 初值从 Flash 拷贝到 RAM。
3. 将 `.bss` 清零。
4. 调用 `SystemInit()` 配置时钟等基础硬件。
5. 调用 `main()`。

伪代码：

```c
void Reset_Handler(void)
{
    uint32_t *src = &_sidata;
    uint32_t *dst = &_sdata;

    while (dst < &_edata) {
        *dst++ = *src++;
    }

    for (dst = &_sbss; dst < &_ebss; ) {
        *dst++ = 0;
    }

    SystemInit();
    main();

    while (1) {
    }
}
```

## 查看固件大小

```bash
arm-none-eabi-size firmware.elf
```

输出示例：

```text
   text    data     bss     dec     hex filename
  42320    1280   10480   54080    d340 firmware.elf
```

含义：

- `text` 主要占 Flash。
- `data` 同时占 Flash 和 RAM。
- `bss` 占 RAM。

RAM 约为 `data + bss + heap + stack`，不能只看 `bss`。

## 常见链接错误

| 错误 | 原因 | 处理 |
|---|---|---|
| undefined reference | 声明了但没有定义，或源文件未加入构建 | 检查源文件、库和函数名 |
| multiple definition | 多个 `.c` 定义同名全局符号 | 头文件只放声明，定义放一个 `.c` |
| region FLASH overflowed | 代码/常量超出 Flash | 开启优化、裁剪模块、换芯片 |
| region RAM overflowed | 全局数据、栈、堆超出 RAM | 减少 buffer、放 Flash、外扩 RAM |
| HardFault after reset | 向量表、栈顶、启动代码或链接地址错误 | 查链接脚本和启动文件 |

## Makefile 最小示例

```makefile
TARGET := firmware
CC := arm-none-eabi-gcc
OBJCOPY := arm-none-eabi-objcopy
SIZE := arm-none-eabi-size

CFLAGS := -mcpu=cortex-m4 -mthumb -Wall -Wextra -Og -g3
LDFLAGS := -T board.ld -Wl,--gc-sections

SRCS := startup.s main.c system.c
OBJS := $(SRCS:.c=.o)
OBJS := $(OBJS:.s=.o)

all: $(TARGET).elf $(TARGET).bin
	$(SIZE) $(TARGET).elf

$(TARGET).elf: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -f $(OBJS) $(TARGET).elf $(TARGET).bin
```

## 练习

1. 修改链接脚本，把应用起始地址改为 `0x08008000`，思考 Bootloader 场景要改哪些地方。
2. 在代码中定义大数组，观察 `.bss` 和 RAM 占用变化。
3. 用 `objdump -d` 反汇编固件，找到 `main` 和中断函数。

