---
tags: [yocto, module]
topic: yocto
week: "7-8"
---
# Yocto Module 2：STM32MP2 環境設定

## 官方 Yocto 發行版：OpenSTLinux

ST 提供 **OpenSTLinux**（基於 Yocto 的 STM32MP2 官方 Linux 發行版）：

```
repo: https://github.com/STMicroelectronics/meta-st-stm32mp
version: Kirkstone (LTS) or Scarthgap

includes:
  ├── meta-st-stm32mp/      STM32MP SoC layer
  ├── meta-st-openstlinux/  ST distro layer
  ├── BSP (kernel config, devicetree)
  └── pre-built TF-A / OP-TEE recipes
```

---

## 環境準備

```bash
# 主機需求：Ubuntu 20.04 / 22.04（x86_64）
# 需要安裝的工具
sudo apt update && sudo apt install -y \
  git build-essential python3 python3-pip \
  gawk wget file texinfo chrpath cpio \
  diffstat unzip gcc-multilib libsdl1.2-dev \
  lz4 zstd

# 確認 Python 版本 >= 3.6
python3 --version

# 安裝 repo tool（Google's multi-repo tool）
curl https://storage.googleapis.com/git-repo-downloads/repo > ~/bin/repo
chmod a+x ~/bin/repo
```

---

## 取得 STM32MP2 Yocto

```bash
# 建立工作目錄
mkdir yocto-stm32mp2 && cd yocto-stm32mp2

# 用 repo 下載（ST 的方式）
repo init -u https://github.com/STMicroelectronics/oe-manifest.git \
          -b refs/tags/openstlinux-6.1-yocto-scarthgap-mpu-v24.11.06
repo sync --no-clone-bundle

# 或手動 clone 各個 layer
git clone https://git.openembedded.org/poky -b scarthgap
git clone https://github.com/STMicroelectronics/meta-st-stm32mp -b scarthgap
git clone https://github.com/STMicroelectronics/meta-st-openstlinux -b scarthgap
```

---

## bblayers.conf 設定

```bitbake
# build/conf/bblayers.conf
BBLAYERS ?= " \
  ${TOPDIR}/../poky/meta \
  ${TOPDIR}/../poky/meta-poky \
  ${TOPDIR}/../meta-st-stm32mp \
  ${TOPDIR}/../meta-st-openstlinux \
  ${TOPDIR}/../meta-rot \
  "
```

---

## local.conf 基本設定

```bitbake
# build/conf/local.conf

# 目標板
MACHINE = "stm32mp215f-dk"

# 發行版
DISTRO = "openstlinux-eglfs"

# 並行編譯
BB_NUMBER_THREADS = "8"
PARALLEL_MAKE = "-j8"

# 下載快取（避免重複下載）
DL_DIR = "/path/to/shared/downloads"
SSTATE_DIR = "/path/to/shared/sstate-cache"

# 額外功能
EXTRA_IMAGE_FEATURES += "debug-tweaks ssh-server-openssh"

# 加入 RoT 工具
IMAGE_INSTALL:append = " \
    rot-verify-tool \
    openssl \
"
```

---

## 第一次編譯

```bash
# 進入 Yocto 環境
cd yocto-stm32mp2
source poky/oe-init-build-env build/

# 第一次編譯會很久（下載 + 編譯全部，可能 2-4 小時）
bitbake st-image-weston

# 輸出在
ls tmp/deploy/images/stm32mp215f-dk/
```

---

## 燒錄到 STM32MP215F-DK

```bash
# 使用 STM32CubeProgrammer CLI
STM32_Programmer_CLI -c port=USB1 \
  -d tmp/deploy/images/stm32mp215f-dk/FlashLayout_sdcard_stm32mp215f-dk-basic.tsv \
  -fl /path/to/programmers/FlashStartUpGeneral_Programmer.stm32 \
  -rdu

# tsv 檔描述各個 partition 要燒什麼
# FlashLayout 由 ST 提供，對應 SD Card / eMMC 佈局
```

---

## 常見問題

```bash
# 問題：編譯到一半斷掉，繼續編
bitbake st-image-weston  # 直接再跑，BitBake 會從斷點繼續

# 問題：某個 recipe 版本衝突
bitbake-layers show-recipes | grep u-boot  # 看有哪些版本
PREFERRED_VERSION_u-boot = "2024.04%"      # 指定版本（local.conf 中）

# 問題：空間不夠
# Yocto build 目錄可能高達 50-100 GB
# 設定 TMPDIR 到空間大的磁碟
TMPDIR = "/data/yocto/tmp"  # local.conf

# 問題：網路抓不到 source
# 設定 proxy
http_proxy = "http://proxy.example.com:3128"
https_proxy = "http://proxy.example.com:3128"
```

---

## 下一步

→ [Module 3：自訂 Image](03-custom-image.md)
