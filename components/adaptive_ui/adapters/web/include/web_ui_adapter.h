/**
 * @file web_ui_adapter.h
 * @brief Web UI adapter for HTTP server, WebSocket and API handling in adaptive_ui system
 */

#ifndef WEB_UI_ADAPTER_H
#define WEB_UI_ADAPTER_H

#include "ui_component_base.h"
#include "ui_filter.h"
#include "lazy_component_loader.h"
#include "esp_http_server.h"
#include <map>
#include <memory>

namespace ModESP::UI {

// Forward declarations
class ApiHandler;

/**
 * @brief Web UI Adapter for adaptive_ui system
 * 
 * Provides:
 * - HTTP server for static files
 * - REST API endpoints
 * - JSON-RPC API
 * - WebSocket for real-time updates
 * - Integration with UI filtering and lazy loading
 */
class WebUIAdapter {
public:
    WebUIAdapter(UIFilter* filter, LazyComponentLoader* loader);
    ~WebUIAdapter();
    
    // Server control
    esp_err_t start(uint16_t port = 80);
    void stop();
    bool is_running() const { return server_ != nullptr; }
    
    // Component rendering
    std::string renderComponents();
    nlohmann::json getComponentsJson();
    
    // HTTP handlers
    static esp_err_t handle_get_index(httpd_req_t* req);
    static esp_err_t handle_get_ui_data(httpd_req_t* req);
    static esp_err_t handle_api_request(httpd_req_t* req);
    static esp_err_t handle_websocket(httpd_req_t* req);
    
private:
    httpd_handle_t server_ = nullptr;
    httpd_config_t config_;
    
    UIFilter* filter_;
    LazyComponentLoader* loader_;
    std::unique_ptr<ApiHandler> api_handler_;
    
    // Helper methods
    esp_err_t register_uri_handlers();
    esp_err_t send_json_response(httpd_req_t* req, const nlohmann::json& data);
    esp_err_t send_error_response(httpd_req_t* req, int code, const std::string& message);
    
    // Static instance for handler callbacks
    static WebUIAdapter* instance_;
};

} // namespace ModESP::UI

#endif // WEB_UI_ADAPTER_H
