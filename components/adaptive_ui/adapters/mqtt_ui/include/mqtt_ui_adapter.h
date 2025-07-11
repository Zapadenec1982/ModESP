/**
 * @file mqtt_ui_adapter.h
 * @brief MQTT protocol adapter for UI
 */

#ifndef MQTT_UI_ADAPTER_H
#define MQTT_UI_ADAPTER_H

#include "ui_adapter_base.h"
#include "mqtt_client.h"
#include <set>

/**
 * @brief MQTT UI Adapter
 * 
 * Automatically exposes module functionality over MQTT:
 * - Telemetry publishing
 * - Command subscription
 * - Discovery for Home Assistant
 * - Last Will and Testament
 */
class MQTTUIAdapter : public UIAdapterBase {
public:
    MQTTUIAdapter();
    ~MQTTUIAdapter() override;
    
    // BaseModule interface
    const char* get_name() const override { return "MQTT_UI"; }
    void configure(const nlohmann::json& config) override;
    void stop() override;
    bool is_healthy() const override;
    uint8_t get_health_score() const override;
    uint32_t get_max_update_time_us() const override { return 10000; }
    
protected:
    // UIAdapterBase interface
    void register_ui_handlers() override;
    
private:
    // Configuration
    struct Config {
        bool enabled = true;
        std::string broker_url = "mqtt://localhost:1883";
        std::string client_id = "modesp";
        std::string base_topic = "modesp";
        std::string username;
        std::string password;
        int keepalive = 60;
        int qos = 1;
        bool retained = true;
        bool discovery = true;  // Home Assistant discovery
        int telemetry_interval_s = 60;
    } config_;
    
    // MQTT client
    esp_mqtt_client_handle_t mqtt_client_ = nullptr;
    bool connected_ = false;
    
    // Topic management
    struct TopicMapping {
        std::string topic;
        std::string method;
        std::string source;  // For telemetry
        int interval_s;
    };
    std::vector<TopicMapping> telemetry_topics_;
    std::map<std::string, std::string> command_topics_;  // topic -> method
    
    // Telemetry timers
    std::map<std::string, int64_t> last_telemetry_time_;
    
    // MQTT operations
    esp_err_t connect();
    esp_err_t disconnect();
    void publish(const std::string& topic, const nlohmann::json& data, bool retained = false);
    void subscribe(const std::string& topic);
    
    // Topic generation
    std::string get_telemetry_topic(const std::string& module, const std::string& key);
    std::string get_command_topic(const std::string& module, const std::string& command);
    std::string get_status_topic();
    
    // Discovery
    void publish_discovery();
    nlohmann::json create_discovery_config(const std::string& module, 
                                          const nlohmann::json& control);
    
    // Handlers
    void handle_telemetry_update();
    void handle_mqtt_message(const std::string& topic, const std::string& data);
    
    // Event handlers
    static void mqtt_event_handler(void* handler_args, esp_event_base_t base,
                                  int32_t event_id, void* event_data);
    
    // Module-specific setup
    void setup_module_topics(const std::string& module, const nlohmann::json& schema);
    void setup_telemetry(const std::string& module, const nlohmann::json& telemetry_config);
    void setup_commands(const std::string& module, const nlohmann::json& controls);
};

#endif // MQTT_UI_ADAPTER_H