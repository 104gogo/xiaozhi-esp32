#pragma once

#include <lvgl.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 128大小表情声明
// extern const lv_image_dsc_t emoji_1f636_128; // neutral
// extern const lv_image_dsc_t emoji_1f60a_128; // smile
// extern const lv_image_dsc_t emoji_1f622_128; // sad
// extern const lv_image_dsc_t emoji_1f620_128; // angry
// extern const lv_image_dsc_t emoji_1f62e_128; // surprise

// 添加表情初始化函数声明
const lv_font_t* font_emoji_240_init(void);

// 设置要显示的表情（通过名称）
bool font_emoji_240_set_emotion(const char* emotion_name);

// 获取当前设置的表情图片
const lv_image_dsc_t* font_emoji_240_get_current_emotion(void);

#ifdef __cplusplus
}
#endif 