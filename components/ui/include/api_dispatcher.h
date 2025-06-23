/**
 * @file api_dispatcher.h
 * @brief API request dispatcher for routing between REST and JSON-RPC
 */

#ifndef API_DISPATCHER_H
#define API_DISPATCHER_H

#include <string>
#include <map>
#include <functional>
#include "esp_err.h"
#include "nlohmann/json.hpp"
#include "json_rpc_interface.h"

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
    
private:
    // Registered RPC methods
    struct RpcMethod {
        JsonRpcHandler handler;
        std::string description;
    };
    std::map<std::string, RpcMethod> rpc_methods_;
    
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
    
    // Default REST mappings
    void setup_default_mappings();
};

#endif // API_DISPATCHER_H