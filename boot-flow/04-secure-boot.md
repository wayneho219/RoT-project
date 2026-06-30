---
tags: [boot-flow, module]
topic: boot-flow
week: "5-6"
---
# Boot Flow Module 4：Secure Boot 驗證機制

## Secure Boot 的核心問題

```
問題：攻擊者如果修改了 NOR Flash 裡的 firmware，怎麼阻止它執行？

解法：數位簽章 + 不可變的信任根（Root of Trust）

信任鏈：
  OTP 中燒錄的 Public Key（不可修改）
    └── 驗證 firmware 的數位簽章
          └── 只有持有對應 Private Key 的人才能建立有效簽章
```

---

## Chain of Trust（信任鏈）

```
Root of Trust（RoT Public Key，燒在 OTP 中）
  │  驗證
  ▼
M33 Secure Firmware（包含 M33 執行碼 + A35 firmware hash）
  │  驗證
  ▼
A35 Firmware（TF-A BL2，FIP）
  │  驗證（TF-A CoT 機制）
  ▼
U-Boot → Linux

每個環節驗證下一個，任一環節失敗則停止。
```

---

## 驗證流程（M33 側實作）

```c
// Firmware Header 格式（自訂）
typedef struct __attribute__((packed)) {
    uint8_t  magic[4];        // "ROTF"
    uint32_t version;         // Firmware version（防 rollback 用）
    uint32_t image_size;      // Body 的大小（bytes）
    uint8_t  sha256[32];      // Body 的 SHA-256（由 build 工具計算）
    uint8_t  signature[64];   // ECDSA-P256 簽章（對 sha256 簽）
    uint32_t reserved[4];
} FirmwareHeader;

// 驗證邏輯
int verify_firmware(uint32_t fw_addr) {
    FirmwareHeader *hdr = (FirmwareHeader *)fw_addr;
    uint32_t body_addr  = fw_addr + sizeof(FirmwareHeader);

    // 1. 檢查 magic
    if (memcmp(hdr->magic, "ROTF", 4) != 0) return -1;

    // 2. 防 rollback：版本必須 >= OTP 中記錄的最小版本
    uint32_t min_version = bsec_read_fw_min_version();
    if (hdr->version < min_version) return -2;

    // 3. 計算 SHA-256
    uint8_t computed_hash[32];
    sha256(body_addr, hdr->image_size, computed_hash);

    // 4. 比較 hash（使用 constant-time 比較，防 timing attack）
    if (memcmp_ct(computed_hash, hdr->sha256, 32) != 0) return -3;

    // 5. ECDSA 驗章
    const uint8_t *pubkey = get_rotpk_from_otp();
    if (!ecdsa_p256_verify(hdr->sha256, hdr->signature, pubkey)) return -4;

    return 0;  // 驗證通過
}
```

---

## Constant-Time 比較（安全關鍵）

**Timing attack（計時攻擊）是什麼：** 攻擊者透過測量程式執行時間來推測內部資料。例如普通的 `memcmp` 一旦發現某 byte 不同就立刻返回，所以前幾個 byte 正確的話花的時間更長。攻擊者可以用這個時間差猜測正確的 hash 值，byte by byte 推算出來。

一般 `memcmp` 在比較失敗時提早返回，這會洩漏 timing 資訊：

```c
// 不安全：攻擊者透過計時知道前幾個 bytes 是否正確
int memcmp(const void *a, const void *b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (((uint8_t*)a)[i] != ((uint8_t*)b)[i]) return -1;  // 提早返回！
    }
    return 0;
}

// 安全：每次都跑完整個比較，時間固定
int memcmp_ct(const void *a, const void *b, size_t n) {
    volatile uint8_t result = 0;
    const uint8_t *pa = (const uint8_t *)a;
    const uint8_t *pb = (const uint8_t *)b;
    for (size_t i = 0; i < n; i++) {
        result |= (pa[i] ^ pb[i]);  // 任何 bit 不同都會讓 result != 0
    }
    return result == 0 ? 0 : -1;
}
```

---

## Rollback Protection（防降版攻擊）

攻擊者可能用舊版（有漏洞的）firmware 替換：

```c
// BSEC OTP 記錄最小允許的 firmware 版本
// 每次 firmware 升版後，燒錄 OTP（不可逆）
// 攻擊者無法回到舊版本

// Anti-rollback counter 用 OTP bit 計數（每 bit 代表一個版本）
// OTP bit 只能從 0 變 1，不能反向
// 版本 0 = 0b0000_0000_0000
// 版本 1 = 0b0000_0000_0001
// 版本 2 = 0b0000_0000_0011
// 版本 3 = 0b0000_0000_0111
uint32_t otp_get_min_version(void) {
    uint32_t otp_val = bsec_read(OTP_ROLLBACK_IDX);
    // 計算有幾個 bit 是 1
    uint32_t version = 0;
    while (otp_val & 1) { version++; otp_val >>= 1; }
    return version;
}
```

---

## 金鑰管理

```
生產環境金鑰管理：

Private Key（ECDSA-P256）
  └── 存在 HSM（Hardware Security Module）中
  └── 永遠不離開 HSM
  └── Build 系統送 hash 進去，HSM 返回簽章

Public Key（ROTPK）
  └── 計算 SHA-256（Public Key Hash = ROTPKH）
  └── 燒錄到 OTP（出廠時，不可逆）
  └── M33 firmware 執行時從 OTP 讀取並驗證 firmware 簽章

開發環境（沒有 HSM）：
  └── 用 openssl 建立 test key pair（不用於生產）
  └── Test ROTPK 燒在 OTP 的 "development" fuse 位置
  └── 正式量產前換成生產 key
```

### Build 工具流程

```bash
# 1. 產生 firmware binary
make all

# 2. 計算 SHA-256
sha256sum a35_firmware.bin > hash.txt

# 3. 用私鑰簽署（開發環境用 openssl；生產用 HSM）
openssl dgst -sha256 -sign private_key.pem \
  -out signature.bin a35_firmware.bin

# 4. 打包 header + firmware
python3 tools/pack_firmware.py \
  --firmware a35_firmware.bin \
  --signature signature.bin \
  --version 1 \
  --output firmware_signed.bin
```

---

## Secure Boot 鎖定選項（OTP Fuses）

```
STM32MP2 相關 OTP 配置：
  SECURE_BOOT_EN  = 1   → 啟用 Secure Boot（燒錄後不可停用）
  JTAG_DISABLE    = 1   → 關閉 JTAG debug 介面（量產必要）
  BOOT_SRC_LOCK   = 1   → 鎖定 boot source（防 SD 卡 bypass）
  ROTPKH[0..3]         → Root of Trust Public Key Hash（128 bits）
```

---

## 下一步

→ 回到 [TrustZone](../trustzone/README.md) 了解軟體隔離
