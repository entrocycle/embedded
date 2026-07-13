# 嵌入式系统化教程

这是一套以真实工程能力为目标的嵌入式教程。内容从 C、CPU、内存、编译链接和硬件阅读出发，分为 MCU/RTOS 与 Embedded Linux/BSP 两条系统路线，最终在网络、安全、OTA、调试验证、架构、CI、量产和现场维护中汇合。

教程不以“覆盖了多少知识点”为完成标准，而要求读者能够：

- 从源码、ELF、运行时、寄存器和物理信号互相解释系统行为。
- 把需求改写为约束、指标、不变量和可复核证据。
- 设计实验区分竞争假设，而不是凭经验不断试参数。
- 处理边界、过载、断电、弱网、版本和资源耗尽。
- 在性能、功耗、成本、复杂度、安全和可靠性之间做取舍。
- 将结论沉淀为接口、测试、诊断、构建与发布资产。

## 从这里开始

1. 阅读 [课程依赖地图与阶段门](knowledge/00-learning-path/course-map.md)。
2. 完成 [嵌入式工程环境准备](knowledge/00-learning-path/environment.md) 的 90 分钟闭环。
3. 按 [实验地图](knowledge/00-learning-path/labs-map.md) 运行仓库主机实验。
4. 进入 [共同基础](knowledge/01-foundations/index.md)。
5. 选择 [MCU/RTOS](knowledge/02-mcu-baremetal/index.md) 或 [Embedded Linux/BSP](knowledge/04-embedded-linux/index.md) 作为第一条主线。

有经验的读者可以直接挑战阶段门。跳过章节的依据是已有证据，而不是“以前看过类似内容”。

## 课程结构

```text
                                  +-> MCU 裸机 -> RTOS ---------+
学习方法 -> 共同基础 -------------+                               |
            C / CPU / 构建 / 硬件 +-> Embedded Linux / BSP -----+-> 产品交付
                                  |                               |
                                  +-> 网络 / 存储 / 安全 / OTA ---+

贯穿全程：调试、测试、可靠性、功耗、可观测性、工具链和工程表达
```

### 共同基础

- [C 语言与内存模型](knowledge/01-foundations/c-language-memory.md)
- [嵌入式 C++](knowledge/01-foundations/embedded-cpp.md)
- [CPU、内存映射与并发](knowledge/01-foundations/cpu-memory-concurrency.md)
- [编译、链接与固件镜像](knowledge/01-foundations/compile-link-firmware.md)
- [硬件基础](knowledge/01-foundations/hardware-basics.md)
- [数据手册与原理图方法](knowledge/01-foundations/datasheet-schematic-method.md)

### MCU/RTOS 路线

- [MCU 启动与最小系统](knowledge/02-mcu-baremetal/mcu-startup.md)
- [时钟、中断与 DMA](knowledge/02-mcu-baremetal/clock-interrupt-dma.md)
- [寄存器与外设驱动](knowledge/02-mcu-baremetal/peripheral-driver.md)
- [裸机架构](knowledge/02-mcu-baremetal/baremetal-architecture.md)
- [传感器与执行器](knowledge/02-mcu-baremetal/sensor-actuator.md)
- [RTOS 核心、移植与工程实践](knowledge/03-rtos/index.md)
- [低功耗](knowledge/02-mcu-baremetal/low-power.md) 与 [Bootloader/OTA](knowledge/02-mcu-baremetal/bootloader-ota.md)

### Embedded Linux/BSP 路线

- [主机到目标机开发闭环](knowledge/04-embedded-linux/linux-development-workflow.md)
- [Embedded Linux 全链路](knowledge/04-embedded-linux/linux-system.md)
- [BSP、RootFS 与系统构建](knowledge/04-embedded-linux/bsp-rootfs-build.md)
- [Linux 驱动模型与调试](knowledge/04-embedded-linux/linux-driver-model.md)
- [用户态服务与系统集成](knowledge/04-embedded-linux/userspace-service.md)

### 产品工程

- [应用层协议设计](knowledge/05-network-iot/application-protocol-design.md)
- [连接状态机与弱网设计](knowledge/05-network-iot/connectivity-state-machine.md)
- [IoT 安全与设备生命周期](knowledge/05-network-iot/iot-security.md)
- [调试、测试与可观测性](knowledge/06-debug-optimization/index.md)
- [需求、风险与证据追踪](knowledge/07-architecture-projects/requirements-risk.md)
- [项目架构、CI、量产与发布](knowledge/07-architecture-projects/index.md)

## 可运行实验

仓库提供三项主机实验，分别训练边界、协议解析和断电一致性：

```bash
make -C labs test
```

| 实验 | 核心问题 |
|---|---|
| [ring-buffer](labs/ring-buffer/README.md) | 空满、wrap、不变量和模型对照 |
| [byte-stream-parser](labs/byte-stream-parser/README.md) | 分片、粘连、畸形长度和有界失败 |
| [host-config-journal](labs/host-config-journal/README.md) | Flash 模型、提交顺序和逐断电点恢复 |

主机实验属于 E1 证据：适合验证纯逻辑和不变量，但不证明目标 CPU 原子性、DMA/cache、外设时序和电气行为。目标板与系统项目的补充验证见 [实验地图](knowledge/00-learning-path/labs-map.md)。

## 综合项目

- [项目阶梯](knowledge/07-architecture-projects/project-ladder.md)：共同基础后分为 MCU/RTOS 与 Linux/BSP 两条项目路线。
- [环境监测节点](knowledge/07-architecture-projects/project-environment-node.md)：MCU/RTOS、传感器、网络、配置、低功耗与 OTA。
- [Embedded Linux 边缘网关](knowledge/07-architecture-projects/project-linux-gateway.md)：BSP、驱动、systemd、存储、弱网、更新与现场诊断。

项目完成不是功能截图，而是需求、架构、预算、原始证据、故障注入、构建、发布、回滚和残余风险组成的证据包。

## 质量标准

[criteria.md](criteria.md) 定义整套教程的上位质量标准，包括十二项质量属性、课程组织、单章/实验/项目契约、评估门禁和完成定义。

站内配套方法：

- [深度教程契约](knowledge/00-learning-path/depth-contract.md)
- [系统思维](knowledge/00-learning-path/system-thinking.md)
- [深度学习方法](knowledge/00-learning-path/deep-learning-method.md)
- [教程质量矩阵](knowledge/00-learning-path/tutorial-quality-matrix.md)

## 本地验证与预览

安装文档依赖并严格构建：

```bash
python3 -m venv .venv
.venv/bin/pip install -r requirements.txt
.venv/bin/mkdocs build --strict
```

启动本地站点：

```bash
.venv/bin/mkdocs serve
```

默认访问 `http://127.0.0.1:8000`。

CI 会同时执行 MkDocs 严格构建和全部主机实验。推送到 `main` 时由 `.github/workflows/deploy-mkdocs.yml` 部署 GitHub Pages；Pull Request 只构建和测试，不部署。
