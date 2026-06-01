---
tags: [stm32mp2, module]
topic: stm32mp2
week: "9+"
---
# STM32MP2 Module 2：BSEC 與 OTP

## BSEC 是什麼

**BSEC（Boot and Security Controller）** 管理 STM32MP215F 的 OTP fuses：

```
OTP（One-Time Programmable）
  └── 一次性燒錄的記憶體（每個 bit 只能從 0 → 1）
  └── 斷電後資料永久保留
  └── 存放：ROTPKH、HUK、debug 鎖定、rollback counter

BSEC 的職責：
  ├── 管理 OTP 讀寫（需要特定 voltage 才能燒錄）
  ├── Secure 側：完整 OTP 存取
  └── Non-Secure 側：只能讀非敏感的 OTP
```

---

## OTP 空間規劃（STM32MP215F）

```
OTP 共有 384 個 32-bit words（12 KB）

Word 0–7    上部 OTP（ST 保留）
Word 8–11   安全 OTP（加密相關，非 Secure 無法讀）
  Word 8    ROTPKH[0:31]       Root of Trust PK Hash（低 32 bits）
  Word 9    ROTPKH[32:63]
  Word 10   ROTPKH[64:95]
  Word 11   ROTPKH[96:127]     共 128 bits = SHA-256 取前半
  
Word 12     HUK[0:31]          Hardware Unique Key（低 32 bits）
Word 13     HUK[32:63]
Word 14     HUK[64:95]
Word 15     HUK[96:127]        共 128 bits

Word 16     Config
  Bit 0     SECURE_BOOT_EN     1 = 啟用 Secure Boot
  Bit 1     JTAG_DISABLE       1 = 關閉 JTAG
  Bit 2     BOOT_SRC_LOCK      1 = 鎖定 boot source
  Bit 3     ECDSA_PK_SHA256    1 = ROTPKH 是 SHA-256（非 SHA-384）

Word 17     FW_MIN_VERSION     Anti-rollback counter（bit 0–31 = 版本 0–31）

Word 18–383  使用者可用 OTP
```

---

## BSEC API

### 讀取 OTP

```c
#include "stm32mp2xx_hal_bsec.h"

// 讀取 ROTPKH（4 個 words = 128 bits）
uint32_t rotpkh[4];
for (int i = 0; i < 4; i++) {
    HAL_BSEC_OTP_Read(&hbsec, 8 + i, &rotpkh[i]);
}

// 組合成 16 bytes array
uint8_t rotpkh_bytes[16];
for (int i = 0; i < 4; i++) {
    rotpkh_bytes[i*4+0] = (rotpkh[i] >>  0) & 0xFF;
    rotpkh_bytes[i*4+1] = (rotpkh[i] >>  8) & 0xFF;
    rotpkh_bytes[i*4+2] = (rotpkh[i] >> 16) & 0xFF;
    rotpkh_bytes[i*4+3] = (rotpkh[i] >> 24) & 0xFF;
}

// 讀取 HUK（Secure Only）
uint32_t huk[4];
for (int i = 0; i < 4; i++) {
    HAL_BSEC_OTP_Read(&hbsec, 12 + i, &huk[i]);
}
```

### 燒錄 OTP（不可逆！）

```c
// 燒錄前必須確認 OTP programming voltage 已啟用
// STM32MP215F-DK 上通過 PMIC 控制

// 燒錄 ROTPKH word 0
uint32_t rotpkh_word0 = 0x2cf24dba;   // SHA-256 前 4 bytes
int ret = HAL_BSEC_OTP_Program(&hbsec, 
                                8,            // word index
                                rotpkh_word0, // value
                                false);       // lock = false（先不鎖）
// 返回 0 = 成功

// 永久鎖定 ROTPKH（讓這幾個 word 不能再寫）
HAL_BSEC_OTP_Lock(&hbsec, 8);
HAL_BSEC_OTP_Lock(&hbsec, 9);
HAL_BSEC_OTP_Lock(&hbsec, 10);
HAL_BSEC_OTP_Lock(&hbsec, 11);
```

---

## Rollback Counter 實作

```c
// 每個 bit 代表一個版本（累計 set bit 數 = 版本號）
// Word 17 = 32-bit anti-rollback counter

uint32_t get_fw_min_version(void) {
    uint32_t otp_val;
    HAL_BSEC_OTP_Read(&hbsec, OTP_FW_MIN_VER_WORD, &otp_val);
    
    // 計算 set bit 數 = 版本
    uint32_t version = 0;
    while (otp_val & 1) {
        version++;
        otp_val >>= 1;
    }
    return version;
}

void update_fw_min_version(uint32_t new_version) {
    // 只能升版（不能降）
    uint32_t current = get_fw_min_version();
    if (new_version <= current) return;
    if (new_version > 32) return;   // 最多 32 個版本
    
    // 計算新的 OTP value：低 new_version 位都是 1
    uint32_t new_otp_val = (1 << new_version) - 1;
    
    // 燒錄（只會讓 0 → 1，安全）
    HAL_BSEC_OTP_Program(&hbsec, OTP_FW_MIN_VER_WORD, new_otp_val, false);
}
```

---

## HUK 使用注意事項

```
規則：
  1. HUK 只在 Secure World 可讀（Word 12–15 設為 Secure-only OTP）
  2. HUK 絕對不能直接用作 AES key（用 HKDF 衍生）
  3. HUK 絕對不能傳出 M33 / Secure World
  4. 用完立刻清除暫存器（memset）
  
  ST 的 SAES 硬體支援直接從 HUK 加載 key，key 值不經過 CPU
  → 推薦用硬體 SAES，不要用軟體 AES
```

```c
// 使用 SAES（Secure AES）直接從 HUK 加載（key 不通過 CPU）
SAES_HandleTypeDef hsaes;

// 配置 SAES 用 HUK 作為 key
hsaes.Init.KeySelect = CRYP_KEYSEL_HW;  // 直接用 HW key（HUK 衍生）
hsaes.Init.KeyMode   = CRYP_KEYMODE_NORMAL;
hsaes.Init.DataType  = CRYP_DATATYPE_8B;
hsaes.Init.Algorithm = CRYP_AES_GCM_GMAC;

HAL_SAES_Init(&hsaes);

// 加密（key 從來不在 CPU register 中）
HAL_SAES_Encrypt(&hsaes, plaintext, ciphertext, len);
```

---

## 開發板 OTP 燒錄流程（STM32CubeProgrammer）

```bash
# 連接 STM32MP215F-DK（JTAG 或 USB）
# 注意：燒錄 OTP 前確認你有備份！

# 1. 查看當前 OTP 狀態
STM32_Programmer_CLI -c port=SWD -otp dump

# 2. 燒錄 ROTPKH（計算自 test_pubkey.pem）
python3 tools/calc_rotpkh.py --key test_pubkey.pem --output rotpkh.hex
STM32_Programmer_CLI -c port=SWD \
  -otp fuse write 8  $(cut -c1-8  rotpkh.hex) \
  -otp fuse write 9  $(cut -c9-16  rotpkh.hex) \
  -otp fuse write 10 $(cut -c17-24 rotpkh.hex) \
  -otp fuse write 11 $(cut -c25-32 rotpkh.hex)

# 3. 確認燒錄結果
STM32_Programmer_CLI -c port=SWD -otp dump | grep "Word 8"

# 4. 在量產時才鎖定（開發期間先不鎖，方便換 key）
```

---

## 下一步

→ [Module 3：M33 TD 初始化](03-m33-td-setup.md)
