#include "lvgl.h"
#include <esp_log.h>
#include <string.h>

extern const lv_image_dsc_t emoji_neutral_120; // neutral
extern const lv_image_dsc_t emoji_happy_120; // happy
extern const lv_image_dsc_t emoji_laughing_120; // laughing
extern const lv_image_dsc_t emoji_1f602_64; // funny
extern const lv_image_dsc_t emoji_1f614_64; // sad
extern const lv_image_dsc_t emoji_1f620_64; // angry
extern const lv_image_dsc_t emoji_1f62d_64; // crying
extern const lv_image_dsc_t emoji_1f60d_64; // loving
extern const lv_image_dsc_t emoji_1f633_64; // embarrassed
extern const lv_image_dsc_t emoji_1f62f_64; // surprised
extern const lv_image_dsc_t emoji_1f631_64; // shocked
extern const lv_image_dsc_t emoji_1f914_64; // thinking
extern const lv_image_dsc_t emoji_1f609_64; // winking
extern const lv_image_dsc_t emoji_1f60e_64; // cool
extern const lv_image_dsc_t emoji_1f60c_64; // relaxed
extern const lv_image_dsc_t emoji_1f924_64; // delicious
extern const lv_image_dsc_t emoji_1f618_64; // kissy
extern const lv_image_dsc_t emoji_1f60f_64; // confident
extern const lv_image_dsc_t emoji_1f634_64; // sleepy
extern const lv_image_dsc_t emoji_1f61c_64; // silly
extern const lv_image_dsc_t emoji_1f644_64; // confused

typedef struct emoji_240 {
    const lv_image_dsc_t* emoji;
    const char* name;
} emoji_240_t;

// 全局变量存储当前要显示的表情图片
static const lv_image_dsc_t* current_emoji = NULL;

// 字符串名称到图片的映射表
static const emoji_240_t emoji_240_table[] = {
    { &emoji_neutral_120, "neutral" },     // 中性
    { &emoji_happy_120, "happy" },         // 快乐
    { &emoji_laughing_120, "laughing" },   // 大笑
    // { &emoji_1f602_64, "funny" },         // 有趣
    // { &emoji_1f614_64, "sad" },           // 悲伤
    // { &emoji_1f620_64, "angry" },         // 愤怒
    // { &emoji_1f62d_64, "crying" },        // 哭泣
    // { &emoji_1f60d_64, "loving" },        // 爱心
    // { &emoji_1f633_64, "embarrassed" },   // 尴尬
    // { &emoji_1f62f_64, "surprised" },     // 惊讶
    // { &emoji_1f631_64, "shocked" },       // 震惊
    // { &emoji_1f914_64, "thinking" },      // 思考
    // { &emoji_1f609_64, "winking" },       // 眨眼
    // { &emoji_1f60e_64, "cool" },          // 酷
    // { &emoji_1f60c_64, "relaxed" },       // 放松
    // { &emoji_1f924_64, "delicious" },     // 美味
    // { &emoji_1f618_64, "kissy" },         // 亲吻
    // { &emoji_1f60f_64, "confident" },     // 自信
    // { &emoji_1f634_64, "sleepy" },        // 困倦
    // { &emoji_1f61c_64, "silly" },         // 愚蠢
    // { &emoji_1f644_64, "confused" },      // 困惑
};

// 设置要显示的表情
bool font_emoji_240_set_emotion(const char* emotion_name) {
    current_emoji = NULL;
    
    for (size_t i = 0; i < sizeof(emoji_240_table) / sizeof(emoji_240_table[0]); i++) {
        if (strcmp(emoji_240_table[i].name, emotion_name) == 0) {
            current_emoji = emoji_240_table[i].emoji;
            return true;
        }
    }
    
    // 如果没找到，使用默认的neutral表情
    current_emoji = &emoji_neutral_120;
    return false;
}

// 获取当前设置的表情图片
const lv_image_dsc_t* font_emoji_240_get_current_emotion(void) {
    return current_emoji ? current_emoji : &emoji_neutral_120;
}

static const void* get_imgfont_path(const lv_font_t * font, uint32_t unicode, uint32_t unicode_next, int32_t * offset_y, void * user_data) {
    // 对于表情字体，我们不使用Unicode，而是返回当前设置的表情图片
    return font_emoji_240_get_current_emotion();
}

const lv_font_t* font_emoji_240_init(void) {
    static lv_font_t* font = NULL;
    if (font == NULL) {
        font = lv_imgfont_create(240, get_imgfont_path, NULL);
        if (font == NULL) {
            LV_LOG_ERROR("Failed to allocate memory for emoji font");
            return NULL;
        }
        font->base_line = 0;
        font->fallback = NULL;
        
        // 设置默认表情为neutral
        current_emoji = &emoji_neutral_120;
    }
    return font;
}


