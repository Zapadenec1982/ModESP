/**
 * @file web_ui_module.h
 * @brief Web UI module for HTTP server, WebSocket and API handling
 */

#ifndef WEB_UI_MODULE_H
#define WEB_UI_MODULE_H

#include "base_module.h"
#include "json_rpc_interface.h"
#include "esp_http_server.h"
#include <map>
#include <memory>

// Forward declarations
class ApiDispatcher;
class WebSocketManager;

/**
 * @brief Web UI Module
 * 
 * Provides:
 * - HTTP server for static files
 * - REST API endpoints
 * - JSON-RPC API
 * - WebSocket for real-time updates
 */
class WebUIModule : public BaseModule, public IJsonRpcRegistrar {
public:
    WebUIModule();
    ~WebUIModule() override;
    
    // BaseModule interface
    const char* get_name() const override { return "WebUI"; }
    esp_err_t init() override;
    void configure(const nlohmann::json& config) override;
    void update() override;
    void stop() override;
    bool is_healthy() const override;
    uint8_t get_health_score() const override;
    uint32_t get_max_update_time_us() const override { return 5000; }
    void register_rpc(IJsonRpcRegistrar& rpc) override;
    
    // IJsonRpcRegistrar interface
    esp_err_t register_method(const std::string& method,
                             JsonRpcHandler handler,
                             const std::string& description = "") override;
    esp_err_t register_notification(const std::string& method,
                                   std::function<void(const nlohmann::json&)> handler) override;
    esp_err_t unregister(const std::string& method) override;
    nlohmann::json get_method_list() const override;
    
private:
    // Configuration
    struct Config {
        bool enabled = true;
        uint16_t port = 80;
        bool auth_required = false;
        std::string username;
        std::string password;
        uint32_t session_timeout = 3600;
        uint8_t max_connections = 4;
        bool json_rpc_enabled = true;
        bool rest_enabled = true;
    } config_;
    
    // HTTP server
    httpd_handle_t server_ = nullptr;
    
    // API components
    std::unique_ptr<ApiDispatcher> api_dispatcher_;
    std::unique_ptr<WebSocketManager> ws_manager_;
    
    // Metrics
    struct Metrics {
        uint32_t total_requests = 0;
        uint32_t total_errors = 0;
        uint32_t active_connections = 0;
        uint64_t total_response_time_us = 0;
    } metrics_;
    
    // Server management
    esp_err_t start_server();
    esp_err_t stop_server();
    
    // HTTP handlers
    static esp_err_t handle_static_get(httpd_req_t* req);
    static esp_err_t handle_api_request(httpd_req_t* req);
    static esp_err_t handle_websocket(httpd_req_t* req);
    
    // Helper methods
    esp_err_t send_json_response(httpd_req_t* req, const nlohmann::json& data, int status = 200);
    esp_err_t send_error_response(httpd_req_t* req, const std::string& error, int status = 400);
    bool authenticate_request(httpd_req_t* req);
    
    // RPC method implementations
    esp_err_t rpc_get_status(const nlohmann::json& params, nlohmann::json& result);
    esp_err_t rpc_get_system_info(const nlohmann::json& params, nlohmann::json& result);
};

#endif // WEB_UI_MODULE_H