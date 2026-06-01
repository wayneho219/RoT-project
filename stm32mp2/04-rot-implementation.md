---
tags: [stm32mp2, module]
topic: stm32mp2
week: "9+"
---
# STM32MP2 Module 4：完整 RoT 實作

## 整體架構回顧

```
專案目標：
  M33-TD 上電後驗證 NOR Flash 中的 A35 firmware（SHA-256 + ECDSA）
  通過 → release A35 reset
  失敗 → A35 永遠不啟動（防止執行被篡改的 firmware）
```

---

## 完整驗證流程

```c
// rot_verify.c

#include "rot_verify.h"
#include "bsec.h"
#include "nor_flash.h"
#include "hal_hash.h"
#include "hal_pka.h"
#include "memcmp_ct.h"

#define NOR_A35_FW_ADDR  0x00040000UL  // NOR Flash offset

typedef struct __attribute__((packed)) {
    uint8_t  magic[4];      // "ROTF"
    uint32_t version;       // firmware 版本號
    uint32_t image_size;    // firmware body 的 bytes 數
    uint8_t  pubkey[65];    // ECDSA P-256 Public Key（04 || x || y）
    uint8_t  sha256[32];    // firmware body 的 SHA-256
    uint8_t  signature[64]; // ECDSA 簽章（r || s）
    uint32_t crc32;         // header 本身的 CRC32（防意外損毀）
} FirmwareHeader;

int verify_a35_firmware(const uint8_t rotpkh[16]) {
    FirmwareHeader hdr;
    int ret;

    // ── Step 1: 讀取 firmware header ────────────────────
    ret = nor_read(NOR_A35_FW_ADDR, (uint8_t *)&hdr, sizeof(hdr));
    if (ret != 0) return ROT_ERR_FLASH_READ;

    // ── Step 2: 驗證 magic ───────────────────────────────
    if (memcmp(hdr.magic, "ROTF", 4) != 0) return ROT_ERR_BAD_MAGIC;

    // ── Step 3: Header CRC32 校驗（防 NOR 靜默錯誤）──────
    uint32_t computed_crc = crc32_compute((uint8_t *)&hdr,
                                           sizeof(hdr) - sizeof(uint32_t));
    if (computed_crc != hdr.crc32) return ROT_ERR_HEADER_CRC;

    // ── Step 4: Rollback 保護 ──────────────────────────
    uint32_t min_ver = bsec_get_fw_min_version();
    if (hdr.version < min_ver) return ROT_ERR_ROLLBACK;

    // ── Step 5: 驗證 Public Key 和 OTP 的 ROTPKH ────────
    uint8_t pk_hash[16];
    // 注意：本專案用前 128 bits（ROTPKH 是 SHA-256 的前半）
    sha256_partial(hdr.pubkey, 65, pk_hash, 16);
    if (memcmp_ct(pk_hash, rotpkh, 16) != 0) return ROT_ERR_BAD_PUBKEY;

    // ── Step 6: 計算 firmware body 的 SHA-256 ──────────
    // firmware body 緊接在 header 後面
    uint32_t body_addr = NOR_A35_FW_ADDR + sizeof(FirmwareHeader);
    uint8_t  computed_hash[32];
    
    ret = hardware_sha256_nor(body_addr, hdr.image_size, computed_hash);
    if (ret != 0) return ROT_ERR_HASH_FAIL;

    // ── Step 7: 比較 hash（constant-time）──────────────
    if (memcmp_ct(computed_hash, hdr.sha256, 32) != 0) return ROT_ERR_HASH_MISMATCH;

    // ── Step 8: ECDSA P-256 驗章（PKA 硬體加速）────────
    ret = hardware_ecdsa_verify(computed_hash, hdr.signature, hdr.pubkey);
    if (ret != 0) return ROT_ERR_SIGNATURE;

    // ── Step 9: 更新 IPC shared memory ────────────────
    ipc_set_verified_version(hdr.version);

    return ROT_OK;
}
```

---

## 硬體 SHA-256（邊讀 NOR Flash 邊計算）

```c
// 不把整個 firmware 載入 RAM（M33 SRAM 有限），邊讀邊 hash
int hardware_sha256_nor(uint32_t nor_offset, uint32_t size, uint8_t out[32]) {
    HASH_HandleTypeDef hhash = {0};
    hhash.Instance = HASH;
    hhash.Init.DataType = HASH_DATATYPE_8B;
    hhash.Init.Algorithm = HASH_ALGOSELECTION_SHA256;
    HAL_HASH_Init(&hhash);

    #define CHUNK_SIZE 256  // 每次讀 256 bytes
    uint8_t chunk[CHUNK_SIZE];
    
    uint32_t remaining = size;
    uint32_t offset = nor_offset;
    
    while (remaining > 0) {
        uint32_t to_read = MIN(remaining, CHUNK_SIZE);
        nor_read(offset, chunk, to_read);
        
        if (remaining == to_read) {
            // 最後一塊：用 HASH_LAST flag 觸發最終計算
            HAL_HASH_Accumulate_IT(&hhash, chunk, to_read);
        } else {
            HAL_HASH_Accumulate(&hhash, chunk, to_read, HAL_MAX_DELAY);
        }
        
        remaining -= to_read;
        offset    += to_read;
    }
    
    HAL_HASH_GetDigest(&hhash, out, HAL_MAX_DELAY);
    HAL_HASH_DeInit(&hhash);
    return 0;
}
```

---

## 硬體 ECDSA 驗章（PKA）

```c
int hardware_ecdsa_verify(
    const uint8_t hash[32],
    const uint8_t sig[64],      // r(32) || s(32)
    const uint8_t pubkey[65]) { // 04 || x(32) || y(32)

    // P-256 曲線參數
    static const uint8_t p256_p[] = {  // 模數
        0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x01,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0xFF,0xFF,0xFF,0xFF,
        0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF };
    static const uint8_t p256_a[] = {  // a = -3
        0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x01,
        /* ... 完整 32 bytes */ };
    static const uint8_t p256_n[] = { /* order */ };

    PKA_ECDSAVerifInTypeDef in = {
        .primeOrderSize    = 32,
        .modulusSize       = 32,
        .coefSign          = 1,         // a 的符號（P-256 a < 0）
        .coef              = p256_a,
        .modulus           = p256_p,
        .primeOrder        = p256_n,
        .pPubKeyCurvePtX   = pubkey + 1,    // 跳過 04 前綴
        .pPubKeyCurvePtY   = pubkey + 33,
        .RSign             = sig,
        .SSign             = sig + 32,
        .hash              = hash,
    };

    PKA_HandleTypeDef hpka = {0};
    hpka.Instance = PKA;
    HAL_PKA_Init(&hpka);
    HAL_PKA_ECDSAVerif(&hpka, &in, HAL_MAX_DELAY);
    
    uint32_t result = HAL_PKA_ECDSAVerif_IsValidSignature(&hpka);
    HAL_PKA_DeInit(&hpka);
    
    return (result == 1) ? 0 : -1;
}
```

---

## 錯誤碼與安全策略

```c
// rot_error.h
typedef enum {
    ROT_OK                 = 0,
    ROT_ERR_FLASH_READ     = 1,
    ROT_ERR_BAD_MAGIC      = 2,
    ROT_ERR_HEADER_CRC     = 3,
    ROT_ERR_ROLLBACK       = 4,
    ROT_ERR_BAD_PUBKEY     = 5,
    ROT_ERR_HASH_FAIL      = 6,
    ROT_ERR_HASH_MISMATCH  = 7,
    ROT_ERR_SIGNATURE      = 8,
} ROT_Error;

// 安全策略：任何錯誤都鎖死
// 錯誤碼只寫入 IPC shared memory（給 A35 的 debug 工具用）
// 外部不可見（防 oracle attack）

void rot_halt(ROT_Error err) {
    ipc->m33_status = M33_STATUS_FAIL | (err << 8);
    __DSB();
    
    // 選項 A：無限迴圈（A35 永遠不啟動）
    while (1) { __WFI(); }
    
    // 選項 B（進階）：關閉所有 peripheral，進入最低功耗狀態
    // HAL_PWR_EnterSTANDBYMode();  // 需要確認不會被 watchdog 喚醒
}
```

---

## Build System（CMake）

```cmake
# CMakeLists.txt

cmake_minimum_required(VERSION 3.20)
project(rot_m33 C ASM)

# 工具鏈
set(CMAKE_C_COMPILER   arm-none-eabi-gcc)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_OBJCOPY      arm-none-eabi-objcopy)

# Cortex-M33 with TrustZone
set(CPU_FLAGS "-mcpu=cortex-m33 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard")
set(SECURITY_FLAGS "-mcmse")  # 啟用 CMSE（TrustZone security）

set(CMAKE_C_FLAGS "${CPU_FLAGS} ${SECURITY_FLAGS} -Os -Wall -ffunction-sections -fdata-sections")

# Linker script
set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/stm32mp215f_td.ld)
set(CMAKE_EXE_LINKER_FLAGS "-T${LINKER_SCRIPT} -Wl,--gc-sections -specs=nano.specs")

add_executable(rot_m33.elf
    startup/startup_stm32mp215f.s
    src/main.c
    src/rot_verify.c
    src/bsec.c
    src/nor_flash.c
    src/sau_init.c
    third_party/mbedtls/library/sha256.c
    third_party/mbedtls/library/ecdsa.c
    third_party/mbedtls/library/ecp.c
)

# 產生 .bin（用於燒錄）
add_custom_command(TARGET rot_m33.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary rot_m33.elf rot_m33.bin
)
```

---

## 測試策略

```
Unit tests（在開發機上模擬）：
  ├── test_sha256：用已知向量驗證 SHA-256 實作
  ├── test_ecdsa_verify：用已知 key/sig/msg 驗證 ECDSA
  ├── test_memcmp_ct：確認 constant-time 比較的時序
  └── test_rollback：驗證 rollback counter 邏輯

Integration tests（在板子上）：
  ├── 正常流程：正確的 firmware → A35 成功啟動
  ├── hash 損毀：改一個 byte → A35 不啟動
  ├── 簽章錯誤：用錯誤 private key 簽 → A35 不啟動
  └── rollback：放舊版 firmware → A35 不啟動（OTP counter 阻擋）
```

---

## 下一步（履歷目標進度）

```
✅ M33-TD hold/release A35 reset（核心 RoT）
✅ Secure Boot 鏈（hash + ECDSA 簽章驗證）
✅ 金鑰儲存在 M33 側，A35 不可直接存取
✅ Rollback protection
⬜ 基礎 Secure Storage（加密 key blob）← 下一個
⬜ Remote Attestation（選配）
```
