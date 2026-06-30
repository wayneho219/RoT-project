---
tags: [boot-flow, module]
topic: boot-flow
week: "5-6"
---
# Boot Flow Module 3：Linux Boot 流程

## 整體流程

```
U-Boot booti
  │
  ▼
Linux kernel Entry（arch/arm64/kernel/head.S）
  │  ├── 確認 CPU 為 ARM64
  │  ├── 清除 BSS
  │  └── 呼叫 start_kernel()
  ▼
start_kernel()
  │  ├── 初始化架構（setup_arch）
  │  │     └── Device Tree 解析
  │  ├── 初始化 Memory（mm_init）
  │  ├── 初始化排程器
  │  ├── 初始化中斷子系統
  │  └── rest_init()
        │
        └── kernel_init（PID 1 前身）
              ├── 掛載 initramfs（initrd）
              └── 執行 /init 或 /sbin/init
                    └── systemd（正式 PID 1）
```

---

## Device Tree（設備樹）

**Device Tree 是什麼：** 用一個結構化文字檔描述硬體有什麼（哪些 UART、位址在哪、用哪個中斷），讓 Linux kernel 不需要為每個 SoC 各自修改。

```
沒有 Device Tree（舊做法）：
  kernel 程式碼裡 hard-code：UART1 在 0x40C00000，中斷號 27...
  每個 SoC 要改 kernel 原始碼 → 維護成本極高

有 Device Tree：
  硬體描述寫在 .dts 檔（文字格式）
  編譯成 .dtb（binary，Device Tree Blob）
  U-Boot 把 dtb 位址傳給 kernel
  kernel 啟動時讀 dtb，自動探測有哪些硬體
```

Linux 用 Device Tree 描述硬體拓撲，而非把硬體資訊 hard-code 進 kernel：

### DTB（Device Tree Blob）結構

```
/ {
    compatible = "st,stm32mp215f-dk";
    model = "STMicroelectronics STM32MP215F-DK";

    cpus {
        cpu@0 {
            compatible = "arm,cortex-a35";
            device_type = "cpu";
            reg = <0>;
        };
    };

    memory@c0000000 {
        device_type = "memory";
        reg = <0 0xc0000000 0 0x40000000>;  // 1 GB DDR
    };

    soc {
        usart2: serial@40c00000 {
            compatible = "st,stm32h7-uart";
            reg = <0x40c00000 0x400>;
            interrupts = <GIC_SPI 27 IRQ_TYPE_LEVEL_HIGH>;
            clocks = <&rcc USART2>;
            status = "okay";
        };
    };
};
```

### U-Boot 如何傳遞 DTB 給 kernel

```bash
# U-Boot 把 DTB 位址放在 X1 暫存器
# kernel entry 呼叫慣例：
#   X0 = 0（保留）
#   X1 = DTB 的實體位址

booti ${kernel_addr_r} - ${fdt_addr_r}
# → 跳到 kernel，X1 = fdt_addr_r
```

---

## initramfs（初始 RAM 檔案系統）

**initramfs 是什麼：** 一個壓縮的迷你 Linux 根目錄（root filesystem），存在記憶體裡。Linux 啟動時先掛載它，用來載入驅動程式、解密磁碟等，然後才切換到真正的 rootfs（在 SD 卡或 eMMC）。

```
為什麼需要 initramfs：
  Linux kernel 本身不包含所有驅動程式
  要讀 eMMC 先要有 eMMC 驅動，但驅動在 eMMC 上... 雞生蛋問題
  解法：initramfs 放在記憶體（U-Boot 載入），包含足夠的驅動，
        掛載真正的 rootfs 後再把控制權交出去
```

Linux 啟動時先掛載 initramfs（在記憶體中），再切換到真正的 rootfs：

```
initramfs 包含：
  /init           ← 第一個執行的程式（shell script 或程式）
  /bin/busybox    ← 基本工具
  /lib/modules/   ← kernel module（驅動程式）
  /etc/           ← 設定檔
```

### Yocto 產生的 initramfs

```bash
# Yocto 自動打包 initramfs 成 cpio.gz
ls tmp/deploy/images/stm32mp2/
  core-image-minimal-stm32mp2.ext4    # rootfs
  core-image-minimal-stm32mp2-initrd  # initramfs
  Image                               # kernel
  stm32mp215f-dk.dtb                  # DTB
```

---

## Linux Kernel 啟動的重要子系統初始化順序

```c
// 對嵌入式工程師最相關的部分
start_kernel()
  setup_arch()          // 解析 DTB，建立記憶體映射
  trap_init()           // 設定 exception handler
  mm_init()             // 初始化 buddy allocator（頁面分配）
  sched_init()          // 排程器初始化
  irq_init()            // 中斷控制器（GIC）初始化
  time_init()           // 計時器、tick
  rest_init()
    kernel_thread(kernel_init) // 啟動 PID 1
    kernel_thread(kthreadd)    // kernel thread daemon
    cpu_startup_entry()        // 進入 idle loop
```

---

## Kernel Module vs 內建驅動

```c
// 內建：編譯進 kernel image，啟動時自動初始化
// CONFIG_STM32_GPIO=y  →  存在 Image 裡

// Module：可動態載入
// CONFIG_STM32_GPIO=m  →  stm32_gpio.ko
insmod stm32_gpio.ko
rmmod stm32_gpio

// 模組初始化（每個 .ko 都有）
static int __init stm32_gpio_init(void)  { /* ... */ return 0; }
static void __exit stm32_gpio_exit(void) { /* ... */ }
module_init(stm32_gpio_init);
module_exit(stm32_gpio_exit);
MODULE_LICENSE("GPL");
```

---

## Linux 與 TrustZone 的互動

```
Linux（EL1-NS）
  │
  │  SMC #0 / HVC #0
  ├────────────────────▶ TF-A BL31（EL3）
  │                         PSCI: CPU on/off, suspend
  │                         SiP SVC: ST 特定服務（電源管理、OTP 讀取）
  │
  │  ioctl /dev/tee0
  └────────────────────▶ OP-TEE driver（kernel module）
                             └── SMC → OP-TEE（EL1-S）
                                   └── Trusted Application
```

---

## 對 RoT 專案的意義

Linux 只是 A35 的最終目標，從 RoT 角度看：

1. Linux **不參與** Secure Boot 驗證（那是 M33 的工作）
2. Linux 啟動前，M33 已確保 firmware 是完整且已授權的
3. 若需要在 Linux 上提供安全服務（如 key-based 簽章 API），用 OP-TEE 的 TA
4. Device Tree 中有 `trustzone-memory` 節點，讓 kernel 知道哪些記憶體不可用

---

## 下一步

→ [Module 4：Secure Boot 機制](04-secure-boot.md)
