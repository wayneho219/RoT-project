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
Normal World（NW）               Secure World（SW）
────────────────                 ────────────────
Linux / Android                  OP-TEE / Custom Secure OS
User apps                        Trusted Applications（TA）
Drivers                          Secure Drivers（UART、Storage）

記憶體：NW 的記憶體                記憶體：SW 的記憶體（對 NW 不可見）
硬體：大多數硬體                   硬體：部分硬體只有 SW 能用（FIQ、安全計時器）
```

---

## TrustZone 保護什麼

在 RoT 專案中，TrustZone 保護：

```
Secure World 持有：
  ├── ECDSA Private Key（永遠不離開 SW）
  ├── AES 加密金鑰（Secure Storage 用）
  ├── 驗證邏輯程式碼（firmware 驗章、rollback 計數器）
  └── OTP 讀取結果（ROTPK hash）

Normal World 只能：
  ├── 請求 SW 做加密運算（結果傳回，key 不傳出）
  ├── 請求 Secure Storage 存取（TA 作仲介）
  └── 透過 SMC / ioctl 呼叫 SW 提供的 API
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

TrustZone 是硬體，**TEE** 是建立在 TrustZone 上的軟體環境標準：

```
GlobalPlatform TEE 規範
  └── OP-TEE（開源 TEE 實作）
        ├── TEE Core（EL1-S 的 OS）
        ├── Trusted Applications（EL0-S）
        │     ├── 用 GlobalPlatform TEE API 呼叫加密服務
        │     └── 存取 Secure Storage
        └── TEE Supplicant（NW 的 daemon，作 TA 的 helper）
```

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
