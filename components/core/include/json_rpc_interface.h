/**
 * @file json_rpc_interface.h
 * @brief JSON-RPC registrar interface for module API registration
 */

#ifndef JSON_RPC_INTERFACE_H
#define JSON_RPC_INTERFACE_H

#include <string>
#include <functional>
#include "esp_err.h"
#include "nlohmann/json.hpp"

/**
 * @brief JSON-RPC method handler type
 * 
 * @param params Input parameters as JSON
 * @param result Output result as JSON (to be filled by handler)
 * @return ESP_OK on success, error code otherwise
 */
using JsonRpcHandler = std::function<esp_err_t(const nlohmann::json& params, 
                                               nlohmann::json& result)>;

/**
 * @brief Interface for JSON-RPC method registration
 * 
 * Modules implement their RPC methods and register them through this interface.
 * The actual RPC server (WebUIModule) provides the implementation.
 */
class IJsonRpcRegistrar {
public:
    virtual ~IJsonRpcRegistrar() = default;
    
    /**
     * @brief Register a JSON-RPC method
     * 
     * @param method Method name (e.g., "sensor.get_value")
     * @param handler Method implementation
     * @param description Optional method description for documentation
     * @return ESP_OK on success
     */
    virtual esp_err_t register_method(const std::string& method,
                                     JsonRpcHandler handler,
                                     const std::string& description = "") = 0;
    
    /**
     * @brief Register a notification handler (no response expected)
     * 
     * @param method Notification name
     * @param handler Notification handler
     * @return ESP_OK on success
     */
    virtual esp_err_t register_notification(const std::string& method,
                                           std::function<void(const nlohmann::json&)> handler) = 0;
    
    /**
     * @brief Unregister a method or notification
     * 
     * @param method Method/notification name
     * @return ESP_OK on success
     */
    virtual esp_err_t unregister(const std::string& method) = 0;
    
    /**
     * @brief Get list of all registered methods
     * 
     * @return JSON array of method descriptions
     */
    virtual nlohmann::json get_method_list() const = 0;
};

#endif // JSON_RPC_INTERFACE_H