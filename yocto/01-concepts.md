---
tags: [yocto, module]
topic: yocto
week: "7-8"
---
# Yocto Module 1：核心概念

## Yocto 是什麼

**Yocto Project** 是一個建立嵌入式 Linux 發行版的工具框架。你告訴它「我要什麼套件、目標板是什麼、用什麼 kernel」，它幫你從原始碼全部交叉編譯好，輸出一個可以燒進板子的 Linux image。

```
你的工作：寫 recipe（.bb 檔）描述要什麼
Yocto：從 source code 交叉編譯所有東西，打包成 rootfs image

類比：
  Yocto ≈ 嵌入式版的 npm/pip
  Recipe（.bb 檔）≈ package.json
  Layer（meta-xxx）≈ npm package（可以複用、分享）
```

**為什麼要用 Yocto 而不是自己裝 Ubuntu：** 嵌入式設備的 Flash 空間有限（可能只有幾 MB），你需要一個只包含你需要的東西的最小 Linux。Ubuntu 有幾 GB，完全不適合。Yocto 讓你精確控制 image 裡有什麼。

---

## Yocto 的三層結構

```
OpenEmbedded Core (OE-Core)
  └── base recipes, tools, classes

BitBake
  └── task execution engine; resolves recipe deps

Layers
  └── meta-xxx/: grouped recipe collections
       ├── meta/            OE-Core layer (base tools)
       ├── meta-poky/       Poky reference distro
       ├── meta-st-stm32mp/ ST official STM32MP layer
       └── meta-rot/        your own layer (RoT)
```

---

## BitBake 的基本概念

**BitBake 是什麼：** Yocto 的建構引擎，負責讀取所有 recipe 並決定執行順序、管理依賴關係。類似 `make`，但專為嵌入式 Linux 設計。

### Recipe（.bb 檔）

描述**如何建立一個 package**（從哪裡下載原始碼、怎麼編譯、裝到哪裡）：

```bitbake
# 例：hello-world_1.0.bb

DESCRIPTION = "A simple hello world program"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://LICENSE;md5=..."

SRC_URI = "file://hello.c \
           file://Makefile"

do_compile() {
    oe_runmake
}

do_install() {
    install -d ${D}${bindir}
    install -m 0755 hello ${D}${bindir}/hello
}
```

### .bbappend（recipe 擴充）

不修改原本 recipe，只在後面附加：

```bitbake
# u-boot_%.bbappend（%代表任意版本）
FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

# 加入自訂 patch
SRC_URI += "file://0001-add-rot-support.patch"

# 修改 U-Boot 的編譯設定
do_configure:append() {
    echo "CONFIG_ROT_VERIFY=y" >> .config
}
```

### Class（.bbclass）

可被多個 recipe 繼承的通用邏輯：

```bitbake
# recipe 中繼承 cmake class
inherit cmake

# 繼承 systemd class（服務自動啟動）
inherit systemd
SYSTEMD_SERVICE:${PN} = "rot-monitor.service"
```

---

## 重要變數

```bitbake
# 交叉編譯工具鏈前綴
${CROSS_COMPILE}  →  aarch64-poky-linux-

# 目標板的 sysroot（包含 headers 和 libraries）
${STAGING_DIR_TARGET}

# 安裝目錄（staging area，打包前）
${D}              →  ${WORKDIR}/image/

# 目標板的標準目錄
${bindir}         →  /usr/bin
${libdir}         →  /usr/lib
${sysconfdir}     →  /etc
${systemd_unitdir} → /lib/systemd

# Package 名稱
${PN}             →  recipe 的名稱（package name）
${PV}             →  版本號（package version）

# 平台
MACHINE           →  "stm32mp215f-dk"
DISTRO            →  "poky"（或 "openstlinux-weston"）
```

---

## 常用 BitBake 指令

```bash
# 建立特定 package
bitbake u-boot

# 建立整個 image
bitbake core-image-minimal
bitbake st-image-weston            # STM32MP 官方 image

# 只執行某個 task
bitbake -c compile u-boot          # 只編譯
bitbake -c deploy u-boot           # 只部署

# 強制重新執行
bitbake -f -c compile u-boot

# 查看 recipe 資訊
bitbake -e u-boot | grep ^SRC_URI  # 查某個變數的值
bitbake -s | grep kernel           # 找有哪些 kernel recipe

# 清除 package 的 build 結果
bitbake -c cleansstate u-boot

# 查看 task 依賴圖
bitbake -g core-image-minimal
dot -Tpng task-depends.dot > dep.png
```

---

## 理解 Build 輸出

```
build/
├── conf/
│   ├── local.conf         ← 你的設定（MACHINE、DISTRO 等）
│   └── bblayers.conf      ← 啟用哪些 layer
├── tmp/
│   ├── deploy/
│   │   └── images/stm32mp215f-dk/
│   │         ├── Image                    ← Linux kernel
│   │         ├── stm32mp215f-dk.dtb       ← Device Tree
│   │         ├── core-image-minimal.ext4  ← rootfs
│   │         ├── u-boot.stm32             ← U-Boot
│   │         └── fip.bin                  ← TF-A FIP
│   └── work/
│       └── cortexa35-poky-linux/
│           └── u-boot/
│               └── 2024.04-r0/
│                   ├── git/              ← source
│                   ├── build/            ← 編譯產物
│                   └── image/            ← 安裝後的內容
```

---

## 下一步

→ [Module 2：STM32MP2 環境設定](02-stm32mp2-setup.md)
