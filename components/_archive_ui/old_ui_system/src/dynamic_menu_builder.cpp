/**
 * @file dynamic_menu_builder.cpp
 * @brief Implementation of dynamic menu builder for LCD UI
 */

#include "dynamic_menu_builder.h"
#include "shared_state.h"
#include "esp_log.h"
#include <algorithm>
#include <sstream>

namespace ModESP {

static const char* TAG = "DynamicMenuBuilder";

DynamicMenuBuilder::DynamicMenuBuilder(ManifestReader& manifest_reader)
    : manifest_reader_(manifest_reader) {
    register_default_conditions();
}

esp_err_t DynamicMenuBuilder::build_menus() {
    ESP_LOGI(TAG, "Building dynamic menus from manifests");
    
    // Clear existing menus
    menus_.clear();
    
    // Get all module manifests
    auto modules = manifest_reader_.get_all_modules();
    
    // Build menus from each module's UI pages
    for (const auto& [module_name, module_info] : modules) {
        auto manifest = manifest_reader_.get_module_manifest(module_name);
        if (!manifest) {
            continue;
        }
        
        // Check for UI pages in manifest
        if (manifest->contains("ui") && (*manifest)["ui"].contains("pages")) {
            for (const auto& page : (*manifest)["ui"]["pages"]) {
                auto menu = create_menu_from_page(module_name, page);
                if (menu) {
                    menus_[menu->id] = std::move(menu);
                }
            }
        }
    }
    
    // Build root menu with module entries
    build_root_menu();
    
    // Validate menu structure
    return validate_menu_structure();
}

const DynamicMenu* DynamicMenuBuilder::get_root_menu() const {
    auto it = menus_.find("root");
    return (it != menus_.end()) ? it->second.get() : nullptr;
}

const DynamicMenu* DynamicMenuBuilder::get_menu(const std::string& menu_id) const {
    auto it = menus_.find(menu_id);
    return (it != menus_.end()) ? it->second.get() : nullptr;
}

std::vector<const DynamicMenuItem*> DynamicMenuBuilder::get_filtered_menu_items(
    const std::string& menu_id,
    AccessLevel access_level) const {
    
    std::vector<const DynamicMenuItem*> filtered_items;
    
    auto menu = get_menu(menu_id);
    if (!menu) {
        return filtered_items;
    }
    
    // Check menu access level
    if (menu->min_access_level > access_level) {
        return filtered_items;
    }
    
    // Filter items by access level and conditions
    for (const auto& item : menu->items) {
        if (item->min_access_level <= access_level &&
            item->visible &&
            evaluate_condition(*item)) {
            filtered_items.push_back(item.get());
        }
    }
    
    return filtered_items;
}

bool DynamicMenuBuilder::evaluate_condition(const DynamicMenuItem& item) const {
    if (item.condition.empty()) {
        return true;  // No condition, always visible
    }
    
    // Check if we have an evaluator for this condition
    auto it = condition_evaluators_.find(item.condition);
    if (it != condition_evaluators_.end()) {
        return it->second();
    }
    
    // Simple expression evaluation for common conditions
    // Format: "state_key > value" or "state_key == value"
    std::istringstream iss(item.condition);
    std::string key, op, value;
    if (iss >> key >> op >> value) {
        auto shared_state = SharedState::get_instance();
        nlohmann::json state_value;
        
        if (shared_state->get(key, state_value)) {
            try {
                if (op == ">" && state_value.is_number()) {
                    return state_value.get<double>() > std::stod(value);
                } else if (op == "<" && state_value.is_number()) {
                    return state_value.get<double>() < std::stod(value);
                } else if (op == "==" && state_value.is_string()) {
                    return state_value.get<std::string>() == value;
                } else if (op == "==" && state_value.is_boolean()) {
                    return state_value.get<bool>() == (value == "true");
                }
            } catch (const std::exception& e) {
                ESP_LOGW(TAG, "Failed to evaluate condition: %s", e.what());
            }
        }
    }
    
    // Default to visible if condition cannot be evaluated
    return true;
}

void DynamicMenuBuilder::register_condition_evaluator(
    const std::string& name,
    std::function<bool()> evaluator) {
    condition_evaluators_[name] = evaluator;
}

void DynamicMenuBuilder::refresh_menus() {
    // Re-evaluate conditions for all menu items
    for (auto& [menu_id, menu] : menus_) {
        for (auto& item : menu->items) {
            // Update visibility based on conditions
            if (!item->condition.empty()) {
                item->visible = evaluate_condition(*item);
            }
        }
    }
}

nlohmann::json DynamicMenuBuilder::get_menu_hierarchy() const {
    nlohmann::json hierarchy = nlohmann::json::object();
    
    for (const auto& [menu_id, menu] : menus_) {
        nlohmann::json menu_json;
        menu_json["id"] = menu->id;
        menu_json["title"] = menu->title;
        menu_json["parent"] = menu->parent_id;
        menu_json["access_level"] = static_cast<int>(menu->min_access_level);
        
        nlohmann::json items = nlohmann::json::array();
        for (const auto& item : menu->items) {
            nlohmann::json item_json;
            item_json["label"] = item->label;
            item_json["type"] = static_cast<int>(item->type);
            item_json["id"] = item->id;
            item_json["visible"] = item->visible;
            item_json["priority"] = item->priority;
            
            // Add type-specific data
            switch (item->type) {
                case MenuItemType::VALUE:
                    item_json["state_key"] = item->value_data.state_key;
                    item_json["unit"] = item->value_data.unit;
                    break;
                case MenuItemType::SUBMENU:
                    item_json["submenu_id"] = item->submenu_data.submenu_id;
                    break;
                case MenuItemType::ACTION:
                    item_json["method"] = item->action_data.method;
                    item_json["module"] = item->action_data.module;
                    break;
                case MenuItemType::SETTING:
                    item_json["config_key"] = item->setting_data.config_key;
                    item_json["type"] = item->setting_data.type;
                    break;
                default:
                    break;
            }
            
            items.push_back(item_json);
        }
        menu_json["items"] = items;
        
        hierarchy[menu_id] = menu_json;
    }
    
    return hierarchy;
}

std::unique_ptr<DynamicMenu> DynamicMenuBuilder::create_menu_from_page(
    const std::string& module_name,
    const nlohmann::json& page) {
    
    auto menu = std::make_unique<DynamicMenu>();
    
    // Generate menu ID
    menu->id = module_name + "_" + page.value("id", "menu");
    menu->title = page.value("label", module_name);
    
    // Parse access level
    if (page.contains("access_level")) {
        menu->min_access_level = parse_access_level(page["access_level"]);
    }
    
    // Create menu items from widgets
    if (page.contains("widgets")) {
        for (const auto& widget : page["widgets"]) {
            auto item = create_menu_item_from_widget(module_name, widget);
            if (item) {
                menu->items.push_back(std::move(item));
            }
        }
    }
    
    // Add back button for non-root menus
    auto back_item = std::make_unique<DynamicMenuItem>("Back", MenuItemType::BACK, "back");
    back_item->priority = 999;  // Always last
    menu->items.push_back(std::move(back_item));
    
    // Sort items by priority
    sort_menu_items(*menu);
    
    return menu;
}

std::unique_ptr<DynamicMenuItem> DynamicMenuBuilder::create_menu_item_from_widget(
    const std::string& module_name,
    const nlohmann::json& widget) {
    
    std::string type = widget.value("type", "");
    std::string label = widget.value("label", "");
    std::string id = widget.value("id", "");
    
    if (type.empty() || label.empty() || id.empty()) {
        return nullptr;
    }
    
    // Determine menu item type based on widget type
    MenuItemType item_type;
    if (type == "value" || type == "gauge") {
        item_type = MenuItemType::VALUE;
    } else if (type == "button") {
        item_type = MenuItemType::ACTION;
    } else if (type == "number" || type == "select" || type == "switch") {
        item_type = MenuItemType::SETTING;
    } else if (type == "navigation") {
        item_type = MenuItemType::SUBMENU;
    } else {
        return nullptr;  // Unsupported widget type
    }
    
    auto item = std::make_unique<DynamicMenuItem>(label, item_type, id);
    
    // Set type-specific data
    switch (item_type) {
        case MenuItemType::VALUE:
            item->value_data.state_key = module_name + "." + id;
            item->value_data.unit = widget.value("unit", "");
            break;
            
        case MenuItemType::ACTION:
            item->action_data.method = widget.value("action", "");
            item->action_data.module = module_name;
            break;
            
        case MenuItemType::SETTING:
            item->setting_data.config_key = module_name + "." + id;
            item->setting_data.type = type;
            if (widget.contains("validation")) {
                item->setting_data.validation = widget["validation"].dump();
            }
            break;
            
        case MenuItemType::SUBMENU:
            item->submenu_data.submenu_id = widget.value("target", "");
            break;
            
        default:
            break;
    }
    
    // Set access control
    if (widget.contains("access_level")) {
        item->min_access_level = parse_access_level(widget["access_level"]);
    }
    
    // Set condition
    if (widget.contains("condition")) {
        item->condition = widget["condition"];
    }
    
    // Set priority
    if (widget.contains("priority")) {
        item->priority = widget["priority"];
    }
    
    return item;
}

void DynamicMenuBuilder::build_root_menu() {
    auto root_menu = std::make_unique<DynamicMenu>();
    root_menu->id = "root";
    root_menu->title = "Main Menu";
    
    // Add entries for each module
    auto modules = manifest_reader_.get_all_modules();
    
    for (const auto& [module_name, module_info] : modules) {
        // Check if module has UI pages
        auto manifest = manifest_reader_.get_module_manifest(module_name);
        if (!manifest || !manifest->contains("ui") || !(*manifest)["ui"].contains("pages")) {
            continue;
        }
        
        // Create submenu item for module
        auto item = std::make_unique<DynamicMenuItem>(
            module_info.description.empty() ? module_name : module_info.description,
            MenuItemType::SUBMENU,
            module_name + "_menu"
        );
        
        // Use first page as submenu target
        auto& pages = (*manifest)["ui"]["pages"];
        if (!pages.empty()) {
            std::string first_page_id = pages[0].value("id", "menu");
            item->submenu_data.submenu_id = module_name + "_" + first_page_id;
        }
        
        // Set priority based on module priority
        if (module_info.priority == "HIGH") {
            item->priority = 1;
        } else if (module_info.priority == "LOW") {
            item->priority = 3;
        } else {
            item->priority = 2;  // NORMAL
        }
        
        root_menu->items.push_back(std::move(item));
    }
    
    // Sort by priority
    sort_menu_items(*root_menu);
    
    menus_["root"] = std::move(root_menu);
}

AccessLevel DynamicMenuBuilder::parse_access_level(const std::string& level) const {
    if (level == "admin") return AccessLevel::ADMIN;
    if (level == "user") return AccessLevel::USER;
    if (level == "service") return AccessLevel::SERVICE;
    return AccessLevel::PUBLIC;
}

void DynamicMenuBuilder::sort_menu_items(DynamicMenu& menu) {
    std::sort(menu.items.begin(), menu.items.end(),
        [](const std::unique_ptr<DynamicMenuItem>& a, 
           const std::unique_ptr<DynamicMenuItem>& b) {
            return a->priority < b->priority;
        });
}

void DynamicMenuBuilder::register_default_conditions() {
    // Register some common condition evaluators
    
    // System state conditions
    register_condition_evaluator("system_running", []() {
        nlohmann::json state;
        auto shared_state = SharedState::get_instance();
        if (shared_state->get("system.state", state)) {
            return state == "running";
        }
        return false;
    });
    
    register_condition_evaluator("alarm_active", []() {
        nlohmann::json active;
        auto shared_state = SharedState::get_instance();
        if (shared_state->get("alarm.active", active)) {
            return active.get<bool>();
        }
        return false;
    });
    
    register_condition_evaluator("wifi_connected", []() {
        nlohmann::json connected;
        auto shared_state = SharedState::get_instance();
        if (shared_state->get("wifi.connected", connected)) {
            return connected.get<bool>();
        }
        return false;
    });
}

esp_err_t DynamicMenuBuilder::validate_menu_structure() const {
    // Validate that all submenu references exist
    for (const auto& [menu_id, menu] : menus_) {
        for (const auto& item : menu->items) {
            if (item->type == MenuItemType::SUBMENU) {
                const std::string& target = item->submenu_data.submenu_id;
                if (!target.empty() && menus_.find(target) == menus_.end()) {
                    ESP_LOGW(TAG, "Menu item '%s' references non-existent submenu '%s'",
                            item->label.c_str(), target.c_str());
                    return ESP_ERR_INVALID_STATE;
                }
            }
        }
    }
    
    // Validate root menu exists
    if (menus_.find("root") == menus_.end()) {
        ESP_LOGE(TAG, "Root menu not found");
        return ESP_ERR_NOT_FOUND;
    }
    
    ESP_LOGI(TAG, "Menu structure validated: %d menus", menus_.size());
    return ESP_OK;
}

// LCDMenuRenderer implementation

std::vector<std::string> LCDMenuRenderer::render_menu(
    const DynamicMenu& menu,
    int display_rows,
    int current_position,
    int scroll_offset) {
    
    std::vector<std::string> lines;
    
    // Title on first row
    lines.push_back(menu.title);
    
    // Menu items on remaining rows
    int available_rows = display_rows - 1;
    int item_count = menu.items.size();
    
    // Calculate visible range with scrolling
    int start_idx = scroll_offset;
    int end_idx = std::min(start_idx + available_rows, item_count);
    
    for (int i = start_idx; i < end_idx; i++) {
        bool selected = (i == current_position);
        lines.push_back(format_menu_item(*menu.items[i], 20, selected));
    }
    
    // Fill remaining rows with empty lines
    while (lines.size() < display_rows) {
        lines.push_back("");
    }
    
    return lines;
}

std::string LCDMenuRenderer::format_menu_item(
    const DynamicMenuItem& item,
    int width,
    bool selected) {
    
    std::string formatted;
    
    // Selection indicator
    if (selected) {
        formatted = "> ";
    } else {
        formatted = "  ";
    }
    
    // Item label
    std::string label = item.label;
    
    // Add type-specific suffix
    switch (item.type) {
        case MenuItemType::VALUE:
            label += ": " + get_value_display(item);
            break;
        case MenuItemType::SUBMENU:
            label += " >";
            break;
        case MenuItemType::SETTING:
            label += " *";
            break;
        default:
            break;
    }
    
    // Truncate or pad to width
    if (label.length() > width - 2) {
        formatted += label.substr(0, width - 2);
    } else {
        formatted += label;
        formatted += std::string(width - 2 - label.length(), ' ');
    }
    
    return formatted;
}

std::string LCDMenuRenderer::get_value_display(const DynamicMenuItem& item) {
    if (item.type != MenuItemType::VALUE) {
        return "";
    }
    
    // Get value from shared state
    auto shared_state = SharedState::get_instance();
    nlohmann::json value;
    
    if (shared_state->get(item.value_data.state_key, value)) {
        std::string display;
        
        // Format based on value type
        if (value.is_number_float()) {
            display = std::to_string(value.get<float>()).substr(0, 5);
        } else if (value.is_number_integer()) {
            display = std::to_string(value.get<int>());
        } else if (value.is_boolean()) {
            display = value.get<bool>() ? "ON" : "OFF";
        } else if (value.is_string()) {
            display = value.get<std::string>();
        } else {
            display = "---";
        }
        
        // Add unit if present
        if (!item.value_data.unit.empty()) {
            display += item.value_data.unit;
        }
        
        return display;
    }
    
    return "---";
}

} // namespace ModESP
