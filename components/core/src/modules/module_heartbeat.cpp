/**
 * @file module_heartbeat.cpp
 * @brief Implementation of module health monitoring
 */

#include "module_heartbeat.h"
#include "module_manager.h"
#include "event_bus.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <inttypes.h>

ModuleHeartbeat::ModuleHeartbeat() : m_mutex(nullptr) {
    m_mutex = xSemaphoreCreateMutex();
    if (!m_mutex) {
        ESP_LOGE(TAG, "Failed to create mutex!");
    }
}

esp_err_t ModuleHeartbeat::init() {
    ESP_LOGI(TAG, "Initializing ModuleHeartbeat...");
    
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    // Reset statistics
    m_stats = Stats{};
    m_last_check_ms = esp_timer_get_time() / 1000;
    
    ESP_LOGI(TAG, "ModuleHeartbeat initialized successfully");
    xSemaphoreGive(m_mutex);
    return ESP_OK;
}

void ModuleHeartbeat::update() {
    if (!m_config.enabled) {
        return;
    }
    
    uint32_t current_ms = esp_timer_get_time() / 1000;    
    // Check if it's time to check modules
    if (current_ms - m_last_check_ms >= m_config.check_interval_ms) {
        ESP_LOGD(TAG, "Performing periodic module health check...");
        check_modules();
        m_last_check_ms = current_ms;
    }
}

void ModuleHeartbeat::stop() {
    ESP_LOGI(TAG, "Stopping ModuleHeartbeat...");
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    m_heartbeats.clear();
    xSemaphoreGive(m_mutex);
}

void ModuleHeartbeat::configure(const nlohmann::json& config) {
    if (config.contains("heartbeat")) {
        auto& hb_config = config["heartbeat"];
        
        if (hb_config.contains("enabled")) {
            m_config.enabled = hb_config["enabled"];
        }
        if (hb_config.contains("check_interval_ms")) {
            m_config.check_interval_ms = hb_config["check_interval_ms"];
        }
        if (hb_config.contains("auto_restart_enabled")) {
            m_config.auto_restart_enabled = hb_config["auto_restart_enabled"];
        }
        
        // Timeout configuration
        if (hb_config.contains("timeouts")) {
            auto& timeouts = hb_config["timeouts"];
            if (timeouts.contains("critical_ms")) {
                m_config.critical_timeout_ms = timeouts["critical_ms"];
            }            if (timeouts.contains("standard_ms")) {
                m_config.standard_timeout_ms = timeouts["standard_ms"];
            }
            if (timeouts.contains("background_ms")) {
                m_config.background_timeout_ms = timeouts["background_ms"];
            }
        }
        
        ESP_LOGI(TAG, "Heartbeat configured: enabled=%d, check_interval=%" PRIu32 " ms",
                 m_config.enabled, m_config.check_interval_ms);
    }
}

bool ModuleHeartbeat::is_healthy() const {
    // Система вважається здоровою, якщо її бал здоров'я вище певного порогу (наприклад, 80)
    return m_system_health_score >= 80;
}

uint8_t ModuleHeartbeat::get_health_score() const {
    return m_system_health_score;
}

void ModuleHeartbeat::register_module(const char* module_name, ModuleType type) {
    if (!module_name) return;
    
    HeartbeatInfo info;
    info.type = type;
    info.is_active = true;
    info.last_update_ms = esp_timer_get_time() / 1000;
    
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    m_heartbeats[module_name] = info;
    xSemaphoreGive(m_mutex);

    ESP_LOGD(TAG, "Registered module: %s (type=%d)", module_name, static_cast<int>(type));
}

void ModuleHeartbeat::unregister_module(const char* module_name) {
    if (!module_name) return;
    
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    m_heartbeats.erase(module_name);
    xSemaphoreGive(m_mutex);

    ESP_LOGD(TAG, "Unregistered module: %s", module_name);
}

void ModuleHeartbeat::update_heartbeat(const char* module_name) {
    if (!module_name) return;
    
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    auto it = m_heartbeats.find(module_name);
    if (it != m_heartbeats.end()) {
        it->second.last_update_ms = esp_timer_get_time() / 1000;
    }
    xSemaphoreGive(m_mutex);
}

bool ModuleHeartbeat::is_module_alive(const char* module_name) const {
    if (!module_name) return false;
    
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    auto it = m_heartbeats.find(module_name);
    if (it == m_heartbeats.end() || !it->second.is_active) {
        xSemaphoreGive(m_mutex);
        return false;
    }
    
    uint32_t current_ms = esp_timer_get_time() / 1000;
    uint32_t timeout = get_timeout_for_type(it->second.type);
    
    xSemaphoreGive(m_mutex);
    return (current_ms - it->second.last_update_ms) <= timeout;
}

uint8_t ModuleHeartbeat::get_restart_count(const char* module_name) const {
    if (!module_name) return 0;
    
    auto it = m_heartbeats.find(module_name);
    uint8_t count = 0;
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    if (it != m_heartbeats.end()) count = it->second.restart_count;
    xSemaphoreGive(m_mutex);
    return count;
}

void ModuleHeartbeat::check_modules() {
    m_stats.total_checks++;
    uint32_t current_ms = esp_timer_get_time() / 1000;
    int healthy_modules = 0;
    int total_active = 0;
    
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    for (auto& [name, info] : m_heartbeats) {
        if (!info.is_active) continue;
        
        total_active++;        
        uint32_t timeout = get_timeout_for_type(info.type);
        uint32_t time_since_update = current_ms - info.last_update_ms;
        
        if (time_since_update > timeout) {
            ESP_LOGW(TAG, "Module %s is unresponsive (last update: %" PRIu32 " ms ago)",
                     name.c_str(), time_since_update);
            handle_unresponsive_module(name, info);
        } else {
            healthy_modules++;
            // Update longest uptime
            if (time_since_update > m_stats.longest_uptime_ms) {
                m_stats.longest_uptime_ms = time_since_update;
            }
        }
    }
    xSemaphoreGive(m_mutex);
    
    // Update system health score
    if (total_active > 0) {
        m_system_health_score = (healthy_modules * 100) / total_active;
    } else {
        m_system_health_score = 100;
    }
}

void ModuleHeartbeat::handle_unresponsive_module(const std::string& module_name, 
                                                 HeartbeatInfo& info) {
    // Increment restart count
    info.restart_count++;
    
    // Log event through EventBus
    log_event(module_name.c_str(), "Module unresponsive");    
    // Check if we should attempt restart
    if (m_config.auto_restart_enabled && info.restart_count <= MAX_RESTART_ATTEMPTS) {
        ESP_LOGW(TAG, "Attempting to restart module %s (attempt %d/%d)", 
                 module_name.c_str(), info.restart_count, MAX_RESTART_ATTEMPTS);
        
        // Call restart callback if available
        if (m_restart_callback) {
            bool restart_success = m_restart_callback(module_name.c_str());
            
            if (restart_success) {
                m_stats.total_restarts++;
                info.last_update_ms = esp_timer_get_time() / 1000;
                ESP_LOGI(TAG, "Module %s restarted successfully", module_name.c_str());
            } else {
                m_stats.failed_restarts++;
                ESP_LOGE(TAG, "Failed to restart module %s", module_name.c_str());
                
                if (info.restart_count >= MAX_RESTART_ATTEMPTS) {
                    info.is_active = false;
                    ESP_LOGE(TAG, "Module %s disabled after %d failed restart attempts", 
                             module_name.c_str(), MAX_RESTART_ATTEMPTS);
                }
            }
        }
    } else if (info.restart_count > MAX_RESTART_ATTEMPTS) {
        // Module is permanently disabled
        info.is_active = false;
    }
}

uint32_t ModuleHeartbeat::get_timeout_for_type(ModuleType type) const {
    switch (type) {
        case ModuleType::CRITICAL:
            return m_config.critical_timeout_ms;
        case ModuleType::HIGH:
            return m_config.critical_timeout_ms * 2;  // 2x critical timeout
        case ModuleType::STANDARD:
            return m_config.standard_timeout_ms;
        case ModuleType::LOW:
            return m_config.standard_timeout_ms * 2;  // 2x standard timeout
        case ModuleType::BACKGROUND:
            return m_config.background_timeout_ms;
        default:
            return m_config.standard_timeout_ms;
    }
}

void ModuleHeartbeat::log_event(const char* module_name, const char* event) {
    // Send event through EventBus
    nlohmann::json data = {
        {"source", "ModuleHeartbeat"},
        {"module", module_name},
        {"event", event},
        {"timestamp", esp_timer_get_time() / 1000}
    };
    EventBus::publish("system.module_health", data, EventBus::Priority::HIGH);
    ESP_LOGW(TAG, "[%s] %s", module_name, event);
}

std::vector<std::pair<std::string, ModuleHeartbeat::HeartbeatInfo>> 
ModuleHeartbeat::get_all_heartbeats() const {
    std::vector<std::pair<std::string, HeartbeatInfo>> result;
    result.reserve(m_heartbeats.size());
    
    xSemaphoreTake(m_mutex, portMAX_DELAY);
    for (const auto& [name, info] : m_heartbeats) {
        result.emplace_back(name, info);
    }
    xSemaphoreGive(m_mutex);
    
    return result;
}