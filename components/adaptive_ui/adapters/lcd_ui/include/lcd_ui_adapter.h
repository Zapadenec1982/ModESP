/**
 * @file lcd_ui_adapter.h
 * @brief LCD display with buttons UI adapter
 */

#ifndef LCD_UI_ADAPTER_H
#define LCD_UI_ADAPTER_H

#include "ui_adapter_base.h"
#include <vector>
#include <functional>

/**
 * @brief LCD UI Adapter for physical display interface
 * 
 * Features:
 * - Auto-generated pages from module schemas
 * - Navigation with buttons
 * - Real-time value updates
 * - Menu system for settings
 */
class LCDUIAdapter : public UIAdapterBase {
public:
    LCDUIAdapter();
    ~LCDUIAdapter() override;
    
    // BaseModule interface
    const char* get_name() const override { return "LCD_UI"; }
    void configure(const nlohmann::json& config) override;
    void stop() override;
    bool is_healthy() const override;
    uint8_t get_health_score() const override;
    uint32_t get_max_update_time_us() const override { return 50000; }
    
    // Button events
    enum class Button {
        UP,
        DOWN,
        ENTER,
        BACK
    };
    
protected:
    // UIAdapterBase interface
    void register_ui_handlers() override;
    
private:
    // Configuration
    struct Config {
        bool enabled = true;
        std::string type = "i2c_20x4";  // or "spi_128x64"
        uint8_t i2c_addr = 0x27;
        int backlight_timeout_s = 30;
        bool auto_scroll = true;
        int update_interval_ms = 500;
    } config_;
    
    // Display interface
    void* display_handle_ = nullptr;
    uint8_t cols_ = 20;
    uint8_t rows_ = 4;
    
    // Page system
    struct Page {
        std::string title;
        std::vector<std::string> lines;
        std::function<void()> update_func;
        std::function<void(Button)> button_handler;
        bool needs_update = true;
    };
    
    std::vector<Page> pages_;
    size_t current_page_ = 0;
    
    // Menu system
    struct MenuItem {
        std::string label;
        std::function<void()> action;
        std::vector<MenuItem> submenu;
    };
    
    std::vector<MenuItem> main_menu_;
    bool in_menu_ = false;
    size_t menu_position_ = 0;
    
    // Display operations
    void init_display();
    void clear();
    void print(uint8_t col, uint8_t row, const std::string& text);
    void set_backlight(bool on);
    
    // Page generation
    void create_pages_from_schemas();
    void create_module_page(const std::string& module, const nlohmann::json& schema);
    void create_overview_page();
    void create_menu_page();
    
    // Page templates
    Page create_sensor_page(const std::string& module, const nlohmann::json& controls);
    Page create_actuator_page(const std::string& module, const nlohmann::json& controls);
    Page create_settings_page(const std::string& module, const nlohmann::json& controls);
    
    // Navigation
    void next_page();
    void previous_page();
    void enter_menu();
    void exit_menu();
    void handle_button(Button btn);
    
    // Display update
    void render_current_page();
    void render_menu();
    std::string format_value(const nlohmann::json& value, const std::string& unit = "");
    std::string truncate(const std::string& text, size_t max_len);
    
    // Auto-scroll for long lists
    struct ScrollState {
        size_t offset = 0;
        size_t total_items = 0;
        int64_t last_scroll_time = 0;
    };
    std::map<size_t, ScrollState> scroll_states_;
    
    // Backlight management
    int64_t last_activity_time_;
    void update_backlight();
    
    // Value caching for display
    std::map<std::string, nlohmann::json> cached_values_;
    void update_cached_value(const std::string& key, const nlohmann::json& value);
};

#endif // LCD_UI_ADAPTER_H