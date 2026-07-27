# Progress Log

專案整體進度紀錄。Append-only 時間軸，新進度往下加，格式為 `### YYYY-MM-DD — 標題`，方便日後用 `grep "^### "` 快速抓時間軸。

目的：對照 README.md 的學習路線表格，記錄「實際做到哪、遇到什麼、下一步是什麼」，而不是教學內容本身（教學內容在各主題資料夾的模組筆記裡）。

---

## 目前狀態總覽

| 週次 | 主題 | 資料夾 | 狀態 | 備註 |
|---|---|---|---|---|
| Week 1–2 | C 語言基礎 | `c-language/` | ✅ 完成 | 8 個模組筆記完成，`practice/module-01~04` 有練習題與預期輸出 |
| Week 3–4 | ARM 架構 | `arm-architecture/` | ✅ 筆記完成 | 尚無對應硬體實作 |
| Week 5–6 | Boot Flow + TrustZone | `boot-flow/` `trustzone/` | ✅ 筆記完成 | 同上，理論階段 |
| Week 7–8 | Yocto 實作 | `yocto/` | 🚧 進行中 | 教學筆記完成；實機 build 已在 `/home/wayne/my-stm/` 跑通並成功產出映像檔（見 2026-07-13），但 repo 內教學文的 `MACHINE` 設定跟實際 build 用的值不一致，待修正 |
| Week 9 | 密碼學 | `cryptography/` | ✅ 筆記完成 | 尚未寫程式驗證 |
| Week 9+ | STM32MP2 實作 / RoT 核心 | `stm32mp2/` | 🚧 規劃階段 | 開發環境三機分工規劃完成；記憶體/暫存器位址已依 RM0506 校對修正；M33-TD reset hold/release、Secure Boot 簽章驗證等核心功能**尚未開始寫程式**，也還沒建立 `meta-rot` 自訂 layer |
| 補充 | 網路與協定 | `networking/` | ✅ 筆記完成 | — |

---

## 時間軸

### 2026-06-01 — 專案啟動

- Repo 初始化（`chore: init repository`、`chore: init main branch`）
- 初版學習筆記架構與 Obsidian 設定建立（PR #1，`5b50131`）

### 2026-06-02 — C 語言練習開始

- 完成 `c-language/practice/module-03`（bit manipulation）練習（PR #2，`27a71b5`）

### 2026-06-30 — 大量補完筆記 + STM32MP2 硬體位址修正

- 補完全模組學習筆記內容、圖表修正、複習答案（PR #3，`831f729`）
- 新增開發板 datasheet PDF，並將 PDF / `.base` 加入 `.gitignore`（避免大型二進位檔進 repo）
- 新增 `raw/` 原始素材與 RPi5 Yocto 筆記（`11fff0b`）
- **依 RM0506（STM32MP2 Reference Manual）逐項校對修正** `stm32mp2/` 的記憶體映射位址、RCC/RIFSC 基底位址、GPIOA 位址（PR #4，`477c5ab` + `f08d94c`）
  - 這是目前唯一一次針對官方規格書逐項核對的紀錄，代表 `stm32mp2/` 筆記中的位址資訊已可信，不是憑印象寫的

### 2026-07-13 — 本機 Yocto Build 環境建立，成功產出可燒錄映像

不在 `RoT-project` repo 內（build tree 太大且含大量產物，另外放在 `/home/wayne/my-stm/`，未進 git）：

- 15:32 建立 `/home/wayne/setup-yocto-stm32.sh` 一鍵建置腳本
- 15:33 clone `poky`（`scarthgap` 分支）
- 陸續 clone `meta-st-stm32mp`、`meta-openembedded`、`meta-st-openstlinux`（皆 `scarthgap` 分支）
- 15:41 設定 `build/conf/local.conf`：
  - `MACHINE = "stm32mp21-disco"`
  - `DISTRO = "openstlinux-weston"`
  - `INIT_MANAGER = "systemd"`
- 加入 8 個 layer 到 `build/conf/bblayers.conf`：`poky/meta`、`meta-poky`、`meta-yocto-bsp`、`meta-oe`、`meta-python`、`meta-networking`、`meta-webserver`、`meta-gnome`、`meta-multimedia`、`meta-st-stm32mp`、`meta-st-openstlinux`
- 執行 `bitbake st-image-weston`，約 17:14 完成，bitbake log 正常結束（非中途中斷）
- 產出完整映像檔於 `build/tmp-glibc/deploy/images/stm32mp21-disco/`，包含：
  - `st-image-weston-...rootfs.ext4`（含 splitted bootfs / rootfs / userfs / vendorfs 分割版本，供燒錄用）
  - `u-boot/`、`arm-trusted-firmware/`、`arm-trusted-firmware-m/`、`optee/`、`fip/` — 開機鏈每一段都有產出
  - `flashlayout_st-image-weston` — 燒錄用 flash layout
  - `devicetree/`、`kernel/`
- 磁碟空間確認：`/`（1007G）已用 114G，剩 842G，短期內不會卡空間

**結論**：Yocto 端到端流程（clone → 設定 layer/machine → build → 產出可燒錄映像）已完整跑通一次，對應 README 週次表 Week 7–8 目標達成。尚未做的是實際燒錄到 STM32MP215F-DK 板子並驗證開機。

### 2026-07-27 — 補完開發環境規劃筆記，本機環境盤點確認

- 補完全模組縮寫全名，新增 `stm32mp2/00-dev-environment.md` 開發環境規劃筆記（PR #5，`c45f780`）：
  - 三台機器分工：MacBook Air M4（編輯 / SSH / 燒錄 / debug）、桌機 r7-7700 + RTX4060（透過 WSL2 跑 build，因 ST 官方工具鏈幾乎只驗證過 x86_64 Linux）
  - WSL2 建置注意事項：repo 要放 WSL2 內部檔案系統、`.wslconfig` vhdx 容量上限、`systemd=true`、USB 裝置需 `usbipd-win`
  - 燒錄/debug 需要實體接線（USB 無法輕易虛擬化到遠端），板子規劃接在 Mac 上
  - 列出 STM32MP215F-DK 所需線材：CN10（USB-C PD 電源+燒錄）、CN1/CN2（3.3V USB-TTL，兩條序列 console）
- 本地 clone repo 到 `/home/wayne/RoT-project`，建立 `Wayne_OuOb` 開發分支，與 `main` 同步到最新 commit `9c77753`
- 盤點並確認本機既有的 `/home/wayne/my-stm/` Yocto build 環境：設定（`MACHINE=stm32mp21-disco`、ST 官方 layer）與 2026-07-13 的產出物均符合專案目標，方向正確
- 建立本檔案（`PROGRESS.md`）作為專案進度的固定追蹤位置

---

## 已知問題 / 待辦

- [ ] `yocto/02-stm32mp2-setup.md` 教學文中寫的 `MACHINE = "stm32mp215f-dk"` / `DISTRO = "openstlinux-eglfs"`，跟實際 `/home/wayne/my-stm/build/conf/local.conf` 用的 `stm32mp21-disco` / `openstlinux-weston` 不一致。需要查 `meta-st-stm32mp/conf/machine/` 確認 STM32MP215F-DK 對應的正確 MACHINE 名稱（disco/dk/ev1 等板卡變體的命名很容易搞混），並回頭更新教學筆記。
- [ ] 尚未把 2026-07-13 產出的映像實際燒錄到 STM32MP215F-DK 板子並驗證開機。硬體線材（USB-C PD、兩條 3.3V USB-TTL 轉接器）仍在採購規劃階段，見 `stm32mp2/00-dev-environment.md`。
- [ ] Week 9+ 的 RoT 核心功能完全尚未動工：M33-TD hold/release A35 reset、Secure Boot（hash + ECDSA 簽章驗證）、金鑰隔離在 M33 側、Rollback protection、基礎 Secure Storage、（選配）Remote Attestation，都還沒有對應程式碼，也還沒建立 `meta-rot` 自訂 layer。
- [ ] `arm-architecture/`、`boot-flow/`、`trustzone/`、`cryptography/` 目前都停留在筆記階段，沒有對應的動手驗證（例如：實際讀寫暫存器、跑一次簽章驗證流程）。

---

## 環境快照（寫於 2026-07-27）

| 項目 | 位置 | 備註 |
|---|---|---|
| RoT-project repo | `/home/wayne/RoT-project`，分支 `Wayne_OuOb` | 追蹤筆記、規劃、練習程式碼；不含 Yocto build 產物 |
| Yocto build tree | `/home/wayne/my-stm/` | 不進 git；內含 `poky`、`meta-openembedded`、`meta-st-openstlinux`、`meta-st-stm32mp`、`build/` |
| 建置自動化腳本 | `/home/wayne/setup-yocto-stm32.sh` | 一鍵 clone + 加 layer + 設定 local.conf；獨立於 RoT-project repo 之外 |
| 磁碟空間 | `/`（1007G，已用 114G，餘 842G） | 足夠再跑數次完整 build |
