#pragma once

#include <lvgl.h>

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

#ifdef __cplusplus
}
#endif 