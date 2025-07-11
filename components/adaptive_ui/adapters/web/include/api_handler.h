/**
 * @file api_handler.h
 * @brief API request handler for adaptive_ui system
 */

#ifndef API_HANDLER_H
#define API_HANDLER_H

#include <string>
#include <map>
#include <functional>
#include "esp_err.h"
#include "nlohmann/json.hpp"

namespace ModESP::UI {

/**
 * @brief API Handler for handling REST and JSON-RPC requests in adaptive_ui
 * 
 * Maps REST endpoints to handlers and manages request routing
 */
class ApiHandler {
public:
    using Handler = std::function<nlohmann::json(const nlohmann::json&)>;
    using AsyncHandler = std::function<void(const nlohmann::json&, std::function<void(nlohmann::json)>)>;
    
    ApiHandler();
    ~ApiHandler() = default;
    
    // Register handlers
    void registerEndpoint(const std::string& method, Handler handler);
    void registerAsyncEndpoint(const std::string& method, AsyncHandler handler);
    
    // Handle requests
    nlohmann::json handleRequest(const std::string& method, const nlohmann::json& params);
    void handleAsyncRequest(const std::string& method, const nlohmann::json& params, 
                           std::function<void(nlohmann::json)> callback);
    
    // Utility methods
    bool hasEndpoint(const std::string& method) const;
    std::vector<std::string> getEndpoints() const;
    
private:
    std::map<std::string, Handler> endpoints_;
    std::map<std::string, AsyncHandler> async_endpoints_;
    
    // Helper methods
    nlohmann::json createErrorResponse(int code, const std::string& message);
    nlohmann::json createSuccessResponse(const nlohmann::json& result);
};

} // namespace ModESP::UI

#endif // API_HANDLER_H
