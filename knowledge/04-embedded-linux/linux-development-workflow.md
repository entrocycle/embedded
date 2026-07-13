# Linux 主机到目标机：交叉开发与调试闭环

Embedded Linux 开发经常同时存在三套环境：开发主机、构建 sysroot 和目标设备。很多“程序运行不了”“库明明存在”“GDB 对不上源码”的根因，不在业务代码，而在目标架构、ABI、动态链接器、运行库、部署产物或符号版本不一致。

本章建立一条可复现闭环：确认工具链与 sysroot，交叉构建，检查 ELF，部署到目标，收集运行证据，远程调试，再把过程自动化。

## 本章契约

### 前置知识

- 理解 C 编译链接、ELF、静态库和动态库。
- 会使用 Linux shell、SSH/SCP 或串口文件传输。
- 有目标板或 QEMU 系统，以及匹配的交叉工具链/sysroot。

### 学完后应能

- 区分 build、host、target 三元概念与主机/目标工具链。
- 用 `file`、`readelf`、动态链接器和依赖检查验证产物。
- 正确使用 sysroot，避免链接到主机头文件和库。
- 将二进制、配置、库和符号作为一致版本部署。
- 使用 `strace`、`gdbserver`、core dump 和符号文件定位问题。
- 建立可重复构建与部署记录，并明确容器不能替代目标内核/硬件验证。

## 三个环境不要混淆

| 环境 | 执行什么 | 典型内容 |
|---|---|---|
| build machine | 运行编译器和构建工具 | x86_64 Linux/macOS 主机 |
| host/target system | 运行生成的程序 | ARM/AArch64/RISC-V 设备 |
| sysroot | 提供目标头文件、库和链接器布局 | 目标 RootFS 的开发视图 |

在 GNU build system 术语中 build、host、target 有特定含义；普通交叉编译应用常关注 build machine 与 host system。不要仅凭 `CC` 文件名判断，执行：

```bash
aarch64-linux-gnu-gcc -dumpmachine
file app
readelf -h app
readelf -l app
readelf -d app
```

检查 ELF class、machine、endianness、interpreter、NEEDED 库和 RPATH/RUNPATH。

## 动态链接器是常见断点

目标上执行文件存在却报告 `No such file or directory`，可能不是 app 缺失，而是 ELF 请求的解释器不存在，例如 `/lib/ld-linux-aarch64.so.1`。

检查：

```bash
readelf -l app | sed -n '/interpreter/p'
ls -l /lib/ld-linux-*.so.*
```

还需检查：

- glibc、musl 或其他 libc 是否匹配。
- 浮点 ABI、架构扩展和最低 CPU 指令集。
- C++ runtime 与 libstdc++ ABI。
- 目标库版本和符号版本。
- RPATH 是否意外指向构建主机目录。

静态链接减少运行库依赖，但增加镜像、许可证/更新责任，且 NSS、DNS、locale 等行为可能不同。它不是默认更可靠。

## sysroot 的职责

sysroot 应提供目标系统的头文件、库和 pkg-config 元数据。错误组合会出现“编译通过，目标崩溃”或链接到不可用 API。

```bash
aarch64-linux-gnu-gcc \
  --sysroot=/opt/target-sysroot \
  -Wall -Wextra -Werror -O2 -g \
  main.c -o app
```

验证编译器搜索路径：

```bash
aarch64-linux-gnu-gcc --sysroot=/opt/target-sysroot -v -E - </dev/null
```

sysroot 应与目标镜像版本关联。直接从运行设备复制 RootFS 可以用于诊断，但符号链接、设备文件和开发头文件可能不完整；更稳妥的是由 Buildroot/Yocto/SDK 生成配套工具链和 sysroot。

## CMake 交叉工具链文件

最小示意：

```cmake
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
set(CMAKE_SYSROOT /opt/target-sysroot)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
```

不要在通用项目文件中硬编码个人 sysroot 绝对路径。通过 SDK 环境、preset 或版本化配置注入，并在构建记录中保存真实值。

交叉配置阶段不能直接运行目标可执行文件。需要 QEMU emulator、预填检测结果，或让构建系统知道是 cross compiling。配置脚本“运行测试程序失败”不等于编译器坏了。

## 部署是版本化事务

一次部署可能包含：

- 应用二进制和动态库。
- systemd unit、配置 schema 和默认配置。
- 数据迁移工具、证书和模型资源。
- 调试符号和 build ID 对照。

不要只 `scp app /usr/bin` 后覆盖正在运行程序。设计：

```text
上传临时路径
  -> 校验 hash/签名
  -> 停止或切换服务
  -> 原子替换 symlink/目录
  -> 启动并健康检查
  -> 确认或回滚
```

开发阶段也应记录部署版本，避免目标运行旧二进制而主机用新源码调试。

## 目标运行证据

基础检查：

```bash
uname -a
cat /etc/os-release
file /usr/bin/app
ldd /usr/bin/app
readelf -n /usr/bin/app
systemctl status app.service
journalctl -u app.service -b
```

`ldd` 在不可信二进制上可能有风险，开发产物可使用；离线分析优先 `readelf -d`。还要保存：内核、RootFS、应用版本、配置 hash、板卡版本和启动时间。

## `strace` 先确认系统调用边界

应用报“设备打不开”时，`strace` 可以区分路径、权限、缺文件和 ioctl 错误：

```bash
strace -ff -tt -T -o /tmp/app.strace /usr/bin/app --config /etc/app.conf
```

关注：

- 实际打开了哪个配置和设备路径。
- `ENOENT`、`EACCES`、`ENODEV`、`EINVAL` 等 errno。
- 哪个调用阻塞最久。
- 子进程/线程和信号行为。

`strace` 会增加开销，不能用来证明严格时序；它适合确认用户态到内核的接口事实。

## 远程 GDB

目标运行：

```bash
gdbserver :2345 /usr/bin/app --config /etc/app.conf
```

主机使用匹配架构 GDB 和未剥离符号：

```bash
aarch64-linux-gnu-gdb build/app
(gdb) set sysroot /opt/target-sysroot
(gdb) target remote target-ip:2345
(gdb) break main
(gdb) continue
```

必须确认：

- 目标运行文件与主机符号具有同一 build ID/hash。
- 主机 sysroot 与目标共享库版本一致。
- 源码路径通过 `set substitute-path` 正确映射。
- 优化可能导致变量 optimized out 和执行顺序变化。

远程调试端口具有高权限风险，只应在受控开发网络使用；生产诊断需认证、授权、审计和受限能力。

## core dump 与离线符号化

无人值守崩溃需要保留：

- core 或最小崩溃上下文。
- 可执行文件 build ID。
- 对应未剥离符号和共享库。
- 运行配置、日志时间窗和系统版本。

systemd-coredump 环境可使用 `coredumpctl info/debug`；资源受限设备可以保存信号、线程、PC、栈摘要和版本，再上传受控诊断包。

发布时常把运行二进制 strip，但符号文件必须按 build ID 归档。没有符号归档的现场地址几乎无法可靠追溯。

## QEMU 与容器的证据边界

容器适合固定构建工具、依赖和静态检查；QEMU 适合验证目标 ISA、用户态 ABI、启动脚本和部分内核行为。它们不能自动证明：

- 真实设备树、时钟、电源、中断和 DMA。
- GPU、NPU、媒体和厂商驱动行为。
- 闪存断电、性能尾部和硬件时序。
- 板卡温度、功耗和电气可靠性。

使用仿真是为了快速发现软件层错误，并明确把剩余平台假设带到目标板。

## 故障实验

1. 用错误架构编译程序，比较 shell、`file` 和内核返回的证据。
2. 指定不存在的动态解释器，复现“文件存在但无法执行”。
3. 让 sysroot 与目标 libc 版本不一致，记录链接和运行差异。
4. 部署旧二进制但使用新符号调试，观察断点和栈为何误导。
5. 删除服务读取配置的权限，用 `strace`、journal 和 systemd 状态定位。
6. 制造 crash，验证 core、build ID 和离线符号化闭环。

## 阶段验收

- [ ] 工具链 target、sysroot、目标 RootFS 和应用版本有明确关联。
- [ ] 交叉产物的架构、解释器和依赖经过静态检查。
- [ ] 部署过程校验版本，不会无记录覆盖运行系统。
- [ ] 能用 `strace` 区分用户态、权限、路径和驱动接口问题。
- [ ] gdbserver、符号、sysroot 和源码版本一致。
- [ ] 崩溃后能够用归档符号离线恢复调用栈。
- [ ] QEMU/容器结论与目标板待验证项分开记录。

## 深入思考

1. 为什么“在目标板上本地编译”减少交叉配置，却可能损害可复现性、速度和发布一致性？
2. 只升级应用不升级 RootFS 时，如何管理 libc、驱动 ABI、配置和数据 schema 兼容？
3. strip 减小产品镜像后，为什么符号归档和 build ID 反而更重要？
4. 容器固定了用户态工具后，宿主内核、CPU 和时间仍会怎样影响构建或测试？

本章的出口是让“主机上编译、目标上运行”成为可追溯的工程链路，而不是一组临时复制命令。
