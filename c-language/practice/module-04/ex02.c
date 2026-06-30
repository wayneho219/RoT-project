/*
 * 練習 04-02：函式指標模擬介面（RoT 情境）
 *
 * 情境：
 *   RoT 需要驗證儲存在不同媒介上的 firmware。
 *   不管是 NOR Flash 還是 eMMC，驗證邏輯是一樣的，
 *   但讀取方式不同。用函式指標模擬「介面」。
 *
 * 任務：
 *   1. 定義 StorageDriver struct，包含：
 *      - name: const char*
 *      - read: 函式指標，簽名為 int(uint32_t addr, uint8_t *buf, uint32_t len)
 *
 *   2. 實作兩個假的 driver：
 *      - nor_read：把 buf 填入 0xAA（模擬 NOR flash 資料）
 *      - emmc_read：把 buf 填入 0xBB（模擬 eMMC 資料）
 *
 *   3. 實作 verify_firmware(driver)：
 *      - 讀 4 bytes（addr=0, len=4）
 *      - 印出：[driver name] read: 0xXX 0xXX 0xXX 0xXX
 *
 *   4. 用兩個不同 driver 各呼叫一次 verify_firmware
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct{
    const char* name;
    int (*read)(uint32_t addr, uint8_t *buf, uint32_t len);
} StorageDriver;

int nor_read(uint32_t addr, uint8_t *buf, uint32_t len){
    for (int i = 0; i < len; i++){
        buf[i] = 0xAA;
    }
    return 0;
}

int emmc_read(uint32_t addr, uint8_t *buf, uint32_t len){
    for (int i = 0; i < len; i++){
        buf[i] = 0xBB;
    }
    return 0;
}

void verify_firmware(StorageDriver driver, uint32_t addr, uint8_t *buf, uint32_t len){
    driver.read(addr, buf, len);
    printf("[%s] read:", driver.name);
    for (int i = 0; i < len; i++) {
        printf(" 0x%02X", buf[i]);
    }
    printf("\n");
}



int main(void) {
    /* TODO: 定義 nor_driver 和 emmc_driver，並各呼叫一次 verify_firmware */

    uint8_t buf[4];
    StorageDriver nor_driver;
    StorageDriver emmc_driver;
    nor_driver.name = "nor";
    emmc_driver.name = "emmc";
    nor_driver.read = nor_read;
    emmc_driver.read = emmc_read;
    verify_firmware(nor_driver, 0, buf, 4);
    verify_firmware(emmc_driver, 0, buf, 4);
    return 0;
}
