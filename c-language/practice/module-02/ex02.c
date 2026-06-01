/*
 * 練習 02-02：指標與 Struct
 *
 * 情境：模擬 RoT 驗證結果的狀態結構
 *
 * 任務：
 *   1. 定義 VerifyResult struct，包含：
 *      - passed: uint8_t（1 = 通過, 0 = 失敗）
 *      - error_code: uint32_t
 *      - bytes_checked: uint32_t
 *
 *   2. 實作 verify_block()：
 *      - 若 len == 0，設定 passed=0, error_code=1
 *      - 否則設定 passed=1, error_code=0, bytes_checked=len
 *
 *   3. 在 main 中用 -> 存取結果
 */

#include <stdint.h>
#include <stdio.h>

/* TODO: 定義 VerifyResult */
typedef struct{
    uint8_t passed;
    uint32_t error_code;
    uint32_t bytes_checked;
} VerifyResult;


/* TODO: 實作 verify_block */
void verify_block(VerifyResult *result, uint32_t len) {
    if (len == 0){
        result -> passed = 0;
        result -> error_code = 1;
    }
    else {
        result -> passed = 1;
        result -> error_code  = 0;
    }
    result -> bytes_checked = len;
}

int main(void) {
    VerifyResult r1, r2;

    verify_block(&r1, 256);
    printf("r1: passed=%u error=%u checked=%u\n",
           r1.passed, r1.error_code, r1.bytes_checked);

    verify_block(&r2, 0);
    printf("r2: passed=%u error=%u checked=%u\n",
           r2.passed, r2.error_code, r2.bytes_checked);

    return 0;
}
