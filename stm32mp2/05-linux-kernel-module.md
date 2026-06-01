---
tags: [stm32mp2, module]
topic: stm32mp2
week: "9+"
---
# STM32MP2 Module 5：Linux Kernel Module（M33 IPC Driver）

## 目標

在 A35 Linux 上寫一個 kernel driver，讀取 M33 寫入共享記憶體的驗證結果，  
並透過 `/dev/rot` 或 sysfs 暴露給 user space。

---

## Linux Kernel Module 基本結構

```c
// rot_ipc.c

#include <linux/module.h>    // module_init, module_exit
#include <linux/kernel.h>    // printk
#include <linux/platform_device.h>
#include <linux/of.h>        // Device Tree 解析
#include <linux/io.h>        // ioremap, readl, writel
#include <linux/miscdevice.h>
#include <linux/uaccess.h>   // copy_to_user

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("M33 RoT IPC Driver for STM32MP215F");

// 模組載入
static int __init rot_ipc_init(void) {
    printk(KERN_INFO "rot_ipc: driver loaded\n");
    return 0;
}

// 模組卸載
static void __exit rot_ipc_exit(void) {
    printk(KERN_INFO "rot_ipc: driver unloaded\n");
}

module_init(rot_ipc_init);
module_exit(rot_ipc_exit);
```

---

## 共享記憶體映射到 Kernel

M33 和 A35 透過 NS SRAM 通訊，Linux 需要用 `ioremap` 映射這段實體位址：

```c
// 共享記憶體的實體位址（和 M33 側定義一致）
#define IPC_PHYS_BASE  0x20020000UL
#define IPC_SIZE       0x1000        // 4 KB

// IPC 結構（和 M33 側定義一致）
struct rot_ipc_header {
    u32 magic;          // 0x524F5449 = "ROTI"
    u32 version;
    u32 m33_status;     // 驗證結果
    u32 fw_version;     // 已驗證的 firmware 版本
    u8  reserved[48];
} __packed;

#define M33_STATUS_VERIFYING  0x00000001
#define M33_STATUS_OK         0x00000002
#define M33_STATUS_FAIL       0x000000FF

struct rot_ipc_priv {
    void __iomem        *base;     // ioremap 後的虛擬位址
    struct miscdevice    miscdev;
};

// 初始化時映射
static int rot_ipc_probe(struct platform_device *pdev) {
    struct rot_ipc_priv *priv;
    
    priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
    if (!priv) return -ENOMEM;

    // 從 Device Tree 取得實體位址
    struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (!res) return -ENODEV;

    // 映射到 kernel 虛擬位址
    priv->base = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(priv->base)) return PTR_ERR(priv->base);

    platform_set_drvdata(pdev, priv);
    return 0;
}
```

---

## 讀取 M33 狀態

```c
// 讀取 IPC 資料（注意要用 readl，不能直接 dereference 指標）
static int rot_get_status(struct rot_ipc_priv *priv, u32 *status, u32 *fw_ver) {
    void __iomem *base = priv->base;

    // 驗證 magic（確認 M33 有初始化 IPC）
    u32 magic = readl(base + offsetof(struct rot_ipc_header, magic));
    if (magic != 0x524F5449) return -ENODEV;

    // 讀取狀態
    *status = readl(base + offsetof(struct rot_ipc_header, m33_status));
    *fw_ver = readl(base + offsetof(struct rot_ipc_header, fw_version));

    // Memory barrier：確保讀到最新值
    rmb();

    return 0;
}
```

---

## Sysfs 介面（推薦方式）

比 `/dev` 更輕量，適合狀態查詢：

```c
// 建立 /sys/devices/platform/rot_ipc/rot_status
static ssize_t rot_status_show(struct device *dev,
                                struct device_attribute *attr,
                                char *buf) {
    struct rot_ipc_priv *priv = dev_get_drvdata(dev);
    u32 status, fw_ver;

    int ret = rot_get_status(priv, &status, &fw_ver);
    if (ret) return ret;

    switch (status) {
    case M33_STATUS_OK:
        return sysfs_emit(buf, "ok fw_version=%u\n", fw_ver);
    case M33_STATUS_FAIL:
        return sysfs_emit(buf, "fail\n");
    case M33_STATUS_VERIFYING:
        return sysfs_emit(buf, "verifying\n");
    default:
        return sysfs_emit(buf, "unknown 0x%08x\n", status);
    }
}

static DEVICE_ATTR_RO(rot_status);

// 建立 /sys/devices/platform/rot_ipc/fw_version
static ssize_t fw_version_show(struct device *dev,
                                struct device_attribute *attr,
                                char *buf) {
    struct rot_ipc_priv *priv = dev_get_drvdata(dev);
    u32 status, fw_ver;
    rot_get_status(priv, &status, &fw_ver);
    return sysfs_emit(buf, "%u\n", fw_ver);
}

static DEVICE_ATTR_RO(fw_version);

static struct attribute *rot_attrs[] = {
    &dev_attr_rot_status.attr,
    &dev_attr_fw_version.attr,
    NULL,
};
ATTRIBUTE_GROUPS(rot);
```

---

## /dev 介面（ioctl 方式）

如果需要更複雜的操作：

```c
// ioctl 命令定義（rot_ipc.h，user space 也 include）
#define ROT_MAGIC 'R'
#define ROT_IOCTL_GET_STATUS   _IOR(ROT_MAGIC, 1, struct rot_status)
#define ROT_IOCTL_GET_FW_VER   _IOR(ROT_MAGIC, 2, uint32_t)

struct rot_status {
    uint32_t state;
    uint32_t fw_version;
    uint32_t error_code;
};

// file_operations
static long rot_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    struct rot_ipc_priv *priv = file->private_data;

    switch (cmd) {
    case ROT_IOCTL_GET_STATUS: {
        struct rot_status s = {0};
        rot_get_status(priv, &s.state, &s.fw_version);
        if (copy_to_user((void __user *)arg, &s, sizeof(s)))
            return -EFAULT;
        return 0;
    }
    default:
        return -ENOTTY;
    }
}

static const struct file_operations rot_fops = {
    .owner          = THIS_MODULE,
    .unlocked_ioctl = rot_ioctl,
};
```

---

## Device Tree Binding

```dts
// stm32mp215f-dk.dts（在 Yocto 的 DTS patch 或 meta-rot 中加入）

reserved_memory {
    m33_ipc: m33-ipc@20020000 {
        compatible = "shared-dma-pool";
        reg = <0x0 0x20020000 0x0 0x1000>;  // 4 KB
        no-map;
    };
};

rot_ipc: rot-ipc@20020000 {
    compatible = "st,stm32mp2-rot-ipc";
    reg = <0x0 0x20020000 0x0 0x1000>;
    memory-region = <&m33_ipc>;
    status = "okay";
};
```

```c
// 對應的 of_device_id table
static const struct of_device_id rot_ipc_of_match[] = {
    { .compatible = "st,stm32mp2-rot-ipc" },
    {}
};
MODULE_DEVICE_TABLE(of, rot_ipc_of_match);

static struct platform_driver rot_ipc_driver = {
    .probe  = rot_ipc_probe,
    .remove = rot_ipc_remove,
    .driver = {
        .name           = "rot_ipc",
        .of_match_table = rot_ipc_of_match,
        .dev_groups     = rot_groups,   // sysfs attrs
    },
};
module_platform_driver(rot_ipc_driver);
```

---

## User Space 使用範例

```c
// rot-status 工具（user space）

// 方式 1：sysfs（簡單）
#include <stdio.h>
int main(void) {
    FILE *f = fopen("/sys/devices/platform/rot_ipc/rot_status", "r");
    if (!f) { perror("open"); return 1; }
    char buf[64];
    fgets(buf, sizeof(buf), f);
    printf("RoT status: %s", buf);
    fclose(f);
    return 0;
}

// 方式 2：ioctl
#include <fcntl.h>
#include <sys/ioctl.h>
#include "rot_ipc.h"

int main(void) {
    int fd = open("/dev/rot", O_RDONLY);
    if (fd < 0) { perror("open"); return 1; }
    
    struct rot_status s;
    ioctl(fd, ROT_IOCTL_GET_STATUS, &s);
    printf("state=0x%08X fw_version=%u\n", s.state, s.fw_version);
    close(fd);
    return 0;
}
```

---

## Yocto Recipe（把 driver 加入 image）

```bitbake
# meta-rot/recipes-rot/rot-ipc-driver/rot-ipc-driver_1.0.bb

SUMMARY = "M33 RoT IPC Kernel Driver"
LICENSE = "GPL-2.0-only"
LIC_FILES_CHKSUM = "file://COPYING;md5=..."

inherit module        # 繼承 kernel module build class

SRC_URI = "file://rot_ipc.c \
           file://rot_ipc.h \
           file://Makefile"

KERNEL_MODULE_AUTOLOAD += "rot_ipc"  # 開機自動載入
```

```makefile
# Makefile（driver 目錄中）
obj-m += rot_ipc.o

all:
	make -C $(KERNEL_SRC) M=$(PWD) modules

clean:
	make -C $(KERNEL_SRC) M=$(PWD) clean
```

---

## 面試常考：Kernel 基本問題

| 問題 | 要點 |
|------|------|
| `ioremap` 和 `*ptr` 有什麼差？ | ioremap 通知 kernel 這段是 IO，禁止 cache；直接指標存取不保證 memory barrier |
| `readl` / `writel` 為什麼要用？ | 確保順序和 volatile 語意；不同架構可能需要 barrier |
| `devm_*` 函式族的意義？ | device-managed，device 移除時自動釋放，不用手動 free |
| 為什麼要 `rmb()` / `wmb()`？ | ARM 弱記憶體序；確保 CPU 看到 M33 寫入的最新值 |
| module_platform_driver 做了什麼？ | 展開成 module_init + module_exit + driver register |
| sysfs vs /dev 選哪個？ | 狀態查詢用 sysfs；需要 ioctl 或 poll 用 /dev |
