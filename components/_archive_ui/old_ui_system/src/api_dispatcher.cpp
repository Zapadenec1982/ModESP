/**
 * @file api_dispatcher.cpp
 * @brief Implementation of API Dispatcher with Hybrid API support
 */

#include "api_dispatcher.h"
#include "static_api_registry.h"
#include "dynamic_api_builder.h"
#include "configuration_manager.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char* TAG = "ApiDispatcher";

// Constructor
ApiDispatcher::ApiDispatcher() {
    ESP_LOGI(TAG, "ApiDispatcher created");
}

// Destructor
ApiDispatcher::~ApiDispatcher() {
    ESP_LOGI(TAG, "ApiDispatcher destroyed");
}

// Register a JSON-RPC method
esp_err_t ApiDispatcher::register_method(const std::string& method,
                                        JsonRpcHandler handler,
                                        const std::string& description) {
    return register_method_internal(method, handler, description, nlohmann::json{}, true);
}

// Register a dynamic JSON-RPC method
esp_err_t ApiDispatcher::register_dynamic_method(const std::string& method,
                                                 JsonRpcHandler handler,
                                                 const std::string& description) {
    return register_method_internal(method, handler, description, nlohmann::json{}, false);
}

// Execute JSON-RPC request
esp_err_t ApiDispatcher::execute_json_rpc(const nlohmann::json& request,
                                         nlohmann::json& response) {
    ESP_LOGD(TAG, "Executing JSON-RPC request");
    
    // Validate request
    esp_err_t ret = validate_json_rpc_request(request);
    if (ret != ESP_OK) {
        response = create_error_response(-32600, "Invalid Request");
        return ret;
    }
    
    std::string method = request["method"];
    nlohmann::json params = request.contains("params") ? request["params"] : nlohmann::json::object();
    
    // Find method
    auto it = rpc_methods_.find(method);
    if (it == rpc_methods_.end()) {
        response = create_error_response(-32601, "Method not found");
        return ESP_ERR_NOT_FOUND;
    }
    
    // Execute method
    nlohmann::json result;
    ret = it->second.handler(params, result);
    
    if (ret == ESP_OK) {
        response = {
            {"jsonrpc", "2.0"},
            {"result", result}
        };
        
        if (request.contains("id")) {
            response["id"] = request["id"];
        }
    } else {
        response = create_error_response(-32603, "Internal error");
    }
    
    return ret;
}

// Execute REST request
esp_err_t ApiDispatcher::execute_rest_request(const std::string& method,
                                             const std::string& path,
                                             const nlohmann::json& body,
                                             nlohmann::json& response) {
    ESP_LOGD(TAG, "Executing REST request: %s %s", method.c_str(), path.c_str());
    
    // Simple REST to RPC mapping
    std::string rpc_method;
    nlohmann::json rpc_params;
    
    if (path == "/api/status") {
        rpc_method = "system.get_status";
        rpc_params = nlohmann::json::object();
    } else if (path.find("/api/") == 0) {
        // Extract method from path
        std::string api_path = path.substr(5); // Remove "/api/"
        rpc_method = "api." + api_path;
        rpc_params = body;
    } else {
        response = create_error_response(-32601, "Endpoint not found");
        return ESP_ERR_NOT_FOUND;
    }
    
    // Execute as JSON-RPC
    nlohmann::json rpc_request = {
        {"jsonrpc", "2.0"},
        {"method", rpc_method},
        {"params", rpc_params},
        {"id", 1}
    };
    
    return execute_json_rpc(rpc_request, response);
}

// Get list of available methods
nlohmann::json ApiDispatcher::get_method_list() const {
    nlohmann::json methods = nlohmann::json::array();
    
    for (const auto& pair : rpc_methods_) {
        methods.push_back({
            {"method", pair.first},
            {"description", pair.second.description}
        });
    }
    
    return methods;
}

// Configure REST to RPC mappings
void ApiDispatcher::configure_rest_mappings() {
    ESP_LOGI(TAG, "Configuring REST mappings");
    
    // Setup default mappings
    setup_default_mappings();
}

// Create error response
nlohmann::json ApiDispatcher::create_error_response(int code, const std::string& message) {
    return {
        {"jsonrpc", "2.0"},
        {"error", {
            {"code", code},
            {"message", message}
        }},
        {"id", nullptr}
    };
}

// Validate JSON-RPC request
esp_err_t ApiDispatcher::validate_json_rpc_request(const nlohmann::json& request) {
    if (!request.is_object()) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!request.contains("jsonrpc") || request["jsonrpc"] != "2.0") {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!request.contains("method") || !request["method"].is_string()) {
        return ESP_ERR_INVALID_ARG;
    }
    
    return ESP_OK;
}

// Setup default mappings
void ApiDispatcher::setup_default_mappings() {
    ESP_LOGI(TAG, "Setting up default REST mappings");
    
    // Add default REST to RPC mappings
    RestMapping status_mapping;
    status_mapping.rpc_method = "system.get_status";
    status_mapping.param_transformer = [](const std::string& path, const nlohmann::json& body) {
        return nlohmann::json::object();
    };
    
    rest_mappings_["/api/status"] = status_mapping;
} 

// NEW: Hybrid API support implementation
esp_err_t ApiDispatcher::initialize_hybrid_apis() {
    ESP_LOGI(TAG, "Initializing hybrid API system...");
    
    if (hybrid_initialized_) {
        ESP_LOGW(TAG, "Hybrid APIs already initialized");
        return ESP_OK;
    }
    
    // 1. Register all static APIs
    esp_err_t ret = StaticApiRegistry::register_all_static_apis(this);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register static APIs: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 2. Build dynamic APIs based on current configuration
    DynamicApiBuilder builder(this);
    ret = builder.build_all_dynamic_apis();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to build dynamic APIs: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 3. Register configuration management APIs
    ret = register_configuration_apis();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register configuration APIs: %s", esp_err_to_name(ret));
        return ret;
    }
    
    hybrid_initialized_ = true;
    ESP_LOGI(TAG, "Hybrid API system initialized successfully");
    
    return ESP_OK;
}
esp_err_t ApiDispatcher::rebuild_dynamic_apis() {
    ESP_LOGI(TAG, "Rebuilding dynamic APIs...");
    
    // Clear existing dynamic methods
    clear_dynamic_methods();
    
    // Rebuild dynamic APIs
    DynamicApiBuilder builder(this);
    esp_err_t ret = builder.build_all_dynamic_apis();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to rebuild dynamic APIs: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "Dynamic APIs rebuilt successfully");
    return ESP_OK;
}

nlohmann::json ApiDispatcher::get_available_methods_by_category() const {
    nlohmann::json result = {
        {"static_methods", nlohmann::json::array()},
        {"dynamic_methods", nlohmann::json::array()}
    };
    
    for (const auto& [method, info] : rpc_methods_) {
        nlohmann::json method_info = {
            {"method", method},
            {"description", info.description}
        };
        
        if (!info.schema.empty()) {
            method_info["schema"] = info.schema;
        }
        
        if (info.is_static) {
            result["static_methods"].push_back(method_info);
        } else {
            result["dynamic_methods"].push_back(method_info);
        }
    }
    
    return result;
}

nlohmann::json ApiDispatcher::get_method_schema(const std::string& method) const {
    auto it = rpc_methods_.find(method);
    if (it != rpc_methods_.end()) {
        return it->second.schema;
    }
    return nlohmann::json{};
}

esp_err_t ApiDispatcher::register_configuration_apis() {
    ESP_LOGI(TAG, "Registering configuration management APIs...");
    
    // Configuration reading APIs
    register_method_internal("config.get_sensors", 
        [](const nlohmann::json& params, nlohmann::json& result) -> esp_err_t {
            // TODO: Load actual sensor configuration
            result = {
                {"sensors", nlohmann::json::array()},
                {"message", "Sensor configuration loaded"},
                {"timestamp", esp_timer_get_time()}
            };
            return ESP_OK;
        }, "Get current sensor configuration", nlohmann::json{}, true);
    
    // Configuration updating APIs (with restart requirement)
    register_method_internal("config.update_sensors",
        [](const nlohmann::json& params, nlohmann::json& result) -> esp_err_t {
            auto& config_mgr = ConfigurationManager::instance();
            esp_err_t ret = config_mgr.update_sensor_configuration(params);
            
            if (ret == ESP_OK) {
                result = {
                    {"success", true},
                    {"restart_required", true},
                    {"message", "Configuration saved. System restart required to apply changes."}
                };
            } else {
                result = {
                    {"success", false},
                    {"error", "Failed to save configuration"}
                };
            }
            
            return ret;
        }, "Update sensor configuration (requires restart)", nlohmann::json{}, true);
    
    // Defrost configuration APIs
    register_method_internal("config.update_defrost",
        [](const nlohmann::json& params, nlohmann::json& result) -> esp_err_t {
            auto& config_mgr = ConfigurationManager::instance();
            esp_err_t ret = config_mgr.update_defrost_configuration(params);
            
            if (ret == ESP_OK) {
                result = {
                    {"success", true},
                    {"restart_required", true},
                    {"message", "Defrost configuration saved. Restart required."}
                };
            }
            
            return ret;
        }, "Update defrost configuration (requires restart)", nlohmann::json{}, true);
    
    // Restart management APIs
    register_method_internal("system.restart_for_config",
        [](const nlohmann::json& params, nlohmann::json& result) -> esp_err_t {
            auto& config_mgr = ConfigurationManager::instance();
            if (config_mgr.is_restart_required()) {
                config_mgr.schedule_restart_if_required();
                result = {
                    {"message", "System restart scheduled"},
                    {"reason", config_mgr.get_restart_reason()}
                };
                return ESP_OK;
            } else {
                result = {
                    {"message", "No restart required"},
                    {"restart_required", false}
                };
                return ESP_ERR_INVALID_STATE;
            }
        }, "Restart system to apply configuration changes", nlohmann::json{}, true);
    
    // Get restart status
    register_method_internal("config.get_restart_status",
        [](const nlohmann::json& params, nlohmann::json& result) -> esp_err_t {
            auto& config_mgr = ConfigurationManager::instance();
            result = {
                {"restart_required", config_mgr.is_restart_required()},
                {"restart_reason", config_mgr.get_restart_reason()}
            };
            return ESP_OK;
        }, "Check if restart is required for configuration changes", nlohmann::json{}, true);
    
    // API documentation
    register_method_internal("system.get_api_documentation",
        [this](const nlohmann::json& params, nlohmann::json& result) -> esp_err_t {
            result = get_available_methods_by_category();
            result["total_methods"] = rpc_methods_.size();
            result["hybrid_initialized"] = hybrid_initialized_;
            return ESP_OK;
        }, "Get complete API documentation with categorization", nlohmann::json{}, true);
    
    ESP_LOGI(TAG, "Configuration management APIs registered");
    return ESP_OK;
}

// Helper method implementations
void ApiDispatcher::clear_dynamic_methods() {
    ESP_LOGI(TAG, "Clearing dynamic methods...");
    
    // Remove all dynamic methods from rpc_methods_
    for (auto it = rpc_methods_.begin(); it != rpc_methods_.end();) {
        if (!it->second.is_static) {
            ESP_LOGD(TAG, "Removing dynamic method: %s", it->first.c_str());
            it = rpc_methods_.erase(it);
        } else {
            ++it;
        }
    }
    
    dynamic_methods_.clear();
    ESP_LOGI(TAG, "Dynamic methods cleared");
}

bool ApiDispatcher::is_dynamic_method(const std::string& method) const {
    return dynamic_methods_.find(method) != dynamic_methods_.end();
}

esp_err_t ApiDispatcher::register_method_internal(const std::string& method,
                                                 JsonRpcHandler handler,
                                                 const std::string& description,
                                                 const nlohmann::json& schema,
                                                 bool is_static) {
    ESP_LOGI(TAG, "Registering %s method: %s", 
             is_static ? "static" : "dynamic", method.c_str());
    
    RpcMethod rpc_method(handler, description, schema, is_static);
    rpc_methods_[method] = rpc_method;
    
    // Track dynamic methods for later removal
    if (!is_static) {
        dynamic_methods_.insert(method);
    }
    
    return ESP_OK;
}
