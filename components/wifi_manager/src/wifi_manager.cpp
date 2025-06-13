/**
 * @file wifi_manager.cpp
 * @brief Implementation of WiFiManager module
 * 
 * WiFiManager provides network connectivity management for the ModuChill system.
 * It handles WiFi connection, monitoring, and automatic reconnection with proper
 * error handling and status reporting.
 */

#include "wifi_manager.h"
#include "shared_state.h"
#include "event_bus.h"
#include "error_handling.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <esp_event.h>
#include <lwip/ip4_addr.h>
#include <cstring>

static const char* TAG = "WiFiManager";

// === WiFiConfig implementation ===

WiFiConfig WiFiConfig::from_json(const nlohmann::json& config) {
    WiFiConfig wifi_config;
    
    if (config.contains("enabled")) {
        wifi_config.enabled = config["enabled"].get<bool>();
    }
    if (config.contains("ssid")) {
        wifi_config.ssid = config["ssid"].get<std::string>();
    }
    if (config.contains("password")) {
        wifi_config.password = config["password"].get<std::string>();
    }
    if (config.contains("static_ip")) {
        wifi_config.static_ip = config["static_ip"].get<bool>();
    }
    if (config.contains("ip")) {
        wifi_config.ip = config["ip"].get<std::string>();
    }
    if (config.contains("gateway")) {
        wifi_config.gateway = config["gateway"].get<std::string>();
    }
    if (config.contains("subnet")) {
        wifi_config.subnet = config["subnet"].get<std::string>();
    }
    if (config.contains("dns")) {
        wifi_config.dns = config["dns"].get<std::string>();
    }
    if (config.contains("reconnect_interval")) {
        wifi_config.reconnect_interval_ms = config["reconnect_interval"].get<uint32_t>();
    }
    if (config.contains("max_reconnect_attempts")) {
        wifi_config.max_reconnect_attempts = config["max_reconnect_attempts"].get<uint8_t>();
    }
    
    return wifi_config;
}

nlohmann::json WiFiConfig::to_json() const {
    return nlohmann::json{
        {"enabled", enabled},
        {"ssid", ssid},
        {"password", password},
        {"static_ip", static_ip},
        {"ip", ip},
        {"gateway", gateway},
        {"subnet", subnet},
        {"dns", dns},
        {"reconnect_interval", reconnect_interval_ms},
        {"max_reconnect_attempts", max_reconnect_attempts}
    };
}

// === WiFiStatus implementation ===

nlohmann::json WiFiStatus::to_json() const {
    std::string state_str;
    switch (state) {
        case WiFiState::DISABLED: state_str = "DISABLED"; break;
        case WiFiState::DISCONNECTED: state_str = "DISCONNECTED"; break;
        case WiFiState::CONNECTING: state_str = "CONNECTING"; break;
        case WiFiState::CONNECTED: state_str = "CONNECTED"; break;
        case WiFiState::RECONNECTING: state_str = "RECONNECTING"; break;
        case WiFiState::ERROR: state_str = "ERROR"; break;
    }
    
    return nlohmann::json{
        {"state", state_str},
        {"ssid", ssid},
        {"ip_address", ip_address},
        {"gateway", gateway},
        {"subnet_mask", subnet_mask},
        {"rssi", rssi},
        {"channel", channel},
        {"connection_time_ms", connection_time_ms},
        {"reconnect_count", reconnect_count},
        {"disconnect_count", disconnect_count},
        {"is_healthy", is_healthy}
    };
}

// === WiFiManager implementation ===

WiFiManager::WiFiManager() {
    ESP_LOGI(TAG, "WiFiManager created");
}

WiFiManager::~WiFiManager() {
    stop();
}

esp_err_t WiFiManager::init() {
    using namespace ModESP;
    
    if (initialized_) {
        ESP_LOGW(TAG, "WiFiManager already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing WiFiManager...");
    
    esp_err_t result = safe_execute("wifi_init", [this]() -> esp_err_t {
        return init_wifi_stack();
    });
    
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi stack: %s", esp_err_to_name(result));
        return result;
    }
    
    // Initialize status
    status_.state = WiFiState::DISABLED;
    status_.is_healthy = true;
    
    // Auto-start connection if WiFi is enabled and configured
    if (config_.enabled && !config_.ssid.empty()) {
        ESP_LOGI(TAG, "Auto-starting WiFi connection...");
        set_state(WiFiState::DISCONNECTED);
        esp_err_t connect_result = start_connection();
        if (connect_result != ESP_OK) {
            ESP_LOGW(TAG, "Failed to auto-start connection: %s", esp_err_to_name(connect_result));
        }
    }
    
    initialized_ = true;
    ESP_LOGI(TAG, "WiFiManager initialized successfully");
    
    return ESP_OK;
}

void WiFiManager::configure(const nlohmann::json& config) {
    using namespace ModESP;
    
    ESP_LOGI(TAG, "Configuring WiFiManager");
    
    safe_execute("wifi_configure", [&]() -> esp_err_t {
        config_ = WiFiConfig::from_json(config);
        
        ESP_LOGI(TAG, "WiFi enabled: %s", config_.enabled ? "true" : "false");
        if (config_.enabled && !config_.ssid.empty()) {
            ESP_LOGI(TAG, "WiFi SSID: %s", config_.ssid.c_str());
            ESP_LOGI(TAG, "Static IP: %s", config_.static_ip ? "enabled" : "disabled");
        }
        
        // Just save the configuration, connection will be started in init() if WiFi stack is ready
        if (config_.enabled && !config_.ssid.empty()) {
            ESP_LOGI(TAG, "WiFi configuration saved, will connect after initialization");
        } else {
            ESP_LOGI(TAG, "WiFi disabled in configuration");
        }
        
        return ESP_OK;
    });
    
    publish_status();
}

void WiFiManager::update() {
    if (!initialized_) {
        return;
    }
    
    uint32_t now_ms = esp_timer_get_time() / 1000;
    
    // Update connection time if connected
    if (status_.state == WiFiState::CONNECTED && connection_start_time_ms_ > 0) {
        status_.connection_time_ms = now_ms - connection_start_time_ms_;
    }
    
    // Handle connection timeout (15 seconds)
    if ((status_.state == WiFiState::CONNECTING || status_.state == WiFiState::RECONNECTING) && 
        connection_start_time_ms_ > 0 && 
        (now_ms - connection_start_time_ms_) > 15000) {
        ESP_LOGW(TAG, "Connection timeout after 15 seconds");
        set_state(WiFiState::DISCONNECTED);
        connection_start_time_ms_ = 0;
    }
    
    // Handle reconnection logic
    if (should_attempt_reconnect()) {
        if (now_ms - last_connect_attempt_ms_ >= config_.reconnect_interval_ms) {
            ESP_LOGI(TAG, "Attempting reconnection (attempt %d/%d)", 
                     reconnect_attempts_ + 1, config_.max_reconnect_attempts);
            
            set_state(WiFiState::RECONNECTING);
            start_connection();
            last_connect_attempt_ms_ = now_ms;
            reconnect_attempts_++;
        }
    }
    
    // Update and publish status periodically
    static uint32_t last_status_update = 0;
    if (now_ms - last_status_update >= 5000) { // Every 5 seconds
        update_status();
        publish_status();
        last_status_update = now_ms;
    }
}

void WiFiManager::stop() {
    if (!initialized_) {
        return;
    }
    
    ESP_LOGI(TAG, "Stopping WiFiManager");
    
    // Disconnect WiFi
    disconnect();
    
    // Deinitialize WiFi stack
    deinit_wifi_stack();
    
    set_state(WiFiState::DISABLED);
    initialized_ = false;
    
    ESP_LOGI(TAG, "WiFiManager stopped");
}

bool WiFiManager::is_healthy() const {
    if (!initialized_) {
        return false;
    }
    
    // Consider healthy if disabled or connected
    return status_.state == WiFiState::DISABLED || 
           status_.state == WiFiState::CONNECTED ||
           (status_.state == WiFiState::CONNECTING && reconnect_attempts_ < config_.max_reconnect_attempts);
}

uint8_t WiFiManager::get_health_score() const {
    if (!initialized_) {
        return 0;
    }
    
    switch (status_.state) {
        case WiFiState::CONNECTED:
            // Health based on signal strength
            if (status_.rssi > -50) return 100;
            if (status_.rssi > -60) return 90;
            if (status_.rssi > -70) return 80;
            if (status_.rssi > -80) return 70;
            return 60;
            
        case WiFiState::DISABLED:
            return 100; // Healthy if intentionally disabled
            
        case WiFiState::CONNECTING:
        case WiFiState::RECONNECTING:
            return 50; // Partial health while connecting
            
        case WiFiState::DISCONNECTED:
            return 30; // Poor health if should be connected
            
        case WiFiState::ERROR:
            return 0;
    }
    
    return 0;
}

uint32_t WiFiManager::get_max_update_time_us() const {
    return 1000; // 1ms - WiFi operations are mostly event-driven
}

void WiFiManager::register_rpc(IJsonRpcRegistrar& rpc) {
    // TODO: Implement RPC registration when IJsonRpcRegistrar is available
    // using namespace ModESP;
    
    // safe_execute("wifi_register_rpc", [&]() -> esp_err_t {
    //     rpc.register_method("wifi.get_status", 
    //         [this](const nlohmann::json& params, nlohmann::json& result) -> esp_err_t {
    //             return rpc_get_status(params, result);
    //         });
            
    //     rpc.register_method("wifi.connect",
    //         [this](const nlohmann::json& params, nlohmann::json& result) -> esp_err_t {
    //             return rpc_connect(params, result);
    //         });
            
    //     rpc.register_method("wifi.disconnect",
    //         [this](const nlohmann::json& params, nlohmann::json& result) -> esp_err_t {
    //             return rpc_disconnect(params, result);
    //         });
            
    //     rpc.register_method("wifi.scan",
    //         [this](const nlohmann::json& params, nlohmann::json& result) -> esp_err_t {
    //             return rpc_scan(params, result);
    //         });
            
    //     return ESP_OK;
    // });
}

// === Public API methods ===

WiFiStatus WiFiManager::get_status() const {
    return status_;
}

esp_err_t WiFiManager::connect(const std::string& ssid, const std::string& password) {
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }
    
    config_.ssid = ssid;
    config_.password = password;
    config_.enabled = true;
    
    return start_connection();
}

esp_err_t WiFiManager::disconnect() {
    using namespace ModESP;
    
    if (!initialized_) {
        return ESP_ERR_INVALID_STATE;
    }
    
    return safe_execute("wifi_disconnect", [this]() -> esp_err_t {
        esp_err_t ret = esp_wifi_disconnect();
        if (ret == ESP_OK) {
            set_state(WiFiState::DISCONNECTED);
            reset_reconnect_attempts();
        }
        return ret;
    });
}

nlohmann::json WiFiManager::scan_networks() {
    using namespace ModESP;
    nlohmann::json networks = nlohmann::json::array();
    
    if (!initialized_) {
        return networks;
    }
    
    safe_execute("wifi_scan", [&]() -> esp_err_t {
        wifi_scan_config_t scan_config = {};
        scan_config.ssid = nullptr;
        scan_config.bssid = nullptr;
        scan_config.channel = 0;
        scan_config.show_hidden = false;
        scan_config.scan_type = WIFI_SCAN_TYPE_ACTIVE;
        scan_config.scan_time.active.min = 100;
        scan_config.scan_time.active.max = 300;
        
        esp_err_t ret = esp_wifi_scan_start(&scan_config, true);
        if (ret != ESP_OK) {
            return ret;
        }
        
        uint16_t ap_count = 0;
        esp_wifi_scan_get_ap_num(&ap_count);
        
        if (ap_count > 0) {
            wifi_ap_record_t* ap_records = new wifi_ap_record_t[ap_count];
            esp_wifi_scan_get_ap_records(&ap_count, ap_records);
            
            for (int i = 0; i < ap_count; i++) {
                nlohmann::json network;
                network["ssid"] = std::string((char*)ap_records[i].ssid);
                network["rssi"] = ap_records[i].rssi;
                network["channel"] = ap_records[i].primary;
                network["authmode"] = static_cast<int>(ap_records[i].authmode);
                networks.push_back(network);
            }
            
            delete[] ap_records;
        }
        
        return ESP_OK;
    });
    
    return networks;
}

bool WiFiManager::is_connected() const {
    return status_.state == WiFiState::CONNECTED;
}

std::string WiFiManager::get_ip_address() const {
    return status_.ip_address;
}

// === Private helper methods ===

esp_err_t WiFiManager::init_wifi_stack() {
    // Initialize TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // Create default WiFi station
    netif_sta_ = esp_netif_create_default_wifi_sta();
    if (!netif_sta_) {
        ESP_LOGE(TAG, "Failed to create default WiFi station");
        return ESP_FAIL;
    }
    
    // Initialize WiFi with default config
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Register event handlers
    ret = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                             &wifi_event_handler_static, this,
                                             &wifi_event_handler_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register WiFi event handler: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                             &ip_event_handler_static, this,
                                             &ip_event_handler_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register IP event handler: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Set WiFi mode to station
    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi mode: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Start WiFi
    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WiFi: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "WiFi stack initialized successfully");
    return ESP_OK;
}

esp_err_t WiFiManager::deinit_wifi_stack() {
    // Unregister event handlers
    if (wifi_event_handler_) {
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler_);
        wifi_event_handler_ = nullptr;
    }
    
    if (ip_event_handler_) {
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, ip_event_handler_);
        ip_event_handler_ = nullptr;
    }
    
    // Stop and deinitialize WiFi
    esp_wifi_stop();
    esp_wifi_deinit();
    
    // Destroy network interface
    if (netif_sta_) {
        esp_netif_destroy_default_wifi(netif_sta_);
        netif_sta_ = nullptr;
    }
    
    return ESP_OK;
}

esp_err_t WiFiManager::start_connection() {
    if (config_.ssid.empty()) {
        ESP_LOGE(TAG, "Cannot connect: SSID is empty");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Configure static IP if enabled
    if (config_.static_ip) {
        esp_err_t ret = configure_static_ip();
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to configure static IP: %s", esp_err_to_name(ret));
            return ret;
        }
    }
    
    // Configure WiFi connection
    wifi_config_t wifi_config = {};
    strncpy((char*)wifi_config.sta.ssid, config_.ssid.c_str(), sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char*)wifi_config.sta.password, config_.password.c_str(), sizeof(wifi_config.sta.password) - 1);
    
    // Set minimum authentication mode to WPA_PSK (allows WPA, WPA2, WPA3)
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;
    
    // Enable scan method for better connection reliability
    wifi_config.sta.scan_method = WIFI_FAST_SCAN;
    wifi_config.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;
    wifi_config.sta.threshold.rssi = -127; // Accept any signal strength
    
    ESP_LOGI(TAG, "WiFi config: SSID=%s, Auth=WPA_PSK+, PMF=capable", config_.ssid.c_str());
    
    esp_err_t ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set WiFi config: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Start connection
    ret = esp_wifi_connect();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WiFi connection: %s", esp_err_to_name(ret));
        return ret;
    }
    
    set_state(WiFiState::CONNECTING);
    connection_start_time_ms_ = esp_timer_get_time() / 1000;
    
    ESP_LOGI(TAG, "Connecting to WiFi network: %s", config_.ssid.c_str());
    return ESP_OK;
}

esp_err_t WiFiManager::configure_static_ip() {
    if (config_.ip.empty() || config_.gateway.empty() || config_.subnet.empty()) {
        ESP_LOGE(TAG, "Static IP configuration incomplete");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Stop DHCP client
    esp_netif_dhcpc_stop(netif_sta_);
    
    // Configure static IP
    esp_netif_ip_info_t ip_info;
    memset(&ip_info, 0, sizeof(esp_netif_ip_info_t));
    
    ip_info.ip.addr = esp_ip4addr_aton(config_.ip.c_str());
    ip_info.gw.addr = esp_ip4addr_aton(config_.gateway.c_str());
    ip_info.netmask.addr = esp_ip4addr_aton(config_.subnet.c_str());
    
    esp_err_t ret = esp_netif_set_ip_info(netif_sta_, &ip_info);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set static IP: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Configure DNS if provided
    if (!config_.dns.empty()) {
        esp_netif_dns_info_t dns_info;
        dns_info.ip.u_addr.ip4.addr = esp_ip4addr_aton(config_.dns.c_str());
        dns_info.ip.type = ESP_IPADDR_TYPE_V4;
        esp_netif_set_dns_info(netif_sta_, ESP_NETIF_DNS_MAIN, &dns_info);
    }
    
    ESP_LOGI(TAG, "Static IP configured: %s", config_.ip.c_str());
    return ESP_OK;
}

void WiFiManager::handle_wifi_event(esp_event_base_t event_base, int32_t event_id, void* event_data) {
    switch (event_id) {
        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG, "WiFi station started");
            break;
            
        case WIFI_EVENT_STA_CONNECTED: {
            wifi_event_sta_connected_t* event = (wifi_event_sta_connected_t*)event_data;
            ESP_LOGI(TAG, "Connected to AP: %s, channel: %d, authmode: %d", 
                     event->ssid, event->channel, event->authmode);
            status_.ssid = std::string((char*)event->ssid);
            status_.channel = event->channel;
            break;
        }
        
        case WIFI_EVENT_STA_DISCONNECTED: {
            wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*)event_data;
            
            // Log detailed disconnect reason
            const char* reason_str = "UNKNOWN";
            switch (event->reason) {
                case WIFI_REASON_UNSPECIFIED: reason_str = "UNSPECIFIED"; break;
                case WIFI_REASON_AUTH_EXPIRE: reason_str = "AUTH_EXPIRE"; break;
                case WIFI_REASON_AUTH_LEAVE: reason_str = "AUTH_LEAVE"; break;
                case WIFI_REASON_ASSOC_EXPIRE: reason_str = "ASSOC_EXPIRE"; break;
                case WIFI_REASON_ASSOC_TOOMANY: reason_str = "ASSOC_TOOMANY"; break;
                case WIFI_REASON_NOT_AUTHED: reason_str = "NOT_AUTHED"; break;
                case WIFI_REASON_NOT_ASSOCED: reason_str = "NOT_ASSOCED"; break;
                case WIFI_REASON_ASSOC_LEAVE: reason_str = "ASSOC_LEAVE"; break;
                case WIFI_REASON_ASSOC_NOT_AUTHED: reason_str = "ASSOC_NOT_AUTHED"; break;
                case WIFI_REASON_DISASSOC_PWRCAP_BAD: reason_str = "DISASSOC_PWRCAP_BAD"; break;
                case WIFI_REASON_DISASSOC_SUPCHAN_BAD: reason_str = "DISASSOC_SUPCHAN_BAD"; break;
                case WIFI_REASON_BSS_TRANSITION_DISASSOC: reason_str = "BSS_TRANSITION_DISASSOC"; break;
                case WIFI_REASON_IE_INVALID: reason_str = "IE_INVALID"; break;
                case WIFI_REASON_MIC_FAILURE: reason_str = "MIC_FAILURE"; break;
                case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT: reason_str = "4WAY_HANDSHAKE_TIMEOUT"; break;
                case WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT: reason_str = "GROUP_KEY_UPDATE_TIMEOUT"; break;
                case WIFI_REASON_IE_IN_4WAY_DIFFERS: reason_str = "IE_IN_4WAY_DIFFERS"; break;
                case WIFI_REASON_GROUP_CIPHER_INVALID: reason_str = "GROUP_CIPHER_INVALID"; break;
                case WIFI_REASON_PAIRWISE_CIPHER_INVALID: reason_str = "PAIRWISE_CIPHER_INVALID"; break;
                case WIFI_REASON_AKMP_INVALID: reason_str = "AKMP_INVALID"; break;
                case WIFI_REASON_UNSUPP_RSN_IE_VERSION: reason_str = "UNSUPP_RSN_IE_VERSION"; break;
                case WIFI_REASON_INVALID_RSN_IE_CAP: reason_str = "INVALID_RSN_IE_CAP"; break;
                case WIFI_REASON_802_1X_AUTH_FAILED: reason_str = "802_1X_AUTH_FAILED"; break;
                case WIFI_REASON_CIPHER_SUITE_REJECTED: reason_str = "CIPHER_SUITE_REJECTED"; break;
                case WIFI_REASON_BEACON_TIMEOUT: reason_str = "BEACON_TIMEOUT"; break;
                case WIFI_REASON_NO_AP_FOUND: reason_str = "NO_AP_FOUND"; break;
                case WIFI_REASON_AUTH_FAIL: reason_str = "AUTH_FAIL"; break;
                case WIFI_REASON_ASSOC_FAIL: reason_str = "ASSOC_FAIL"; break;
                case WIFI_REASON_HANDSHAKE_TIMEOUT: reason_str = "HANDSHAKE_TIMEOUT"; break;
                case WIFI_REASON_CONNECTION_FAIL: reason_str = "CONNECTION_FAIL"; break;
                case WIFI_REASON_AP_TSF_RESET: reason_str = "AP_TSF_RESET"; break;
                case WIFI_REASON_ROAMING: reason_str = "ROAMING"; break;
            }
            
            ESP_LOGW(TAG, "Disconnected from AP: %s, reason: %d (%s)", 
                     event->ssid, event->reason, reason_str);
            
            status_.disconnect_count++;
            
            if (status_.state == WiFiState::CONNECTED) {
                set_state(WiFiState::DISCONNECTED);
                last_connect_attempt_ms_ = 0; // Allow immediate reconnection attempt
            } else if (status_.state == WiFiState::CONNECTING) {
                // Connection failed during initial attempt
                set_state(WiFiState::DISCONNECTED);
                ESP_LOGE(TAG, "Initial connection failed: %s", reason_str);
            }
            
            publish_event("wifi.disconnected", {
                {"ssid", std::string((char*)event->ssid)},
                {"reason", event->reason},
                {"reason_str", reason_str}
            });
            break;
        }
        
        default:
            ESP_LOGD(TAG, "WiFi event: %ld", event_id);
            break;
    }
    
    update_status();
    publish_status();
}

void WiFiManager::handle_ip_event(esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
        
        char ip_str[16];
        esp_ip4addr_ntoa(&event->ip_info.ip, ip_str, sizeof(ip_str));
        
        ESP_LOGI(TAG, "Got IP address: %s", ip_str);
        
        status_.ip_address = std::string(ip_str);
        esp_ip4addr_ntoa(&event->ip_info.gw, ip_str, sizeof(ip_str));
        status_.gateway = std::string(ip_str);
        esp_ip4addr_ntoa(&event->ip_info.netmask, ip_str, sizeof(ip_str));
        status_.subnet_mask = std::string(ip_str);
        
        set_state(WiFiState::CONNECTED);
        reset_reconnect_attempts();
        
        publish_event("wifi.connected", {
            {"ssid", status_.ssid},
            {"ip_address", status_.ip_address}
        });
    }
    
    update_status();
    publish_status();
}

void WiFiManager::update_status() {
    if (status_.state == WiFiState::CONNECTED) {
        // Get WiFi info
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            status_.rssi = ap_info.rssi;
        }
        
        status_.is_healthy = true;
    } else {
        status_.rssi = 0;
        status_.is_healthy = (status_.state == WiFiState::DISABLED);
    }
}

void WiFiManager::publish_status() {
    SharedState::set("state.network.wifi", status_.to_json());
}

void WiFiManager::publish_event(const std::string& event_type, const nlohmann::json& data) {
    nlohmann::json event_data = data;
    event_data["timestamp"] = esp_timer_get_time() / 1000;
    EventBus::publish(event_type, event_data);
}

void WiFiManager::set_state(WiFiState new_state) {
    if (status_.state != new_state) {
        WiFiState old_state = status_.state;
        status_.state = new_state;
        
        ESP_LOGI(TAG, "State changed: %d -> %d", (int)old_state, (int)new_state);
        
        publish_event("wifi.state_changed", {
            {"old_state", (int)old_state},
            {"new_state", (int)new_state}
        });
    }
}

bool WiFiManager::should_attempt_reconnect() const {
    return config_.enabled && 
           !config_.ssid.empty() &&
           (status_.state == WiFiState::DISCONNECTED || status_.state == WiFiState::RECONNECTING) &&
           reconnect_attempts_ < config_.max_reconnect_attempts;
}

void WiFiManager::reset_reconnect_attempts() {
    reconnect_attempts_ = 0;
    last_connect_attempt_ms_ = 0;
}

// === Static event handlers ===

void WiFiManager::wifi_event_handler_static(void* arg, esp_event_base_t event_base, 
                                           int32_t event_id, void* event_data) {
    WiFiManager* manager = static_cast<WiFiManager*>(arg);
    manager->handle_wifi_event(event_base, event_id, event_data);
}

void WiFiManager::ip_event_handler_static(void* arg, esp_event_base_t event_base,
                                         int32_t event_id, void* event_data) {
    WiFiManager* manager = static_cast<WiFiManager*>(arg);
    manager->handle_ip_event(event_base, event_id, event_data);
}

// === RPC methods ===

esp_err_t WiFiManager::rpc_get_status(const nlohmann::json& params, nlohmann::json& result) {
    result = get_status().to_json();
    return ESP_OK;
}

esp_err_t WiFiManager::rpc_connect(const nlohmann::json& params, nlohmann::json& result) {
    if (!params.contains("ssid")) {
        return ESP_ERR_INVALID_ARG;
    }
    
    std::string ssid = params["ssid"].get<std::string>();
    std::string password = params.value("password", "");
    
    esp_err_t ret = connect(ssid, password);
    result["success"] = (ret == ESP_OK);
    
    return ret;
}

esp_err_t WiFiManager::rpc_disconnect(const nlohmann::json& params, nlohmann::json& result) {
    esp_err_t ret = disconnect();
    result["success"] = (ret == ESP_OK);
    return ret;
}

esp_err_t WiFiManager::rpc_scan(const nlohmann::json& params, nlohmann::json& result) {
    result["networks"] = scan_networks();
    return ESP_OK;
} 