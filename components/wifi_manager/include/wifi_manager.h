/**
 * @file wifi_manager.h
 * @brief WiFiManager - Network connectivity module for ModuChill
 * 
 * WiFiManager handles all WiFi connectivity operations including:
 * - Station mode connection to access points
 * - Connection monitoring and auto-reconnection
 * - Network status reporting to SharedState
 * - Configuration management through JSON
 * 
 * Key features:
 * - Automatic reconnection with exponential backoff
 * - Static IP configuration support
 * - Connection status monitoring
 * - Event-driven architecture
 * - Health monitoring and diagnostics
 */

#pragma once

#include "base_module.h"
#include "shared_state.h"
#include "event_bus.h"
#include "error_handling.h"
#include "nlohmann/json.hpp"
#include <esp_wifi.h>
#include <esp_netif.h>
#include <esp_event.h>
#include <string>
#include <memory>

/**
 * @brief WiFi connection states
 */
enum class WiFiState {
    DISABLED,           // WiFi is disabled
    DISCONNECTED,       // WiFi enabled but not connected
    CONNECTING,         // Attempting to connect
    CONNECTED,          // Connected to AP
    RECONNECTING,       // Attempting to reconnect after disconnect
    ERROR               // Error state
};

/**
 * @brief WiFi configuration structure
 */
struct WiFiConfig {
    bool enabled = false;
    std::string ssid;
    std::string password;
    bool static_ip = false;
    std::string ip;
    std::string gateway;
    std::string subnet;
    std::string dns;
    uint32_t reconnect_interval_ms = 30000;  // 30 seconds
    uint8_t max_reconnect_attempts = 10;
    
    // Parse from JSON
    static WiFiConfig from_json(const nlohmann::json& config);
    nlohmann::json to_json() const;
};

/**
 * @brief WiFi status information
 */
struct WiFiStatus {
    WiFiState state = WiFiState::DISABLED;
    std::string ssid;
    std::string ip_address;
    std::string gateway;
    std::string subnet_mask;
    int8_t rssi = 0;
    uint8_t channel = 0;
    uint32_t connection_time_ms = 0;
    uint32_t reconnect_count = 0;
    uint32_t disconnect_count = 0;
    bool is_healthy = false;
    
    nlohmann::json to_json() const;
};

/**
 * @brief WiFiManager - Network connectivity module
 * 
 * Manages WiFi connectivity for the ModuChill system. Handles connection
 * establishment, monitoring, and automatic reconnection with proper error
 * handling and status reporting.
 */
class WiFiManager : public BaseModule {
public:
    /**
     * @brief Constructor
     */
    WiFiManager();
    
    /**
     * @brief Destructor
     */
    ~WiFiManager() override;
    
    // Non-copyable, non-movable
    WiFiManager(const WiFiManager&) = delete;
    WiFiManager& operator=(const WiFiManager&) = delete;
    WiFiManager(WiFiManager&&) = delete;
    WiFiManager& operator=(WiFiManager&&) = delete;

    // === BaseModule interface ===
    
    const char* get_name() const override {
        return "WiFi";  // Config section: "wifi"
    }
    
    esp_err_t init() override;
    void update() override;
    void stop() override;
    void configure(const nlohmann::json& config) override;
    bool is_healthy() const override;
    uint8_t get_health_score() const override;
    uint32_t get_max_update_time_us() const override;
    void register_rpc(IJsonRpcRegistrar& rpc) override;
    
    // === WiFiManager specific methods ===
    
    /**
     * @brief Get current WiFi status
     * @return Current status information
     */
    WiFiStatus get_status() const;
    
    /**
     * @brief Connect to WiFi network
     * @param ssid Network SSID
     * @param password Network password
     * @return ESP_OK on success
     */
    esp_err_t connect(const std::string& ssid, const std::string& password);
    
    /**
     * @brief Disconnect from current network
     * @return ESP_OK on success
     */
    esp_err_t disconnect();
    
    /**
     * @brief Scan for available networks
     * @return JSON array of available networks
     */
    nlohmann::json scan_networks();
    
    /**
     * @brief Check if connected to WiFi
     * @return true if connected
     */
    bool is_connected() const;
    
    /**
     * @brief Get current IP address
     * @return IP address string or empty if not connected
     */
    std::string get_ip_address() const;

private:
    // Configuration
    WiFiConfig config_;
    
    // Status tracking
    WiFiStatus status_;
    bool initialized_ = false;
    
    // Connection management
    uint32_t last_connect_attempt_ms_ = 0;
    uint32_t connection_start_time_ms_ = 0;
    uint8_t reconnect_attempts_ = 0;
    
    // ESP-IDF handles
    esp_netif_t* netif_sta_ = nullptr;
    
    // Event handling
    esp_event_handler_instance_t wifi_event_handler_ = nullptr;
    esp_event_handler_instance_t ip_event_handler_ = nullptr;
    
    // Helper methods
    esp_err_t init_wifi_stack();
    esp_err_t deinit_wifi_stack();
    esp_err_t start_connection();
    esp_err_t configure_static_ip();
    void handle_wifi_event(esp_event_base_t event_base, int32_t event_id, void* event_data);
    void handle_ip_event(esp_event_base_t event_base, int32_t event_id, void* event_data);
    void update_status();
    void publish_status();
    void publish_event(const std::string& event_type, const nlohmann::json& data = {});
    
    // State management
    void set_state(WiFiState new_state);
    bool should_attempt_reconnect() const;
    void reset_reconnect_attempts();
    
    // Static event handlers (required by ESP-IDF)
    static void wifi_event_handler_static(void* arg, esp_event_base_t event_base, 
                                         int32_t event_id, void* event_data);
    static void ip_event_handler_static(void* arg, esp_event_base_t event_base,
                                       int32_t event_id, void* event_data);
    
    // RPC methods
    esp_err_t rpc_get_status(const nlohmann::json& params, nlohmann::json& result);
    esp_err_t rpc_connect(const nlohmann::json& params, nlohmann::json& result);
    esp_err_t rpc_disconnect(const nlohmann::json& params, nlohmann::json& result);
    esp_err_t rpc_scan(const nlohmann::json& params, nlohmann::json& result);
}; 