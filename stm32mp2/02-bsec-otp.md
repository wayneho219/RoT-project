---
tags: [stm32mp2, module]
topic: stm32mp2
week: "9+"
---
# STM32MP2 Module 2：BSEC 與 OTP

## BSEC 是什麼

**BSEC（Boot and Security Controller）** 管理 STM32MP215F 的 OTP fuses：

```
OTP (One-Time Programmable)
  └── Each bit can only be set 0 -> 1 (irreversible)
  └── Data persists across power cycles
  └── Stores: ROTPKH, HUK, debug lock, rollback counter

BSEC responsibilities:
  ├── Manage OTP read/write (requires specific voltage to program)
  ├── Secure side: full OTP access
  └── Non-Secure side: read non-sensitive OTP only
```

---

## OTP 空間規劃（STM32MP215F）

OTP 共有 384 個 32-bit words（12,288 bits），分三個區域（RM0506 Table 33-36）：

```
Lower OTP（OTP0–OTP127）
  OTP0–9    OTP_HW_WORD / ID   ST 生命週期管理（BSEC 安全狀態、元件 ID）
  OTP10–22  BOOTROM_CONFIG     Boot ROM 設定（見下方重要欄位）
  OTP24–31  BOOTROM_TZ_EPOCH   Secure side 防回滾 Epoch（8 words = 256 bits）
  OTP32–39  BOOTROM_NS_EPOCH   NS side 防回滾 Epoch（8 words = 256 bits）
  OTP40–101                    Customer 可用
  OTP102–127                   ST 校準 / 工程 / 記憶體修復（不可動）

Mid OTP（OTP128–OTP255）
  OTP128–151 STM32CERTIF0–23  ST 裝置憑證公鑰（ST 使用）
  OTP152–159 OEM_KEY1_ROT0–7  OEM Key1 ROTPKH（Root-of-Trust PK Hash）
                                8 × 32bit = 256 bits（完整 SHA-256 hash）← 這是我們燒的
  OTP160–167 OEM_KEY2_ROT0–7  OEM Key2 ROTPKH（備用）
  OTP168–179 STM32PUBKEY0–11  ST 裝置公鑰（ST 使用）
  OTP180–254                   Customer 可用

Upper OTP（OTP256–OTP383）
  OTP256–259 OTP_RMA_LOCK_PSWD  RMA 密碼
  OTP260–347 OEM Secrets         OEM 機密（客戶可用）
  OTP348–355 OEM_KEY2_EDMK0–7  OEM Key2 加密主金鑰（FSBLM 解密）
  OTP356–363 OEM_KEY1_EDMK0–7  OEM Key1 加密主金鑰（FSBLA/M 解密）
  OTP364–375 STM32ENCPRVKEY     ST ECC 裝置加密私鑰
  OTP376–383 HWKEY0–7           Hardware Unique Key（HUK）
                                 8 × 32bit = 256 bits
                                 Wired to SAES，No Access（軟體完全無法讀取）
```

關鍵 Boot ROM 配置欄位：
```
OTP12 (BOOTROM_CONFIG_3)
  bit[31:0] = oem_fsbla_monotonic_counter
  OEM FSBL 防回滾計數器（版本號 = 最高有效 bit 的位置，MSB-encoded）

OTP18 (BOOTROM_CONFIG_9)
  bit[3:0]  = secure_boot（0=CLOSED_UNLOCKED，[1-15]=CLOSED_LOCKED 強制驗章）
  bit[11:8] = debug_lock（0=允許 debug，[1-15]=鎖住 JTAG）
```

---

## BSEC API

### 讀取 OTP

```c
#include "stm32mp2xx_hal_bsec.h"

// 讀取 ROTPKH（OEM_KEY1_ROT0-7：OTP152-159，8 words = 32 bytes = 256 bits）
uint32_t rotpkh[8];
for (int i = 0; i < 8; i++) {
    HAL_BSEC_OTP_Read(&hbsec, 152 + i, &rotpkh[i]);
}

// 組合成 32 bytes array（完整 SHA-256 hash）
uint8_t rotpkh_bytes[32];
for (int i = 0; i < 8; i++) {
    rotpkh_bytes[i*4+0] = (rotpkh[i] >>  0) & 0xFF;
    rotpkh_bytes[i*4+1] = (rotpkh[i] >>  8) & 0xFF;
    rotpkh_bytes[i*4+2] = (rotpkh[i] >> 16) & 0xFF;
    rotpkh_bytes[i*4+3] = (rotpkh[i] >> 24) & 0xFF;
}

// HUK（HWKEY，OTP376-383）: No Access，軟體無法讀取！
// 直接由 SAES 硬體透過內部 wire 使用（見下方 HUK 章節）
```

### 燒錄 OTP（不可逆！）

```c
// 燒錄前必須確認 OTP programming voltage 已啟用
// STM32MP215F-DK 上通過 PMIC 控制

// 燒錄 ROTPKH word 0（OEM_KEY1_ROT0 = OTP152）
uint32_t rotpkh_word0 = 0x2cf24dba;   // SHA-256 前 4 bytes
int ret = HAL_BSEC_OTP_Program(&hbsec,
                                152,          // word index（OTP152 = OEM_KEY1_ROT0）
                                rotpkh_word0, // value
                                false);       // lock = false（先不鎖）
// 返回 0 = 成功

// 永久鎖定 ROTPKH（讓 OTP152-159 不能再寫）
for (int i = 0; i < 8; i++) {
    HAL_BSEC_OTP_Lock(&hbsec, 152 + i);
}
```

---

## Rollback Counter 實作

```c
// MSB-encoded monotonic counter：版本號 = 最高有效 bit 的位置（從 1 算起）
// OTP12 (BOOTROM_CONFIG_3) bit[31:0] = oem_fsbla_monotonic_counter
#define OTP_FW_MIN_VER_WORD  12

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

HUK 使用規則：
```
HWKEY（HUK）：OTP376-383，8 × 32bit = 256 bits
  - No Access：軟體完全無法讀取，HAL_BSEC_OTP_Read() 對這些 word 無效
  - Wired directly to SAES：透過硬體內部 wire 連接，key 值永遠不進入 CPU
  - 使用方式：SAES 設定 CRYP_KEYSEL_HW，讓 SAES 自行從 HWKEY wire 取 key
  - 不需要（也不可能）手動讀取 HUK；不需要 memset 清除（因為根本讀不到）
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

# 2. 燒錄 ROTPKH（計算自 test_pubkey.pem，完整 SHA-256 = 32 bytes = 8 words）
#    OEM_KEY1_ROT0-7 = OTP152-159
python3 tools/calc_rotpkh.py --key test_pubkey.pem --output rotpkh.hex
for i in $(seq 0 7); do
  STM32_Programmer_CLI -c port=SWD \
    -otp fuse write $((152 + i)) $(cut -c$((i*8+1))-$((i*8+8)) rotpkh.hex)
done

# 3. 確認燒錄結果
STM32_Programmer_CLI -c port=SWD -otp dump | grep "OTP15[2-9]"

# 4. 在量產時才鎖定（開發期間先不鎖，方便換 key）
```

---

## 下一步

→ [Module 3：M33 TD 初始化](03-m33-td-setup.md)
