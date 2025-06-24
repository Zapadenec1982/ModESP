/**
 * @file web_ui_module_optimized.cpp
 * @brief Optimized Web UI Module using compile-time generated UI
 */

#include "web_ui_module.h"
#include "generated/web_ui_generated.h"
#include "generated/ui_registry_generated.h"
#include "system_contract.h"
#include "shared_state.h"
#include "esp_log.h"

static const char* TAG = "WebUI";

// HTTP handlers implementation
esp_err_t WebUIModule::handle_static_get(httpd_req_t* req) {
    WebUIModule* module = (WebUIModule*)req->user_ctx;
    
    // Serve pre-generated HTML from PROGMEM
    if (strcmp(req->uri, "/") == 0 || strcmp(req->uri, "/index.html") == 0) {
        httpd_resp_set_type(req, "text/html");
        httpd_resp_set_hdr(req, "Cache-Control", "max-age=3600");
        
        // Send compressed if client supports it
        char encoding[100];
        if (httpd_req_get_hdr_value_str(req, "Accept-Encoding", encoding, sizeof(encoding)) == ESP_OK) {
            if (strstr(encoding, "gzip") != nullptr) {
                httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
                // TODO: Serve pre-compressed version
            }
        }
        
        // HTML is in PROGMEM, no RAM usage
        return httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
    }
    
    // 404 for unknown files
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not Found");
    return ESP_FAIL;
}

esp_err_t WebUIModule::handle_api_request(httpd_req_t* req) {
    WebUIModule* module = (WebUIModule*)req->user_ctx;
    
    // Efficient API data endpoint
    if (strcmp(req->uri, "/api/data") == 0) {
        return module->handle_data_request(req);
    }
    
    // RPC endpoint
    if (strcmp(req->uri, "/api/rpc") == 0) {
        return module->handle_rpc_request(req);
    }
    
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Unknown API endpoint");
    return ESP_FAIL;
}

esp_err_t WebUIModule::handle_data_request(httpd_req_t* req) {
    // Build response from UI element map
    nlohmann::json response;
    
    // Iterate through generated element map
    for (size_t i = 0; i < UI_ELEMENT_COUNT; i++) {
        UIElementMap elem;
        // Copy from PROGMEM
        memcpy_P(&elem, &UI_ELEMENT_MAP[i], sizeof(UIElementMap));
        
        // Get value from SharedState
        nlohmann::json value;
        if (strlen(elem.state_key) > 0) {
            // Try to get from SharedState first
            float float_val;
            if (SharedState::get(elem.state_key, float_val)) {
                value = float_val;
            } else {
                bool bool_val;
                if (SharedState::get(elem.state_key, bool_val)) {
                    value = bool_val;
                } else {
                    // If not in SharedState, try RPC method
                    if (strlen(elem.rpc_method) > 0) {
                        nlohmann::json params;
                        api_dispatcher_->execute(elem.rpc_method, params, value);
                    }
                }
            }
        }
        
        if (!value.is_null()) {
            response[elem.element_id] = value;
        }
    }
    
    // Send compact JSON response
    return send_json_response(req, response);
}

esp_err_t WebUIModule::handle_rpc_request(httpd_req_t* req) {
    // Read request body
    char* buf = nullptr;
    size_t buf_len = httpd_req_get_content_length(req);
    
    if (buf_len > 0) {
        buf = (char*)malloc(buf_len + 1);
        if (!buf) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
            return ESP_FAIL;
        }
        
        int ret = httpd_req_recv(req, buf, buf_len);
        if (ret <= 0) {
            free(buf);
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to read request");
            return ESP_FAIL;
        }
        buf[buf_len] = '\0';
        
        // Parse JSON-RPC request
        try {
            nlohmann::json request = nlohmann::json::parse(buf);
            nlohmann::json response;
            
            // Execute through dispatcher
            esp_err_t err = api_dispatcher_->execute_json_rpc(request, response);
            
            free(buf);
            
            if (err == ESP_OK) {
                return send_json_response(req, response);
            } else {
                return send_error_response(req, "RPC execution failed", 500);
            }
        } catch (const std::exception& e) {
            free(buf);
            return send_error_response(req, "Invalid JSON", 400);
        }
    }
    
    return send_error_response(req, "Empty request", 400);
}

// Optimized server configuration
esp_err_t WebUIModule::start_server() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    
    // Optimize for embedded use
    config.task_priority = 5;
    config.stack_size = 4096;  // Reduced stack size
    config.core_id = 1;        // Run on APP CPU
    config.max_open_sockets = 4;
    config.recv_wait_timeout = 5;
    config.send_wait_timeout = 5;
    config.lru_purge_enable = true;
    
    // Custom URI match function for efficiency
    config.uri_match_fn = httpd_uri_match_wildcard;
    
    ESP_LOGI(TAG, "Starting HTTP server on port %d", config.server_port);
    
    esp_err_t ret = httpd_start(&server_, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Register URI handlers
    httpd_uri_t static_get = {
        .uri = "/*",
        .method = HTTP_GET,
        .handler = handle_static_get,
        .user_ctx = this
    };
    httpd_register_uri_handler(server_, &static_get);
    
    httpd_uri_t api_handler = {
        .uri = "/api/*",
        .method = HTTP_GET,
        .handler = handle_api_request,
        .user_ctx = this
    };
    httpd_register_uri_handler(server_, &api_handler);
    
    httpd_uri_t api_post_handler = {
        .uri = "/api/*",
        .method = HTTP_POST,
        .handler = handle_api_request,
        .user_ctx = this
    };
    httpd_register_uri_handler(server_, &api_post_handler);
    
    ESP_LOGI(TAG, "HTTP server started successfully");
    return ESP_OK;
}

// Minimal update method
void WebUIModule::update() {
    // Update metrics periodically
    static int64_t last_metrics_update = 0;
    int64_t now = esp_timer_get_time();
    
    if (now - last_metrics_update > 10000000) {  // Every 10 seconds
        // Update metrics in SharedState
        nlohmann::json metrics = {
            {"requests", metrics_.total_requests},
            {"errors", metrics_.total_errors},
            {"connections", metrics_.active_connections},
            {"avg_response_ms", metrics_.total_response_time_us / 1000 / max(metrics_.total_requests, 1UL)}
        };
        
        SharedState::set(ModespContract::State::ApiMetrics, metrics);
        last_metrics_update = now;
    }
}