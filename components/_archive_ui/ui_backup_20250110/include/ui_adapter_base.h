/**
 * @file ui_adapter_base.h
 * @brief Base class for all UI adapters
 */

#ifndef UI_ADAPTER_BASE_H
#define UI_ADAPTER_BASE_H

#include "base_module.h"
#include "system_contract.h"
#include <map>
#include <vector>

/**
 * @brief Control types supported by UI adapters
 */
enum class UIControlType {
    VALUE,      // Read-only value display
    GAUGE,      // Visual gauge/meter
    NUMBER,     // Numeric input
    SELECT,     // Dropdown/selection
    SWITCH,     // Boolean toggle
    BUTTON,     // Action button
    SLIDER,     // Range slider
    TEXT,       // Text input
    LIST,       // List of items
    CHART       // Time-series chart
};

/**
 * @brief Base class for all UI protocol adapters
 * 
 * Provides common functionality for discovering modules and
 * building UI based on their schemas
 */
class UIAdapterBase : public BaseModule {
protected:
    // UI schemas from all modules
    std::map<std::string, nlohmann::json> module_schemas_;
    
    // Module capabilities
    std::map<std::string, nlohmann::json> module_capabilities_;
    
    /**
     * @brief Discover all registered modules and their UI schemas
     */
    virtual void discover_modules();
    
    /**
     * @brief Register protocol-specific handlers
     * Must be implemented by each UI adapter
     */
    virtual void register_ui_handlers() = 0;
    
    /**
     * @brief Handle read request from UI
     */
    virtual esp_err_t handle_read_request(const std::string& method,
                                         const nlohmann::json& params,
                                         nlohmann::json& response);
    
    /**
     * @brief Handle write request from UI
     */
    virtual esp_err_t handle_write_request(const std::string& method,
                                          const nlohmann::json& params);
    
    /**
     * @brief Subscribe to telemetry updates
     */
    virtual void subscribe_telemetry(const std::string& source,
                                    std::function<void(const nlohmann::json&)> handler);
    
    /**
     * @brief Get control type from string
     */
    UIControlType get_control_type(const std::string& type_str) const;
    
public:
    // BaseModule interface
    esp_err_t init() override;
    void update() override;
    
    /**
     * @brief Handle module registration event
     */
    void on_module_registered(const std::string& name, const nlohmann::json& schema);
    
    /**
     * @brief Handle module unregistration event
     */
    void on_module_unregistered(const std::string& name);
};

#endif // UI_ADAPTER_BASE_H