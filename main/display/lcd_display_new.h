#ifndef LCD_DISPLAY_H
#define LCD_DISPLAY_H

#include "display.h"

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <font_emoji.h>

#include <atomic>

class LcdDisplayNew : public Display {
protected:
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    
    lv_draw_buf_t draw_buf_;
    lv_obj_t* status_bar_ = nullptr;
    lv_obj_t* content_ = nullptr;
    lv_obj_t* container_ = nullptr;
    lv_obj_t* side_bar_ = nullptr;
    lv_obj_t* update_info_ = nullptr; // 升级信息区域
    lv_obj_t* update_label_ = nullptr; // 升级信息标签

    // 表情显示标签
    lv_obj_t* emoji_label_ = nullptr;
    const lv_font_t* emoji_font_240_ = nullptr;

    DisplayFonts fonts_;

    void SetupUI();
    virtual bool Lock(int timeout_ms = 0) override;
    virtual void Unlock() override;

    // 表情标签相关方法
    void CreateEmojiLabel();

protected:
    // 添加protected构造函数
    LcdDisplayNew(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel, DisplayFonts fonts)
        : panel_io_(panel_io), panel_(panel), fonts_(fonts) {}
    
public:
    ~LcdDisplayNew();
    virtual void SetEmotion(const char* emotion) override;
    virtual void SetIcon(const char* icon) override;
#if CONFIG_USE_WECHAT_MESSAGE_STYLE
    virtual void SetChatMessage(const char* role, const char* content) override; 
#endif  

    // 控制底部栏显示和隐藏
    virtual void ShowSideBar() override;
    virtual void HideSideBar() override;

    // 显示更新信息
    virtual void SetUpdateMessage(const char* message) override;
    
    // 隐藏更新信息区域
    virtual void HideUpdateInfo() override;

    // Add theme switching function
    virtual void SetTheme(const std::string& theme_name) override;

    // 表情相关公共方法
    virtual void ShowEmoji(const char* emotion);
    virtual void HideEmoji();
};

// RGB LCD显示器
class RgbLcdDisplayNew : public LcdDisplayNew {
public:
    RgbLcdDisplayNew(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                  int width, int height, int offset_x, int offset_y,
                  bool mirror_x, bool mirror_y, bool swap_xy,
                  DisplayFonts fonts);
};

// MIPI LCD显示器
class MipiLcdDisplayNew : public LcdDisplayNew {
public:
    MipiLcdDisplayNew(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                   int width, int height, int offset_x, int offset_y,
                   bool mirror_x, bool mirror_y, bool swap_xy,
                   DisplayFonts fonts);
};

// // SPI LCD显示器
class SpiLcdDisplayNew : public LcdDisplayNew {
public:
    SpiLcdDisplayNew(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                  int width, int height, int offset_x, int offset_y,
                  bool mirror_x, bool mirror_y, bool swap_xy,
                  DisplayFonts fonts);
};

// QSPI LCD显示器
class QspiLcdDisplayNew : public LcdDisplayNew {
public:
    QspiLcdDisplayNew(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                   int width, int height, int offset_x, int offset_y,
                   bool mirror_x, bool mirror_y, bool swap_xy,
                   DisplayFonts fonts);
};

// MCU8080 LCD显示器
class Mcu8080LcdDisplayNew : public LcdDisplayNew {
public:
    Mcu8080LcdDisplayNew(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                      int width, int height, int offset_x, int offset_y,
                      bool mirror_x, bool mirror_y, bool swap_xy,
                      DisplayFonts fonts);
};
#endif // LCD_DISPLAY_H
 