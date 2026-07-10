# Bootloader 与 OTA 基础

Bootloader 是产品化嵌入式设备的关键组件。它负责选择、校验、启动和恢复应用固件。OTA 则是在设备部署后安全更新固件的机制。

## 为什么需要 Bootloader

裸机实验可以直接从 Flash 起始地址运行应用，但真实产品通常需要：

- 固件升级。
- 固件校验。
- 失败回滚。
- 工厂测试模式。
- 多应用分区。
- 加密/签名验证。
- 串口、USB、CAN、BLE、网络下载。

## 典型 Flash 分区

单应用 + 下载区：

```text
0x08000000  ┌──────────────┐
            │ Bootloader   │ 32 KB
0x08008000  ├──────────────┤
            │ App          │ 224 KB
0x08040000  ├──────────────┤
            │ Download     │ 224 KB
0x08078000  ├──────────────┤
            │ Config       │ 32 KB
            └──────────────┘
```

A/B 分区：

```text
Bootloader
App A
App B
Config / Metadata
```

A/B 更适合高可靠 OTA，因为可以在新固件失败时回滚到旧固件。

## 应用跳转

Bootloader 跳转到 App 前要做：

1. 校验 App 栈顶地址是否在 RAM 范围。
2. 校验 Reset_Handler 是否在 Flash App 范围。
3. 关闭中断和外设。
4. 设置向量表偏移。
5. 设置 MSP。
6. 跳转到 App Reset_Handler。

示例：

```c
typedef void (*entry_t)(void);

void boot_jump(uint32_t app_addr)
{
    uint32_t app_sp = *(uint32_t *)app_addr;
    uint32_t app_reset = *(uint32_t *)(app_addr + 4u);

    __disable_irq();
    SysTick->CTRL = 0;

    SCB->VTOR = app_addr;
    __set_MSP(app_sp);

    entry_t entry = (entry_t)app_reset;
    entry();
}
```

## 固件元数据

固件头建议包含：

```c
typedef struct {
    uint32_t magic;
    uint32_t header_version;
    uint32_t image_size;
    uint32_t image_crc;
    uint32_t version_major;
    uint32_t version_minor;
    uint32_t version_patch;
    uint32_t build_number;
    uint32_t flags;
    uint8_t  signature[64];
} image_header_t;
```

关键字段：

- magic：识别合法镜像。
- image_size：防止越界。
- image_crc/hash：完整性校验。
- version：版本管理和防回退。
- signature：真实性校验。
- flags：是否压缩、加密、差分升级等。

## OTA 状态机

```text
IDLE
  ↓ 有新版本
CHECK_VERSION
  ↓
DOWNLOAD
  ↓
VERIFY
  ↓
MARK_PENDING
  ↓
REBOOT
  ↓
BOOT_NEW_APP
  ↓ 自检通过
CONFIRM

任一失败 -> ROLLBACK/IDLE
```

Bootloader 和 App 之间通过 metadata 共享状态：

| 状态 | 含义 |
|---|---|
| VALID | 当前固件已确认可用 |
| PENDING | 新固件待启动验证 |
| TESTING | 新固件正在自检 |
| FAILED | 新固件失败，需要回滚 |

## 完整性与真实性

完整性校验：

- CRC32：快，适合检测传输错误，不防攻击。
- SHA-256：更强，适合配合签名。

真实性校验：

- ECDSA/RSA 签名。
- Bootloader 内置公钥。
- App 包含签名，Bootloader 验证。

不要只依赖 CRC 做安全 OTA。

## 断电保护

Flash 写入过程中可能断电。设计时要保证：

- 不覆盖当前可运行固件，直到新固件完整校验通过。
- metadata 更新采用双副本或日志式写入。
- 标记状态的写入顺序可恢复。
- Bootloader 永远有一个明确的可启动选择。

## App 侧职责

App 负责：

- 检查新版本。
- 下载固件。
- 写入下载区。
- 校验下载内容。
- 设置升级标记。
- 重启。
- 新固件启动后完成自检并确认。

Bootloader 负责：

- 读取升级标记。
- 验证镜像。
- 选择启动分区。
- 回滚。
- 进入恢复模式。

## 常见坑

| 问题 | 原因 |
|---|---|
| 跳转后马上 HardFault | MSP/VTOR 未设置，外设中断未关闭 |
| App 中断进错地址 | 向量表偏移错误 |
| OTA 后变量初值异常 | 链接脚本和启动地址不一致 |
| 升级失败无法恢复 | 覆盖了唯一可运行镜像 |
| 版本降级攻击 | 未做版本防回退 |
| 固件被篡改仍能启动 | 只有 CRC，没有签名 |

## 练习

1. 设计一个 512 KB Flash 的 Bootloader/App/Download/Config 分区。
2. 写一个固件头结构体，并说明每个字段用途。
3. 画出 OTA 断电发生在下载、校验、切换、首次启动时的恢复逻辑。
