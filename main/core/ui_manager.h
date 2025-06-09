#pragma once

#include <string>
#include <unordered_map>
#include <functional>
#include <vector>
#include <nlohmann/json.hpp>
#include "base_module.h"

namespace ModuChill {

using json = nlohmann::json;

/**
 * UI element types
 */
enum class UIElementType {
    PAGE,
    GROUP,
    LABEL,
    VALUE,
    INPUT,
    BUTTON,
    TOGGLE,
    SLIDER,
    SELECT,
    CHART,
    STATUS,
    ALERT
};

/**
 * UI command from user interface
 */
struct UICommand {
    std::string source;      // Interface that sent command (web, display, etc)
    std::string target;      // Target element or module
    std::string action;      // Action to perform
    json parameters;         // Action parameters
    uint32_t timestamp;
};

/**
 * UI element descriptor
 */
struct UIElement {
    std::string id;
    UIElementType type;
    std::string label;
    json properties;         // Element-specific properties
    json constraints;        // Validation constraints
    std::string data_source; // SharedState key or computed value
    bool visible;
    bool enabled;
};

/**
 * UI page descriptor
 */
struct UIPage {
    std::string id;
    std::string title;
    std::string icon;
    std::vector<UIElement> elements;
    json layout;             // Layout hints for renderers
    uint32_t refresh_rate;   // Suggested refresh rate in ms
};

/**
 * UIManager - Central UI coordinator
 * 
 * Generates UI schemas for different interfaces (web, display, etc).
 * Handles user commands and updates configuration/state accordingly.
 */
class UIManager : public BaseModule {
private:
    // UI pages registry
    std::unordered_map<std::string, UIPage> pages_;
    
    // Command handlers
    using CommandHandler = std::function<void(const UICommand&)>;
    std::unordered_map<std::string, CommandHandler> command_handlers_;
    
    // Current UI state
    std::string active_page_;
    bool config_dirty_;
    
    // Update tracking
    uint32_t last_state_update_;
    uint32_t last_config_update_;
    
    // Singleton
    static UIManager* instance_;
    
    UIManager();
    ~UIManager() = default;
    
    // Internal methods
    void register_default_pages();
    void register_default_handlers();
    json generate_element_value(const UIElement& element);
    void handle_value_change(const std::string& element_id, const json& value);
    void sync_config_to_state();

public:
    // BaseModule implementation
    const char* get_name() const override { return "ui_manager"; }
    esp_err_t init() override;
    void update() override;
    void stop() override;
    void configure(const json& config) override;
    
    // UI Schema generation
    static json get_ui_schema(const std::string& page_id);
    static json get_menu_schema();
    static json get_status_schema();
    
    // Page management
    static void register_page(const UIPage& page);
    static void unregister_page(const std::string& page_id);
    static std::vector<std::string> get_page_ids();
    
    // Command execution
    static void execute_command(const UICommand& command);
    static void register_command_handler(const std::string& action, CommandHandler handler);
    
    // Configuration management
    static void save_config();
    static void discard_config_changes();
    static bool has_unsaved_changes() { return instance_->config_dirty_; }
    
    // Notifications
    static json get_notifications();
    static void add_notification(const std::string& type, const std::string& message);
    static void clear_notifications();
    
    // State synchronization
    static void refresh_ui_state();
    
private:
    static UIManager* get_instance();
};

} // namespace ModuChill