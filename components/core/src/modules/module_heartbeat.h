/**
 * @file module_heartbeat.h
 * @brief Module health monitoring and watchdog functionality
 * 
 * Monitors module activity and automatically restarts unresponsive modules.
 * Designed for minimal resource usage on ESP32.
 */

#ifndef MODULE_HEARTBEAT_H
#define MODULE_HEARTBEAT_H

#include "base_module.h"
#include <unordered_map>
#include <string>
#include <chrono>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"

namespace ModuleManager {
    // Forward declaration
    class ModuleManagerImpl;
}

class ModuleHeartbeat : public BaseModule {
public:
    static constexpr size_t MAX_MODULES = 32;
    static constexpr uint8_t MAX_RESTART_ATTEMPTS = 3;
    
    ModuleHeartbeat();
    virtual ~ModuleHeartbeat() = default;
    
    // BaseModule interface
    const char* get_name() const override { return "ModuleHeartbeat"; }
    esp_err_t init() override;
    void update() override;
    void stop() override;
    void configure(const nlohmann::json& config) override;
    bool is_healthy() const override;
    uint8_t get_health_score() const override;
    
    // Heartbeat interface
    void register_module(const char* module_name, ModuleType type);
    void unregister_module(const char* module_name);
    void update_heartbeat(const char* module_name);
    bool is_module_alive(const char* module_name) const;
    uint8_t get_restart_count(const char* module_name) const;
    
    // Configuration
    struct Config {
        bool enabled = true;
        uint32_t check_interval_ms = 5000;      // Check every 5 seconds
        uint32_t critical_timeout_ms = 5000;    // 5 sec for critical
        uint32_t standard_timeout_ms = 30000;   // 30 sec for standard
        uint32_t background_timeout_ms = 300000; // 5 min for background
        bool auto_restart_enabled = true;
    };

private:
    struct HeartbeatInfo {
        uint32_t last_update_ms = 0;
        uint8_t restart_count = 0;
        ModuleType type = ModuleType::STANDARD;
        bool is_active = false;
        bool is_remote = false;  // Reserved for future use
    };
    
    // Module tracking
    std::unordered_map<std::string, HeartbeatInfo> m_heartbeats;
    mutable SemaphoreHandle_t m_mutex; // `mutable` дозволяє використовувати в const методах
    
    // Configuration
    Config m_config;
    
    // Timing
    uint32_t m_last_check_ms = 0;
    
    // Statistics
    struct Stats {
        uint32_t total_checks = 0;
        uint32_t total_restarts = 0;
        uint32_t failed_restarts = 0;
        uint32_t longest_uptime_ms = 0;
    } m_stats;
    
    // Friend class for integration
    friend class ModuleManager::ModuleManagerImpl;
    
    // Logging tag
    static constexpr const char* TAG = "ModuleHeartbeat";

    // Internal methods
    void check_modules();
    void handle_unresponsive_module(const std::string& module_name, HeartbeatInfo& info);
    uint32_t get_timeout_for_type(ModuleType type) const;
    void log_event(const char* module_name, const char* event);
    
    // System health calculation
    void update_system_health();
    uint8_t m_system_health_score = 100;
    
    // Module restart callback (will be set by ModuleManager)
    std::function<bool(const char*)> m_restart_callback;
    
public:
    // Set restart callback for integration with ModuleManager
    void set_restart_callback(std::function<bool(const char*)> callback) {
        m_restart_callback = callback;
    }
    
    // Get statistics
    const Stats& get_stats() const { return m_stats; }
    
    // Get all heartbeat info (for debugging/monitoring)
    std::vector<std::pair<std::string, HeartbeatInfo>> get_all_heartbeats() const;
};

#endif // MODULE_HEARTBEAT_H