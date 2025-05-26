#include "iot/thing.h"
#include "board.h"
#include "display/lcd_display.h"
#include "settings.h"

#include <esp_log.h>
#include <string>

#define TAG "Screen"

namespace iot {

// 这里仅定义 Screen 的属性和方法，不包含具体的实现
class Screen : public Thing {
public:
    Screen() : Thing("Screen", "A screen that can set theme and brightness") {
        // 定义设备的属性
        properties_.AddStringProperty("theme", "Current theme", [this]() -> std::string {
            auto theme = Board::GetInstance().GetDisplay()->GetTheme();
            return theme;
        });

        properties_.AddNumberProperty("brightness", "Current brightness percentage", [this]() -> int {
            // 这里可以添加获取当前亮度的逻辑
            auto backlight = Board::GetInstance().GetBacklight();
            return backlight ? backlight->brightness() : 100;
        });

        // 定义设备可以被远程执行的指令
        methods_.AddMethod("set_theme", "Set the screen theme", ParameterList({
            Parameter("theme_name", "Valid string values are 'light' and 'dark'", kValueTypeString, true)
        }), [this](const ParameterList& parameters) {
            std::string theme_name = static_cast<std::string>(parameters["theme_name"].string());
            auto display = Board::GetInstance().GetDisplay();
            if (display) {
                display->SetTheme(theme_name);
            }
        });
        
        methods_.AddMethod("set_brightness", "Set the brightness", ParameterList({
            Parameter("brightness", "An integer between 0 and 100", kValueTypeNumber, true)
        }), [this](const ParameterList& parameters) {
            uint8_t brightness = static_cast<uint8_t>(parameters["brightness"].number());
            auto backlight = Board::GetInstance().GetBacklight();
            if (backlight) {
                backlight->SetBrightness(brightness, true);
            }
        });

        // 添加通用切换场景方法，支持参数指定场景索引
        methods_.AddMethod("SwitchSceneWithIndex", "切换到指定场景", ParameterList({
            Parameter("scene_index", "场景索引(0:场景1, 1:场景2)", kValueTypeNumber, true)
        }), [this](const ParameterList& parameters) {
            int scene_index = static_cast<int>(parameters["scene_index"].number());
            ESP_LOGI(TAG, "执行场景切换到索引 %d...", scene_index);
            
            // 获取Board实例并调用带参数的SwitchScene方法
            auto& board = Board::GetInstance();
            board.SwitchScene(scene_index);
            
            ESP_LOGI(TAG, "场景切换请求已完成");
        });

        // 添加切换到背景1图片集的方法
        methods_.AddMethod("SwitchToXinyi", "我想新一了", ParameterList(), [this](const ParameterList& parameters) {
            ESP_LOGI(TAG, "执行场景切换到背景1图片集...");
            
            // 获取Board实例并调用SwitchScene方法，参数0表示背景1图片集
            auto& board = Board::GetInstance();
            board.SwitchScene(0);
            
            ESP_LOGI(TAG, "场景切换请求已完成");
        });

        // 添加切换到背景2图片集的方法
        methods_.AddMethod("SwitchToPanda", "回家", ParameterList(), [this](const ParameterList& parameters) {
            ESP_LOGI(TAG, "执行场景切换到背景1图片集...");
            
            // 获取Board实例并调用SwitchScene方法，参数1表示背景2图片集
            auto& board = Board::GetInstance();
            board.SwitchScene(1);
            
            ESP_LOGI(TAG, "场景切换请求已完成");
        });

        // 添加切换到机器人图片集的方法
        // methods_.AddMethod("SwitchToRobot", "一起去玩", ParameterList(), [this](const ParameterList& parameters) {
        //     ESP_LOGI(TAG, "执行场景切换到机器人图片集...");
            
        //     // 获取Board实例并调用SwitchScene方法，参数2表示机器人图片集
        //     auto& board = Board::GetInstance();
        //     board.SwitchScene(2);
            
        //     ESP_LOGI(TAG, "场景切换请求已完成");
        // });
    }
};

} // namespace iot

DECLARE_THING(Screen);