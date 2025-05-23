#include "lcd_display_new.h"

#include <vector>
#include <font_awesome_symbols.h>
#include <esp_log.h>
#include <esp_err.h>
#include <esp_lvgl_port.h>
#include "assets/lang_config.h"
#include <cstring>
#include "settings.h"
#include "board.h"
#include "images/xingzhi-cube-1.54/panda/gImage_output_0001.h"
#include "../emojis/font_emoji_240.h"

#define TAG "LcdDisplayNew"

// Color definitions for dark theme
#define DARK_BACKGROUND_COLOR       lv_color_hex(0x121212)     // Dark background
#define DARK_TEXT_COLOR             lv_color_white()           // White text
#define DARK_CHAT_BACKGROUND_COLOR  lv_color_hex(0x1E1E1E)     // Slightly lighter than background
#define DARK_USER_BUBBLE_COLOR      lv_color_hex(0x1A6C37)     // Dark green
#define DARK_ASSISTANT_BUBBLE_COLOR lv_color_hex(0x333333)     // Dark gray
#define DARK_SYSTEM_BUBBLE_COLOR    lv_color_hex(0x2A2A2A)     // Medium gray
#define DARK_SYSTEM_TEXT_COLOR      lv_color_hex(0xAAAAAA)     // Light gray text
#define DARK_BORDER_COLOR           lv_color_hex(0x333333)     // Dark gray border
#define DARK_LOW_BATTERY_COLOR      lv_color_hex(0xFF0000)     // Red for dark mode

// Color definitions for light theme
#define LIGHT_BACKGROUND_COLOR       lv_color_white()           // White background
#define LIGHT_TEXT_COLOR             lv_color_black()           // Black text
#define LIGHT_CHAT_BACKGROUND_COLOR  lv_color_hex(0xE0E0E0)     // Light gray background
#define LIGHT_USER_BUBBLE_COLOR      lv_color_hex(0x95EC69)     // WeChat green
#define LIGHT_ASSISTANT_BUBBLE_COLOR lv_color_white()           // White
#define LIGHT_SYSTEM_BUBBLE_COLOR    lv_color_hex(0xE0E0E0)     // Light gray
#define LIGHT_SYSTEM_TEXT_COLOR      lv_color_hex(0x666666)     // Dark gray text
#define LIGHT_BORDER_COLOR           lv_color_hex(0xE0E0E0)     // Light gray border
#define LIGHT_LOW_BATTERY_COLOR      lv_color_black()           // Black for light mode

// 升级信息区域背景色
#define DARK_UPDATE_INFO_BG_COLOR    lv_color_hex(0x1A6C37)     // 深绿色背景
#define LIGHT_UPDATE_INFO_BG_COLOR   lv_color_hex(0x95EC69)     // 浅绿色背景

// Theme color structure
struct ThemeColors {
    lv_color_t background;
    lv_color_t text;
    lv_color_t chat_background;
    lv_color_t user_bubble;
    lv_color_t assistant_bubble;
    lv_color_t system_bubble;
    lv_color_t system_text;
    lv_color_t border;
    lv_color_t low_battery;
    lv_color_t update_info_bg; // 升级信息区域背景色
};

// Define dark theme colors
static const ThemeColors DARK_THEME = {
    .background = DARK_BACKGROUND_COLOR,
    .text = DARK_TEXT_COLOR,
    .chat_background = DARK_CHAT_BACKGROUND_COLOR,
    .user_bubble = DARK_USER_BUBBLE_COLOR,
    .assistant_bubble = DARK_ASSISTANT_BUBBLE_COLOR,
    .system_bubble = DARK_SYSTEM_BUBBLE_COLOR,
    .system_text = DARK_SYSTEM_TEXT_COLOR,
    .border = DARK_BORDER_COLOR,
    .low_battery = DARK_LOW_BATTERY_COLOR,
    .update_info_bg = DARK_UPDATE_INFO_BG_COLOR
};

// Define light theme colors
static const ThemeColors LIGHT_THEME = {
    .background = LIGHT_BACKGROUND_COLOR,
    .text = LIGHT_TEXT_COLOR,
    .chat_background = LIGHT_CHAT_BACKGROUND_COLOR,
    .user_bubble = LIGHT_USER_BUBBLE_COLOR,
    .assistant_bubble = LIGHT_ASSISTANT_BUBBLE_COLOR,
    .system_bubble = LIGHT_SYSTEM_BUBBLE_COLOR,
    .system_text = LIGHT_SYSTEM_TEXT_COLOR,
    .border = LIGHT_BORDER_COLOR,
    .low_battery = LIGHT_LOW_BATTERY_COLOR,
    .update_info_bg = LIGHT_UPDATE_INFO_BG_COLOR
};

// Current theme - initialize based on default config
static ThemeColors current_theme = LIGHT_THEME;


LV_FONT_DECLARE(font_awesome_30_4);

SpiLcdDisplayNew::SpiLcdDisplayNew(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                           int width, int height, int offset_x, int offset_y, bool mirror_x, bool mirror_y, bool swap_xy,
                           DisplayFonts fonts)
    : LcdDisplayNew(panel_io, panel, fonts) {
    width_ = width;
    height_ = height;

    // draw white
    std::vector<uint16_t> buffer(width_, 0xFFFF);
    for (int y = 0; y < height_; y++) {
        esp_lcd_panel_draw_bitmap(panel_, 0, y, width_, y + 1, buffer.data());
    }

    // Set the display to on
    ESP_LOGI(TAG, "Turning display on");
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();

    ESP_LOGI(TAG, "Initialize LVGL port");
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 1;
    port_cfg.timer_period_ms = 50;
    lvgl_port_init(&port_cfg);

    ESP_LOGI(TAG, "Adding LCD screen");
    const lvgl_port_display_cfg_t display_cfg = {
        .io_handle = panel_io_,
        .panel_handle = panel_,
        .control_handle = nullptr,
        .buffer_size = static_cast<uint32_t>(width_ * 10),
        .double_buffer = false,
        .trans_size = 0,
        .hres = static_cast<uint32_t>(width_),
        .vres = static_cast<uint32_t>(height_),
        .monochrome = false,
        .rotation = {
            .swap_xy = swap_xy,
            .mirror_x = mirror_x,
            .mirror_y = mirror_y,
        },
        .color_format = LV_COLOR_FORMAT_RGB565,
        .flags = {
            .buff_dma = 1,
            .buff_spiram = 0,
            .sw_rotate = 0,
            .swap_bytes = 1,
            .full_refresh = 0,
            .direct_mode = 0,
        },
    };

    display_ = lvgl_port_add_disp(&display_cfg);
    if (display_ == nullptr) {
        ESP_LOGE(TAG, "Failed to add display");
        return;
    }

    if (offset_x != 0 || offset_y != 0) {
        lv_display_set_offset(display_, offset_x, offset_y);
    }

    // Update the theme
    if (current_theme_name_ == "dark") {
        current_theme = DARK_THEME;
    } else if (current_theme_name_ == "light") {
        current_theme = LIGHT_THEME;
    }

    SetupUI();
}

// RGB LCD实现
RgbLcdDisplayNew::RgbLcdDisplayNew(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                           int width, int height, int offset_x, int offset_y,
                           bool mirror_x, bool mirror_y, bool swap_xy,
                           DisplayFonts fonts)
    : LcdDisplayNew(panel_io, panel, fonts) {
    width_ = width;
    height_ = height;
    
    // draw white
    std::vector<uint16_t> buffer(width_, 0xFFFF);
    for (int y = 0; y < height_; y++) {
        esp_lcd_panel_draw_bitmap(panel_, 0, y, width_, y + 1, buffer.data());
    }

    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();

    ESP_LOGI(TAG, "Initialize LVGL port");
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 1;
    port_cfg.timer_period_ms = 50;
    lvgl_port_init(&port_cfg);

    ESP_LOGI(TAG, "Adding LCD screen");
    const lvgl_port_display_cfg_t display_cfg = {
        .io_handle = panel_io_,
        .panel_handle = panel_,
        .buffer_size = static_cast<uint32_t>(width_ * 10),
        .double_buffer = true,
        .hres = static_cast<uint32_t>(width_),
        .vres = static_cast<uint32_t>(height_),
        .rotation = {
            .swap_xy = swap_xy,
            .mirror_x = mirror_x,
            .mirror_y = mirror_y,
        },
        .flags = {
            .buff_dma = 1,
            .swap_bytes = 0,
            .full_refresh = 1,
            .direct_mode = 1,
        },
    };

    const lvgl_port_display_rgb_cfg_t rgb_cfg = {
        .flags = {
            .bb_mode = true,
            .avoid_tearing = true,
        }
    };
    
    display_ = lvgl_port_add_disp_rgb(&display_cfg, &rgb_cfg);
    if (display_ == nullptr) {
        ESP_LOGE(TAG, "Failed to add RGB display");
        return;
    }
    
    if (offset_x != 0 || offset_y != 0) {
        lv_display_set_offset(display_, offset_x, offset_y);
    }

    // Update the theme
    if (current_theme_name_ == "dark") {
        current_theme = DARK_THEME;
    } else if (current_theme_name_ == "light") {
        current_theme = LIGHT_THEME;
    }

    SetupUI();
}

LcdDisplayNew::~LcdDisplayNew() {
    // 清理表情标签
    if (emoji_label_ != nullptr) {
        lv_obj_del(emoji_label_);
        emoji_label_ = nullptr;
    }
    
    // 然后再清理 LVGL 对象
    if (content_ != nullptr) {
        lv_obj_del(content_);
    }
    if (status_bar_ != nullptr) {
        lv_obj_del(status_bar_);
    }
    if (side_bar_ != nullptr) {
        lv_obj_del(side_bar_);
    }
    if (update_info_ != nullptr) {
        lv_obj_del(update_info_);
    }
    if (container_ != nullptr) {
        lv_obj_del(container_);
    }
    if (display_ != nullptr) {
        lv_display_delete(display_);
    }

    if (panel_ != nullptr) {
        esp_lcd_panel_del(panel_);
    }
    if (panel_io_ != nullptr) {
        esp_lcd_panel_io_del(panel_io_);
    }
}

bool LcdDisplayNew::Lock(int timeout_ms) {
    return lvgl_port_lock(timeout_ms);
}

void LcdDisplayNew::Unlock() {
    lvgl_port_unlock();
}

void LcdDisplayNew::SetupUI() {
    DisplayLockGuard lock(this);

    auto screen = lv_screen_active();
    lv_obj_set_style_text_font(screen, fonts_.text_font, 0);
    lv_obj_set_style_text_color(screen, current_theme.text, 0);
    lv_obj_set_style_bg_color(screen, current_theme.background, 0);

    /* 主容器 */
    container_ = lv_obj_create(screen);
    lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_pad_row(container_, 0, 0);
    lv_obj_set_style_bg_color(container_, current_theme.background, 0);
    lv_obj_set_style_border_color(container_, current_theme.border, 0);

    /* 中间内容区域（占满全屏） */
    content_ = lv_obj_create(container_);
    lv_obj_set_style_radius(content_, 0, 0);
    lv_obj_set_size(content_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_pad_all(content_, 0, 0);
    lv_obj_set_style_bg_color(content_, current_theme.chat_background, 0);
    lv_obj_set_style_border_width(content_, 0, 0);
    lv_obj_set_scrollbar_mode(content_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_dir(content_, LV_DIR_VER);
    lv_obj_set_flex_flow(content_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // 创建画布并显示图片
    CreateCanvas();
    
    // 创建表情标签
    CreateEmojiLabel();
    
    /* 顶部状态栏（半透明覆盖） */
    status_bar_ = lv_obj_create(screen);
    lv_obj_set_size(status_bar_, LV_HOR_RES, 25);
    lv_obj_set_style_radius(status_bar_, 0, 0);
    lv_obj_set_style_bg_color(status_bar_, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(status_bar_, LV_OPA_30, 0);
    lv_obj_set_style_border_width(status_bar_, 0, 0);
    lv_obj_set_style_text_color(status_bar_, lv_color_white(), 0);
    lv_obj_set_flex_flow(status_bar_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(status_bar_, 5, 0);
    lv_obj_set_style_pad_left(status_bar_, 10, 0);
    lv_obj_set_style_pad_right(status_bar_, 10, 0);
    lv_obj_set_style_pad_top(status_bar_, 5, 0);
    lv_obj_set_style_pad_bottom(status_bar_, 5, 0);
    lv_obj_set_scrollbar_mode(status_bar_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_flex_align(status_bar_, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    /* 底部文字区域（半透明覆盖） */
    side_bar_ = lv_obj_create(screen);
    lv_obj_set_size(side_bar_, LV_HOR_RES, 30);
    lv_obj_align(side_bar_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_radius(side_bar_, 0, 0);
    lv_obj_set_style_bg_color(side_bar_, lv_color_hex(0x888888), 0);
    lv_obj_set_style_bg_opa(side_bar_, LV_OPA_70, 0);
    lv_obj_set_style_border_width(side_bar_, 0, 0);
    lv_obj_set_style_text_color(side_bar_, lv_color_black(), 0);
    lv_obj_set_style_pad_all(side_bar_, 5, 0);
    lv_obj_set_scrollbar_mode(side_bar_, LV_SCROLLBAR_MODE_OFF);
    // 默认隐藏底部栏
    lv_obj_add_flag(side_bar_, LV_OBJ_FLAG_HIDDEN);

    /* 状态栏内容 */
    // 网络状态图标（放在最左侧）
    network_label_ = lv_label_create(status_bar_);
    lv_label_set_text(network_label_, "");
    lv_obj_set_style_text_font(network_label_, fonts_.icon_font, 0);
    lv_obj_set_style_text_color(network_label_, lv_color_white(), 0);
    
    // 中间状态文本
    status_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(status_label_, 1);
    lv_label_set_long_mode(status_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(status_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(status_label_, lv_color_white(), 0);
    // 显式设置使用配置的字体
    lv_obj_set_style_text_font(status_label_, fonts_.text_font, 0);
    lv_label_set_text(status_label_, Lang::Strings::INITIALIZING);

    // 右侧状态图标
    mute_label_ = lv_label_create(status_bar_);
    lv_label_set_text(mute_label_, "");
    lv_obj_set_style_text_font(mute_label_, fonts_.icon_font, 0);
    lv_obj_set_style_text_color(mute_label_, lv_color_white(), 0);

    battery_label_ = lv_label_create(status_bar_);
    lv_label_set_text(battery_label_, "");
    lv_obj_set_style_text_font(battery_label_, fonts_.icon_font, 0);
    lv_obj_set_style_text_color(battery_label_, lv_color_white(), 0);
    
    /* 底部文字区域 */
    chat_message_label_ = lv_label_create(side_bar_);
    lv_obj_set_width(chat_message_label_, LV_HOR_RES - 20);
    lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(chat_message_label_, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_style_text_color(chat_message_label_, lv_color_black(), 0);
    // 显式设置使用配置的字体
    lv_obj_set_style_text_font(chat_message_label_, fonts_.text_font, 0);
    lv_label_set_text(chat_message_label_, "");
    lv_obj_align(chat_message_label_, LV_ALIGN_RIGHT_MID, -10, 0);

    /* 通知弹窗 */
    notification_label_ = lv_label_create(screen);
    lv_obj_set_style_text_align(notification_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(notification_label_, lv_color_white(), 0);
    lv_obj_set_style_bg_color(notification_label_, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(notification_label_, LV_OPA_50, 0);
    lv_obj_set_style_radius(notification_label_, 5, 0);
    lv_obj_set_style_pad_all(notification_label_, 5, 0);
    lv_label_set_text(notification_label_, "");
    lv_obj_align(notification_label_, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);

    /* 低电量弹窗 */
    low_battery_popup_ = lv_obj_create(screen);
    lv_obj_set_scrollbar_mode(low_battery_popup_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_size(low_battery_popup_, LV_HOR_RES * 0.9, fonts_.text_font->line_height * 2);
    lv_obj_align(low_battery_popup_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(low_battery_popup_, current_theme.low_battery, 0);
    lv_obj_set_style_radius(low_battery_popup_, 10, 0);
    low_battery_label_ = lv_label_create(low_battery_popup_);
    lv_label_set_text(low_battery_label_, Lang::Strings::BATTERY_NEED_CHARGE);
    lv_obj_set_style_text_color(low_battery_label_, lv_color_white(), 0);
    lv_obj_center(low_battery_label_);
    lv_obj_add_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);
    
    /* 中部升级信息区域 */
    update_info_ = lv_obj_create(screen);
    lv_obj_set_size(update_info_, LV_HOR_RES * 0.8, 50);
    lv_obj_align(update_info_, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(update_info_, 8, 0); // 圆角效果
    lv_obj_set_style_bg_color(update_info_, current_theme.update_info_bg, 0);
    lv_obj_set_style_bg_opa(update_info_, LV_OPA_90, 0);
    lv_obj_set_style_border_width(update_info_, 0, 0);
    lv_obj_set_style_text_color(update_info_, lv_color_white(), 0);
    lv_obj_set_style_pad_all(update_info_, 10, 0);
    lv_obj_set_scrollbar_mode(update_info_, LV_SCROLLBAR_MODE_OFF);
    
    // 创建更新信息标签
    update_label_ = lv_label_create(update_info_);
    lv_obj_set_width(update_label_, LV_HOR_RES * 0.7);
    lv_label_set_long_mode(update_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_align(update_label_, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(update_label_, lv_color_white(), 0);
    lv_obj_set_style_text_font(update_label_, fonts_.text_font, 0);
    lv_label_set_text(update_label_, "");
    lv_obj_center(update_label_);
    
    // 默认隐藏升级信息区域
    lv_obj_add_flag(update_info_, LV_OBJ_FLAG_HIDDEN);
}

void LcdDisplayNew::SetEmotion(const char* emotion) {
    // 使用新的表情画布显示表情
    ShowEmoji(emotion);
}

void LcdDisplayNew::SetIcon(const char* icon) {
    // 此方法已被弃用，不再显示图标
}

// 显示底部栏
void LcdDisplayNew::ShowSideBar() {
    DisplayLockGuard lock(this);
    if (side_bar_ != nullptr) {
        lv_obj_clear_flag(side_bar_, LV_OBJ_FLAG_HIDDEN);
    }
}

// 隐藏底部栏
void LcdDisplayNew::HideSideBar() {
    DisplayLockGuard lock(this);
    if (side_bar_ != nullptr) {
        lv_obj_add_flag(side_bar_, LV_OBJ_FLAG_HIDDEN);
    }
}

void LcdDisplayNew::SetTheme(const std::string& theme_name) {
    DisplayLockGuard lock(this);
    
    if (theme_name == "dark" || theme_name == "DARK") {
        current_theme = DARK_THEME;
    } else if (theme_name == "light" || theme_name == "LIGHT") {
        current_theme = LIGHT_THEME;
    } else {
        // Invalid theme name, return false
        ESP_LOGE(TAG, "Invalid theme name: %s", theme_name.c_str());
        return;
    }
    
    // Get the active screen
    lv_obj_t* screen = lv_screen_active();
    
    // Update the screen colors
    lv_obj_set_style_bg_color(screen, current_theme.background, 0);
    lv_obj_set_style_text_color(screen, current_theme.text, 0);
    
    // Update container colors
    if (container_ != nullptr) {
        lv_obj_set_style_bg_color(container_, current_theme.background, 0);
        lv_obj_set_style_border_color(container_, current_theme.border, 0);
    }
    
    // Update status bar colors
    if (status_bar_ != nullptr) {
        lv_obj_set_style_bg_color(status_bar_, current_theme.background, 0);
        lv_obj_set_style_text_color(status_bar_, current_theme.text, 0);
        
        // Update status bar elements
        if (network_label_ != nullptr) {
            lv_obj_set_style_text_color(network_label_, current_theme.text, 0);
        }
        if (status_label_ != nullptr) {
            lv_obj_set_style_text_color(status_label_, current_theme.text, 0);
        }
        if (notification_label_ != nullptr) {
            lv_obj_set_style_text_color(notification_label_, current_theme.text, 0);
        }
        if (mute_label_ != nullptr) {
            lv_obj_set_style_text_color(mute_label_, current_theme.text, 0);
        }
        if (battery_label_ != nullptr) {
            lv_obj_set_style_text_color(battery_label_, current_theme.text, 0);
        }
    }
    
    // Update content area colors
    if (content_ != nullptr) {
        lv_obj_set_style_bg_color(content_, current_theme.chat_background, 0);
        lv_obj_set_style_border_color(content_, current_theme.border, 0);
        
        // If we have the chat message style, update all message bubbles
#if CONFIG_USE_WECHAT_MESSAGE_STYLE
        // Iterate through all children of content (message containers or bubbles)
        uint32_t child_count = lv_obj_get_child_cnt(content_);
        for (uint32_t i = 0; i < child_count; i++) {
            lv_obj_t* obj = lv_obj_get_child(content_, i);
            if (obj == nullptr) continue;
            
            lv_obj_t* bubble = nullptr;
            
            // 检查这个对象是容器还是气泡
            // 如果是容器（用户或系统消息），则获取其子对象作为气泡
            // 如果是气泡（助手消息），则直接使用
            if (lv_obj_get_child_cnt(obj) > 0) {
                // 可能是容器，检查它是否为用户或系统消息容器
                // 用户和系统消息容器是透明的
                lv_opa_t bg_opa = lv_obj_get_style_bg_opa(obj, 0);
                if (bg_opa == LV_OPA_TRANSP) {
                    // 这是用户或系统消息的容器
                    bubble = lv_obj_get_child(obj, 0);
                } else {
                    // 这可能是助手消息的气泡自身
                    bubble = obj;
                }
            } else {
                // 没有子元素，可能是其他UI元素，跳过
                continue;
            }
            
            if (bubble == nullptr) continue;
            
            // 使用保存的用户数据来识别气泡类型
            void* bubble_type_ptr = lv_obj_get_user_data(bubble);
            if (bubble_type_ptr != nullptr) {
                const char* bubble_type = static_cast<const char*>(bubble_type_ptr);
                
                // 根据气泡类型应用正确的颜色
                if (strcmp(bubble_type, "user") == 0) {
                    lv_obj_set_style_bg_color(bubble, current_theme.user_bubble, 0);
                } else if (strcmp(bubble_type, "assistant") == 0) {
                    lv_obj_set_style_bg_color(bubble, current_theme.assistant_bubble, 0); 
                } else if (strcmp(bubble_type, "system") == 0) {
                    lv_obj_set_style_bg_color(bubble, current_theme.system_bubble, 0);
                }
                
                // Update border color
                lv_obj_set_style_border_color(bubble, current_theme.border, 0);
                
                // Update text color for the message
                if (lv_obj_get_child_cnt(bubble) > 0) {
                    lv_obj_t* text = lv_obj_get_child(bubble, 0);
                    if (text != nullptr) {
                        // 根据气泡类型设置文本颜色
                        if (strcmp(bubble_type, "system") == 0) {
                            lv_obj_set_style_text_color(text, current_theme.system_text, 0);
                        } else {
                            lv_obj_set_style_text_color(text, current_theme.text, 0);
                        }
                    }
                }
            } else {
                // 如果没有标记，回退到之前的逻辑（颜色比较）
                // ...保留原有的回退逻辑...
                lv_color_t bg_color = lv_obj_get_style_bg_color(bubble, 0);
            
                // 改进bubble类型检测逻辑，不仅使用颜色比较
                bool is_user_bubble = false;
                bool is_assistant_bubble = false;
                bool is_system_bubble = false;
            
                // 检查用户bubble
                if (lv_color_eq(bg_color, DARK_USER_BUBBLE_COLOR) || 
                    lv_color_eq(bg_color, LIGHT_USER_BUBBLE_COLOR) ||
                    lv_color_eq(bg_color, current_theme.user_bubble)) {
                    is_user_bubble = true;
                }
                // 检查系统bubble
                else if (lv_color_eq(bg_color, DARK_SYSTEM_BUBBLE_COLOR) || 
                         lv_color_eq(bg_color, LIGHT_SYSTEM_BUBBLE_COLOR) ||
                         lv_color_eq(bg_color, current_theme.system_bubble)) {
                    is_system_bubble = true;
                }
                // 剩余的都当作助手bubble处理
                else {
                    is_assistant_bubble = true;
                }
            
                // 根据bubble类型应用正确的颜色
                if (is_user_bubble) {
                    lv_obj_set_style_bg_color(bubble, current_theme.user_bubble, 0);
                } else if (is_assistant_bubble) {
                    lv_obj_set_style_bg_color(bubble, current_theme.assistant_bubble, 0);
                } else if (is_system_bubble) {
                    lv_obj_set_style_bg_color(bubble, current_theme.system_bubble, 0);
                }
                
                // Update border color
                lv_obj_set_style_border_color(bubble, current_theme.border, 0);
                
                // Update text color for the message
                if (lv_obj_get_child_cnt(bubble) > 0) {
                    lv_obj_t* text = lv_obj_get_child(bubble, 0);
                    if (text != nullptr) {
                        // 回退到颜色检测逻辑
                        if (lv_color_eq(bg_color, current_theme.system_bubble) ||
                            lv_color_eq(bg_color, DARK_SYSTEM_BUBBLE_COLOR) || 
                            lv_color_eq(bg_color, LIGHT_SYSTEM_BUBBLE_COLOR)) {
                            lv_obj_set_style_text_color(text, current_theme.system_text, 0);
                        } else {
                            lv_obj_set_style_text_color(text, current_theme.text, 0);
                        }
                    }
                }
            }
        }
#else
        // Simple UI mode - just update the main chat message
        if (chat_message_label_ != nullptr) {
            lv_obj_set_style_text_color(chat_message_label_, current_theme.text, 0);
        }
#endif
    }
    
    // Update low battery popup
    if (low_battery_popup_ != nullptr) {
        lv_obj_set_style_bg_color(low_battery_popup_, current_theme.low_battery, 0);
    }

    // No errors occurred. Save theme to settings
    Display::SetTheme(theme_name);
}

#if CONFIG_USE_WECHAT_MESSAGE_STYLE
virtual void SetChatMessage(const char* role, const char* content) override; 
#endif  

// 显示更新信息
void LcdDisplayNew::SetUpdateMessage(const char* message) {
    DisplayLockGuard lock(this);
    if (update_info_ == nullptr || update_label_ == nullptr) {
        return;
    }
    
    // 设置更新信息文本
    lv_label_set_text(update_label_, message);
    
    // 显示更新信息区域
    lv_obj_clear_flag(update_info_, LV_OBJ_FLAG_HIDDEN);
    
    // 确保更新信息区域显示在最前面
    lv_obj_move_foreground(update_info_);
}

void LcdDisplayNew::HideUpdateInfo() {
    DisplayLockGuard lock(this);
    if (update_info_ != nullptr) {
        // 隐藏更新信息区域
        lv_obj_add_flag(update_info_, LV_OBJ_FLAG_HIDDEN);
    }
}

#if CONFIG_USE_WECHAT_MESSAGE_STYLE
// 此处实现聊天消息方法，暂不修改
#endif

// 表情标签相关方法实现
void LcdDisplayNew::CreateEmojiLabel() {
    DisplayLockGuard lock(this);
    
    ESP_LOGI(TAG, "Creating emoji label");
    
    // 初始化表情字体
    emoji_font_240_ = font_emoji_240_init();
    if (emoji_font_240_ == nullptr) {
        ESP_LOGE(TAG, "Failed to initialize emoji font");
        return;
    }
    
    // 获取活动屏幕
    lv_obj_t* screen = lv_screen_active();
    
    // 创建表情标签
    emoji_label_ = lv_label_create(screen);
    if (emoji_label_ == nullptr) {
        ESP_LOGE(TAG, "Failed to create emoji label");
        return;
    }
    
    // 设置表情标签位置为屏幕中央
    lv_obj_set_size(emoji_label_, 240, 240);
    lv_obj_set_pos(emoji_label_, (width_ - 240) / 2, (height_ - 240) / 2);
    
    // 设置字体和样式
    lv_obj_set_style_text_font(emoji_label_, emoji_font_240_, 0);
    lv_obj_set_style_text_color(emoji_label_, lv_color_white(), 0);
    lv_obj_set_style_text_align(emoji_label_, LV_TEXT_ALIGN_CENTER, 0);
    
    // 设置完全透明的背景和边框
    lv_obj_set_style_bg_opa(emoji_label_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_opa(emoji_label_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_outline_opa(emoji_label_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_shadow_opa(emoji_label_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_pad_all(emoji_label_, 0, 0);
    lv_obj_set_style_border_width(emoji_label_, 0, 0);
    lv_obj_set_style_outline_width(emoji_label_, 0, 0);
    lv_obj_set_style_shadow_width(emoji_label_, 0, 0);
    
    // 设置标签为顶层（但在状态栏之下）
    lv_obj_move_foreground(emoji_label_);
    if (status_bar_ != nullptr) {
        lv_obj_move_foreground(status_bar_);
    }
    
    // 默认隐藏表情标签
    lv_obj_add_flag(emoji_label_, LV_OBJ_FLAG_HIDDEN);
    
    ESP_LOGI(TAG, "Emoji label created successfully");
}

void LcdDisplayNew::ShowEmoji(const char* emotion) {
    DisplayLockGuard lock(this);
    
    // 确保有表情标签和字体
    if (emoji_label_ == nullptr || emoji_font_240_ == nullptr) {
        ESP_LOGE(TAG, "Emoji label or font not initialized");
        return;
    }
    
    // 表情名称到Unicode的映射表
    struct EmotionMapping {
        const char* name;
        uint32_t unicode;
    };
    
    static const EmotionMapping emotion_map[] = {
        {"neutral", 0x1f636},     // 中性
        {"happy", 0x1f642},       // 快乐
        {"laughing", 0x1f606},    // 大笑
        {"funny", 0x1f602},       // 有趣
        {"sad", 0x1f614},         // 悲伤
        {"angry", 0x1f620},       // 愤怒
        {"crying", 0x1f62d},      // 哭泣
        {"loving", 0x1f60d},      // 爱心
        {"embarrassed", 0x1f633}, // 尴尬
        {"surprised", 0x1f62f},   // 惊讶
        {"shocked", 0x1f631},     // 震惊
        {"thinking", 0x1f914},    // 思考
        {"winking", 0x1f609},     // 眨眼
        {"cool", 0x1f60e},        // 酷
        {"relaxed", 0x1f60c},     // 放松
        {"delicious", 0x1f924},   // 美味
        {"kissy", 0x1f618},       // 亲吻
        {"confident", 0x1f60f},   // 自信
        {"sleepy", 0x1f634},      // 困倦
        {"silly", 0x1f61c},       // 愚蠢
        {"confused", 0x1f644},    // 困惑
    };
    
    // 查找对应的表情Unicode
    uint32_t unicode = 0x1f636; // 默认为中性表情
    for (size_t i = 0; i < sizeof(emotion_map) / sizeof(emotion_map[0]); i++) {
        if (strcmp(emotion, emotion_map[i].name) == 0) {
            unicode = emotion_map[i].unicode;
            break;
        }
    }
    
    // 将Unicode码转换为UTF-8字符串
    char emoji_text[8];
    if (unicode <= 0x7F) {
        emoji_text[0] = (char)unicode;
        emoji_text[1] = '\0';
    } else if (unicode <= 0x7FF) {
        emoji_text[0] = 0xC0 | (unicode >> 6);
        emoji_text[1] = 0x80 | (unicode & 0x3F);
        emoji_text[2] = '\0';
    } else if (unicode <= 0xFFFF) {
        emoji_text[0] = 0xE0 | (unicode >> 12);
        emoji_text[1] = 0x80 | ((unicode >> 6) & 0x3F);
        emoji_text[2] = 0x80 | (unicode & 0x3F);
        emoji_text[3] = '\0';
    } else {
        emoji_text[0] = 0xF0 | (unicode >> 18);
        emoji_text[1] = 0x80 | ((unicode >> 12) & 0x3F);
        emoji_text[2] = 0x80 | ((unicode >> 6) & 0x3F);
        emoji_text[3] = 0x80 | (unicode & 0x3F);
        emoji_text[4] = '\0';
    }
    
    // 设置表情字体和文本
    lv_obj_set_style_text_font(emoji_label_, emoji_font_240_, 0);
    lv_label_set_text(emoji_label_, emoji_text);
    
    // 显示表情标签
    lv_obj_clear_flag(emoji_label_, LV_OBJ_FLAG_HIDDEN);
    
    // 确保表情标签在前面显示
    lv_obj_move_foreground(emoji_label_);
    if (status_bar_ != nullptr) {
        lv_obj_move_foreground(status_bar_);
    }
    
    ESP_LOGI(TAG, "Emoji displayed: %s (0x%X)", emotion, (unsigned int)unicode);
}

void LcdDisplayNew::HideEmoji() {
    DisplayLockGuard lock(this);
    if (emoji_label_ != nullptr) {
        lv_obj_add_flag(emoji_label_, LV_OBJ_FLAG_HIDDEN);
    }
}
