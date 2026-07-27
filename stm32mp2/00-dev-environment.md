---
tags: [stm32mp2, module]
topic: stm32mp2
week: "9+"
---
# STM32MP2 Module 0：開發環境規劃

## 三台機器怎麼分工

手上可用機器：MacBook Air M4、家用桌機（r7-7700 + RTX4060 + 32GB RAM + 2TB SSD）。

```
問題：ST 官方的 Yocto BSP / STM32CubeProgrammer 命令列工具 / 簽章工具
     幾乎只驗證過 x86_64 Linux

Mac（ARM, macOS）     → 架構 + OS 都不符，build 這塊做不了
桌機 r7-7700（x86_64）→ 唯一符合條件，透過 WSL2 跑 Ubuntu 做 build
```

**結論：build 固定在桌機的 WSL2；Mac 負責編輯、SSH、燒錄跟看 log。**

## WSL2 建置注意事項

- repo 要放在 WSL2 **內部檔案系統**（`~/repo`），不要放 `/mnt/c/...`，跨檔案系統掛載點對 Yocto 這種大量小檔案 I/O 會慢很多
- `.wslconfig` 記得檢查 vhdx 容量上限，Yocto build 產物很容易衝到上百 GB，舊版 Windows 預設上限可能只有 256GB
- `/etc/wsl.conf` 開 `systemd=true`，避免 udev 裝置權限問題
- USB 裝置（ST-Link、序列埠）不會自動進 WSL2，需要裝 `usbipd-win`，在 Windows 端 attach 後才看得到

## 燒錄與 Debug 不能遠端

Build（編譯）不用接線，純軟體工作。但 **燒錄 / 看開機 log / debug 一定要實體接線**，USB 沒辦法輕易虛擬化到遠端主機。

板子插在哪台機器，這件事就得在哪台機器做。目前規劃：板子接在 **Mac** 上，方便日常查看。

### 需要買的線材（STM32MP215F-DK）

板子沒有內建 VCP（跟一般 Nucleo/Discovery 板「一條 USB 全搞定」不一樣），需要：

| 接口 | 用途 | 線材 |
|---|---|---|
| CN10 | 電源 + 燒錄（STM32CubeProgrammer） | USB-C，需支援 **USB PD** |
| CN1 | 序列 console（推測 A35 / Linux） | USB to TTL 轉接器（3.3V，不要買 5V 版本） |
| CN2 | 序列 console（推測 M33 / RoT firmware） | USB to TTL 轉接器（同上，3.3V） |

## 最終工作流程

```
桌機 WSL2                    Mac
─────────                    ───
1. 寫 code / build image
2. ─────── scp 傳 image ───→
                              3. STM32CubeProgrammer 燒錄
                              4. picocom 看兩條 UART log（CN1/CN2）
```

日常大部分時間板子插著不動，只有下列情況才需要手動碰：
- 切換 boot mode（進 USB DFU 救磚模式時，board 上的 BOOT pin/開關要手動撥）
- Reset 板子 / 序列埠沒反應時拔插電源
- WSL2 重開機後，`usbipd attach` 可能要重下一次（可寫腳本，不算碰板子）

## 各機器要裝什麼

**桌機（WSL2/Ubuntu）**

| 項目 | 大小 |
|---|---|
| Ubuntu + build-essential 等 Yocto build 依賴 | 幾百 MB |
| OpenSTLinux Yocto BSP（source + sstate-cache + 產物） | 100-200 GB |
| GNU Arm Embedded toolchain（編 M33 韌體） | 1-2 GB |
| `repo` 工具 | 幾 MB |

**Mac**

| 項目 | 大小 |
|---|---|
| STM32CubeProgrammer（macOS 版，需手動解除 Gatekeeper 隔離） | 500MB-1GB |
| USB-UART 驅動（依買的轉接晶片，FTDI 通常內建，CP210x 需另裝 Silicon Labs 驅動） | 10-20MB |
| `picocom`（`brew install picocom`） | 幾 MB |
| VS Code + Remote-SSH extension（編輯時連進 WSL2） | ~500MB |

Mac 端總共不到 2GB，不會有空間壓力；100-200GB 的 Yocto build 全部算在桌機的 2TB SSD 上。
