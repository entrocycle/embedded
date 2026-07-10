# 嵌入式系统化教程

这是一套面向工程实践的嵌入式教程。它把嵌入式学习拆成十个层次：学习路线、基础能力、MCU 裸机、RTOS、嵌入式 Linux、网络物联网、调试优化、项目架构、职业面试、进阶专题与资源索引。

## 知识结构

```text
应用与产品
  ↑
项目架构 / 工程管理 / 量产维护
  ↑
调试优化 / 可靠性 / 性能功耗
  ↑
网络协议 / IoT / OTA / 安全
  ↑
RTOS / Embedded Linux
  ↑
MCU / 启动流程 / 外设驱动
  ↑
C 语言 / 编译链接 / 硬件基础
```

## 目录入口

- [00 学习路线](00-learning-path/index.md)：明确方向、阶段目标和环境准备。
- [01 基础能力](01-foundations/index.md)：C、内存、编译链接、计算机与硬件基础。
- [02 MCU 裸机](02-mcu-baremetal/index.md)：从启动文件、外设驱动到 Bootloader、OTA 和低功耗。
- [03 RTOS](03-rtos/index.md)：任务、调度、同步、移植和工程使用。
- [04 嵌入式 Linux](04-embedded-linux/index.md)：U-Boot、Kernel、设备树、驱动模型、RootFS。
- [05 网络与 IoT](05-network-iot/index.md)：串口、CAN、TCP/IP、MQTT、BLE、LoRa、OTA、安全。
- [06 调试与优化](06-debug-optimization/index.md)：JTAG/SWD、GDB、示波器、性能功耗、故障案例。
- [07 架构与项目](07-architecture-projects/index.md)：BSP、模块化、项目实战、CI、量产发布。
- [08 职业与面试](08-career-interview/index.md)：知识清单、面试题库、简历项目表达。
- [09 进阶专题](09-trends-advanced/index.md)：存储文件系统、GUI、TinyML、安全与可靠性。
- [资源索引](resources/index.md)：书籍、工具、社区和资料管理方法。

## 核心新增专题

- [嵌入式能力矩阵](00-learning-path/competency-matrix.md)
- [如何把本教程学深](00-learning-path/deep-learning-method.md)
- [数据手册与原理图阅读方法](01-foundations/datasheet-schematic-method.md)
- [时钟、中断与 DMA](02-mcu-baremetal/clock-interrupt-dma.md)
- [传感器与执行器](02-mcu-baremetal/sensor-actuator.md)
- [硬件基础与数据手册阅读](01-foundations/hardware-basics.md)
- [Bootloader 与 OTA 基础](02-mcu-baremetal/bootloader-ota.md)
- [低功耗设计](02-mcu-baremetal/low-power.md)
- [RTOS 移植与配置](03-rtos/rtos-porting-config.md)
- [RTOS 工程实践](03-rtos/rtos-engineering.md)
- [Linux 驱动模型与调试](04-embedded-linux/linux-driver-model.md)
- [BSP、RootFS 与系统构建](04-embedded-linux/bsp-rootfs-build.md)
- [用户态服务与系统集成](04-embedded-linux/userspace-service.md)
- [连接状态机与弱网设计](05-network-iot/connectivity-state-machine.md)
- [IoT 安全与设备生命周期](05-network-iot/iot-security.md)
- [现场诊断与可观测性](06-debug-optimization/field-diagnostics.md)
- [典型故障案例库](06-debug-optimization/fault-cases.md)
- [项目阶梯](07-architecture-projects/project-ladder.md)
- [工具链、构建与 CI](07-architecture-projects/toolchain-ci.md)
- [量产、测试与发布](07-architecture-projects/production-release.md)
- [岗位路线与能力准备](08-career-interview/role-roadmap.md)
- [面试题库](08-career-interview/question-bank.md)
- [中间件、存储与文件系统](09-trends-advanced/middleware-storage-filesystem.md)
- [图形界面：LVGL 与 Qt](09-trends-advanced/gui-lvgl-qt.md)
- [Edge AI 与 TinyML](09-trends-advanced/edge-ai-tinyml.md)
- [安全、可靠性与功能安全入门](09-trends-advanced/safety-reliability.md)

## 学习原则

- 不把“会调库”当作“会嵌入式”。真正的能力来自理解启动、内存、时钟、中断、外设、调度和系统边界。
- 不把“裸机”和“RTOS/Linux”割裂。裸机训练硬件控制，RTOS 训练并发和实时性，Linux 训练系统集成和驱动模型。
- 不只学 API。每个 API 背后都要追问：它访问了什么寄存器、消耗了什么资源、失败时会发生什么、怎么定位问题。
- 不脱离项目。每一层知识最终都要落到可运行、可调试、可维护的工程上。
- 不只追求跑通。产品化还要考虑安全、OTA、低功耗、产测、发布、回滚和现场诊断。
- 不只接受结论。每章都要追问“证据是什么、还有什么方案、失败后如何恢复、换平台后哪些结论仍成立”。
