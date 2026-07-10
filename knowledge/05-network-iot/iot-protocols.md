# 网络与物联网协议

IoT 通信要从“链路是否可靠、数据是否重要、功耗是否敏感、是否需要云端”四个维度选型。

## UART、RS-485、CAN

### UART

UART 简单、常用，适合：

- 调试日志。
- 模组 AT 指令。
- MCU 间低速通信。

注意：

- 双方波特率一致。
- TTL、RS-232、RS-485 电平不同。
- 接收必须考虑粘包、半包、丢包。

### RS-485

RS-485 是差分总线，适合工业现场长距离通信。常搭配 Modbus RTU。

要点：

- 半双工方向控制。
- 终端电阻。
- 总线偏置。
- 主从轮询。
- 超时和 CRC。

### CAN

CAN 适合汽车和工业控制。

特点：

- 多主。
- 仲裁机制。
- 抗干扰强。
- 短帧高可靠。

工程要点：

- 波特率和采样点。
- 终端电阻。
- 标准帧/扩展帧。
- 过滤器配置。
- 总线关闭恢复。

## TCP/IP

TCP 提供可靠字节流，UDP 提供轻量数据报。

| 协议 | 特点 | 适用 |
|---|---|---|
| TCP | 可靠、有序、连接开销 | MQTT、HTTP、文件传输 |
| UDP | 无连接、低开销、不保证到达 | 广播、实时音视频、CoAP |

嵌入式 TCP/IP 常见栈：

- lwIP：MCU/RTOS 常用。
- Linux TCP/IP：Linux 设备直接使用 socket。

Socket 客户端基本流程：

```c
int fd = socket(AF_INET, SOCK_STREAM, 0);
connect(fd, (struct sockaddr *)&addr, sizeof(addr));
send(fd, data, len, 0);
recv(fd, buf, sizeof(buf), 0);
close(fd);
```

工程关注：

- DNS 失败。
- 连接超时。
- 网络切换。
- 发送阻塞。
- TCP keepalive。
- 断线重连退避。

## MQTT

MQTT 是 IoT 最常见的发布/订阅协议。

核心对象：

- Client：设备或应用。
- Broker：消息代理。
- Topic：主题。
- QoS：消息服务质量。

Topic 示例：

```text
device/{product_id}/{device_id}/telemetry
device/{product_id}/{device_id}/event
device/{product_id}/{device_id}/command/down
device/{product_id}/{device_id}/command/up
```

QoS 选择：

| QoS | 含义 | 适用 |
|---|---|---|
| 0 | 最多一次 | 高频传感器数据，允许丢失 |
| 1 | 至少一次 | 告警、状态变更，需要应用去重 |
| 2 | 仅一次 | 极少用，开销大 |

设备端状态机：

```text
INIT
  ↓
NET_CONNECT
  ↓
MQTT_CONNECT
  ↓
SUBSCRIBE
  ↓
ONLINE
  ↓ 断线/错误
BACKOFF_RECONNECT
```

## HTTP/HTTPS

HTTP 适合请求/响应模型：

- 设备激活。
- 配置拉取。
- 文件下载。
- OTA 包获取。
- REST API。

不适合频繁小消息低功耗上报，因为连接和头部开销较大。

## BLE

BLE 适合低功耗短距离通信。

核心概念：

- Advertising：广播。
- Connection：连接。
- GATT：服务和特征值。
- Service：服务。
- Characteristic：特征值。
- Notify：通知。

典型用途：

- 手机配网。
- 可穿戴数据同步。
- Beacon。
- 近场控制。

设计要点：

- 广播间隔影响功耗和发现速度。
- 连接间隔影响功耗和延迟。
- MTU 影响吞吐。
- Notify 需要客户端订阅。

## LoRa 和 NB-IoT

LoRa 适合低速、长距离、低功耗场景，如农业、抄表、环境监测。

NB-IoT 使用蜂窝网络，适合广覆盖、低速上报。

选型考虑：

- 覆盖范围。
- 数据量。
- 功耗。
- 模组成本和运营成本。
- 云平台接入能力。

## OTA

OTA 是联网设备产品化能力的核心。

基本流程：

```text
检查版本
  ↓
下载固件到备用分区
  ↓
校验 hash/signature
  ↓
标记待升级
  ↓
重启进入新固件
  ↓
运行自检
  ↓
确认成功或回滚
```

关键设计：

- A/B 分区或 Bootloader + App 双区。
- 固件签名验证。
- 断点续传。
- 断电保护。
- 版本防回退。
- 升级失败自动回滚。

## 安全

最低要求：

- 每台设备有唯一身份。
- 不在固件中硬编码通用私钥。
- 使用 TLS。
- 校验服务器证书。
- 重要命令带时间戳/nonce 防重放。
- OTA 包签名。
- Debug 口量产后按策略关闭或授权开启。

## 练习

1. 设计一个 MQTT topic 规范，覆盖上报、命令、事件和 OTA。
2. 写出设备断线重连状态机，加入指数退避。
3. 给一个 OTA 流程补充失败场景和恢复策略。

