# 嵌入式项目架构

一个好的嵌入式架构需要同时照顾硬件差异、实时性、资源限制、调试能力和产品生命周期。

## 推荐目录结构

```text
firmware/
├── app/
│   ├── main.c
│   ├── app_state.c
│   └── app_config.c
├── services/
│   ├── logger/
│   ├── ota/
│   ├── storage/
│   └── comm/
├── middleware/
│   ├── freertos/
│   ├── lwip/
│   └── mqtt/
├── drivers/
│   ├── sensor/
│   ├── display/
│   └── flash/
├── bsp/
│   ├── board.c
│   ├── clock.c
│   └── pinmux.c
├── platform/
│   ├── stm32/
│   └── posix_sim/
├── tests/
├── scripts/
└── CMakeLists.txt
```

## 依赖方向

```text
app -> services -> drivers -> bsp -> platform
       services -> middleware
```

规则：

- 上层可以调用下层。
- 下层不要包含上层头文件。
- 驱动不直接调用业务逻辑，可通过回调或事件通知。
- 平台相关代码集中在 BSP/platform。
- 第三方库包一层适配，避免 API 扩散到全项目。

## BSP 设计

BSP 负责板级差异：

- 时钟。
- 引脚复用。
- 外设实例。
- 电源控制。
- 复位控制。
- 板载设备。

接口示例：

```c
void board_init(void);
void board_reboot(void);
uint32_t board_get_reset_reason(void);
const char *board_get_name(void);
```

不要让业务代码散落 `GPIOA`、`USART1`、`I2C2` 这类具体硬件名，否则换板会很痛苦。

## 驱动抽象

驱动建议分两层：

```text
device driver：传感器/芯片逻辑，尽量平台无关
port layer：I2C/SPI/GPIO/delay 适配具体平台
```

示例：

```c
typedef struct {
    int (*i2c_read)(uint8_t addr, uint8_t reg, uint8_t *buf, uint16_t len);
    int (*i2c_write)(uint8_t addr, uint8_t reg, const uint8_t *buf, uint16_t len);
    void (*delay_ms)(uint32_t ms);
} temp_sensor_port_t;
```

## 状态机

联网设备建议用显式状态机，而不是一堆布尔变量。

```text
BOOT
  -> SELF_TEST
  -> CONFIG_LOAD
  -> NETWORK_CONNECT
  -> ONLINE
  -> OTA
  -> ERROR_RECOVERY
```

状态机要定义：

- 状态。
- 事件。
- 转移条件。
- 入口动作。
- 退出动作。
- 超时处理。

## 配置管理

配置分三类：

| 类型 | 示例 | 存储 |
|---|---|---|
| 编译期配置 | 板型、功能开关 | 头文件/CMake |
| 出厂配置 | SN、校准参数、密钥索引 | OTP/Flash/安全芯片 |
| 运行期配置 | 上报周期、阈值 | NVM/文件系统 |

配置必须有：

- 版本号。
- CRC/hash。
- 默认值。
- 迁移逻辑。
- 恢复出厂机制。

## 错误码

错误码要可定位：

```c
typedef enum {
    ERR_OK = 0,
    ERR_I2C_TIMEOUT = -1001,
    ERR_SENSOR_ID_MISMATCH = -1101,
    ERR_MQTT_CONNECT = -2001,
    ERR_OTA_SIGNATURE = -3001,
} error_t;
```

日志中只打印“失败”没有意义，要打印模块、错误码、关键参数。

## 测试策略

| 层级 | 方法 |
|---|---|
| 纯逻辑 | PC 单元测试 |
| 驱动逻辑 | mock I2C/SPI/GPIO |
| BSP | 板上 smoke test |
| 协议 | 本地 broker/server |
| 系统 | 长稳、断网、断电、弱网、OTA |

能放到 PC 上测的逻辑，不要都等硬件。

## CI

最低 CI：

- 格式检查。
- 静态检查。
- 单元测试。
- 固件编译。
- 输出固件大小。

进阶 CI：

- HIL 硬件在环测试。
- 自动下载固件。
- 串口日志判定。
- 功耗回归。

## 量产关注点

- SN 写入和绑定。
- 校准流程。
- 固件版本和硬件版本匹配。
- 产测程序。
- 测试日志追溯。
- Debug 口策略。
- 失败品分析流程。

## 练习

1. 为一个温湿度设备画出模块依赖图。
2. 设计一套错误码，覆盖传感器、网络和 OTA。
3. 把一个传感器驱动拆成 driver 和 port 两层。

