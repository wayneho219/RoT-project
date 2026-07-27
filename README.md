# RoT (Root of Trust) 實作專案

## 目標
在 STM32MP215F-DK 上實作基礎 Root of Trust，作為韌體工程師求職履歷專案。

## 硬體
- **開發板**：STM32MP215F-DK（~NT$2,603，Mouser）
- **架構**：Cortex-A35（SoC，System on Chip，單晶片系統）+ Cortex-M33 TD（TD，Trusted Domain，可信任域；RoT MCU）
- **儲存**：microSD（boot source，TF-A（Trusted Firmware-A，ARM（Advanced RISC Machines，架構/晶片 IP 供應商，現稱 Arm）官方開源 Secure Boot 實作）FIP（Firmware Image Package，把多個開機映像檔打包成一個檔案的格式） + Linux image）

## 專案架構概念
```
M33-TD（RoT）
 ├── 上電後先執行
 ├── 從 microSD 讀取 A35 firmware
 ├── 驗證 hash / 簽章
 ├── 通過 → release A35 reset
 └── 失敗 → A35 永遠不開機

A35（Application Processor）
 ├── TF-A BL2（Boot Loader stage 2）→ BL31（Boot Loader stage 3.1，常駐 Secure Monitor）
 ├── U-Boot
 └── Linux（Yocto build）
```

## 學習路線與時程

| 週次 | 主題 | 資料夾 | 重點 |
|------|------|--------|------|
| Week 1–2 | C 語言基礎 | `c-language/` | Module 01–08，指標、位元操作、嵌入式語法 |
| Week 3–4 | ARM 架構概念 | `arm-architecture/` | ARMv8-A（A35）、Exception Level、Cortex-M33 |
| Week 5–6 | Boot Flow + TrustZone | `boot-flow/` `trustzone/` | TF-A 鏈、SAU（Security Attribution Unit）/IDAU（Implementation Defined Attribution Unit）、Secure Storage |
| Week 7–8 | Yocto 實作 | `yocto/` | Layer、Recipe、STM32MP2 image 建立 |
| Week 9 | 密碼學基礎 | `cryptography/` | SHA-256、ECDSA、AES-GCM、PKI（Public Key Infrastructure，公開金鑰基礎設施）鏈 |
| Week 9+ | STM32MP2 實作 | `stm32mp2/` | BSEC（Boot and Security，ST 的 OTP 控制器）/OTP（One-Time Programmable，一次性可燒錄記憶體）、M33 TD 初始化、完整 RoT |
| 補充 | 網路與協定 | `networking/` | OSI 7 層、UART/SPI/I2C、CAN Bus、TLS、MQTT |

## 履歷目標功能
1. M33-TD hold/release A35 reset（核心 RoT）
2. Secure Boot 鏈（hash + ECDSA 簽章驗證）
3. 金鑰儲存在 M33 側，A35 不可直接存取
4. Rollback protection
5. 基礎 Secure Storage
6. （選配）Remote Attestation
