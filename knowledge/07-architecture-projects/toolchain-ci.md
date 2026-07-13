# 工具链、构建与 CI

嵌入式工程的可维护性，很大程度取决于构建是否可复现、工具链是否稳定、代码质量是否自动检查、发布产物是否可追溯。

## 本章任务与边界

完成本章后，你应能固定工具链与依赖、从干净环境构建、分层运行静态/主机/仿真/HIL 测试，追踪 size 与 hash，并生成包含组件、许可证、版本、配置和符号的可审计发布包。

CI 只能执行被编码的规则，不能替代需求评审、真实时序、功耗、硬件批次和安全判断。容器固定用户态工具，不自动固定宿主内核、CPU、外设和时间源。

完成证据：同一 commit 两次干净构建比较、CI 失败分类、size 回归门、SBOM/依赖清单、归档符号和一次使用真实发布包的烧录或升级回归。

## 工具链组成

| 类型 | 示例 | 作用 |
|---|---|---|
| 编译器 | arm-none-eabi-gcc、clang、aarch64-linux-gnu-gcc | 编译固件/应用 |
| 构建系统 | Make、CMake、Ninja、West、PlatformIO | 管理编译规则 |
| 烧录调试 | OpenOCD、J-Link、pyOCD、STM32CubeProgrammer | 下载和调试 |
| 静态分析 | cppcheck、clang-tidy、scan-build | 提前发现缺陷 |
| 单元测试 | Unity、CMock、Ceedling、GoogleTest | 测纯逻辑 |
| 仿真/模拟 | QEMU、Renode、POSIX port | 无硬件测试 |
| HIL | 串口继电器、电源控制、测试夹具 | 板上回归 |

## 可复现构建

要求：

- 固定工具链版本。
- 固定依赖版本。
- 构建脚本一条命令可运行。
- 输出固件版本、commit、dirty 状态。
- 保存 map、size、hash。

固件版本示例：

```c
typedef struct {
    const char *version;
    const char *git_commit;
    const char *build_time;
    const char *target_board;
} firmware_info_t;
```

## CMake 裸机项目要点

至少要明确：

- `CMAKE_SYSTEM_NAME Generic`
- 交叉编译器。
- CPU/FPU/ABI 参数。
- 链接脚本。
- 启动文件。
- map 文件输出。
- objcopy 生成 bin/hex。
- size 统计。

构建产物建议：

```text
build/
  firmware.elf
  firmware.bin
  firmware.hex
  firmware.map
  firmware.size.txt
  compile_commands.json
```

## 代码质量

最低检查：

- 编译警告全开，核心模块尽量 `-Werror`。
- 格式化。
- 静态检查。
- 单元测试。
- 固件大小回归。

常见规则：

- 禁止未检查返回值。
- 禁止裸 `malloc/free` 出现在核心实时路径。
- ISR 中禁止阻塞 API。
- 驱动必须有超时。
- 公共接口必须有错误码。

## PC 单元测试

适合放到 PC 上测的逻辑：

- 协议编码/解码。
- 状态机。
- 环形缓冲。
- 配置校验。
- CRC/hash。
- OTA 元数据解析。
- 传感器换算公式。

不适合直接 PC 测但可以 mock 的逻辑：

- I2C/SPI 驱动。
- Flash 读写。
- RTOS 队列。
- 时间和定时器。

## CI 分层

### 基础 CI

每次提交运行：

- 格式检查。
- 静态分析。
- PC 单元测试。
- 固件编译。
- 固件大小统计。

### 进阶 CI

合并前运行：

- 多板型编译。
- QEMU/Renode smoke test。
- 协议集成测试。
- 生成文档。

### 硬件在环

夜间或发布前运行：

- 自动烧录。
- 串口日志判定。
- 外设自检。
- 断电恢复。
- OTA。
- 功耗采样。

## GitHub Actions 示例思路

```yaml
name: Firmware CI

on:
  pull_request:
  push:
    branches: [ main ]

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Install toolchain
        run: sudo apt-get update && sudo apt-get install -y gcc-arm-none-eabi cmake ninja-build
      - name: Configure
        run: cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
      - name: Build
        run: cmake --build build
      - name: Size
        run: arm-none-eabi-size build/firmware.elf
```

实际项目应把工具链固定到容器或缓存版本，避免系统包升级引入差异。

## 依赖、SBOM 与供应链

固件和系统镜像通常包含编译器运行库、RTOS、协议栈、加密库、驱动、生成工具、容器基础镜像和主机工具。只记录应用 Git commit 无法回答“现场设备是否包含某个漏洞组件”。

至少维护：

| 信息 | 作用 |
|---|---|
| 组件名称、版本、来源和 commit | 识别实际进入产物的代码 |
| 下载 URL、hash、签名 | 防止依赖被静默替换 |
| 许可证与义务 | 满足源代码、声明和分发要求 |
| 构建选项与补丁 | 区分同版本不同安全能力 |
| 目标/板型使用关系 | 判断漏洞影响哪些产品 |
| 维护者与更新策略 | 明确谁负责评估和升级 |

SBOM 可以使用 SPDX、CycloneDX 或组织已有格式，但“生成了一个文件”不代表完整。编译期未链接组件、静态链接库、厂商二进制 blob、Bootloader、设备固件和构建容器都要定义是否纳入。

依赖获取应尽量满足：

- 固定不可变版本或 commit，不跟随浮动分支。
- 校验 hash/签名，限制下载源和凭据权限。
- CI 使用最小权限，保护发布密钥和 artifact。
- 第三方代码更新经过差异、许可证、测试和回滚评审。
- 发布 artifact 有签名、来源证明或组织等价机制。

### 漏洞响应闭环

收到 CVE 或供应链事件后：

```text
识别组件与实际配置
  -> 判断代码是否进入可达路径
  -> 评估攻击前提、影响与现场暴露
  -> 选择缓解/升级/接受
  -> 构建和回归
  -> 灰度发布与监控
  -> 更新 SBOM、公告和残余风险
```

版本命中 CVE 不等于产品一定可利用，版本未命中也不等于安全。补丁可能被厂商 backport，漏洞也可能存在于私有修改。结论要关联真实源码、构建选项、攻击面和测试证据。

### 可复现构建的现实边界

目标是相同输入得到可验证相同产物。常见非确定来源：

- 构建时间、绝对路径、随机种子和文件排序。
- 未固定依赖、网络下载和生成工具。
- 编译器/链接器、主机 locale、时区和文件系统差异。
- 签名时间戳或随机化签名。

若暂时不能 bit-for-bit 重现，至少固定输入并比较解包后的关键 section、组件 manifest 和功能证据，公开说明剩余差异。不要把容器镜像 tag 当成不可变版本，应记录 digest。

## 发布包

发布目录建议：

```text
release-v1.2.0/
├── firmware.bin
├── firmware.hex
├── firmware.elf
├── firmware.map
├── manifest.json
├── changelog.md
├── test-report.md
└── flash-instructions.md
```

manifest 示例字段：

```json
{
  "version": "1.2.0",
  "board": "env-node-r2",
  "git_commit": "abc1234",
  "firmware_sha256": "...",
  "build_type": "release"
}
```

## 练习

1. 给一个裸机工程增加 `version` 命令，输出版本、commit 和构建时间。
2. 为环形缓冲写 PC 单元测试。
3. 设计一个 CI 流程，包含编译、测试、size 和发布包生成。

## 工程洞察

工具链和 CI 的作用不是追求自动化形式，而是建立可重复、可追溯、可阻断错误发布的工程系统。嵌入式项目尤其需要关注“产物”和“源码”之间的链路，因为现场设备运行的是固件镜像，不是 Git 仓库。

CI 至少要回答：

- 这个固件由哪个 commit、工具链、配置、链接脚本和依赖版本构建？
- 构建产物的 size、hash、map、符号和版本信息是否归档？
- 主机测试、静态检查、格式检查、目标板测试和发布打包分别在哪个门禁执行？
- 失败时是否能区分代码错误、环境错误、硬件资源占用和测试不稳定？
- 发布包是否能被 Bootloader、产测工装和现场诊断系统识别？

成熟 CI 不只是让机器跑命令，而是把团队约定固化为门禁。它会让“这次只是小改动”这类主观判断减少，让证据和规则成为发布决策的基础。
