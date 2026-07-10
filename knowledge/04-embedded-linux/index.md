# 嵌入式 Linux

嵌入式 Linux 关注的是完整系统：Bootloader、内核、设备树、驱动、根文件系统、系统服务和应用。它比 MCU 裸机复杂，但抽象更强，适合资源更丰富、功能更复杂的设备。

## 章节

- [嵌入式 Linux 全链路](linux-system.md)
- [Linux 驱动模型与调试](linux-driver-model.md)
- [BSP、RootFS 与系统构建](bsp-rootfs-build.md)
- [用户态服务与系统集成](userspace-service.md)

## 典型启动链路

```text
BootROM -> SPL -> U-Boot -> Linux Kernel + DTB -> RootFS -> init/systemd -> App
```

## 学习重点

- 能从串口启动日志判断系统停在哪一阶段。
- 能修改 U-Boot 环境变量启动内核。
- 能写和修改设备树节点。
- 能编译内核、模块和 RootFS。
- 能写基础字符设备、platform、I2C/SPI 驱动。
- 能用 `dmesg`、`/proc`、`/sys`、`strace`、`gdbserver` 定位问题。
- 能判断一个功能应该放在内核驱动、用户态服务还是脚本配置中。
- 能用 Buildroot/Yocto 或手工方式构建 RootFS，并把应用做成可管理服务。

## 深挖问题

- 从 BootROM 到 App，每一层交给下一层的是什么：镜像、参数、设备树还是文件系统？
- DTS 源码正确是否等于运行时设备树正确？如何证明内核实际加载的是哪个 DTB？
- `probe()` 失败、`/dev` 节点缺失和用户态权限错误如何区分？
- 业务逻辑放进内核驱动会带来哪些调试、稳定性和升级成本？

## 阶段产出

- 一份完整启动日志标注，说明系统停留或切换的每个阶段。
- 一个 DTS 修改记录，包含源码、编译产物和运行时 `/proc/device-tree` 验证。
- 一个用户态服务示例，包含 systemd 配置、日志、重启策略和诊断命令。
