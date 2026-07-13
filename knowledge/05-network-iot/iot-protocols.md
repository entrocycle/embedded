# 网络与物联网协议

IoT 通信要从“链路是否可靠、数据是否重要、功耗是否敏感、是否需要云端”四个维度选型。

## 本章任务与边界

完成本章后，你应能比较 UART/RS-485/CAN、TCP/UDP、MQTT/HTTP、BLE 与低功耗广域链路提供的保证和成本，区分字节、帧、消息和业务动作成功，并为具体数据语义选择协议组合。

本章是协议地图，不替代每项标准和地区/运营商规范。吞吐、距离和功耗不能只引用理论值，需在目标天线、网络、负载和法规条件下验证。具体 framing、版本和幂等设计继续学习 [应用层协议设计](application-protocol-design.md)。

完成证据：一份带约束的协议选型矩阵、`labs/byte-stream-parser` 分片/畸形实验、一次抓包与设备日志对齐，以及“传输 ACK 到物理动作”的成功语义分层。

## UART、RS-485、CAN

### UART

UART 简单、常用，适合：

- 调试日志。
- 模组 AT 指令。
- MCU 间低速通信。

注意：

- 双方波特率一致。
- TTL、RS-232、RS-485 电平不同。
- UART 只提供字节序列，应用必须定义帧边界、长度上限、校验和超时恢复。

“一次发送对应一次接收”不是可靠协议。常见 framing 方案有固定长度、长度前缀、分隔符加转义，以及 COBS/SLIP。选择时要回答：遇到丢字节后如何重新同步，恶意或损坏长度如何限制内存，超时后丢弃到哪里。

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

Socket API 的最短主路径是 `socket -> connect -> send/recv -> close`，但它只适合说明调用顺序，不能直接作为产品代码。TCP 是**有序字节流**，不保留应用消息边界：一次 `send` 可能需要多次 `recv` 才读完，也可能与后续消息一起被读到。

一种简单应用帧可以是 4 字节网络序长度加 payload：

```text
uint32_be payload_length | payload bytes
```

下面是 POSIX 阻塞 socket 的短读辅助函数，调用者还必须在 socket 上配置有限超时，或改用非阻塞 socket + `poll`/`epoll` 管理 deadline：

```c
#include <errno.h>
#include <stdint.h>
#include <sys/socket.h>

typedef enum {
    IO_OK = 0,
    IO_PEER_CLOSED,
    IO_TIMEOUT,
    IO_ERROR,
} io_status_t;

static io_status_t recv_exact(int fd, uint8_t *buf, size_t len)
{
    size_t offset = 0;

    while (offset < len) {
        ssize_t n = recv(fd, buf + offset, len - offset, 0);
        if (n > 0) {
            offset += (size_t)n;
            continue;
        }
        if (n == 0) {
            return IO_PEER_CLOSED;
        }
        if (errno == EINTR) {
            continue;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return IO_TIMEOUT;
        }
        return IO_ERROR;
    }
    return IO_OK;
}
```

发送端也必须循环处理短写。Linux 上还要避免写已关闭连接触发 `SIGPIPE`，例如使用 `MSG_NOSIGNAL` 或进程级策略。读取长度前缀后应先转为主机字节序，再检查 `0 < len <= MAX_FRAME` 和所有加法溢出，最后才分配或读取 payload。

“发送返回成功”只说明数据被本机协议栈接受，不证明对端业务已执行。连接在回复前断开时，客户端通常无法判断请求是未到达、已执行但回复丢失，还是只执行了一半，因此协议层需要 request ID、幂等操作或持久化去重。

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
| 2 | MQTT 协议链路按握手去重交付 | 极少用，状态和开销更大 |

QoS 描述相邻 MQTT 客户端与 broker 之间的交付流程，不自动保证“云端数据库写入”或“继电器动作”端到端恰好执行一次。QoS 1 重连和重发会产生重复消息；QoS 2 也无法覆盖 broker 之外的业务事务。非幂等命令仍应包含稳定的 `request_id`，设备把已完成结果持久化或保留在有明确生命周期的去重表中。

还要联合设计：

- clean start、持久会话或 session expiry。
- inflight 消息上限和掉电后的恢复行为。
- retained 命令的过期与清除，避免新设备执行旧动作。
- DUP 和 packet identifier 只用于 MQTT 协议状态，不能代替业务 ID。
- 命令 ACK 区分“已接收、执行中、已完成、物理反馈已确认”。

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
4. 设计一个带长度前缀的 TCP 帧解析器测试表，覆盖短读、EOF、超时、超长、零长度和长度整数溢出。
5. 对“远程开阀”命令说明 MQTT QoS、业务去重和执行器反馈分别证明什么，不能证明什么。

## 深入思考

1. TCP 已经可靠，为什么应用仍需要长度、校验、request ID 和超时？哪些机制解决的是不同问题？
2. 如果设备收到命令、完成动作并断电，但还未来得及发送 ACK，重启后如何避免重复副作用？
3. 遥测“只保留最新值”和告警“每条都保留”背后分别是什么数据语义？缓存满时为什么不应使用同一策略？
4. TLS 连接成功能证明对端和传输链路的哪些属性？它为什么不能替代固件签名、命令授权和本地安全策略？

## 工程洞察

协议选择不是按流行程度排序，而是按数据语义、时延、可靠性、功耗、部署环境和运维能力取舍。UART、CAN、TCP、MQTT、HTTP、BLE、LoRa 和 NB-IoT 解决的问题不同，失败方式也不同；把一个协议的成功语义误用到另一个协议，是 IoT 系统常见风险。

设计协议链路时先回答：

- 数据是状态、事件、命令、文件、流还是诊断日志？
- 丢失、重复、乱序、延迟和过期分别如何处理？
- ACK 表示收到字节、收到消息、业务已处理，还是物理动作已完成？
- 设备离线时，哪些数据必须保存，哪些可以覆盖，哪些应丢弃并计数？
- 协议错误如何映射到设备状态、错误码和远程诊断？

优秀协议设计会承认网络不可靠，并通过 request ID、幂等、状态机、超时、重试、去重、缓存和业务确认把不可靠性控制在可理解范围内。协议层不能替代业务层语义，业务层也不能假装传输层永远成功。
