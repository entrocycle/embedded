# 书籍、工具与社区

本页按学习阶段整理资源类型。具体版本和链接会变化，实际使用时以官方页面和当前项目要求为准。

## 基础书籍方向

| 主题 | 推荐类型 |
|---|---|
| C 语言 | C 语言基础、指针、内存、嵌入式 C 编程 |
| 计算机系统 | 程序如何运行、链接、内存、汇编 |
| ARM Cortex-M | Cortex-M 权威指南、芯片参考手册 |
| RTOS | FreeRTOS 官方文档、RT-Thread 文档 |
| Linux | Linux 设备驱动、内核文档、Buildroot/Yocto 手册 |
| 网络 | TCP/IP、MQTT、TLS 基础 |

## 官方文档优先级

学习某个芯片或外设时，资料优先级：

1. Datasheet。
2. Reference Manual。
3. Programming Manual。
4. Errata。
5. Application Note。
6. 官方 SDK 示例。
7. 社区文章。

社区文章适合快速理解，但不能替代官方手册。

## 常用工具

### MCU/RTOS

- arm-none-eabi-gcc
- CMake/Make/Ninja
- OpenOCD
- J-Link
- pyOCD
- STM32CubeIDE/CubeMX
- VS Code + Cortex-Debug
- PlatformIO
- FreeRTOS Trace/SystemView

### Linux/BSP

- cross gcc toolchain
- U-Boot
- Linux kernel build tools
- Buildroot
- Yocto
- dtc
- busybox
- gdbserver
- strace
- perf/ftrace

### 调试仪器

- 万用表。
- 可调电源。
- 示波器。
- 逻辑分析仪。
- 电流分析仪。
- J-Link/ST-Link。
- USB-TTL。
- CAN 分析仪。

### 质量工具

- cppcheck。
- clang-tidy。
- clang-format。
- gcov/lcov。
- Unity/CMock/Ceedling。
- GoogleTest。
- GitHub Actions/Jenkins。
- HIL 测试脚本。

## 社区和资料

建议关注：

- 芯片厂商官方论坛。
- FreeRTOS、RT-Thread、Zephyr 社区。
- Linux kernel 文档和邮件列表资料。
- Buildroot 和 Yocto 官方文档。
- LVGL、Qt 官方文档。
- TinyML 和 TensorFlow Lite Micro 资料。

## 资料阅读策略

读资料时输出三类笔记：

```text
概念：这个东西解决什么问题？
接口：怎么在代码里使用？
故障：失败时有哪些现象和证据？
```

如果一份资料没有转化为代码、图、检查清单或故障案例，它对工程能力的帮助有限。

## 练习

1. 为你手上的开发板整理官方资料清单。
2. 列出你项目中所有工具的版本和安装方式。
3. 把一次论坛/博客中找到的解决方案，回到官方手册中验证依据。

## 工程洞察

书籍、工具和社区各自提供不同价值。书籍帮助建立稳定概念，官方文档定义真实契约，工具提供证据，社区提供经验线索。不能用社区经验替代官方约束，也不能用工具输出替代理解。

建议建立个人资料分层：

- 官方一手资料：数据手册、参考手册、Errata、应用笔记、内核文档、RTOS 官方文档。
- 源码和示例：芯片 SDK、驱动源码、bootloader、内核子系统、成熟开源项目。
- 工具手册：编译器、链接器、调试器、逻辑分析仪、示波器、抓包工具。
- 社区线索：论坛、issue、博客、问答，用于发现方向，但必须回到一手资料验证。

当你从社区找到一个“神奇修复”，最有价值的不是照抄，而是追问它改变了哪个寄存器、时序、资源或状态机。这样资料才会从临时答案变成可迁移理解。
