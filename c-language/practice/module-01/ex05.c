/*
 * 練習 01-05：狀態機（State Machine）
 *
 * 情境：
 *   RoT 的開機流程是一個狀態機：
 *
 *   IDLE → READING → VERIFYING → BOOTING → DONE
 *                        ↓
 *                      ERROR（任何階段都可能發生）
 *
 * 任務：
 *   1. 定義 BootState enum：
 *      BOOT_IDLE, BOOT_READING, BOOT_VERIFYING, BOOT_BOOTING, BOOT_DONE, BOOT_ERROR
 *
 *   2. 定義 BootContext struct，包含：
 *      - state: BootState
 *      - error_code: uint32_t
 *      - bytes_read: uint32_t
 *
 *   3. 用 static 區域變數讓 BootContext 只存在於 boot 模組內
 *      實作以下函式（不可在函式外宣告 BootContext）：
 *
 *      void boot_init(void)
 *        → 設定 state=IDLE, error_code=0, bytes_read=0
 *
 *      int boot_next(void)
 *        → 推進到下一個狀態（IDLE→READING→VERIFYING→BOOTING→DONE）
 *        → 若已是 DONE 或 ERROR，回傳 0（無法繼續）
 *        → 否則回傳 1
 *
 *      void boot_fail(uint32_t error_code)
 *        → 設定 state=ERROR，記錄 error_code
 *
 *      void boot_print(void)
 *        → 印出目前狀態（格式見 expected）
 *
 * 提示：
 *   state 的名稱用字串陣列對應會比較方便
 */

#include <stdint.h>
#include <stdio.h>

/* TODO: 定義 BootState enum */
typedef enum {
    BOOT_IDLE, 
    BOOT_READING, 
    BOOT_VERIFYING, 
    BOOT_BOOTING, 
    BOOT_DONE, 
    BOOT_ERROR
} BootState;

const char *state_names[] = {
    "IDLE",
    "READING",
    "VERIFYING",
    "BOOTING",
    "DONE",
    "ERROR"
};

/* TODO: 定義 BootContext struct */
typedef struct {
    BootState state;
    uint32_t error_code;
    uint32_t bytes_read;
} BootContext;

static BootContext b;
/* TODO: 實作四個函式，內部用 static BootContext */
void boot_init(void)            { 
    b.state = BOOT_IDLE;
    b.error_code = 0;
    b.bytes_read = 0;
}
int  boot_next(void)            { 
    if(b.state == BOOT_DONE || b.state == BOOT_ERROR) {
        return 0;
    } else {
        b.state++;
        return 1;
    }
}
void boot_fail(uint32_t code)   { 
    b.state = BOOT_ERROR;
    b.error_code = code;
}
void boot_print(void)           { 
    printf("state=%s error=0x%08X\n", state_names[b.state], b.error_code);
}

int main(void) {
    boot_init();
    boot_print();

    boot_next();
    boot_print();

    boot_next();
    boot_print();

    /* 模擬驗證失敗 */
    boot_fail(0xE001);
    boot_print();

    /* ERROR 狀態下呼叫 next 應該無效 */
    int result = boot_next();
    printf("next after error: %d\n", result);
    boot_print();

    /* 重新初始化，跑完整流程 */
    boot_init();
    while (boot_next()) {}
    boot_print();

    return 0;
}
