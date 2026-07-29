幫我處理以下操作

# STM32MP215F-DK Yocto (scarthgap) Flow

```bash
# 1. 建立 workspace (可自己更改工作資料夾檔名) 並 clone 所有必要 repo
mkdir my-stm && cd my-stm

git clone -b scarthgap git://git.yoctoproject.org/poky
git clone -b scarthgap https://github.com/stmicroelectronics/meta-st-stm32mp
git clone -b scarthgap git://git.openembedded.org/meta-openembedded
git clone -b scarthgap https://github.com/STMicroelectronics/meta-st-openstlinux

# 2. 啟動 build 環境
source poky/oe-init-build-env build

# 3. 加入所有必要 layer
bitbake-layers add-layer ../meta-st-stm32mp
bitbake-layers add-layer ../meta-openembedded/meta-oe
bitbake-layers add-layer ../meta-openembedded/meta-python
bitbake-layers add-layer ../meta-st-openstlinux
bitbake-layers add-layer ../meta-openembedded/meta-networking
bitbake-layers add-layer ../meta-openembedded/meta-webserver
bitbake-layers add-layer ../meta-openembedded/meta-gnome
bitbake-layers add-layer ../meta-openembedded/meta-multimedia

# 4. 檢查 layer 是否都正確加進去了
cat conf/bblayers.conf
# 進階檢查（含 priority、實際被 bitbake parse 到的 layer）：
bitbake-layers show-layers

# 5. 使用編輯器編輯 conf/local.conf，原本可能有預設 machine 和 distro，請改/加入：
    MACHINE = "stm32mp21-disco"
    DISTRO = "openstlinux-weston"
    INIT_MANAGER = "systemd"

# 6. 開始編譯（注意：應該要三到四個小時）
bitbake st-image-weston
```

`cat conf/bblayers.conf` 應該要看到類似這樣的內容（順序不重要，但 7 個 layer 都要在）：

```conf
BBLAYERS ?= " \
  /path/to/my-stm/poky/meta \
  /path/to/my-stm/poky/meta-poky \
  /path/to/my-stm/poky/meta-yocto-bsp \
  /path/to/my-stm/meta-st-stm32mp \
  /path/to/my-stm/meta-openembedded/meta-oe \
  /path/to/my-stm/meta-openembedded/meta-python \
  /path/to/my-stm/meta-st-openstlinux \
  /path/to/my-stm/meta-openembedded/meta-networking \
  /path/to/my-stm/meta-openembedded/meta-webserver \
  /path/to/my-stm/meta-openembedded/meta-gnome \
  /path/to/my-stm/meta-openembedded/meta-multimedia \
  "
```

若少了 `meta-st-openstlinux` 或任何一個 meta-openembedded 子 layer，`bitbake st-image-weston` 在 parse 階段就會直接報錯（layer dependency 找不到，或 recipe 找不到）。