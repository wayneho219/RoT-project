---
tags: [yocto, module]
topic: yocto
week: "7-8"
---
# Yocto Module 3：自訂 Image 與 RoT 整合

## 建立自訂 Layer

```bash
# 使用 bitbake-layers 工具建立新 layer
cd yocto-stm32mp2
bitbake-layers create-layer ../meta-rot

# 目錄結構
meta-rot/
├── conf/
│   └── layer.conf          ← layer 設定
├── recipes-core/
│   └── images/
│       └── rot-image.bb    ← 自訂 image recipe
├── recipes-rot/
│   ├── rot-firmware/
│   │   └── rot-m33_1.0.bb  ← M33 firmware recipe
│   └── rot-tools/
│       └── rot-verify_1.0.bb  ← 驗證工具 recipe
└── README
```

### layer.conf

```bitbake
# meta-rot/conf/layer.conf
BBPATH .= ":${LAYERDIR}"
BBFILES += "${LAYERDIR}/recipes-*/*/*.bb \
             ${LAYERDIR}/recipes-*/*/*.bbappend"
BBFILE_COLLECTIONS += "meta-rot"
BBFILE_PATTERN_meta-rot = "^${LAYERDIR}/"
BBFILE_PRIORITY_meta-rot = "10"
LAYERVERSION_meta-rot = "1"
LAYERSERIES_COMPAT_meta-rot = "scarthgap"
```

---

## 自訂 Image Recipe

```bitbake
# meta-rot/recipes-core/images/rot-image.bb

# 繼承 STM32MP 的最小 image
require recipes-core/images/core-image-minimal.bb

DESCRIPTION = "Minimal image with RoT tools"

IMAGE_INSTALL:append = " \
    packagegroup-core-ssh-openssh \
    rot-verify-tool \
    openssl \
    libmbedtls \
"

# 設定 root password（開發用）
EXTRA_IMAGE_FEATURES += "debug-tweaks"

# 產生可燒錄的 wic 格式
IMAGE_FSTYPES = "ext4 wic wic.bmap"
WKS_FILE = "stm32mp2-sdcard.wks"
```

---

## 交叉編譯 M33 Firmware Recipe

```bitbake
# meta-rot/recipes-rot/rot-firmware/rot-m33_1.0.bb

DESCRIPTION = "M33 Trusted Domain RoT Firmware"
LICENSE = "CLOSED"  # 或 MIT

# 原始碼在本機
SRC_URI = "file://rot-m33-src.tar.gz"
S = "${WORKDIR}/rot-m33-src"

# 使用 arm-none-eabi toolchain
TOOLCHAIN = "arm-none-eabi"

# 依賴：mbedTLS（for crypto）
DEPENDS = "mbedtls-native"

do_compile() {
    cd ${S}
    make CROSS_COMPILE=arm-none-eabi- all
}

do_install() {
    install -d ${D}/boot
    install -m 0644 ${S}/build/rot-m33.bin ${D}/boot/
    install -m 0644 ${S}/build/rot-m33.elf ${D}/boot/
}

# 打包到 /boot 目錄
FILES:${PN} = "/boot/rot-m33.bin"
FILES:${PN}-dbg = "/boot/rot-m33.elf"
```

---

## Kernel Config 客製化（.bbappend）

```bitbake
# meta-rot/recipes-kernel/linux/linux-stm32mp_%.bbappend

FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

# 加入自訂 kernel config 片段
SRC_URI += " \
    file://rot-tee.cfg \
"

# rot-tee.cfg 內容（啟用 OP-TEE 相關設定）
# CONFIG_TEE=y
# CONFIG_OPTEE=y
# CONFIG_OPTEE_SHM_NUM_PRIV_PAGES=1
```

---

## U-Boot 環境設定（.bbappend）

```bitbake
# meta-rot/recipes-bsp/u-boot/u-boot-stm32mp_%.bbappend

FILESEXTRAPATHS:prepend := "${THISDIR}/files:"

# 加入自訂 U-Boot env
SRC_URI += "file://rot-uboot-env.h"

# U-Boot 啟動腳本：只有 M33 驗證通過後才啟動 Linux
do_configure:append() {
    cat >> include/configs/stm32mp2.h << 'EOF'
#define CONFIG_BOOTCOMMAND \
    "run rot_verify_boot"
#define CONFIG_EXTRA_ENV_SETTINGS \
    "rot_verify_boot="\
    "if test ${rot_status} = ok; then "\
    "  booti ${kernel_addr_r} - ${fdt_addr_r}; "\
    "else "\
    "  echo RoT verification failed!; "\
    "fi\0"
EOF
}
```

---

## Device Tree 修改：M33 共享記憶體

```dts
/* meta-rot/recipes-kernel/linux/files/0001-rot-shm.dts.patch */
/* 在 DTS 中保留 M33 的共享記憶體區域 */

&reserved_memory {
    m33_shared: m33-shared@20020000 {
        compatible = "shared-dma-pool";
        reg = <0x0 0x20020000 0x0 0x20000>;  /* 128 KB NS SRAM */
        no-map;
    };
};

/* M33 共享記憶體的 device node */
rot_m33: rot-m33 {
    compatible = "st,stm32mp2-rot-m33";
    memory-region = <&m33_shared>;
    status = "okay";
};
```

---

## 編譯並測試

```bash
# 加入自訂 layer
bitbake-layers add-layer ../meta-rot

# 編譯自訂 image
bitbake rot-image

# 查看 image 內容
ls tmp/deploy/images/stm32mp215f-dk/
# rot-image-stm32mp215f-dk.ext4
# rot-image-stm32mp215f-dk.wic

# 燒錄到 SD card
bmap-tools copy \
    tmp/deploy/images/stm32mp215f-dk/rot-image.wic \
    /dev/sdX

# 開機後確認 RoT 工具存在
ssh root@192.168.0.xxx
rot-verify-tool --help
```

---

## 自動化測試（devshell）

```bash
# 進入 package 的 build 環境（可手動測試編譯）
bitbake -c devshell rot-m33

# 進入 image 的 build 環境
bitbake -c devshell rot-image

# 查看某個 package 的 install 內容
bitbake -c package rot-verify-tool
cat tmp/work/.../rot-verify-tool/1.0-r0/packages-split/
```
