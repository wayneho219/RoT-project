/*
 * 練習 02：Struct 與 typedef
 *
 * 情境：
 *   在嵌入式系統中，我們常用 struct 來描述一個硬體裝置的狀態，
 *   例如一個 LED 的狀態（是否亮著、亮度）。
 *
 * 任務：
 *   1. 用 typedef struct 定義 Led，包含兩個欄位：
 *      - is_on：uint8_t，表示是否亮著（1 = 亮，0 = 熄）
 *      - brightness：uint8_t，亮度（0–100）
 *
 *   2. 實作 led_on()：設定 is_on = 1，brightness 為傳入值
 *   3. 實作 led_off()：設定 is_on = 0，brightness = 0
 *   4. 實作 led_print()：印出狀態（格式見 ex02.expected）
 */

#include <stdint.h>
#include <stdio.h>

/* TODO: 定義 Led struct */
typedef struct{
    uint8_t is_on;
    uint8_t brightness;
} Led;

/* TODO: 實作以下函式 */
void led_on(Led *led, uint8_t brightness) {
    led -> is_on = 1;
    led -> brightness = brightness;
}

void led_off(Led *led) {
    led -> is_on = 0;
    led -> brightness = 0;
}

void led_print(const Led *led) {
    /* 輸出格式：
     *   LED: ON  brightness=80
     *   LED: OFF brightness=0
     */
    printf("LED: %s brightness=%d\n", led -> is_on ? "ON" : "OFF", led -> brightness);
}

int main(void) {
    Led my_led;   /* TODO: 宣告一個 Led 變數 */

    led_on(&my_led, 80);
    led_print(&my_led);

    led_off(&my_led);
    led_print(&my_led);

    return 0;
}
