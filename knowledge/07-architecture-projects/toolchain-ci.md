# 工具链、构建与 CI

嵌入式工程的可维护性，很大程度取决于构建是否可复现、工具链是否稳定、代码质量是否自动检查、发布产物是否可追溯。

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
