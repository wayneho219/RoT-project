---
tags: [stm32mp2, module]
topic: stm32mp2
week: "9+"
---
# STM32MP2 Module 6：RTOS on M33（FreeRTOS / Zephyr）

## 為什麼 M33 要跑 RTOS

裸機 vs RTOS 比較：
```
Bare-metal (this project early stage):
  main() -> verify -> while(1) { WFI; }
  Suitable for: single function, no concurrency needed

RTOS (mature product):
  Multiple tasks running concurrently:
    Task 1: monitor A35 heartbeat continuously
    Task 2: handle A35 Secure API calls (via IPC)
    Task 3: periodic integrity check (re-verify firmware)
  Suitable for: concurrency, timing requirements
```

---

## FreeRTOS 核心概念

**RTOS（Real-Time Operating System）是什麼：** 一個極小的作業系統核心，讓嵌入式設備可以「同時」執行多個任務（多工）。「即時」指的是任務的執行時間有保證（高優先級任務一定在指定時間內被執行）。FreeRTOS 整個 kernel 只有幾 KB，適合 Cortex-M33 這種 MCU。

```
Without RTOS (bare-metal):
  main() executes tasks sequentially
  Task A runs too long -> Task B must wait

With RTOS (multi-task):
  Scheduler switches tasks (every few milliseconds)
  Task A and Task B appear to run "simultaneously"
  Higher-priority task can preempt lower-priority task
```

### Task（任務）

```c
// 建立 task
void vRoT_Monitor_Task(void *pvParameters) {
    for (;;) {
        // 每 1 秒檢查 A35 heartbeat
        uint32_t heartbeat = ipc_read_heartbeat();
        if (heartbeat == last_heartbeat) {
            // A35 沒有更新 heartbeat → 可能當機
            rot_handle_a35_unresponsive();
        }
        last_heartbeat = heartbeat;
        vTaskDelay(pdMS_TO_TICKS(1000));  // 等 1000ms
    }
}

void vRoT_IPC_Task(void *pvParameters) {
    for (;;) {
        // 等待 A35 透過 IPC 發出的服務請求（用 semaphore）
        if (xSemaphoreTake(ipc_request_sem, portMAX_DELAY) == pdTRUE) {
            rot_handle_ipc_request();
        }
    }
}

int main(void) {
    // 建立 task
    xTaskCreate(vRoT_Monitor_Task, "Monitor", 512, NULL, 2, NULL);
    xTaskCreate(vRoT_IPC_Task,     "IPC",     1024, NULL, 3, NULL);

    // 啟動排程器（不會返回）
    vTaskStartScheduler();
}
```

### 優先級

FreeRTOS 優先級：數字越大越高優先。
```
Priority 3: IPC handler       (fast response)
Priority 2: Heartbeat monitor
Priority 1: Periodic integrity check (non-urgent)
Priority 0: Idle task         (auto-created by FreeRTOS)
```

### Queue（任務間通訊）

```c
// 定義訊息格式
typedef struct {
    uint32_t cmd;
    uint8_t  data[64];
} IPC_Message;

// 建立 queue
QueueHandle_t xIPC_Queue;
xIPC_Queue = xQueueCreate(8, sizeof(IPC_Message));  // 深度 8

// ISR 中放入 queue（A35 觸發的中斷）
void SWI_IRQHandler(void) {
    IPC_Message msg;
    msg.cmd = ipc_read_cmd();
    // 從 ISR 用 FromISR 版本
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(xIPC_Queue, &msg, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// Task 中取出
void vRoT_IPC_Task(void *pvParameters) {
    IPC_Message msg;
    for (;;) {
        if (xQueueReceive(xIPC_Queue, &msg, portMAX_DELAY) == pdTRUE) {
            rot_handle_cmd(msg.cmd, msg.data);
        }
    }
}
```

### Mutex（互斥存取）

```c
// 保護 microSD 存取（讀 firmware 和讀 secure storage 不能同時）
MutexHandle_t xSD_Mutex;
xSD_Mutex = xSemaphoreCreateMutex();

void rot_read_secure_storage(void) {
    if (xSemaphoreTake(xSD_Mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        // 安全存取 microSD（SDMMC1）
        sdmmc_read(...);
        xSemaphoreGive(xNorFlash_Mutex);
    } else {
        // timeout，處理錯誤
    }
}
```

---

## RTOS + TrustZone 的互動

FreeRTOS 有支援 ARMv8-M TrustZone（FreeRTOS-Plus-POSIX 或 TFM）：

```c
// Secure tasks 在 Secure World 執行
// Non-Secure tasks 在 Non-Secure World 執行
// 切換 task 時同時切換 Security State

// portALLOCATE_SECURE_CONTEXT(ulSecureStackSize)
// 讓 NS task 在呼叫 Secure 函式時有獨立的 Secure stack
portALLOCATE_SECURE_CONTEXT(256);
```

---

## Zephyr RTOS（另一個選項）

Zephyr 是 Linux Foundation 的 RTOS，對 STM32MP2 M33 的支援越來越好：

```c
// Zephyr 風格（和 FreeRTOS 概念相同但 API 不同）

// Thread（等同 FreeRTOS task）
K_THREAD_DEFINE(rot_monitor, 1024,
                rot_monitor_thread, NULL, NULL, NULL,
                K_PRIO_PREEMPT(2), 0, 0);

void rot_monitor_thread(void *a, void *b, void *c) {
    while (1) {
        k_sleep(K_SECONDS(1));
        check_a35_heartbeat();
    }
}

// Message Queue
K_MSGQ_DEFINE(ipc_msgq, sizeof(IPC_Message), 8, 4);

// 放入 queue
k_msgq_put(&ipc_msgq, &msg, K_NO_WAIT);

// 取出 queue
k_msgq_get(&ipc_msgq, &msg, K_FOREVER);
```

---

## 面試常考：RTOS 概念

| 問題 | 要點 |
|------|------|
| preemptive vs cooperative 排程？ | preemptive = 高優先 task 可打斷低優先；cooperative = task 主動讓出 CPU |
| priority inversion 是什麼？ | 低優先 task 持有 mutex，高優先 task 被阻塞，等低優先執行 |
| 解決方法？ | Priority Inheritance（FreeRTOS mutex 預設支援）|
| Tick 是什麼？ | 計時器中斷頻率，決定最小時間粒度（通常 1ms = 1000Hz）|
| vTaskDelay vs vTaskDelayUntil？ | Delay = 從呼叫時開始等；DelayUntil = 精確週期（不受執行時間影響）|
| ISR 和 task 通訊為什麼要用 FromISR？ | ISR 中不能 block，FromISR 版本不會等待，且通知排程器 |
| stack overflow 怎麼偵測？ | FreeRTOS configCHECK_FOR_STACK_OVERFLOW = 2，用 canary 值 |
