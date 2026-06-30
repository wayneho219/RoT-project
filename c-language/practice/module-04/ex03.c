/*
 * 練習 04-03：Jump Table（RoT 指令分派）
 *
 * 情境：
 *   M33 作為 RoT，接收來自 A35 的指令 ID，
 *   根據 ID 分派到對應的處理函式。
 *
 * 任務：
 *   實作三個指令：
 *     ID 0 = CMD_GET_VERSION：印出 "version: 1.0.0"
 *     ID 1 = CMD_VERIFY：印出 "verifying firmware..."
 *     ID 2 = CMD_LOCK：印出 "locking device"
 *
 *   用 jump table 分派，不可用 switch/if-else。
 *   若 ID 超出範圍，印出 "unknown command"。
 */

#include <stdint.h>
#include <stdio.h>

typedef void (*Command)(void);
/* TODO: 實作三個 command handler */
void version_handler(void){
    printf("version: 1.0.0\n");
}

void verify_firmware_handler(void){
    printf("verifying firmware...\n");
}

void lock_handler(void){
    printf("locking device\n");
}
/* TODO: 定義 jump table */
Command dispatch_table[] = {
    version_handler,
    verify_firmware_handler,
    lock_handler,
};

#define CMD_COUNT 3
/* TODO: 實作 dispatch(cmd_id)，用 jump table 分派 */
void dispatch(uint8_t cmd_id) {
    if (cmd_id < CMD_COUNT){
        dispatch_table[cmd_id]();
    }else {
        printf("unknown command\n");
    }
}

int main(void) {
    dispatch(0);
    dispatch(1);
    dispatch(2);
    dispatch(9);  // 超出範圍

    return 0;
}
