---
tags: [trustzone, module]
topic: trustzone
week: "5-6"
---
# TrustZone Module 1：概覽

## 核心概念

```
沒有 TrustZone：
  軟體漏洞 → 攻擊者讀取 key → 偽造簽章 → 系統被控制

有 TrustZone：
  Secure World 的 key 在硬體層面對 Normal World 不可見
  即使 Linux 被完全 root，攻擊者也無法讀取 Secure 側的 key
```

**TrustZone 不是軟體的沙盒（sandbox），是硬體的隔離邊界。**

---

## 兩個世界（World）

```
Normal World (NW)                Secure World (SW)
────────────────                 ────────────────
Linux / Android                  OP-TEE / Custom Secure OS
User apps                        Trusted Applications (TA)
Drivers                          Secure Drivers (UART, Storage)
Memory: NW RAM                   Memory: SW RAM (invisible to NW)
HW: most peripherals             HW: secure-only (FIQ, secure timer)
```
NW 記憶體對 SW 可見；SW 記憶體對 NW 不可見。SW 獨佔部分硬體（FIQ、安全計時器）。

---

## TrustZone 保護什麼

在 RoT 專案中，TrustZone 保護：

```
Secure World holds:                   Normal World can only:
  ├── ECDSA Private Key (never         ├── request crypto ops (result
  │    leaves SW)                      │    returned, key stays in SW)
  ├── AES Key (Secure Storage)         ├── request Secure Storage access
  ├── verify logic (fw sig,            │    (TA acts as broker)
  │    rollback counter)               └── call SW APIs via SMC / ioctl
  └── OTP read result (ROTPK hash)
```

---

## 使用場景

| 場景 | TrustZone 扮演的角色 |
|------|---------------------|
| Mobile Payment | 指紋資料在 Secure World 驗證 |
| DRM 金鑰保護 | 解密 key 永遠在 Secure World |
| Secure Boot | M33 在 Secure 側驗證 firmware |
| 遠端驗證（Attestation）| SW 產生的 report 帶有硬體簽章 |
| OTA 更新 | 只有帶有合法簽章的 image 才能 apply |

---

## TEE（Trusted Execution Environment）

**TEE = 可信任執行環境**：TrustZone 是硬體隔離機制，TEE 是建立在上面的**軟體執行環境標準**，規定 Secure World 的 OS 和應用程式怎麼互動。

```
TrustZone (hardware)
  └── TEE (Secure World software environment)
        └── OP-TEE (open-source TEE OS)
              ├── TEE Core (Secure EL1 OS: manages TA, memory, crypto API)
              ├── Trusted Applications (TA):
              │     EL0-S secure programs, one TA per function
              │     e.g. fingerprint_ta.ta, keystore_ta.ta
              └── TEE Supplicant:
                    helper daemon running in Normal World
                    lets TA access NW filesystem (TA cannot reach Linux fs directly)
```
TrustZone 是硬體機制；TEE 是建立在上面的軟體執行環境標準；OP-TEE 是最常用的開源實作。

**類比：**
- TEE Core = Secure World 的「迷你 OS」（像 Linux kernel，但只有幾萬行）
- Trusted Application (TA) = Secure World 的「應用程式」（像 Linux 的 user app）
- Normal World app 透過 ioctl 呼叫 TA，就像呼叫 web service 的 API

---

## SMC 呼叫慣例（ARM SMCCC）

NW 透過 SMC 指令呼叫 SW：

```c
// SMC Calling Convention（SMCCC）
// X0 = Function Identifier（決定要叫什麼服務）
// X1–X7 = 參數
// X0 返回結果

// OP-TEE SMC Identifier 範例
#define OPTEE_SMC_CALL_WITH_ARG  0x32000004

// Linux kernel 的 OP-TEE driver 呼叫方式
arm_smccc_smc(OPTEE_SMC_CALL_WITH_ARG, 
              arg_pa,     // 實體位址（包含 session、命令、參數）
              0, 0, 0, 0, 0, 0,
              &res);      // 結果
```

---

## TrustZone 的限制

TrustZone **不能** 防禦：
- 硬體攻擊（探針、DPA 側信道）
- Secure World 自身的軟體漏洞（TA 寫得不好）
- M33 ROM code 的漏洞（這是真正的硬體 RoT）

TrustZone 是整個安全系統的一部分，不是全部。

---

## 下一步

→ [Module 2：A-profile TrustZone（A35 側）](02-tz-a-profile.md)
