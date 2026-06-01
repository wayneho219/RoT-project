#include <stdint.h>
#include <stdio.h>
#include <string.h>
/*
 * 練習 01-06：模組設計（綜合）
 *
 * 情境：
 *   設計一個簡單的 key-value 設定儲存模組（config store）。
 *   RoT 需要儲存少量設定（例如：允許的 firmware 版本、debug 模式開關）。
 *
 * 規格：
 *   - 最多儲存 4 筆設定
 *   - 每筆設定有 key（字串，最長 15 字元）和 value（uint32_t）
 *   - 內部狀態完全用 static 隱藏，外部只能透過函式存取
 *
 * 任務：
 *   實作以下函式：
 *
 *   void config_init(void)
 *     → 清空所有設定
 *
 *   int config_set(const char *key, uint32_t value)
 *     → 新增或更新一筆設定
 *     → 成功回傳 1，失敗（已滿且 key 不存在）回傳 0
 *
 *   int config_get(const char *key, uint32_t *out_value)
 *     → 找到回傳 1 並把值寫入 *out_value
 *     → 找不到回傳 0
 *
 *   int config_count(void)
 *     → 回傳目前儲存的筆數
 *
 * 限制：
 *   - 不可用 malloc
 *   - 字串比較用 strcmp（#include <string.h>）
 *   - 字串複製用 strncpy（注意 null terminator）
 *
 * 提示：
 *   定義一個 ConfigEntry struct，再用 static 陣列存它
 */
 
 #define CONFIG_MAX_ENTRIES  4
 #define CONFIG_KEY_MAX_LEN  15
 /* TODO: 定義 ConfigEntry struct 和 static 內部狀態 */
 typedef struct {
    char key[CONFIG_KEY_MAX_LEN + 1];
    uint32_t value;
 } ConfigEntry;
 static ConfigEntry config_entries[CONFIG_MAX_ENTRIES];

/* TODO: 實作四個函式 */
void config_init(void)                              { 
    for(int i = 0; i < CONFIG_MAX_ENTRIES; i++) {
        config_entries[i].key[0] = '\0';
        config_entries[i].value = 0;
    }
}
int  config_set(const char *key, uint32_t value)    { 
    for(int i = 0; i < CONFIG_MAX_ENTRIES; i++) {
        if(strcmp(config_entries[i].key, key) == 0){
            config_entries[i].value = value;
            return 1;
        }
        else{
            if(config_entries[i].key[0] == '\0'){
                strncpy(config_entries[i].key, key, CONFIG_KEY_MAX_LEN);
                config_entries[i].value = value;
                return 1;
            }
        }
    }
    return 0;
}
int  config_get(const char *key, uint32_t *out)     { 
    for(int i = 0; i < CONFIG_MAX_ENTRIES; i++){
        if(strcmp(config_entries[i].key, key) == 0){
            *out = config_entries[i].value;
            return 1;
        }
    }
    return 0;
}
int  config_count(void)                             { 
    int res = 0;
    for(int i = 0; i < CONFIG_MAX_ENTRIES; i++){
        if(config_entries[i].key[0] != '\0'){
            res++;
        }
    }
    return res;
}

int main(void) {
    config_init();
    printf("count=%d\n", config_count());

    config_set("fw_version", 3);
    config_set("debug_mode", 0);
    config_set("max_retry",  5);
    printf("count=%d\n", config_count());

    uint32_t val;
    if (config_get("debug_mode", &val)) printf("debug_mode=%u\n", val);
    if (config_get("fw_version", &val)) printf("fw_version=%u\n", val);

    /* 更新已存在的 key */
    config_set("fw_version", 4);
    if (config_get("fw_version", &val)) printf("fw_version=%u\n", val);
    printf("count=%d\n", config_count());  /* 更新不增加數量 */

    /* 找不到的 key */
    int found = config_get("nonexist", &val);
    printf("nonexist found=%d\n", found);

    /* 塞滿後拒絕新的 key */
    config_set("slot4", 99);
    int ok = config_set("slot5", 100);  /* 應該失敗 */
    printf("slot5 set=%d\n", ok);
    printf("count=%d\n", config_count());

    return 0;
}
