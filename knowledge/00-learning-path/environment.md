# 嵌入式工程环境准备

环境准备的目标不是装最多的软件，而是形成一套可重复、可调试、可迁移的工作流。

## 先选择一条可完成路径

不要在第一天同时安装 MCU、Linux、RTOS、仿真器和所有 IDE。先完成主机路径，再按目标添加交叉工具链：

| 路径 | 需要的设备 | 第一次可验证产出 |
|---|---|---|
| Host C | macOS/Linux/WSL 电脑 | 可执行文件、测试结果、反汇编、故障注入报告 |
| MCU | Cortex-M 开发板 + 调试器 + 串口 | ELF/map/bin、断点停在 `main`、串口启动日志 |
| Embedded Linux | Linux 板或 QEMU | 启动日志、交叉编译程序、systemd 服务 |

主机实验不能证明外设时序、中断和功耗，但适合先训练 C、状态机、序列化、测试和证据记录。之后把相同的纯逻辑模块移植到目标板，专门验证平台假设。

## 第一次成功：90 分钟闭环

目标不是“装好环境”，而是在没有开发板的情况下得到四类证据：编译产物、程序输出、内存/符号信息和故障测试结果。

### 0-15 分钟：确认基础工具

macOS 安装 Command Line Tools：

```bash
xcode-select --install
```

Debian/Ubuntu/WSL：

```bash
sudo apt update
sudo apt install build-essential git python3
```

Fedora：

```bash
sudo dnf install @development-tools git python3
```

验证并把输出保存到实验记录：

```bash
cc --version
make --version
git --version
python3 --version
```

Windows 初学者建议使用 WSL2 的 Ubuntu 环境完成主机和 Linux 实验；若使用原生 IDE，命令、路径、调试器驱动和脚本行为会不同，应在记录中注明，不要混用两套产物。

### 15-35 分钟：编译一个可检查程序

```bash
mkdir -p first-embedded-lab
cd first-embedded-lab
printf '%s\n' '#include <stdio.h>' \
  'static unsigned boot_count;' \
  'int main(void) {' \
  '    boot_count++;' \
  '    printf("boot_count=%u\\n", boot_count);' \
  '    return 0;' \
  '}' > main.c
cc -std=c11 -Wall -Wextra -Werror -O0 -g main.c -o app
./app
```

预期看到 `boot_count=1`。如果编译失败，不要立刻换 IDE；先完整记录命令、退出码和第一条错误，再确认当前目录和编译器。

### 35-55 分钟：观察而不是猜内存

Linux：

```bash
size app
nm -n app | grep boot_count
objdump -d app | less
```

macOS 对应：

```bash
size app
nm -n app | grep boot_count
otool -tvV app | less
```

回答：`boot_count` 为什么不在只读代码段？重新运行进程后为何又从 0 开始？主机进程的结论哪些不能直接迁移到 MCU 启动流程？

### 55-75 分钟：让测试主动失败

把 `boot_count++` 改成 `boot_count += 2`，同时新增一个退出条件：

```c
if (boot_count != 1u) {
    fprintf(stderr, "unexpected boot_count=%u\n", boot_count);
    return 1;
}
```

重新编译运行，执行 `echo $?` 查看非零退出码。再修复代码，确认退出码恢复为 0。这一步训练的是“让机器判定结果”，而不是肉眼看到一行日志就认为正确。

### 75-90 分钟：形成证据包

保留一份 `record.md`：

```text
目标与通过条件：
OS / 编译器 / 命令：
源码 commit 或 hash：
预期：
实际输出与退出码：
符号或反汇编证据：
主动制造的失败：
修复后回归结果：
仍未证明的事情：
```

完成标准不是记住命令，而是另一位学习者能根据记录复现结果，并看出这次实验没有证明真实 MCU 的链接地址、复位初始化和寄存器行为。

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

三条命令都应来自同一套工具链并记录版本。若系统提示 `command not found`，先确认安装位置：

```bash
command -v arm-none-eabi-gcc
printf '%s\n' "$PATH"
```

IDE 能编译而终端不能编译，通常说明 IDE 使用了私有工具链路径。把真实编译命令导出或写入脚本，避免工程只能从某台电脑的按钮启动。

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

## 常见失败与证据

| 现象 | 先收集什么 | 不要先做什么 |
|---|---|---|
| 找不到编译器 | `command -v`、`PATH`、IDE 实际命令 | 反复重装多个版本 |
| 编译器能运行但目标错误 | `-dumpmachine`、ELF header、构建日志 | 只看文件扩展名 |
| OpenOCD 找不到探针 | USB 枚举、权限、线缆、探针序列号 | 先改业务代码 |
| 能下载但不进 `main` | PC、SP、向量表、复位原因、链接地址 | 加更多串口打印 |
| 串口乱码 | 外设时钟、实际波特率、逻辑分析仪位宽 | 随机尝试波特率 |

## 进入下一阶段前的自测

- [ ] 能从干净目录用一条命令重建产物。
- [ ] 能说出编译器版本、目标架构和产物格式。
- [ ] 能让一个测试稳定失败，再修复并回归。
- [ ] 能找到一个符号并查看对应反汇编。
- [ ] 能区分“主机已验证”和“目标板仍待验证”的假设。

完成后进入 [CPU、内存映射与并发基础](../01-foundations/cpu-memory-concurrency.md)，或先做 [主机实验：双槽配置日志与断电一致性](host-config-journal.md)。

## 环境也是工程能力的一部分

环境准备不是安装工具的杂务，而是建立可重复工程的第一课。一个不稳定的环境会把真实问题和工具问题混在一起，让学习者把时间浪费在不可复现的失败上。专业环境至少要回答：

- 能否从干净机器或容器重建同样产物？
- 编译器、SDK、脚本、配置和环境变量是否有版本记录？
- 构建失败、测试失败、烧录失败和运行失败是否能被区分？
- 主机验证、仿真验证和目标板验证的证据边界是否清楚？
- 当工具升级或芯片包变化时，哪些结果必须重新验证？

把环境当作系统来管理，会提前训练版本意识、证据意识和自动化意识。真正的项目问题往往不是“某段代码能不能跑”，而是“团队能否长期、稳定、可追溯地构建、验证和发布同一套系统”。
