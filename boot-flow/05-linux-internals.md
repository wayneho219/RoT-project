---
tags: [boot-flow, module]
topic: boot-flow
week: "5-6"
---
# Boot Flow Module 5：Linux Internals（面試必備）

## Process vs Thread

```c
// Process：獨立的虛擬位址空間
fork()    // 建立子 process（複製父 process 的頁表）
exec()    // 用新程式替換目前 process

// Thread：同一個 process 內，共享記憶體
pthread_create(&tid, NULL, thread_func, NULL);
// Thread 之間共享 heap、global 變數，但各有自己的 stack

// Kernel 角度：Linux 用同一個 task_struct 表示 process 和 thread
// 差別只在是否共享頁表（CLONE_VM flag）
```

---

## 記憶體佈局（User Space）

每個 process 的虛擬位址空間從低位址到高位址分成幾個固定區段：

高位址 → 低位址（由上而下）；Stack 向下成長，Heap 向上成長。

```
高位址 (0xFFFF_0000_0000_0000+)
  ┌──────────────────────┐
  │ Kernel Space         │  ← 不可見；user 存取 → segfault
  ├──────────────────────┤
  │ Stack (grows down)   │  區域變數、返回位址
  │   ▼                  │
  │   ▼                  │
  ├──────────────────────┤
  │ Memory-mapped files  │  mmap()、動態函式庫（.so）
  ├──────────────────────┤
  │   ▲                  │
  │   ▲                  │
  │ Heap (grows up)      │  malloc() / free()
  ├──────────────────────┤
  │ BSS                  │  未初始化全域/靜態變數，OS 清零
  │                      │  e.g. int global_arr[1000];
  ├──────────────────────┤
  │ Data                 │  已初始化全域變數，e.g. int x = 42;
  ├──────────────────────┤
  │ Text (read-only)     │  程式碼（機器碼），PC 在這裡執行
  └──────────────────────┘
低位址 (0x0000_0000...)
```

---

## Syscall 機制

User space 呼叫 kernel 服務的唯一方式：

```c
// C 語言層面
#include <unistd.h>
ssize_t n = read(fd, buf, len);

// 背後的機制（AArch64）：
// 1. User space 把參數放進 X0–X5
// 2. syscall 號碼放進 X8
// 3. SVC #0 指令（Supervisor Call）→ 觸發 EL0 → EL1 切換
// 4. Kernel 的 syscall handler 執行
// 5. 結果放在 X0，返回 EL0

// AArch64 syscall 號碼
// SYS_read   = 63
// SYS_write  = 64
// SYS_open   = 56（不推薦，用 openat）
// SYS_ioctl  = 29（和 kernel driver 通訊）
```

---

## File Descriptor 與 /dev

Linux 的「一切皆文件」哲學：

```c
// 開啟 /dev/rot（我們的 kernel driver）
int fd = open("/dev/rot", O_RDONLY);

// ioctl：發送 driver 特定命令
struct rot_status s;
ioctl(fd, ROT_IOCTL_GET_STATUS, &s);

// poll：等待 driver 事件（非阻塞）
struct pollfd pfd = { .fd = fd, .events = POLLIN };
int ret = poll(&pfd, 1, 5000);  // 等 5 秒
if (pfd.revents & POLLIN) {
    // 有新的 M33 事件
}

close(fd);
```

---

## Signal（訊號）

```c
// Kernel 通知 process 的機制（異步）
// SIGSEGV：存取非法記憶體（segfault）
// SIGKILL：強制終止（無法忽略）
// SIGTERM：請求終止（可以攔截）
// SIGUSR1/2：使用者自訂

// 安裝 handler
void handle_sig(int sig) {
    if (sig == SIGUSR1) {
        printf("Got SIGUSR1: M33 alert!\n");
    }
}
signal(SIGUSR1, handle_sig);

// Kernel driver 可以向 process 發 signal（通知有事件）
// Driver 端：
kill_pid(pid, SIGUSR1, 1);  // kernel 呼叫
```

---

## Shared Memory（IPC）

User space 的 shared memory（和 M33 的共享記憶體概念不同）：

```c
// POSIX shared memory（兩個 process 共享）
// 不是 M33/A35 共享，而是兩個 Linux process 之間

// Process 1（建立）
int fd = shm_open("/rot_data", O_CREAT | O_RDWR, 0666);
ftruncate(fd, 4096);
void *ptr = mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);

// Process 2（存取）
int fd = shm_open("/rot_data", O_RDONLY, 0666);
void *ptr = mmap(NULL, 4096, PROT_READ, MAP_SHARED, fd, 0);

// 直接存取 M33 的 NS SRAM（透過 /dev/mem 或 kernel driver 的 mmap）
// 通常透過 driver 的 mmap 操作（比直接 /dev/mem 安全）
int fd = open("/dev/rot", O_RDONLY);
void *ipc = mmap(NULL, 4096, PROT_READ, MAP_SHARED, fd, 0);
```

---

## 中斷處理（Driver 觀點）

```c
// M33 通知 A35：透過硬體中斷（或軟體中斷 EXTI）

// Driver 中申請中斷
static irqreturn_t rot_irq_handler(int irq, void *dev_id) {
    struct rot_ipc_priv *priv = dev_id;
    
    // 在 IRQ context 中：不能 sleep，不能 block，要快
    // 讀一個 flag 就好，剩下的在 workqueue 裡做
    u32 status = readl(priv->base + IPC_STATUS_OFFSET);
    
    // 喚醒等待的 task
    wake_up_interruptible(&priv->wait_queue);
    
    return IRQ_HANDLED;
}

// probe 中申請
int irq = platform_get_irq(pdev, 0);
devm_request_irq(&pdev->dev, irq, rot_irq_handler, 
                 IRQF_TRIGGER_RISING, "rot_ipc", priv);

// poll / blocking read 等待中斷
DECLARE_WAIT_QUEUE_HEAD(priv->wait_queue);

static ssize_t rot_read(struct file *file, ...) {
    wait_event_interruptible(priv->wait_queue, 
                             has_new_event(priv));
    // 被喚醒後讀取資料
    copy_to_user(buf, &event, sizeof(event));
}
```

---

## /proc 和 /sys 的差異

```
/proc/               → Process 資訊、kernel 執行時狀態
  /proc/1234/maps    → PID 1234 的記憶體映射
  /proc/cpuinfo      → CPU 資訊
  /proc/interrupts   → 中斷統計

/sys/                → Kernel 的結構化資訊（sysfs）
  /sys/devices/      → 裝置樹
  /sys/class/        → 裝置分類
  /sys/bus/          → 匯流排
  /sys/firmware/     → 韌體相關（如 ACPI）

M33 Driver 的 sysfs：
  /sys/devices/platform/rot_ipc.0/rot_status
  /sys/devices/platform/rot_ipc.0/fw_version
```

---

## 面試常考：Linux 相關

| 問題 | 回答要點 |
|------|---------|
| fork 和 exec 的差別？ | fork 複製 process（copy-on-write）；exec 替換目前 process 的程式碼 |
| 什麼是 zombie process？ | 已結束但父 process 還沒 wait()，task_struct 還在 |
| mmap 和 read 的差別？ | mmap 把檔案映射到虛擬位址（直接存取）；read 要複製到 user buffer |
| copy_on_write 是什麼？ | fork 後父子共用頁表，寫入時才複製頁面（節省記憶體）|
| kernel space 和 user space 為什麼要隔離？ | user space 的 bug 不能損壞 kernel；EL0 沒有存取 kernel 記憶體的權限 |
| `copy_to_user` / `copy_from_user` 為什麼需要？ | user 指標可能是 NULL 或非法位址；直接 dereference 會在 kernel 造成 panic |
| Kernel module 和 user space daemon 的選擇？ | 需要存取硬體（mmio、irq）→ kernel；其他能在 user space 做就做 |
| Interrupt vs Polling？ | 中斷：低延遲、省 CPU；Polling：確定性強，適合高頻事件 |
