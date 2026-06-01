---
tags: [boot-flow, module]
topic: boot-flow
week: "5-6"
---
# Boot Flow Module 1：完整開機鏈

## 概覽

```
                    ┌─────────────────────────────────────────────────┐
                    │  STM32MP215F-DK 開機鏈                          │
                    │                                                  │
  上電 ──────────▶  M33 ROM Code                                      │
                    │  └── 從 OTP/BSEC 決定 boot source              │
                    │  └── 載入 M33 Secure Firmware（NOR Flash）      │
                    ▼                                                  │
                    M33 Secure Firmware                               │
                    │  └── 驗證 A35 firmware（SHA-256 + ECDSA）       │
                    │  └── 通過 → release A35 reset                  │
                    │  └── 失敗 → 鎖死                               │
                    ▼                                                  │
  A35 reset ──▶    A35 ROM Code（BootROM）                           │
  released          │  └── 決定 boot device（NOR Flash / eMMC / USB）│
                    │  └── 載入並驗證 TF-A BL2                        │
                    ▼                                                  │
                    TF-A BL2（EL1-S，Secure）                         │
                    │  └── 初始化 DDR                                  │
                    │  └── 載入 BL31（Secure Monitor）                 │
                    │  └── 載入 BL33（U-Boot）                         │
                    │  └── 可選：載入 BL32（OP-TEE）                   │
                    ▼                                                  │
                    TF-A BL31（EL3，常駐）                             │
                    │  └── 初始化 Secure Monitor                       │
                    │  └── 配置 TZASC（TrustZone Address Space Ctrl） │
                    │  └── ERET 跳到 BL33                             │
                    ▼                                                  │
                    U-Boot（EL1-NS，BL33）                             │
                    │  └── 初始化周邊（網路、USB 等）                   │
                    │  └── 載入 Linux kernel + DTB + initramfs        │
                    │  └── booti 指令跳到 Linux                        │
                    ▼                                                  │
                    Linux kernel（EL1-NS）                             │
                    │  └── 掛載 rootfs                                 │
                    │  └── 啟動 init / systemd                         │
                    ▼                                                  │
                    User space                                         │
                    └─────────────────────────────────────────────────┘
```

---

## 各階段詳解

### M33 ROM Code

- 存在晶片內部，無法修改（真正的 Root of Trust 起點）
- 讀取 OTP/BSEC 中的設定（boot source、debug 鎖定狀態）
- 從 NOR Flash 載入 M33 Secure Firmware

### M33 Secure Firmware（我們要實作的部分）

```c
// 大略邏輯
int main(void) {
    // 1. 初始化硬體
    crypto_engine_init();
    nor_flash_init();

    // 2. 從 NOR Flash 讀取 A35 firmware header
    FirmwareHeader hdr;
    nor_read(A35_FW_HEADER_ADDR, &hdr, sizeof(hdr));

    // 3. 驗證 magic 和 version
    if (hdr.magic != EXPECTED_MAGIC) goto fail;

    // 4. 計算 firmware body 的 SHA-256
    uint8_t digest[32];
    sha256_compute(A35_FW_BODY_ADDR, hdr.size, digest);

    // 5. 用 public key 驗證 ECDSA 簽章
    if (!ecdsa_verify(digest, hdr.signature, PUBLIC_KEY)) goto fail;

    // 6. 通過 → release A35 reset
    RCC->A35_RSTCR &= ~RCC_A35_RST;  // 解除 A35 reset
    return 0;

fail:
    // 失敗 → 永遠鎖死
    while (1) { /* nothing */ }
}
```

### A35 BootROM

- ST 出廠燒錄，不可修改
- 讀取 boot pins / OTP 決定 boot source
- 載入 TF-A BL2 並做基本驗證（hash 由 OTP 記錄）

### TF-A BL2

- 在 EL1-S（Secure EL1）執行
- 初始化 DRAM（DDR），需要配置 PHY 和 controller
- 使用 FIP（Firmware Image Package）格式載入後續組件
- 結束後跳到 BL31

### TF-A BL31

- 常駐在 EL3，永遠不被卸載
- 實作 PSCI（電源狀態控制介面）：CPU on/off、suspend、reset
- 作為 Secure Monitor：Linux 透過 SMC 呼叫它管理世界切換
- 設定 TZASC：隔離 DDR 的 Secure 和 Non-Secure 區域

### U-Boot（BL33）

```bash
# U-Boot 啟動 Linux 的指令（在 autoboot 倒數後自動執行）
load mmc 0:1 ${kernel_addr_r} Image          # 載入 kernel
load mmc 0:1 ${fdt_addr_r}    stm32mp215.dtb  # 載入 Device Tree
load mmc 0:1 ${ramdisk_addr_r} initramfs.cpio # 載入 initramfs
booti ${kernel_addr_r} ${ramdisk_addr_r} ${fdt_addr_r}
```

---

## Boot Source 選擇

STM32MP2 透過 boot pins 決定從哪裡開機：

```
BOOT0, BOOT1, BOOT2 引腳組合：
  000 → Engineering Boot Mode（JTAG）
  001 → NOR Flash（SPI）
  010 → eMMC
  011 → SD Card
  100 → USB DFU（韌體燒錄模式）
  ...
```

OTP fuses 可以鎖定 boot source，防止攻擊者透過 SD 卡 bypass Secure Boot。

---

## Firmware Image Package（FIP）格式

TF-A 使用 FIP 把多個 image 打包成一個檔案：

```
fip.bin
  ├── bl2.bin     (TF-A BL2)
  ├── bl31.bin    (TF-A BL31 Secure Monitor)
  ├── bl32.bin    (OP-TEE，可選)
  ├── bl33.bin    (U-Boot)
  └── nt_fw_config.dtb (Non-Trusted Firmware Config)
```

建立 FIP：
```bash
fiptool create \
  --tb-fw build/stm32mp2/release/bl2.bin \
  --soc-fw build/stm32mp2/release/bl31.bin \
  --nt-fw u-boot.bin \
  fip.bin
```

---

## 下一步

→ [Module 2：TF-A 深入](02-tf-a.md)
