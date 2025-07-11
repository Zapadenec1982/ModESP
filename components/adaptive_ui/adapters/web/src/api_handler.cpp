/**
 * @file api_handler.cpp
 * @brief Implementation of API Handler for adaptive_ui system
 */

#include "api_handler.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char* TAG = "ApiHandler";

namespace ModESP::UI {

// Constructor
ApiHandler::ApiHandler() {
    // Register default endpoints
    registerEndpoint("system.info", [](const nlohmann::json& params) {
        nlohmann::json result;
        result["version"] = "1.0.0";
        result["name"] = "ModESP Adaptive UI";
        result["uptime"] = esp_timer_get_time() / 1000000; // seconds
        return result;
    });
    
    registerEndpoint("ui.refresh", [](const nlohmann::json& params) {
        nlohmann::json result;
        result["status"] = "refreshed";
        return result;
    });
}

// Register synchronous endpoint
void ApiHandler::registerEndpoint(const std::string& method, Handler handler) {
    endpoints_[method] = handler;
    ESP_LOGI(TAG, "Registered endpoint: %s", method.c_str());
}

// Register asynchronous endpoint
void ApiHandler::registerAsyncEndpoint(const std::string& method, AsyncHandler handler) {
    async_endpoints_[method] = handler;
    ESP_LOGI(TAG, "Registered async endpoint: %s", method.c_str());
}

// Handle synchronous request
nlohmann::json ApiHandler::handleRequest(const std::string& method, const nlohmann::json& params) {
    // Check if endpoint exists
    auto it = endpoints_.find(method);
    if (it != endpoints_.end()) {
        // Safe call to handler - if it fails, return error
        // Note: In ESP-IDF, handlers should be designed to not throw
        nlohmann::json result;
        
        // Validate input first
        if (params.is_null() && method.find("get") == std::string::npos) {
            return createErrorResponse(-32602, "Invalid params for method: " + method);
        }
        
        result = it->second(params);
        return createSuccessResponse(result);
    }
    
    // Check async endpoints
    if (async_endpoints_.find(method) != async_endpoints_.end()) {
        return createErrorResponse(-32601, "Method requires async handling");
    }
    
    // Method not found
    return createErrorResponse(-32601, "Method not found: " + method);
}

// Handle asynchronous request
void ApiHandler::handleAsyncRequest(const std::string& method, const nlohmann::json& params,
                                   std::function<void(nlohmann::json)> callback) {
    auto it = async_endpoints_.find(method);
    if (it != async_endpoints_.end()) {
        it->second(params, [this, callback](nlohmann::json result) {
            callback(createSuccessResponse(result));
        });
    } else if (endpoints_.find(method) != endpoints_.end()) {
        // Fall back to sync handler
        callback(handleRequest(method, params));
    } else {
        callback(createErrorResponse(-32601, "Method not found: " + method));
    }
}

// Check if endpoint exists
bool ApiHandler::hasEndpoint(const std::string& method) const {
    return endpoints_.find(method) != endpoints_.end() || 
           async_endpoints_.find(method) != async_endpoints_.end();
}

// Get all endpoints
std::vector<std::string> ApiHandler::getEndpoints() const {
    std::vector<std::string> result;
    
    for (const auto& [method, _] : endpoints_) {
        result.push_back(method);
    }
    
    for (const auto& [method, _] : async_endpoints_) {
        result.push_back(method + " (async)");
    }
    
    return result;
}

// Create error response
nlohmann::json ApiHandler::createErrorResponse(int code, const std::string& message) {
    nlohmann::json response;
    response["jsonrpc"] = "2.0";
    response["error"]["code"] = code;
    response["error"]["message"] = message;
    return response;
}

// Create success response
nlohmann::json ApiHandler::createSuccessResponse(const nlohmann::json& result) {
    nlohmann::json response;
    response["jsonrpc"] = "2.0";
    response["result"] = result;
    return response;
}

} // namespace ModESP::UI
