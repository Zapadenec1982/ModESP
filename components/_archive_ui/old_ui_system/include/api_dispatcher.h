/**
 * @file api_dispatcher.h
 * @brief API request dispatcher for routing between REST and JSON-RPC
 */

#ifndef API_DISPATCHER_H
#define API_DISPATCHER_H

#include <string>
#include <map>
#include <set>
#include <functional>
#include "esp_err.h"
#include "nlohmann/json.hpp"
#include "json_rpc_interface.h"

// Forward declarations for hybrid API support
class StaticApiRegistry;
class DynamicApiBuilder;
class ConfigurationManager;

/**
 * @brief API Dispatcher for handling REST and JSON-RPC requests
 * 
 * Maps REST endpoints to JSON-RPC methods and manages request routing
 */
class ApiDispatcher {
public:
    ApiDispatcher();
    ~ApiDispatcher();
    
    /**
     * @brief Register a JSON-RPC method
     */
    esp_err_t register_method(const std::string& method,
                             JsonRpcHandler handler,
                             const std::string& description = "");
    
    /**
     * @brief Register a dynamic JSON-RPC method (for DynamicApiBuilder)
     * 
     * @param method Method name
     * @param handler Method implementation
     * @param description Method description
     * @return ESP_OK on success
     */
    esp_err_t register_dynamic_method(const std::string& method,
                                     JsonRpcHandler handler,
                                     const std::string& description = "");
    
    /**
     * @brief Execute JSON-RPC request
     */
    esp_err_t execute_json_rpc(const nlohmann::json& request,
                              nlohmann::json& response);
    
    /**
     * @brief Map REST request to JSON-RPC
     */
    esp_err_t execute_rest_request(const std::string& method,
                                  const std::string& path,
                                  const nlohmann::json& body,
                                  nlohmann::json& response);
    
    /**
     * @brief Get list of available methods
     */
    nlohmann::json get_method_list() const;
    
    /**
     * @brief Configure REST to RPC mappings
     */
    void configure_rest_mappings();
    
    // NEW: Hybrid API support
    /**
     * @brief Initialize hybrid APIs (static + dynamic)
     * 
     * This initializes both static APIs (from StaticApiRegistry) and
     * dynamic APIs (from DynamicApiBuilder based on configuration).
     * 
     * @return ESP_OK on success
     */
    esp_err_t initialize_hybrid_apis();
    
    /**
     * @brief Rebuild dynamic APIs
     * 
     * Called after configuration changes to rebuild only the dynamic APIs.
     * Static APIs remain unchanged.
     * 
     * @return ESP_OK on success
     */
    esp_err_t rebuild_dynamic_apis();
    
    /**
     * @brief Get available methods categorized by type
     * 
     * @return JSON object with methods categorized as static/dynamic
     */
    nlohmann::json get_available_methods_by_category() const;
    
    /**
     * @brief Get schema for specific method
     * 
     * @param method Method name
     * @return JSON schema for the method, or empty if not found
     */
    nlohmann::json get_method_schema(const std::string& method) const;
    
    /**
     * @brief Register configuration APIs
     * 
     * Registers APIs for configuration management (update_sensor_config, etc.)
     * 
     * @return ESP_OK on success
     */
    esp_err_t register_configuration_apis();
    
private:
    // Registered RPC methods
    struct RpcMethod {
        JsonRpcHandler handler;
        std::string description;
        nlohmann::json schema;           // NEW: Optional validation schema
        bool is_static;                  // NEW: Track if method is static or dynamic
        
        RpcMethod(JsonRpcHandler h, const std::string& desc, bool static_method = true)
            : handler(h), description(desc), is_static(static_method) {}
            
        RpcMethod(JsonRpcHandler h, const std::string& desc, 
                 const nlohmann::json& s, bool static_method = true)
            : handler(h), description(desc), schema(s), is_static(static_method) {}
    };
    std::map<std::string, RpcMethod> rpc_methods_;
    
    // NEW: Hybrid API support
    bool hybrid_initialized_ = false;
    std::set<std::string> dynamic_methods_;  // Track dynamic methods for rebuild
    
    // REST to RPC mappings
    struct RestMapping {
        std::string rpc_method;
        std::function<nlohmann::json(const std::string& path, 
                                     const nlohmann::json& body)> param_transformer;
    };
    std::map<std::string, RestMapping> rest_mappings_;
    
    // Helper methods
    nlohmann::json create_error_response(int code, const std::string& message);
    esp_err_t validate_json_rpc_request(const nlohmann::json& request);
    
    // NEW: Hybrid API helpers
    void clear_dynamic_methods();
    bool is_dynamic_method(const std::string& method) const;
    esp_err_t register_method_internal(const std::string& method,
                                      JsonRpcHandler handler,
                                      const std::string& description,
                                      const nlohmann::json& schema,
                                      bool is_static);
    
    // Default REST mappings
    void setup_default_mappings();
};

#endif // API_DISPATCHER_H