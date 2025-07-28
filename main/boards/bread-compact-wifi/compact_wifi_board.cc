#include "wifi_board.h"
#include "codecs/no_audio_codec.h"
#include "display/oled_display.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "mcp_server.h"
#include "lamp_controller.h"
#include "led/single_led.h"
#include "assets/lang_config.h"

#include <wifi_station.h>
#include <esp_log.h>
#include <driver/i2c_master.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include "touch_button.h"
#include "touch_sensor_lowlevel.h"

#ifdef SH1106
#include <esp_lcd_panel_sh1106.h>
#endif

#define TAG "CompactWifiBoard"

// 触摸通道定义
#define TOUCH_CHANNEL_1        (8)
#define TOUCH_CHANNEL_2        (9)


// 触摸阈值定义
#define LIGHT_TOUCH_THRESHOLD  (0.1)
#define HEAVY_TOUCH_THRESHOLD  (0.4)


LV_FONT_DECLARE(font_puhui_14_1);
LV_FONT_DECLARE(font_awesome_14_1);

class CompactWifiBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t display_i2c_bus_;
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    Display* display_ = nullptr;
    Button boot_button_;
    Button touch_button_;
    Button volume_up_button_;
    Button volume_down_button_;

    // 触摸按钮句柄
    button_handle_t touch_btn_light_1_ = nullptr;
    button_handle_t touch_btn_light_2_ = nullptr;

    static void touch_event_light_1(void *arg, void *data)
    {
        button_handle_t btn_handle = static_cast<button_handle_t>(arg);
        button_event_t event = iot_button_get_event(btn_handle);
        ESP_LOGI(TAG, "Light Button 1: %s", iot_button_get_event_str(event));
    }

    static void touch_event_light_2(void *arg, void *data)
    {
        button_handle_t btn_handle = static_cast<button_handle_t>(arg);
        button_event_t event = iot_button_get_event(btn_handle);
        ESP_LOGI(TAG, "Light Button 2: %s", iot_button_get_event_str(event));
    }

    void InitializeDisplayI2c() {
        i2c_master_bus_config_t bus_config = {
            .i2c_port = (i2c_port_t)0,
            .sda_io_num = DISPLAY_SDA_PIN,
            .scl_io_num = DISPLAY_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &display_i2c_bus_));
    }

    void InitializeSsd1306Display() {
        // SSD1306 config
        esp_lcd_panel_io_i2c_config_t io_config = {
            .dev_addr = 0x3C,
            .on_color_trans_done = nullptr,
            .user_ctx = nullptr,
            .control_phase_bytes = 1,
            .dc_bit_offset = 6,
            .lcd_cmd_bits = 8,
            .lcd_param_bits = 8,
            .flags = {
                .dc_low_on_data = 0,
                .disable_control_phase = 0,
            },
            .scl_speed_hz = 400 * 1000,
        };

        ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c_v2(display_i2c_bus_, &io_config, &panel_io_));

        ESP_LOGI(TAG, "Install SSD1306 driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = -1;
        panel_config.bits_per_pixel = 1;

        esp_lcd_panel_ssd1306_config_t ssd1306_config = {
            .height = static_cast<uint8_t>(DISPLAY_HEIGHT),
        };
        panel_config.vendor_config = &ssd1306_config;

#ifdef SH1106
        ESP_ERROR_CHECK(esp_lcd_new_panel_sh1106(panel_io_, &panel_config, &panel_));
#else
        ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(panel_io_, &panel_config, &panel_));
#endif
        ESP_LOGI(TAG, "SSD1306 driver installed");

        // Reset the display
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
        if (esp_lcd_panel_init(panel_) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize display");
            display_ = new NoDisplay();
            return;
        }
        ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_, false));

        // Set the display to on
        ESP_LOGI(TAG, "Turning display on");
        ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_, true));

        display_ = new OledDisplay(panel_io_, panel_, DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y,
            {&font_puhui_14_1, &font_awesome_14_1});
    }

    // 初始化触摸按钮
    void InitializeTouchButtons() {
        ESP_LOGI(TAG, "开始初始化触摸按钮");
        
        // 注册所有触摸通道
        uint32_t touch_channel_list[] = {TOUCH_CHANNEL_1, TOUCH_CHANNEL_2};
        uint32_t total_channel_num = sizeof(touch_channel_list) / sizeof(touch_channel_list[0]);
        
        ESP_LOGI(TAG, "触摸通道: IO%d, IO%d", TOUCH_CHANNEL_1, TOUCH_CHANNEL_2);

        // 为每个按钮分配通道类型
        touch_lowlevel_type_t *channel_type = (touch_lowlevel_type_t*)calloc(total_channel_num, sizeof(touch_lowlevel_type_t));
        if (channel_type == NULL) {
            ESP_LOGE(TAG, "内存分配失败");
            return;
        }
        
        for (uint32_t i = 0; i < total_channel_num; i++) {
            channel_type[i] = TOUCH_LOWLEVEL_TYPE_TOUCH;
        }

        touch_lowlevel_config_t low_config = {
            .channel_num = total_channel_num,
            .channel_list = touch_channel_list,
            .channel_type = channel_type,
        };
        
        esp_err_t ret = touch_sensor_lowlevel_create(&low_config);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "触摸传感器底层创建失败: %d", ret);
            free(channel_type);
            return;
        }
        
        ESP_LOGI(TAG, "触摸传感器底层创建成功");
        free(channel_type);

        // 按钮配置
        const button_config_t btn_cfg = {
            .long_press_time = 2000,
            .short_press_time = 300,
        };

        // 配置触摸按钮 1 
        button_touch_config_t touch_cfg_1 = {
            .touch_channel = static_cast<int32_t>(touch_channel_list[0]),
            .channel_threshold = LIGHT_TOUCH_THRESHOLD,
            .skip_lowlevel_init = true,
        };
        
        ret = iot_button_new_touch_button_device(&btn_cfg, &touch_cfg_1, &touch_btn_light_1_);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "创建触摸按钮1失败: %d", ret);
            return;
        }
        ESP_LOGI(TAG, "触摸按钮1创建成功");

        // 配置触摸按钮 2 
        button_touch_config_t touch_cfg_2 = {
            .touch_channel = static_cast<int32_t>(touch_channel_list[1]),
            .channel_threshold = LIGHT_TOUCH_THRESHOLD,
            .skip_lowlevel_init = true,
        };
        
        ret = iot_button_new_touch_button_device(&btn_cfg, &touch_cfg_2, &touch_btn_light_2_);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "创建触摸按钮2失败: %d", ret);
            return;
        }
        ESP_LOGI(TAG, "触摸按钮2创建成功");

        iot_button_register_cb(touch_btn_light_1_, BUTTON_PRESS_DOWN, NULL, touch_event_light_1, NULL);
        iot_button_register_cb(touch_btn_light_2_, BUTTON_PRESS_DOWN, NULL, touch_event_light_2, NULL);

        // 启动触摸传感器
        ret = touch_sensor_lowlevel_start();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "触摸传感器启动失败: %d", ret);
            return;
        }
        
        ESP_LOGI(TAG, "触摸按钮初始化完成");
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            app.ToggleChatState();
        });
        touch_button_.OnPressDown([this]() {
            Application::GetInstance().StartListening();
        });
        touch_button_.OnPressUp([this]() {
            Application::GetInstance().StopListening();
        });

        volume_up_button_.OnClick([this]() {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() + 10;
            if (volume > 100) {
                volume = 100;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });

        volume_up_button_.OnLongPress([this]() {
            GetAudioCodec()->SetOutputVolume(100);
            GetDisplay()->ShowNotification(Lang::Strings::MAX_VOLUME);
        });

        volume_down_button_.OnClick([this]() {
            auto codec = GetAudioCodec();
            auto volume = codec->output_volume() - 10;
            if (volume < 0) {
                volume = 0;
            }
            codec->SetOutputVolume(volume);
            GetDisplay()->ShowNotification(Lang::Strings::VOLUME + std::to_string(volume));
        });

        volume_down_button_.OnLongPress([this]() {
            GetAudioCodec()->SetOutputVolume(0);
            GetDisplay()->ShowNotification(Lang::Strings::MUTED);
        });
    }

    // 物联网初始化，逐步迁移到 MCP 协议
    void InitializeTools() {
        static LampController lamp(LAMP_GPIO);
    }

public:
    CompactWifiBoard() :
        boot_button_(BOOT_BUTTON_GPIO),
        touch_button_(TOUCH_BUTTON_GPIO),
        volume_up_button_(VOLUME_UP_BUTTON_GPIO),
        volume_down_button_(VOLUME_DOWN_BUTTON_GPIO) {
        InitializeDisplayI2c();
        InitializeSsd1306Display();
        InitializeButtons();
        InitializeTouchButtons();
        InitializeTools();
    }

    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
#ifdef AUDIO_I2S_METHOD_SIMPLEX
        static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT, AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
#else
        static NoAudioCodecDuplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN);
#endif
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }
};

DECLARE_BOARD(CompactWifiBoard);
