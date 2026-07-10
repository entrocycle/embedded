# 用户态服务与系统集成

嵌入式 Linux 的业务逻辑大多应该运行在用户态。驱动负责硬件访问，用户态服务负责策略、协议、配置、日志、升级和与云端交互。

## 用户态和内核边界

放在内核：

- 中断处理。
- 寄存器访问。
- DMA 和硬件缓冲。
- 标准内核子系统接口。
- 对实时性要求极高且无法用户态完成的逻辑。

放在用户态：

- 业务状态机。
- 网络协议和云接入。
- 配置管理。
- 日志上报。
- OTA。
- 数据处理。
- UI 和本地 API。

原则：能稳定放在用户态的策略逻辑，不要塞进内核驱动。

## 设备访问方式

| 接口 | 适合 |
|---|---|
| `/dev/xxx` | 字符设备、ioctl、自定义读写 |
| `/sys/class` | 简单属性、状态、控制 |
| `/proc` | 内核信息和调试，不建议做产品控制接口 |
| `netlink` | 内核和用户态事件通信 |
| `input` | 按键、触摸、输入设备 |
| `iio` | ADC、传感器、采样设备 |
| `v4l2` | 摄像头、视频采集 |

优先接入标准子系统，减少自定义接口成本。

## systemd 服务

服务示例：

```ini
[Unit]
Description=Device Agent
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
ExecStart=/usr/bin/device-agent --config /etc/device-agent.toml
Restart=always
RestartSec=3
WatchdogSec=30

[Install]
WantedBy=multi-user.target
```

关注点：

- 启动依赖是否明确。
- 退出后是否重启。
- 日志进入 journald 还是文件。
- 是否配置 watchdog。
- 配置文件是否可恢复默认。

## 配置管理

用户态配置建议包含：

```text
版本号
设备标识
网络参数
云端地址
采样周期
日志级别
功能开关
校准参数索引
```

要求：

- 原子写入。
- CRC/hash 校验。
- 默认值。
- 版本迁移。
- 恢复出厂。
- 权限控制。

## 日志策略

日志要同时满足现场定位和 Flash 寿命约束。

建议：

- 日志分级：ERROR/WARN/INFO/DEBUG。
- 循环文件或大小限制。
- 关键故障单独持久化。
- 支持远程拉取。
- 敏感信息脱敏。
- 限制频繁错误刷屏。

不要让日志把根分区写满。产品系统应监控磁盘空间。

## IPC 选择

| 方法 | 适合 |
|---|---|
| Unix domain socket | 本机服务间通信 |
| D-Bus | 桌面/系统服务集成 |
| MQTT local broker | 模块解耦和消息总线 |
| shared memory | 高吞吐数据 |
| REST/gRPC | 对外 API 或复杂服务 |
| pipe/fifo | 简单流式数据 |

IPC 要考虑权限、超时、重连和背压，不要只考虑“能传数据”。

## 守护进程健康

设备长期运行时，服务要能自诊断：

- 主循环心跳。
- 线程状态。
- 队列积压。
- 内存占用。
- 文件描述符数量。
- 网络连接状态。
- 最近错误码。

可以暴露一个本地诊断命令：

```bash
device-agent status --json
```

输出示例：

```json
{
  "version": "1.2.0",
  "uptime_sec": 3600,
  "mqtt": "connected",
  "queue_depth": 3,
  "last_error": 0
}
```

## 应用更新

用户态应用更新策略：

- 包更新：适合 Debian/Ubuntu 系统。
- A/B RootFS：适合整系统升级。
- 单应用替换：适合简单设备，但要做好回滚。
- 容器：适合资源较大的网关。

无论哪种方式，都要有：

- 签名校验。
- 版本检查。
- 断点或失败恢复。
- 启动后健康确认。
- 失败回滚。

## 调试命令

```bash
systemctl status device-agent
journalctl -u device-agent -f
strace -ff -p <pid>
lsof -p <pid>
cat /proc/<pid>/status
cat /proc/meminfo
ip addr
ss -tanp
```

## 练习

1. 写一个 systemd 服务，让它崩溃后自动重启。
2. 为一个用户态程序增加 `status --json` 输出。
3. 设计一个日志轮转策略，限制总日志空间不超过 20 MB。
