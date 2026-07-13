# Bootloader 与 OTA 基础

Bootloader 是产品化嵌入式设备的关键组件。它负责选择、校验、启动和恢复应用固件。OTA 则是在设备部署后安全更新固件的机制。

## 本章任务与边界

完成本章后，你应能定义 Flash 分区、镜像元数据、启动选择和首次启动确认，画出下载到回滚的状态机，并用“不论何时断电，至少有一个可信可启动镜像”等不变量评审方案。

本章是 MCU Bootloader/OTA 的机制基础，不提供某芯片可直接量产的安全启动实现。Flash 擦写粒度、option bytes、ROM 启动能力、签名算法与防回滚根必须以芯片安全手册和产品威胁模型为准。

完成证据：分区预算、镜像/配置兼容表、逐阶段断电矩阵和启动决策日志。先通过 [双槽配置日志实验](../00-learning-path/host-config-journal.md) 训练提交与恢复，再把原子模型替换为目标 Flash 保证。

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

“App + Download”不等于真正 A/B。若 Bootloader 需要把 Download 复制并覆盖唯一 App，安装过程中断电可能同时得到半写 App 和一份仍在 Download 的完整镜像。此时只有在 Bootloader 具备恢复安装能力、复制进度日志和可重入写入流程时才能恢复；若 Download 会被原地解包、swap 或覆盖，还需要 scratch 区和经过证明的交换算法。真正独立的 A/B 执行分区能在切换前保留旧镜像，恢复不变量更容易建立，但成本是更多 Flash。

分区选择前先写出不变量：**任意一次可预期复位后，Bootloader 都能找到一个已验证的可启动镜像，或进入有权限控制的恢复通道。** 然后逐个检查 erase、program、metadata 更新和首次启动确认之间的每个断电点。

## 应用跳转

Bootloader 跳转到 App 前要做：

1. 校验 App 栈顶地址是否在 RAM 范围。
2. 校验 Reset_Handler 是否在 Flash App 范围。
3. 关闭中断和外设。
4. 设置向量表偏移。
5. 设置 MSP。
6. 跳转到 App Reset_Handler。

下面只展示跳转前的验证和清理框架。地址范围要用真实芯片内存图替换，并覆盖所有已使能 IRQ；时钟、DMA、cache、MPU 和安全状态的交接还依赖具体平台。

```c
typedef void (*entry_t)(void);

#define APP_START  0x08008000u
#define APP_END    0x08040000u
#define SRAM_START 0x20000000u
#define SRAM_END   0x20020000u

static bool boot_vector_valid(uint32_t app_sp, uint32_t app_reset)
{
    bool sp_ok = (app_sp >= SRAM_START) && (app_sp <= SRAM_END) &&
                 ((app_sp & 0x7u) == 0u);
    uint32_t reset_addr = app_reset & ~1u;
    bool reset_ok = ((app_reset & 1u) != 0u) &&
                    (reset_addr >= APP_START) && (reset_addr < APP_END);

    return sp_ok && reset_ok;
}

/* 平台相关实现必须保证切换 MSP 后不再访问旧 C 栈帧。 */
__attribute__((noreturn))
void platform_tail_jump(uint32_t app_sp, uint32_t app_reset);

bool boot_prepare_jump(uint32_t app_addr)
{
    uint32_t app_sp = *(const uint32_t *)app_addr;
    uint32_t app_reset = *(const uint32_t *)(app_addr + 4u);

    if (app_addr != APP_START || !boot_vector_valid(app_sp, app_reset)) {
        return false;
    }

    __disable_irq();
    SysTick->CTRL = 0;

    for (uint32_t i = 0; i < NVIC_BANK_COUNT; ++i) {
        NVIC->ICER[i] = 0xFFFFFFFFu;
        NVIC->ICPR[i] = 0xFFFFFFFFu;
    }

    platform_deinit_before_jump();

    SCB->VTOR = app_addr;
    __DSB();
    __ISB();

    platform_tail_jump(app_sp, app_reset);
}
```

`platform_tail_jump` 通常由启动汇编或经过反汇编确认的 `naked` 尾跳实现：设置 MSP 后立即把 `app_reset` 装入 PC，不再执行函数尾声或普通 C 调用。不要在普通 C 函数中 `__set_MSP()` 后继续运行，因为编译器仍可能通过新 MSP 访问旧栈帧。还要定义 PRIMASK/FAULTMASK、FPU、cache、MPU/TrustZone 和时钟由哪一侧初始化；常见约定是 App 的 Reset_Handler 从已屏蔽普通中断的受控状态重新初始化，再显式开中断。

跳转前必须先完成整镜像长度、硬件兼容性、hash、签名和防回退验证。向量表范围检查只能防止明显非法跳转，不提供真实性。

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

用断电点枚举验证，而不是只在下载中途拔一次电：

| 断电点 | 重启后必须成立 |
|---|---|
| 下载分片前后 | 当前确认镜像仍可启动；未完成镜像不参与选择 |
| 擦除/复制 App 中 | 能从完整下载镜像继续安装，或回到未被覆盖的旧槽 |
| 写 metadata 单个字段时 | 双副本或日志记录能选出最后一个完整事务 |
| 首次启动自检前 | 启动计数有上限，超限后回滚 |
| 写 CONFIRM 时 | 重启后只能得到“继续试运行”或“已确认”，不能误标坏镜像 |

CRC 能发现部分随机损坏，不能证明事务顺序正确。测试应在每个 Flash erase/program 操作后注入复位，检查上面的不变量。

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

## 深入思考

1. 真 A/B、App + Download 复制安装、scratch swap 三种方案各自用什么资源换取恢复能力？
2. 新 App 能运行到 `main()` 是否足以 CONFIRM？哪些自检必须完成，谁决定超时？
3. 如果 Bootloader 的公钥需要轮换，如何避免旧 Bootloader 无法验证新密钥，又不允许攻击者恢复已撤销密钥？
4. Bootloader 自身升级时，原来的“Bootloader 永远可用”不变量如何重写？

## 练习

1. 设计一个 512 KB Flash 的 Bootloader/App/Download/Config 分区。
2. 写一个固件头结构体，并说明每个字段用途。
3. 画出 OTA 断电发生在下载、校验、切换、首次启动时的恢复逻辑。

## 工程洞察

Bootloader 和 OTA 的本质不是“把新固件写进去”，而是在不可信输入、随时断电、版本不兼容和远程不可达的条件下，维护“设备至少还能启动到一个可信可恢复状态”的不变量。这个主题天然训练系统思维，因为它同时涉及 Flash 分区、链接地址、签名、状态机、应用健康检查、配置兼容和现场诊断。

设计 OTA 时至少分清四个成功层级：

- 下载成功：字节被接收并写入某个区域。
- 校验成功：镜像完整且格式合法。
- 认证成功：镜像来自可信发布方且未被篡改。
- 业务成功：新固件启动、完成健康检查，并能继续执行核心业务。

只证明前两个层级，不能说明 OTA 可上线。上线方案还要处理回滚、灰度、版本限制、配置迁移、失败计数、错误码和远程可观测性。

开放问题：

1. 如果新 App 能启动但无法联网上报健康状态，Bootloader 应该何时判定失败？
2. 如果配置 schema 随固件升级变化，回滚到旧固件时如何避免读取不兼容配置？
3. 如果 Flash 空间不足以做完整双分区，单分区差分、压缩包、外部存储各自牺牲了什么不变量？
