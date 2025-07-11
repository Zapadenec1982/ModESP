/**
 * @file dynamic_menu_builder.h
 * @brief Dynamic menu builder for LCD UI based on module manifests
 * 
 * Part of Phase 3: Dynamic UI System implementation
 * Generates LCD menu structures at runtime from manifest data
 */

#pragma once

#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <string>
#include "esp_err.h"
#include "manifest_reader.h"
#include "ui_adapter_base.h"

namespace ModESP {

/**
 * @brief Menu item types for LCD display
 */
enum class MenuItemType {
    VALUE,      // Display a value from shared state
    SUBMENU,    // Navigate to submenu
    ACTION,     // Execute an action/command
    SETTING,    // Editable setting
    BACK        // Return to previous menu
};

/**
 * @brief Access levels for menu items
 */
enum class AccessLevel {
    PUBLIC = 0,
    USER = 1,
    ADMIN = 2,
    SERVICE = 3
};

/**
 * @brief Single menu item definition
 */
struct DynamicMenuItem {
    std::string label;
    MenuItemType type;
    std::string id;  // Unique identifier
    
    // Type-specific data
    union {
        struct {
            std::string state_key;  // SharedState key for VALUE items
            std::string unit;       // Optional unit display
        } value_data;
        
        struct {
            std::string submenu_id;  // ID of submenu to navigate to
        } submenu_data;
        
        struct {
            std::string method;      // RPC method to call
            std::string module;      // Module that handles the action
        } action_data;
        
        struct {
            std::string config_key;  // Configuration key
            std::string type;        // Setting type (number, select, bool)
            std::string validation;  // Validation rules
        } setting_data;
    };
    
    // Access control
    AccessLevel min_access_level = AccessLevel::PUBLIC;
    std::string condition;  // Optional condition expression
    
    // Display properties
    bool visible = true;
    int priority = 0;  // Sort order
    
    // Constructor
    DynamicMenuItem(const std::string& label, MenuItemType type, const std::string& id)
        : label(label), type(type), id(id) {
        // Initialize union based on type
        switch (type) {
            case MenuItemType::VALUE:
                new (&value_data) decltype(value_data){};
                break;
            case MenuItemType::SUBMENU:
                new (&submenu_data) decltype(submenu_data){};
                break;
            case MenuItemType::ACTION:
                new (&action_data) decltype(action_data){};
                break;
            case MenuItemType::SETTING:
                new (&setting_data) decltype(setting_data){};
                break;
            default:
                break;
        }
    }
    
    // Destructor to handle union cleanup
    ~DynamicMenuItem() {
        // Union members are POD types, no cleanup needed
    }
};

/**
 * @brief Menu structure containing items
 */
struct DynamicMenu {
    std::string id;
    std::string title;
    std::vector<std::unique_ptr<DynamicMenuItem>> items;
    std::string parent_id;  // Empty for root menu
    AccessLevel min_access_level = AccessLevel::PUBLIC;
};

/**
 * @brief Builds LCD menu structures dynamically from manifest data
 */
class DynamicMenuBuilder {
public:
    /**
     * @brief Constructor
     * @param manifest_reader Reference to manifest reader
     */
    explicit DynamicMenuBuilder(ManifestReader& manifest_reader);
    
    /**
     * @brief Build menus from all registered modules
     * @return ESP_OK on success
     */
    esp_err_t build_menus();
    
    /**
     * @brief Get root menu
     * @return Pointer to root menu or nullptr if not built
     */
    const DynamicMenu* get_root_menu() const;
    
    /**
     * @brief Get menu by ID
     * @param menu_id Menu identifier
     * @return Pointer to menu or nullptr if not found
     */
    const DynamicMenu* get_menu(const std::string& menu_id) const;
    
    /**
     * @brief Get filtered menu for user access level
     * @param menu_id Menu identifier
     * @param access_level User's access level
     * @return Filtered menu items accessible to user
     */
    std::vector<const DynamicMenuItem*> get_filtered_menu_items(
        const std::string& menu_id,
        AccessLevel access_level) const;
    
    /**
     * @brief Evaluate menu item condition
     * @param item Menu item with condition
     * @return true if condition is met or no condition
     */
    bool evaluate_condition(const DynamicMenuItem& item) const;
    
    /**
     * @brief Register condition evaluator
     * @param name Condition name
     * @param evaluator Function that evaluates the condition
     */
    void register_condition_evaluator(
        const std::string& name,
        std::function<bool()> evaluator);
    
    /**
     * @brief Refresh dynamic menu content
     * Updates visibility and conditions without rebuilding
     */
    void refresh_menus();
    
    /**
     * @brief Get menu hierarchy as JSON
     * @return JSON representation of menu structure
     */
    nlohmann::json get_menu_hierarchy() const;
    
private:
    ManifestReader& manifest_reader_;
    std::map<std::string, std::unique_ptr<DynamicMenu>> menus_;
    std::map<std::string, std::function<bool()>> condition_evaluators_;
    
    /**
     * @brief Create menu from UI page definition
     * @param module_name Module that owns the page
     * @param page UI page from manifest
     * @return Created menu
     */
    std::unique_ptr<DynamicMenu> create_menu_from_page(
        const std::string& module_name,
        const nlohmann::json& page);
    
    /**
     * @brief Create menu item from widget definition
     * @param module_name Module that owns the widget
     * @param widget Widget definition from manifest
     * @return Created menu item
     */
    std::unique_ptr<DynamicMenuItem> create_menu_item_from_widget(
        const std::string& module_name,
        const nlohmann::json& widget);
    
    /**
     * @brief Build root menu with module entries
     */
    void build_root_menu();
    
    /**
     * @brief Parse access level from string
     * @param level Access level string
     * @return Parsed access level
     */
    AccessLevel parse_access_level(const std::string& level) const;
    
    /**
     * @brief Sort menu items by priority
     * @param menu Menu to sort
     */
    void sort_menu_items(DynamicMenu& menu);
    
    /**
     * @brief Register default condition evaluators
     */
    void register_default_conditions();
    
    /**
     * @brief Validate menu structure
     * @return ESP_OK if valid
     */
    esp_err_t validate_menu_structure() const;
};

/**
 * @brief Helper class to render dynamic menus on LCD
 */
class LCDMenuRenderer {
public:
    /**
     * @brief Render menu on LCD display
     * @param menu Menu to render
     * @param display_rows Number of display rows
     * @param current_position Current cursor position
     * @param scroll_offset Scroll offset for long menus
     * @return Lines to display
     */
    static std::vector<std::string> render_menu(
        const DynamicMenu& menu,
        int display_rows,
        int current_position,
        int scroll_offset);
    
    /**
     * @brief Format menu item for display
     * @param item Menu item
     * @param width Display width
     * @param selected Whether item is selected
     * @return Formatted string
     */
    static std::string format_menu_item(
        const DynamicMenuItem& item,
        int width,
        bool selected);
    
    /**
     * @brief Get value display string
     * @param item Value menu item
     * @return Current value as string
     */
    static std::string get_value_display(const DynamicMenuItem& item);
};

} // namespace ModESP
