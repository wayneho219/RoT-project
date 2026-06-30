---
tags: [boot-flow, module]
topic: boot-flow
week: "5-6"
---
# Boot Flow Module 1：完整開機鏈

## 概覽

上電後進入開機鏈；A35 reset 由 M33 Secure Firmware 驗證通過後 release。

```
STM32MP215F-DK Boot Chain

Power-on
  │
  ▼
A35 ROM Code (BootROM)
  │  └── boot device (microSD / eMMC / USB)
  │  └── load & verify TF-A BL2（fsbl1/fsbl2 partition）
  ▼
TF-A BL2（early stage）
  │  └── load M33 Secure Firmware into SYSRAM
  │  └── start M33 via RCC
  ▼
M33 Secure Firmware                   ← runs in parallel with A35 boot
  │  └── verify A35 fw (SHA-256 + ECDSA) from microSD
  │  └── hold A35 in reset until verification done
  │  └── pass → release A35 reset
  │  └── fail → lockdown
  ▼
A35 ROM Code (re-entry after M33 release)  ← A35 reset released here
  │  └── boot device (microSD, STM32MP215F-DK 預設)
  │  └── load & verify TF-A BL2
  ▼
TF-A BL2 (EL1-S, Secure)
  │  └── init DDR
  │  └── load BL31 (Secure Monitor)
  │  └── load BL33 (U-Boot)
  │  └── optional: load BL32 (OP-TEE)
  ▼
TF-A BL31 (EL3, resident)
  │  └── init Secure Monitor
  │  └── config TZASC (TrustZone Address Space Ctrl)
  │  └── ERET to BL33
  ▼
U-Boot (EL1-NS, BL33)
  │  └── init peripherals (net, USB ...)
  │  └── load Linux kernel + DTB + initramfs
  │  └── booti → jump to Linux
  ▼
Linux kernel (EL1-NS)
  │  └── mount rootfs
  │  └── start init / systemd
  ▼
User space
```

---

## 各階段詳解

### M33 Secure Firmware（我們要實作的部分）

M33 firmware 由 TF-A BL2 載入 SYSRAM 並啟動，不從外部 Flash 自行 boot。

```c
// 大略邏輯
int main(void) {
    // 1. 初始化硬體
    crypto_engine_init();
    sdmmc_init();          // microSD（SDMMC1）

    // 2. 從 microSD 讀取 A35 firmware header（在 fip partition）
    FirmwareHeader hdr;
    sdmmc_read(A35_FW_LBA, &hdr, sizeof(hdr));

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
STM32MP215F-DK 板上 4 個 boot pin 撥碼開關（BOOT[3:0]）：
  預設 = SD card boot（STM32MP215F-DK 出廠設定）
  其他模式（eMMC、QSPI NOR、USB DFU 等）透過撥碼開關切換
  完整 BOOT pin 對應表見 RM0506 OTP11(BOOTROM_CONFIG_2) boot_source_disable 欄位
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
