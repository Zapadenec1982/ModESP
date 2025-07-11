/**
 * @file web_ui_adapter.cpp
 * @brief Implementation of Web UI Adapter for adaptive_ui system
 */

#include "web_ui_adapter.h"
#include "api_handler.h"
#include "esp_log.h"
#include <sstream>

static const char* TAG = "WebUIAdapter";

namespace ModESP::UI {

// Static instance for callbacks
WebUIAdapter* WebUIAdapter::instance_ = nullptr;

// Constructor
WebUIAdapter::WebUIAdapter(UIFilter* filter, LazyComponentLoader* loader)
    : filter_(filter), loader_(loader) {
    instance_ = this;
    api_handler_ = std::make_unique<ApiHandler>();
    
    // Configure HTTP server
    config_ = HTTPD_DEFAULT_CONFIG();
    config_.uri_match_fn = httpd_uri_match_wildcard;
    config_.max_uri_handlers = 16;
    config_.stack_size = 8192;
}

// Destructor
WebUIAdapter::~WebUIAdapter() {
    stop();
    instance_ = nullptr;
}

// Start HTTP server
esp_err_t WebUIAdapter::start(uint16_t port) {
    if (server_ != nullptr) {
        ESP_LOGW(TAG, "Server already running");
        return ESP_OK;
    }
    
    config_.server_port = port;
    
    ESP_LOGI(TAG, "Starting HTTP server on port %d", port);
    esp_err_t ret = httpd_start(&server_, &config_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Register URI handlers
    ret = register_uri_handlers();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register URI handlers");
        httpd_stop(server_);
        server_ = nullptr;
        return ret;
    }
    
    ESP_LOGI(TAG, "Web UI started successfully on port %d", port);
    return ESP_OK;
}

// Stop HTTP server
void WebUIAdapter::stop() {
    if (server_ != nullptr) {
        ESP_LOGI(TAG, "Stopping HTTP server");
        httpd_stop(server_);
        server_ = nullptr;
    }
}

// Register URI handlers
esp_err_t WebUIAdapter::register_uri_handlers() {
    esp_err_t ret;
    
    // Index page
    httpd_uri_t index_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = handle_get_index,
        .user_ctx = nullptr
    };
    ret = httpd_register_uri_handler(server_, &index_uri);
    if (ret != ESP_OK) return ret;
    
    // UI data endpoint
    httpd_uri_t ui_data_uri = {
        .uri = "/api/ui/data",
        .method = HTTP_GET,
        .handler = handle_get_ui_data,
        .user_ctx = nullptr
    };
    ret = httpd_register_uri_handler(server_, &ui_data_uri);
    if (ret != ESP_OK) return ret;
    
    // API endpoint
    httpd_uri_t api_uri = {
        .uri = "/api/*",
        .method = HTTP_POST,
        .handler = handle_api_request,
        .user_ctx = nullptr
    };
    ret = httpd_register_uri_handler(server_, &api_uri);
    if (ret != ESP_OK) return ret;
    
    ESP_LOGI(TAG, "All URI handlers registered");
    return ESP_OK;
}

// Handle index page
esp_err_t WebUIAdapter::handle_get_index(httpd_req_t* req) {
    const char* html = R"(<!DOCTYPE html>
<html>
<head>
    <title>ModESP Adaptive UI</title>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        .component { margin: 10px 0; padding: 10px; border: 1px solid #ddd; }
        .hidden { display: none; }
    </style>
</head>
<body>
    <h1>ModESP Adaptive UI</h1>
    <div id="components"></div>
    <script>
        async function loadUI() {
            const response = await fetch('/api/ui/data');
            const data = await response.json();
            const container = document.getElementById('components');
            
            data.components.forEach(comp => {
                const div = document.createElement('div');
                div.className = 'component';
                div.innerHTML = `<h3>${comp.label}</h3><p>Type: ${comp.type}</p>`;
                container.appendChild(div);
            });
        }
        loadUI();
    </script>
</body>
</html>)";
    
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_sendstr(req, html);
}

// Handle UI data request
esp_err_t WebUIAdapter::handle_get_ui_data(httpd_req_t* req) {
    if (instance_ == nullptr) {
        return ESP_FAIL;
    }
    
    nlohmann::json response = instance_->getComponentsJson();
    return instance_->send_json_response(req, response);
}

// Handle API request
esp_err_t WebUIAdapter::handle_api_request(httpd_req_t* req) {
    if (instance_ == nullptr) {
        return ESP_FAIL;
    }
    
    // Get request body
    char content[1024];
    int received = httpd_req_recv(req, content, sizeof(content) - 1);
    if (received <= 0) {
        return instance_->send_error_response(req, 400, "No data received");
    }
    content[received] = '\0';
    
    // Validate JSON content first
    if (received == 0 || content[0] == '\0') {
        return instance_->send_error_response(req, 400, "Empty request body");
    }
    
    // Safe JSON parsing - check for basic JSON structure
    nlohmann::json request;
    if (content[0] != '{' && content[0] != '[') {
        return instance_->send_error_response(req, 400, "Invalid JSON format");
    }
    
    // Parse JSON with error checking
    bool parse_success = false;
    std::string parse_error;
    
    // Use nlohmann::json::accept to check validity first
    if (nlohmann::json::accept(content)) {
        request = nlohmann::json::parse(content, nullptr, false);
        parse_success = !request.is_discarded();
    }
    
    if (!parse_success) {
        return instance_->send_error_response(req, 400, "JSON parse error");
    }
    
    // Extract method from URI
    std::string uri(req->uri);
    if (uri.length() <= 5) {
        return instance_->send_error_response(req, 400, "Invalid API endpoint");
    }
    std::string method = uri.substr(5); // Remove "/api/"
    
    // Handle request
    nlohmann::json response = instance_->api_handler_->handleRequest(method, request);
    return instance_->send_json_response(req, response);
}

// Render components to HTML
std::string WebUIAdapter::renderComponents() {
    std::stringstream html;
    
    // Get filtered components
    auto visible_components = filter_->getVisibleComponents();
    
    for (const auto& comp_id : visible_components) {
        // Lazy load component if needed
        auto* component = loader_->getComponent(comp_id);
        if (component) {
            // TODO: Implement component rendering
            html << "<div class='component'>" << comp_id << "</div>\n";
        }
    }
    
    return html.str();
}

// Get components as JSON
nlohmann::json WebUIAdapter::getComponentsJson() {
    nlohmann::json result;
    result["components"] = nlohmann::json::array();
    
    // Get filtered components
    auto visible_components = filter_->getVisibleComponents();
    
    for (const auto& comp_id : visible_components) {
        nlohmann::json comp_data;
        comp_data["id"] = comp_id;
        comp_data["type"] = "unknown"; // TODO: Get from component metadata
        comp_data["label"] = comp_id; // TODO: Get proper label
        
        result["components"].push_back(comp_data);
    }
    
    return result;
}

// Send JSON response
esp_err_t WebUIAdapter::send_json_response(httpd_req_t* req, const nlohmann::json& data) {
    httpd_resp_set_type(req, "application/json");
    std::string json_str = data.dump();
    return httpd_resp_sendstr(req, json_str.c_str());
}

// Send error response
esp_err_t WebUIAdapter::send_error_response(httpd_req_t* req, int code, const std::string& message) {
    httpd_resp_set_status(req, std::to_string(code).c_str());
    nlohmann::json error;
    error["error"] = message;
    error["code"] = code;
    return send_json_response(req, error);
}

} // namespace ModESP::UI
