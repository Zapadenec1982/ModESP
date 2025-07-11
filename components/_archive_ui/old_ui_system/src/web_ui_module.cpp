/**
 * @file web_ui_module.cpp
 * @brief Implementation of Web UI Module
 */

#include "web_ui_module.h"
#include "api_dispatcher.h"
#include "system_contract.h"
#include "shared_state.h"
#include "event_bus.h"
#include "esp_log.h"
#include <memory>

static const char* TAG = "WebUIModule";

using namespace ModespContract;

// Constructor
WebUIModule::WebUIModule() 
    : api_dispatcher_(std::make_unique<ApiDispatcher>()) {
}

// Destructor
WebUIModule::~WebUIModule() {
    stop();
}

// Initialize module
esp_err_t WebUIModule::init() {
    ESP_LOGI(TAG, "Initializing Web UI Module");
    
    // TODO-006: Initialize Hybrid API System
    ESP_LOGI(TAG, "Initializing Hybrid API System in WebUIModule...");
    esp_err_t ret = api_dispatcher_->initialize_hybrid_apis();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize hybrid APIs: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "Hybrid API System initialized successfully");
    
    // Initialize API dispatcher
    api_dispatcher_->configure_rest_mappings();
    
    // Publish initial status
    SharedState::set(std::string(State::ApiStatus), nlohmann::json{
        {"running", false},
        {"port", config_.port}
    });
    
    return ESP_OK;
}

// Configure module
void WebUIModule::configure(const nlohmann::json& config) {
    ESP_LOGI(TAG, "Configuring Web UI Module");
    
    if (config.contains("enabled")) {
        config_.enabled = config["enabled"];
    }
    if (config.contains("port")) {
        config_.port = config["port"];
    }
    if (config.contains("auth_required")) {
        config_.auth_required = config["auth_required"];
    }
    if (config.contains("max_connections")) {
        config_.max_connections = config["max_connections"];
    }
}

// Update module
void WebUIModule::update() {
    // Update metrics
    metrics_.active_connections = 0; // TODO: Get actual count from server
    
    // Update status in shared state
    SharedState::set(std::string(State::ApiStatus), nlohmann::json{
        {"running", server_ != nullptr},
        {"port", config_.port},
        {"connections", metrics_.active_connections},
        {"total_requests", metrics_.total_requests}
    });
}

// Stop module
void WebUIModule::stop() {
    ESP_LOGI(TAG, "Stopping Web UI Module");
    
    if (server_) {
        stop_server();
    }
    
    // Update status
    SharedState::set(std::string(State::ApiStatus), nlohmann::json{
        {"running", false},
        {"port", config_.port}
    });
}

// Check if module is healthy
bool WebUIModule::is_healthy() const {
    return config_.enabled ? (server_ != nullptr) : true;
}

// Get health score
uint8_t WebUIModule::get_health_score() const {
    if (!config_.enabled) return 100;
    if (!server_) return 0;
    
    // Calculate health based on error rate
    if (metrics_.total_requests > 0) {
        uint32_t error_rate = (metrics_.total_errors * 100) / metrics_.total_requests;
        return (error_rate > 50) ? 50 : (100 - error_rate);
    }
    
    return 100;
}

// Register RPC methods
void WebUIModule::register_rpc(IJsonRpcRegistrar& rpc) {
    ESP_LOGI(TAG, "Registering Web UI RPC methods");
    
    // Register status method
    rpc.register_method("webui.get_status", 
        [this](const nlohmann::json& params, nlohmann::json& result) -> esp_err_t {
            return rpc_get_status(params, result);
        }, "Get Web UI status");
    
    // Register system info method
    rpc.register_method("webui.get_system_info",
        [this](const nlohmann::json& params, nlohmann::json& result) -> esp_err_t {
            return rpc_get_system_info(params, result);
        }, "Get system information");
}

// IJsonRpcRegistrar interface implementation
esp_err_t WebUIModule::register_method(const std::string& method,
                                      JsonRpcHandler handler,
                                      const std::string& description) {
    ESP_LOGI(TAG, "Registering RPC method: %s", method.c_str());
    // TODO: Store methods in internal registry
    return ESP_OK;
}

esp_err_t WebUIModule::register_notification(const std::string& method,
                                            std::function<void(const nlohmann::json&)> handler) {
    ESP_LOGI(TAG, "Registering RPC notification: %s", method.c_str());
    // TODO: Store notifications in internal registry
    return ESP_OK;
}

esp_err_t WebUIModule::unregister(const std::string& method) {
    ESP_LOGI(TAG, "Unregistering RPC method: %s", method.c_str());
    // TODO: Remove from internal registry
    return ESP_OK;
}

nlohmann::json WebUIModule::get_method_list() const {
    // TODO: Return actual registered methods
    return nlohmann::json::array({
        {{"method", "webui.get_status"}, {"description", "Get Web UI status"}},
        {{"method", "webui.get_system_info"}, {"description", "Get system information"}}
    });
}

// Server management
esp_err_t WebUIModule::start_server() {
    ESP_LOGI(TAG, "Starting HTTP server on port %d", config_.port);
    
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = config_.port;
    config.max_open_sockets = config_.max_connections;
    config.task_priority = 5;
    config.stack_size = 8192;
    config.lru_purge_enable = true;
    
    esp_err_t ret = httpd_start(&server_, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Register URI handlers
    httpd_uri_t static_uri = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = handle_static_get,
        .user_ctx = this
    };
    httpd_register_uri_handler(server_, &static_uri);
    
    httpd_uri_t api_uri = {
        .uri = "/api/*",
        .method = HTTP_POST,
        .handler = handle_api_request,
        .user_ctx = this
    };
    httpd_register_uri_handler(server_, &api_uri);
    
    ESP_LOGI(TAG, "HTTP server started successfully");
    return ESP_OK;
}

esp_err_t WebUIModule::stop_server() {
    if (server_) {
        esp_err_t ret = httpd_stop(server_);
        server_ = nullptr;
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to stop HTTP server: %s", esp_err_to_name(ret));
            return ret;
        }
        ESP_LOGI(TAG, "HTTP server stopped");
    }
    return ESP_OK;
}

// HTTP handlers
esp_err_t WebUIModule::handle_static_get(httpd_req_t* req) {
    WebUIModule* module = (WebUIModule*)req->user_ctx;
    module->metrics_.total_requests++;
    
    // Simple index page
    if (strcmp(req->uri, "/") == 0) {
        httpd_resp_set_type(req, "text/html");
        const char* html = R"(
<!DOCTYPE html>
<html>
<head><title>ModESP Control Panel</title></head>
<body>
<h1>ModESP Control Panel</h1>
<p>Web UI Module is running!</p>
<p>API available at: <a href="/api/status">/api/status</a></p>
</body>
</html>)";
        return httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
    }
    
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not Found");
    module->metrics_.total_errors++;
    return ESP_FAIL;
}

esp_err_t WebUIModule::handle_api_request(httpd_req_t* req) {
    WebUIModule* module = (WebUIModule*)req->user_ctx;
    module->metrics_.total_requests++;
    
    // Simple status endpoint
    if (strcmp(req->uri, "/api/status") == 0) {
        nlohmann::json response = {
            {"status", "ok"},
            {"module", "WebUI"},
            {"running", true},
            {"port", module->config_.port},
            {"connections", module->metrics_.active_connections}
        };
        
        return module->send_json_response(req, response);
    }
    
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "API endpoint not found");
    module->metrics_.total_errors++;
    return ESP_FAIL;
}

esp_err_t WebUIModule::handle_websocket(httpd_req_t* req) {
    // TODO: Implement WebSocket handling
    ESP_LOGW(TAG, "WebSocket not implemented yet");
    return ESP_FAIL;
}

// Helper methods
esp_err_t WebUIModule::send_json_response(httpd_req_t* req, const nlohmann::json& data, int status) {
    std::string response = data.dump();
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    
    if (status != 200) {
        httpd_resp_set_status(req, std::to_string(status).c_str());
    }
    
    return httpd_resp_send(req, response.c_str(), response.length());
}

esp_err_t WebUIModule::send_error_response(httpd_req_t* req, const std::string& error, int status) {
    nlohmann::json error_response = {
        {"error", error},
        {"status", status}
    };
    
    return send_json_response(req, error_response, status);
}

bool WebUIModule::authenticate_request(httpd_req_t* req) {
    if (!config_.auth_required) {
        return true;
    }
    
    // TODO: Implement authentication
    ESP_LOGW(TAG, "Authentication not implemented yet");
    return true;
}

// RPC method implementations
esp_err_t WebUIModule::rpc_get_status(const nlohmann::json& params, nlohmann::json& result) {
    result = {
        {"module", "WebUI"},
        {"running", server_ != nullptr},
        {"port", config_.port},
        {"connections", metrics_.active_connections},
        {"total_requests", metrics_.total_requests},
        {"total_errors", metrics_.total_errors},
        {"health_score", get_health_score()}
    };
    
    return ESP_OK;
}

esp_err_t WebUIModule::rpc_get_system_info(const nlohmann::json& params, nlohmann::json& result) {
    result = {
        {"system", "ModESP"},
        {"uptime", esp_timer_get_time() / 1000000}, // seconds
        {"free_heap", esp_get_free_heap_size()},
        {"min_free_heap", esp_get_minimum_free_heap_size()},
        {"chip_model", "ESP32-S3"}
    };
    
    return ESP_OK;
}