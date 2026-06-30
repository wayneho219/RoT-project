# RPi5 Yocto Project

## Setup

```bash
mkdir my-rpi5 && cd my-rpi5

git clone -b scarthgap git://git.yoctoproject.org/poky
git clone -b scarthgap https://github.com/agherzan/meta-raspberrypi
git clone -b scarthgap git://git.openembedded.org/meta-openembedded

source poky/oe-init-build-env build

bitbake-layers add-layer ../meta-raspberrypi
```

## Expected bblayers.conf

``` 
$ cat conf/bblayers.conf

# POKY_BBLAYERS_CONF_VERSION is increased each time build/conf/bblayers.conf
# changes incompatibly
POKY_BBLAYERS_CONF_VERSION = "2"

BBPATH = "${TOPDIR}"
BBFILES ?= ""

BBLAYERS ?= " \\
  /openbmc_bring_up_env/my-rpi5/poky/meta \\
  /openbmc_bring_up_env/my-rpi5/poky/meta-poky \\
  /openbmc_bring_up_env/my-rpi5/poky/meta-yocto-bsp \\
  /openbmc_bring_up_env/my-rpi5/meta-raspberrypi \\
  "
```

## local.conf (append at the bottom)

```
MACHINE = "raspberrypi5"
RPI_USE_U_BOOT = "1"
INIT_MANAGER = "systemd"
LICENSE_FLAGS_ACCEPTED = "synaptics-killswitch"
```

## Build (first time will take a long time)

```bash
bitbake core-image-base
```

---

## VM - OrbStack

```bash
brew install orbstack
orb create -a arm64 ubuntu:jammy <VM-Name>
orb -m <VM-Name>  # inside VM

sudo apt update && sudo apt install -y \\
    gawk wget git diffstat unzip texinfo gcc build-essential \\
    chrpath socat cpio python3 python3-pip python3-pexpect \\
    xz-utils debianutils iputils-ping python3-git python3-jinja2 \\
    libegl1-mesa libsdl1.2-dev zstd liblz4-tool file locales libacl1

sudo locale-gen en_US.UTF-8

orb -m <VM-Name> ssh-config >> ~/.ssh/config
```