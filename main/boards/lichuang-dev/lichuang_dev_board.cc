#include "wifi_board.h"
#include "audio_codecs/box_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "i2c_device.h"
#include "iot/thing_manager.h"
#include "audio_processing/audio_processor.h"

#include <esp_log.h>
#include <esp_lcd_panel_vendor.h>
#include <driver/i2c_master.h>
#include <driver/spi_common.h>
#include <wifi_station.h>
#include "esp_camera.h" 

// youyong2文件夹中的图片
#include "images/youyong2/gImage_youyong2_0001.h"
#include "images/youyong2/gImage_youyong2_0002.h"
#include "images/youyong2/gImage_youyong2_0003.h"
#include "images/youyong2/gImage_youyong2_0004.h"
#include "images/youyong2/gImage_youyong2_0005.h"
#include "images/youyong2/gImage_youyong2_0006.h"
#include "images/youyong2/gImage_youyong2_0007.h"
#include "images/youyong2/gImage_youyong2_0008.h"
#include "images/youyong2/gImage_youyong2_0009.h"
#include "images/youyong2/gImage_youyong2_0010.h"
#include "images/youyong2/gImage_youyong2_0011.h"
#include "images/youyong2/gImage_youyong2_0012.h"

// youyong3文件夹中的图片
#include "images/youyong3/gImage_youyong3_0001.h"
#include "images/youyong3/gImage_youyong3_0002.h"
#include "images/youyong3/gImage_youyong3_0003.h"
#include "images/youyong3/gImage_youyong3_0004.h"
#include "images/youyong3/gImage_youyong3_0005.h"
#include "images/youyong3/gImage_youyong3_0006.h"

#define TAG "LichuangDevBoard"

LV_FONT_DECLARE(font_puhui_20_4);
LV_FONT_DECLARE(font_awesome_20_4);

class Pca9557 : public I2cDevice {
public:
    Pca9557(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {
        WriteReg(0x01, 0x03);
        WriteReg(0x03, 0xf8);
    }

    void SetOutputState(uint8_t bit, uint8_t level) {
        uint8_t data = ReadReg(0x01);
        data = (data & ~(1 << bit)) | (level << bit);
        WriteReg(0x01, data);
    }
};


class LichuangDevBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    i2c_master_dev_handle_t pca9557_handle_;
    Button boot_button_;
    LcdDisplay* display_;
    Pca9557* pca9557_;
    TaskHandle_t image_task_handle_ = nullptr; // 图片显示任务句柄
    
    void InitializeI2c() {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = (i2c_port_t)0,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));

        // Initialize PCA9557
        pca9557_ = new Pca9557(i2c_bus_, 0x19);
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.mosi_io_num = GPIO_NUM_40;
        buscfg.miso_io_num = GPIO_NUM_NC;
        buscfg.sclk_io_num = GPIO_NUM_41;
        buscfg.quadwp_io_num = GPIO_NUM_NC;
        buscfg.quadhd_io_num = GPIO_NUM_NC;
        buscfg.max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            app.ToggleChatState();
        });
    }

    void InitializeSt7789Display() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;
        // 液晶屏控制IO初始化
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = GPIO_NUM_NC;
        io_config.dc_gpio_num = GPIO_NUM_39;
        io_config.spi_mode = 2;
        io_config.pclk_hz = 80 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &panel_io));

        // 初始化液晶屏驱动芯片ST7789
        ESP_LOGD(TAG, "Install LCD driver");
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = GPIO_NUM_NC;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));
        
        esp_lcd_panel_reset(panel);
        pca9557_->SetOutputState(0, 0);

        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, true);
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        display_ = new SpiLcdDisplay(panel_io, panel,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY,
                                    {
                                        .text_font = &font_puhui_20_4,
                                        .icon_font = &font_awesome_20_4,
#if CONFIG_USE_WECHAT_MESSAGE_STYLE
                                        .emoji_font = font_emoji_32_init(),
#else
                                        .emoji_font = font_emoji_64_init(),
#endif
                                    });
    }


    // 摄像头硬件初始化
    void InitializeCamera(void)
    {

        camera_config_t config;
        config.ledc_channel = LEDC_CHANNEL_1;  // LEDC通道选择  用于生成XCLK时钟 但是S3不用
        config.ledc_timer = LEDC_TIMER_1; // LEDC timer选择  用于生成XCLK时钟 但是S3不用
        config.pin_d0 = CAMERA_PIN_D0;
        config.pin_d1 = CAMERA_PIN_D1;
        config.pin_d2 = CAMERA_PIN_D2;
        config.pin_d3 = CAMERA_PIN_D3;
        config.pin_d4 = CAMERA_PIN_D4;
        config.pin_d5 = CAMERA_PIN_D5;
        config.pin_d6 = CAMERA_PIN_D6;
        config.pin_d7 = CAMERA_PIN_D7;
        config.pin_xclk = CAMERA_PIN_XCLK;
        config.pin_pclk = CAMERA_PIN_PCLK;
        config.pin_vsync = CAMERA_PIN_VSYNC;
        config.pin_href = CAMERA_PIN_HREF;
        config.pin_sccb_sda = -1;   // 这里写-1 表示使用已经初始化的I2C接口
        config.pin_sccb_scl = CAMERA_PIN_SIOC;
        config.sccb_i2c_port = 0;
        config.pin_pwdn = CAMERA_PIN_PWDN;
        config.pin_reset = CAMERA_PIN_RESET;
        config.xclk_freq_hz = XCLK_FREQ_HZ;
        config.pixel_format = PIXFORMAT_RGB565;
        config.frame_size = FRAMESIZE_QVGA;
        config.jpeg_quality = 12;
        config.fb_count = 2;
        config.fb_location = CAMERA_FB_IN_PSRAM;
        config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

        // camera init
        esp_err_t err = esp_camera_init(&config); // 配置上面定义的参数
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Camera init failed with error 0x%x", err);
            return;
        }

        sensor_t *s = esp_camera_sensor_get(); // 获取摄像头型号

        if (s->id.PID == GC0308_PID) {
            s->set_hmirror(s, 1);  // 这里控制摄像头镜像 写1镜像 写0不镜像
        }
    }

    // 物联网初始化，添加对 AI 可见设备
    void InitializeIot() {
        auto& thing_manager = iot::ThingManager::GetInstance();
        thing_manager.AddThing(iot::CreateThing("Speaker"));
        thing_manager.AddThing(iot::CreateThing("Screen"));
        thing_manager.AddThing(iot::CreateThing("Camera"));

    }
    
    // 启动图片循环显示任务
    void StartImageSlideshow() {
        xTaskCreate(ImageSlideshowTask, "img_slideshow", 4096, this, 3, &image_task_handle_);
        ESP_LOGI(TAG, "图片循环显示任务已启动");
    }
    
    // 图片循环显示任务函数
    static void ImageSlideshowTask(void* arg) {
        LichuangDevBoard* board = static_cast<LichuangDevBoard*>(arg);
        Display* display = board->GetDisplay();
        
        if (!display) {
            ESP_LOGE(TAG, "无法获取显示设备");
            vTaskDelete(NULL);
            return;
        }
        
        // 获取Application实例
        auto& app = Application::GetInstance();
        
        // 创建画布（如果不存在）
        if (!display->HasCanvas()) {
            display->CreateCanvas();
        }
        
        // 设置图片显示参数
        int imgWidth = 320;
        int imgHeight = 240;
        int x = 0;
        int y = 0;
        
        // 设置youyong2图片数组 - 用于开始和结束
        const uint8_t* youyong2ImageArray[] = {
            gImage_youyong2_0001,
            gImage_youyong2_0002,
            gImage_youyong2_0003,
            gImage_youyong2_0004,
            gImage_youyong2_0005,
            gImage_youyong2_0006,
            gImage_youyong2_0007,
            gImage_youyong2_0008,
            gImage_youyong2_0009,
            gImage_youyong2_0010,
            gImage_youyong2_0011,
            gImage_youyong2_0012,
        };
        const int youyong2ImageCount = sizeof(youyong2ImageArray) / sizeof(youyong2ImageArray[0]);
        
        // 设置youyong3图片数组 - 用于中间过程
        const uint8_t* youyong3ImageArray[] = {
            gImage_youyong3_0001,
            gImage_youyong3_0002,
            gImage_youyong3_0003,
            gImage_youyong3_0004,
            gImage_youyong3_0005,
            gImage_youyong3_0006,
            gImage_youyong3_0005,
            gImage_youyong3_0004,
            gImage_youyong3_0003,
            gImage_youyong3_0002,
            gImage_youyong3_0001,
        };
        const int youyong3ImageCount = sizeof(youyong3ImageArray) / sizeof(youyong3ImageArray[0]);
        
        // 创建临时缓冲区用于字节序转换
        uint16_t* convertedData = new uint16_t[imgWidth * imgHeight];
        if (!convertedData) {
            ESP_LOGE(TAG, "无法分配内存进行图像转换");
            vTaskDelete(NULL);
            return;
        }
        
        // 先显示youyong2的第一张图片
        const uint8_t* currentImage = youyong2ImageArray[0];
        
        // 转换并显示第一张图片
        for (int i = 0; i < imgWidth * imgHeight; i++) {
            uint16_t pixel = ((uint16_t*)currentImage)[i];
            convertedData[i] = ((pixel & 0xFF) << 8) | ((pixel & 0xFF00) >> 8);
        }
        display->DrawImageOnCanvas(x, y, imgWidth, imgHeight, (const uint8_t*)convertedData);
        ESP_LOGI(TAG, "初始显示youyong2第一张图片");
        
        // 持续监控和处理图片显示
        TickType_t lastUpdateTime = xTaskGetTickCount();
        const TickType_t cycleInterval = pdMS_TO_TICKS(50); // 图片切换间隔50毫秒
        
        // 定义用于判断设备状态的变量
        DeviceState prevDeviceState = kDeviceStateIdle;
        DeviceState currentDeviceState = kDeviceStateIdle;
        
        // 初始化状态变量
        bool isShowingYouyong2Intro = false;  // 是否正在显示youyong2开场动画
        bool hasFinishedYouyong2Intro = false; // 是否已完成youyong2开场动画
        bool isShowingYouyong3 = false;       // 是否正在显示youyong3
        bool isShowingYouyong2Outro = false;  // 是否正在显示youyong2结束动画
        
        // 当前播放的图片索引
        int youyong2IntroIndex = 0;  // youyong2开场动画索引
        int youyong3Index = 0;       // youyong3索引
        int youyong2OutroIndex = 0;  // youyong2结束动画索引
        
        while (true) {
            // 获取当前设备状态
            prevDeviceState = currentDeviceState;
            currentDeviceState = app.GetDeviceState();
            
            // 当设备状态从非"听"非"说"状态变为"听"状态时，才开始播放youyong2开场动画
            if (currentDeviceState == kDeviceStateListening && 
                prevDeviceState != kDeviceStateListening && 
                prevDeviceState != kDeviceStateSpeaking) {
                isShowingYouyong2Intro = true;
                hasFinishedYouyong2Intro = false;
                isShowingYouyong3 = false;
                isShowingYouyong2Outro = false;
                youyong2IntroIndex = 0;
                ESP_LOGI(TAG, "开始播放youyong2开场动画（从非对话状态进入听状态）");
            }
            
            // 当从"说"状态变为"听"状态时，继续播放youyong3
            if (prevDeviceState == kDeviceStateSpeaking && currentDeviceState == kDeviceStateListening) {
                isShowingYouyong2Intro = false;
                hasFinishedYouyong2Intro = true;
                isShowingYouyong3 = true;
                isShowingYouyong2Outro = false;
                ESP_LOGI(TAG, "从说变成听，继续播放youyong3");
            }
            
            // 当设备状态从"说"或"听"状态变为非"听"非"说"状态时，开始播放youyong2结束动画
            if ((prevDeviceState == kDeviceStateSpeaking || prevDeviceState == kDeviceStateListening) && 
                currentDeviceState != kDeviceStateSpeaking && 
                currentDeviceState != kDeviceStateListening) {
                isShowingYouyong2Intro = false;
                hasFinishedYouyong2Intro = true;
                isShowingYouyong3 = false;
                isShowingYouyong2Outro = true;
                youyong2OutroIndex = youyong2ImageCount - 1; // 从最后一张开始倒序播放
                ESP_LOGI(TAG, "开始播放youyong2结束动画（从说或听状态退出到非听非说状态）");
            }
            
            TickType_t currentTime = xTaskGetTickCount();
            
            // 如果时间到了切换间隔
            if (currentTime - lastUpdateTime >= cycleInterval) {
                // 更新上次更新时间
                lastUpdateTime = currentTime;
                
                // 根据当前状态进行图片切换
                if (isShowingYouyong2Intro) {
                    // 播放youyong2开场动画
                    currentImage = youyong2ImageArray[youyong2IntroIndex];
                    
                    // 转换并显示图片
                    for (int i = 0; i < imgWidth * imgHeight; i++) {
                        uint16_t pixel = ((uint16_t*)currentImage)[i];
                        convertedData[i] = ((pixel & 0xFF) << 8) | ((pixel & 0xFF00) >> 8);
                    }
                    display->DrawImageOnCanvas(x, y, imgWidth, imgHeight, (const uint8_t*)convertedData);
                    ESP_LOGI(TAG, "显示youyong2开场图片 %d", youyong2IntroIndex);
                    
                    // 更新索引
                    youyong2IntroIndex++;
                    if (youyong2IntroIndex >= youyong2ImageCount) {
                        // 如果已经播放完youyong2的所有图片，切换到youyong3
                        isShowingYouyong2Intro = false;
                        hasFinishedYouyong2Intro = true;
                        isShowingYouyong3 = true;
                        youyong3Index = 0;
                        ESP_LOGI(TAG, "youyong2开场动画播放完毕，切换到youyong3");
                    }
                } else if (isShowingYouyong3 && (currentDeviceState == kDeviceStateListening || currentDeviceState == kDeviceStateSpeaking)) {
                    // 当设备在聆听或说话状态时，播放youyong3
                    currentImage = youyong3ImageArray[youyong3Index];
                    
                    // 转换并显示图片
                    for (int i = 0; i < imgWidth * imgHeight; i++) {
                        uint16_t pixel = ((uint16_t*)currentImage)[i];
                        convertedData[i] = ((pixel & 0xFF) << 8) | ((pixel & 0xFF00) >> 8);
                    }
                    display->DrawImageOnCanvas(x, y, imgWidth, imgHeight, (const uint8_t*)convertedData);
                    ESP_LOGI(TAG, "显示youyong3图片 %d（状态：%d）", youyong3Index, currentDeviceState);
                    
                    // 更新索引，循环播放youyong3
                    youyong3Index = (youyong3Index + 1) % youyong3ImageCount;
                    
                    // 延长youyong3帧间隔，减慢播放速度
                    lastUpdateTime += pdMS_TO_TICKS(250); // 增加额外250ms的延迟
                } else if (isShowingYouyong2Outro) {
                    // 播放youyong2结束动画
                    currentImage = youyong2ImageArray[youyong2OutroIndex];
                    
                    // 转换并显示图片
                    for (int i = 0; i < imgWidth * imgHeight; i++) {
                        uint16_t pixel = ((uint16_t*)currentImage)[i];
                        convertedData[i] = ((pixel & 0xFF) << 8) | ((pixel & 0xFF00) >> 8);
                    }
                    display->DrawImageOnCanvas(x, y, imgWidth, imgHeight, (const uint8_t*)convertedData);
                    ESP_LOGI(TAG, "显示youyong2结束图片 %d", youyong2OutroIndex);
                    
                    // 更新索引，倒序播放
                    youyong2OutroIndex--;
                    if (youyong2OutroIndex < 0) {
                        // 如果已经播放完youyong2的所有图片，恢复到初始状态
                        isShowingYouyong2Outro = false;
                        
                        // 显示第一张图片作为默认状态
                        currentImage = youyong2ImageArray[0];
                        for (int i = 0; i < imgWidth * imgHeight; i++) {
                            uint16_t pixel = ((uint16_t*)currentImage)[i];
                            convertedData[i] = ((pixel & 0xFF) << 8) | ((pixel & 0xFF00) >> 8);
                        }
                        display->DrawImageOnCanvas(x, y, imgWidth, imgHeight, (const uint8_t*)convertedData);
                        ESP_LOGI(TAG, "youyong2结束动画播放完毕，恢复到初始状态");
                    }
                } else if (!isShowingYouyong2Intro && !isShowingYouyong3 && !isShowingYouyong2Outro) {
                    // 显示youyong2的第一张图片作为默认状态
                    currentImage = youyong2ImageArray[0];
                    for (int i = 0; i < imgWidth * imgHeight; i++) {
                        uint16_t pixel = ((uint16_t*)currentImage)[i];
                        convertedData[i] = ((pixel & 0xFF) << 8) | ((pixel & 0xFF00) >> 8);
                    }
                    display->DrawImageOnCanvas(x, y, imgWidth, imgHeight, (const uint8_t*)convertedData);
                }
            }
            
            // 短暂延时，避免CPU占用过高
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        
        // 释放资源（实际上不会执行到这里，除非任务被外部终止）
        delete[] convertedData;
        vTaskDelete(NULL);
    }

public:
    LichuangDevBoard() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeI2c();
        InitializeSpi();
        InitializeSt7789Display();
        InitializeButtons();
        InitializeCamera();
        InitializeIot();
        GetBacklight()->RestoreBrightness();
        
        // 启动图片循环显示任务
        StartImageSlideshow();
    }

    virtual AudioCodec* GetAudioCodec() override {
        static BoxAudioCodec audio_codec(
            i2c_bus_, 
            AUDIO_INPUT_SAMPLE_RATE, 
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, 
            AUDIO_I2S_GPIO_BCLK, 
            AUDIO_I2S_GPIO_WS, 
            AUDIO_I2S_GPIO_DOUT, 
            AUDIO_I2S_GPIO_DIN,
            GPIO_NUM_NC, 
            AUDIO_CODEC_ES8311_ADDR, 
            AUDIO_CODEC_ES7210_ADDR, 
            AUDIO_INPUT_REFERENCE);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }
    
    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }
};

DECLARE_BOARD(LichuangDevBoard);
