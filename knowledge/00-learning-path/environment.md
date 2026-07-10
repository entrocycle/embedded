# 嵌入式工程环境准备

环境准备的目标不是装最多的软件，而是形成一套可重复、可调试、可迁移的工作流。

## 基础软件

| 类别 | 推荐工具 | 用途 |
|---|---|---|
| 编辑器 | VS Code、CLion、STM32CubeIDE | 编辑、索引、调试 |
| MCU 工具链 | `arm-none-eabi-gcc` | Cortex-M 裸机/RTOS 编译 |
| Linux 交叉工具链 | `aarch64-linux-gnu-gcc`、`arm-linux-gnueabihf-gcc` | 嵌入式 Linux 应用/内核模块 |
| 构建系统 | Make、CMake、Ninja | 自动化构建 |
| 调试 | GDB、OpenOCD、pyOCD | 下载、断点、单步 |
| 串口 | minicom、picocom、MobaXterm、串口助手 | 日志和交互 |
| 协议分析 | Wireshark、逻辑分析仪软件 | 网络和总线分析 |
| 版本管理 | Git | 工程协作和变更追踪 |

## 硬件准备

入门建议准备两类板卡：

| 类型 | 示例 | 学习价值 |
|---|---|---|
| MCU 开发板 | STM32 Nucleo、ESP32 DevKit、nRF52 DK | 裸机、RTOS、外设、低功耗、BLE/Wi-Fi |
| Linux 开发板 | Raspberry Pi、正点原子/野火 i.MX6ULL、RK356x 开发板 | U-Boot、Kernel、DTS、驱动、RootFS |

常用仪器：

- ST-Link/J-Link：下载和 SWD/JTAG 调试。
- USB 转串口：查看启动日志和 shell。
- 万用表：检查供电、电平、短路。
- 逻辑分析仪：分析 UART/SPI/I2C/CAN 等数字总线。
- 示波器：分析电源纹波、时钟、PWM、模拟信号。
- 可调电源或功耗计：低功耗和稳定性调试。

## 推荐目录结构

```text
embedded-workspace/
├── toolchains/          # 交叉编译工具链
├── boards/              # 板级配置、原理图、数据手册
├── projects/            # 实验和项目
├── third_party/         # HAL、RTOS、协议栈等
├── scripts/             # 下载、构建、测试脚本
└── notes/               # 调试记录和复盘
```

## 最小工具链验证

安装 `arm-none-eabi-gcc` 后验证：

```bash
arm-none-eabi-gcc --version
arm-none-eabi-objdump --version
arm-none-eabi-size --version
```

编译固件后常用检查：

```bash
arm-none-eabi-size firmware.elf
arm-none-eabi-objdump -d firmware.elf > firmware.asm
arm-none-eabi-objcopy -O binary firmware.elf firmware.bin
```

## 调试连接检查

OpenOCD 连接 ST-Link 的典型命令：

```bash
openocd -f interface/stlink.cfg -f target/stm32f4x.cfg
```

另一个终端连接 GDB：

```bash
arm-none-eabi-gdb build/firmware.elf
(gdb) target remote :3333
(gdb) monitor reset halt
(gdb) load
(gdb) break main
(gdb) continue
```

## 环境原则

- 工具链版本要记录在 README 或构建脚本中。
- 生成文件不要手工改，用户代码放在明确的模块中。
- 下载、擦除、调试命令尽量脚本化。
- 每个实验保留硬件连接、软件版本、现象和结论。
- 出问题先确认供电、时钟、复位、下载口、串口波特率，再看代码。

