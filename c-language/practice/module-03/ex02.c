/*
 * 練習 03-02：Multi-bit field 操作
 *
 * 情境：
 *   模擬一個 STM32 GPIO 的 MODER 暫存器
 *   每個 pin 佔 2 bits：
 *     00 = Input
 *     01 = Output
 *     10 = Alternate Function
 *     11 = Analog
 *
 *   pin N 的欄位在 bit [(2N+1):(2N)]
 *
 * 任務：
 *   1. 實作 gpio_set_mode(reg, pin, mode)：設定指定 pin 的 mode
 *   2. 實作 gpio_get_mode(reg, pin)：讀取指定 pin 的 mode
 */

#include <stdint.h>
#include <stdio.h>

#define GPIO_MODE_INPUT  0x0U
#define GPIO_MODE_OUTPUT 0x1U
#define GPIO_MODE_AF     0x2U
#define GPIO_MODE_ANALOG 0x3U

/* TODO */
uint32_t gpio_set_mode(uint32_t moder, uint8_t pin, uint8_t mode) {
    /* 提示：
     *   shift = pin * 2
     *   mask  = 0x3 << shift
     *   先清除，再寫入
     */
}

/* TODO */
uint8_t gpio_get_mode(uint32_t moder, uint8_t pin) {
    /* ... */
}

int main(void) {
    uint32_t moder = 0;

    moder = gpio_set_mode(moder, 0, GPIO_MODE_OUTPUT);
    moder = gpio_set_mode(moder, 3, GPIO_MODE_AF);
    moder = gpio_set_mode(moder, 5, GPIO_MODE_ANALOG);

    printf("pin0 mode=%u\n", gpio_get_mode(moder, 0));  // 1
    printf("pin3 mode=%u\n", gpio_get_mode(moder, 3));  // 2
    printf("pin5 mode=%u\n", gpio_get_mode(moder, 5));  // 3
    printf("pin1 mode=%u\n", gpio_get_mode(moder, 1));  // 0

    return 0;
}
