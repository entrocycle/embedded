# 嵌入式系统化教程

> 面向嵌入式软件工程师的体系化学习教程：从 C 语言、MCU 裸机、外设驱动、RTOS、嵌入式 Linux、网络物联网，到调试优化、项目架构与面试成长。

## 适合谁

- 刚开始学习嵌入式，希望有完整路径的新手。
- 已会单片机点灯，但对驱动、RTOS、Linux、网络缺乏体系的人。
- 从应用软件转嵌入式，需要快速建立硬件、系统、工具链认知的人。
- 准备嵌入式软件、BSP、驱动、IoT、边缘 AI 相关岗位的人。

## 课程地图

```text
knowledge/
├── 00-learning-path/        # 学习路线、环境准备、能力模型
├── 01-foundations/          # C/C++、编译链接、硬件常识、数据手册
├── 02-mcu-baremetal/        # MCU、启动、外设、Bootloader、OTA、低功耗
├── 03-rtos/                 # FreeRTOS/RT-Thread、任务、同步、工程实践
├── 04-embedded-linux/       # U-Boot、Kernel、DTS、驱动模型、RootFS
├── 05-network-iot/          # UART/CAN/TCP/IP/MQTT/BLE/LoRa、安全、OTA
├── 06-debug-optimization/   # GDB/OpenOCD、仪器、性能、功耗、故障案例
├── 07-architecture-projects/# 工程架构、BSP、项目实战、CI、量产发布
├── 08-career-interview/     # 面试题库、简历、成长路线
├── 09-trends-advanced/      # 中间件、GUI、TinyML、安全可靠性
├── resources/               # 书籍、工具、社区和资料管理
└── templates/               # 教程和项目模板
```

## 推荐学习顺序

1. [学习路线总览](knowledge/00-learning-path/index.md)
2. [如何把本教程学深](knowledge/00-learning-path/deep-learning-method.md)
3. [嵌入式工程环境准备](knowledge/00-learning-path/environment.md)
4. [C 语言与内存模型](knowledge/01-foundations/c-language-memory.md)
5. [编译、链接与固件镜像](knowledge/01-foundations/compile-link-firmware.md)
6. [硬件基础与数据手册阅读](knowledge/01-foundations/hardware-basics.md)
7. [MCU 启动流程与最小系统](knowledge/02-mcu-baremetal/mcu-startup.md)
8. [寄存器与外设驱动方法](knowledge/02-mcu-baremetal/peripheral-driver.md)
9. [Bootloader 与 OTA 基础](knowledge/02-mcu-baremetal/bootloader-ota.md)
10. [低功耗设计](knowledge/02-mcu-baremetal/low-power.md)
11. [RTOS 核心机制](knowledge/03-rtos/rtos-core.md)
12. [RTOS 工程实践](knowledge/03-rtos/rtos-engineering.md)
13. [嵌入式 Linux 全链路](knowledge/04-embedded-linux/linux-system.md)
14. [Linux 驱动模型与调试](knowledge/04-embedded-linux/linux-driver-model.md)
15. [网络与物联网协议](knowledge/05-network-iot/iot-protocols.md)
16. [IoT 安全与设备生命周期](knowledge/05-network-iot/iot-security.md)
17. [调试、优化与故障定位](knowledge/06-debug-optimization/debugging.md)
18. [典型故障案例库](knowledge/06-debug-optimization/fault-cases.md)
19. [嵌入式项目架构](knowledge/07-architecture-projects/project-architecture.md)
20. [综合项目：环境监测节点](knowledge/07-architecture-projects/project-environment-node.md)
21. [量产、测试与发布](knowledge/07-architecture-projects/production-release.md)
22. [面试题库](knowledge/08-career-interview/question-bank.md)

扩展专题建议按需穿插：

- [嵌入式能力矩阵](knowledge/00-learning-path/competency-matrix.md)
- [如何把本教程学深](knowledge/00-learning-path/deep-learning-method.md)
- [数据手册与原理图阅读方法](knowledge/01-foundations/datasheet-schematic-method.md)
- [时钟、中断与 DMA](knowledge/02-mcu-baremetal/clock-interrupt-dma.md)
- [传感器与执行器](knowledge/02-mcu-baremetal/sensor-actuator.md)
- [RTOS 移植与配置](knowledge/03-rtos/rtos-porting-config.md)
- [BSP、RootFS 与系统构建](knowledge/04-embedded-linux/bsp-rootfs-build.md)
- [用户态服务与系统集成](knowledge/04-embedded-linux/userspace-service.md)
- [连接状态机与弱网设计](knowledge/05-network-iot/connectivity-state-machine.md)
- [现场诊断与可观测性](knowledge/06-debug-optimization/field-diagnostics.md)
- [项目阶梯](knowledge/07-architecture-projects/project-ladder.md)
- [工具链、构建与 CI](knowledge/07-architecture-projects/toolchain-ci.md)
- [岗位路线与能力准备](knowledge/08-career-interview/role-roadmap.md)
- [进阶专题](knowledge/09-trends-advanced/index.md)
- [资源索引](knowledge/resources/index.md)

## 阶段产出

| 阶段 | 建议产出 |
|---|---|
| 基础阶段 | 一份内存布局笔记、一份数据手册摘录、一份 map 文件分析 |
| MCU 阶段 | UART/I2C/SPI/ADC/DMA 驱动实验、Bootloader 跳转实验、低功耗测量记录 |
| RTOS 阶段 | 多任务环境监测程序、任务栈统计、队列满和看门狗故障注入报告 |
| Linux 阶段 | 可启动镜像、DTS 修改记录、一个 platform 或 I2C 驱动 |
| IoT 阶段 | MQTT 上报状态机、断线重连、OTA 和安全设计 |
| 工程阶段 | 项目架构图、产测清单、发布包、面试项目复盘 |
| 进阶阶段 | 文件系统方案、GUI demo、TinyML demo、安全启动/可靠性设计 |

## 每章学习方法

每个主题建议按同一个节奏推进：

1. 先看“概念边界”：知道这个东西解决什么问题，不解决什么问题。
2. 再追“底层机制”：寄存器、时序、调度、内存、状态机或调用链。
3. 再做“最小可运行路径”：用一个最小实验理解主流程。
4. 然后做“故障注入”：主动制造异常，记录日志、波形、寄存器或 dump。
5. 最后做“复盘输出”：画图、写接口、补故障清单，回答设计取舍。

更具体的方法见 [如何把本教程学深](knowledge/00-learning-path/deep-learning-method.md)。

## 站点发布

本仓库包含基础 `mkdocs.yml`，后续可以直接用 MkDocs Material 发布：

```bash
pip install mkdocs mkdocs-material mkdocs-awesome-pages-plugin
mkdocs serve
```

启动后访问 `http://127.0.0.1:8000`。

仓库也已包含 GitHub Actions：

- `.github/workflows/deploy-mkdocs.yml`
- 推送到 `main` 时自动构建并部署到 GitHub Pages。
- Pull Request 时只执行 `mkdocs build --strict` 构建检查，不部署。
- 需要在 GitHub 仓库设置中启用 Pages，并选择 GitHub Actions 作为部署来源。
