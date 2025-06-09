#include "module_manager.h"
#include "event_bus.h"
#include <esp_log.h>
#include <algorithm>

namespace ModuChill {

static const char* TAG = "ModuleManager";

ModuleManager* ModuleManager::instance_ = nullptr;

ModuleManager::ModuleManager() {
    modules_.reserve(16);
}

ModuleManager* ModuleManager::get_instance() {
    if (!instance_) {
        instance_ = new ModuleManager();
    }
    return instance_;
}

void ModuleManager::init() {
    ESP_LOGI(TAG, "ModuleManager initialized");
}

esp_err_t ModuleManager::register_module(BaseModule* module, ModulePriority priority) {
    auto* manager = get_instance();
    
    if (!module) {
        ESP_LOGE(TAG, "Cannot register null module");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Check for duplicate
    const char* name = module->get_name();
    for (const auto& entry : manager->modules_) {
        if (strcmp(entry.module->get_name(), name) == 0) {
            ESP_LOGE(TAG, "Module %s already registered", name);
            return ESP_ERR_INVALID_STATE;
        }
    }
    
    // Add module
    ModuleEntry entry;
    entry.module = module;
    entry.priority = priority;
    entry.state = ModuleState::REGISTERED;
    entry.enabled = true;
    entry.health_score = 100;
    entry.total_update_time = 0;
    entry.update_count = 0;
    entry.error_count = 0;
    entry.last_error = ESP_OK;
    
    manager->modules_.push_back(entry);
    
    // Sort by priority
    std::sort(manager->modules_.begin(), manager->modules_.end(),
              [](const ModuleEntry& a, const ModuleEntry& b) {
                  return a.priority < b.priority;
              });
    
    ESP_LOGI(TAG, "Registered module: %s (priority: %d)", name, (int)priority);
    
    return ESP_OK;
}

void ModuleManager::configure_modules(const json& config) {
    auto* manager = get_instance();
    
    ESP_LOGI(TAG, "Configuring %zu modules", manager->modules_.size());
    
    for (auto& entry : manager->modules_) {
        try {
            entry.module->configure(config);
            entry.state = ModuleState::CONFIGURED;
        } catch (const std::exception& e) {
            ESP_LOGE(TAG, "Failed to configure %s: %s", 
                     entry.module->get_name(), e.what());
            entry.state = ModuleState::ERROR;
        }
    }
}

esp_err_t ModuleManager::init_modules() {
    auto* manager = get_instance();
    
    ESP_LOGI(TAG, "Initializing %zu modules", manager->modules_.size());
    
    for (auto& entry : manager->modules_) {
        if (entry.state != ModuleState::CONFIGURED) {
            ESP_LOGW(TAG, "Skipping unconfigured module: %s", 
                     entry.module->get_name());
            continue;
        }
        
        ESP_LOGI(TAG, "Initializing %s...", entry.module->get_name());
        
        esp_err_t ret = entry.module->init();
        if (ret == ESP_OK) {
            entry.state = ModuleState::INITIALIZED;
            ESP_LOGI(TAG, "  [OK] %s", entry.module->get_name());
        } else {
            entry.state = ModuleState::ERROR;
            entry.last_error = ret;
            ESP_LOGE(TAG, "  [FAIL] %s: %s", entry.module->get_name(), 
                     esp_err_to_name(ret));
            
            // Only fail for critical modules
            if (entry.priority == ModulePriority::CRITICAL) {
                return ret;
            }
        }
    }
    
    return ESP_OK;
}
void ModuleManager::tick_all(uint32_t max_ms) {
    auto* manager = get_instance();
    
    uint32_t start_time = esp_timer_get_time() / 1000;
    
    for (auto& entry : manager->modules_) {
        // Skip disabled or error modules
        if (!entry.enabled || entry.state != ModuleState::INITIALIZED) {
            continue;
        }
        
        // Check time budget
        uint32_t elapsed = (esp_timer_get_time() / 1000) - start_time;
        if (elapsed >= max_ms) {
            ESP_LOGW(TAG, "Module update budget exceeded (%lu ms)", elapsed);
            break;
        }
        
        // Update module
        uint32_t update_start = esp_timer_get_time();
        
        try {
            entry.module->update();
            
            // Update metrics
            uint32_t update_time = esp_timer_get_time() - update_start;
            entry.total_update_time += update_time;
            entry.update_count++;
            
            // Check deadline
            uint32_t max_time = entry.module->get_max_update_time_us();
            if (update_time > max_time) {
                entry.deadline_misses++;
                ESP_LOGW(TAG, "%s exceeded deadline: %lu us (max: %lu us)",
                        entry.module->get_name(), update_time, max_time);
            }
            
        } catch (const std::exception& e) {
            ESP_LOGE(TAG, "Exception in %s::update(): %s",
                    entry.module->get_name(), e.what());
            entry.error_count++;
            entry.last_error = ESP_FAIL;
        } catch (...) {
            ESP_LOGE(TAG, "Unknown exception in %s::update()",
                    entry.module->get_name());
            entry.error_count++;
            entry.last_error = ESP_FAIL;
        }
        
        // Update health score
        update_health_score(entry);
    }
}

void ModuleManager::stop_all() {
    auto* manager = get_instance();
    
    ESP_LOGI(TAG, "Stopping all modules");
    
    // Stop in reverse order
    for (auto it = manager->modules_.rbegin(); it != manager->modules_.rend(); ++it) {
        if (it->state == ModuleState::INITIALIZED) {
            ESP_LOGI(TAG, "Stopping %s", it->module->get_name());
            it->module->stop();
            it->state = ModuleState::STOPPED;
        }
    }
}

void ModuleManager::update_health_score(ModuleEntry& entry) {
    // Start at 100
    uint8_t score = 100;
    
    // Module-specific health
    if (!entry.module->is_healthy()) {
        score -= 10;
    }
    
    // Error penalty (max -50)
    if (entry.error_count > 0) {
        score -= std::min(50, (int)(entry.error_count * 10));
    }
    
    // Deadline miss penalty
    if (entry.update_count > 0) {
        float miss_rate = (float)entry.deadline_misses / entry.update_count;
        if (miss_rate > 0.1f) { // >10% misses
            score -= 20;
        }
    }
    
    // Performance penalty
    if (entry.update_count > 0) {
        uint32_t avg_time = entry.total_update_time / entry.update_count;
        uint32_t max_time = entry.module->get_max_update_time_us();
        if (avg_time > max_time) {
            score -= 20;
        }
    }
    
    entry.health_score = score;
}

void ModuleManager::check_health() {
    auto* manager = get_instance();
    
    size_t healthy_count = 0;
    size_t degraded_count = 0;
    size_t failed_count = 0;
    
    for (auto& entry : manager->modules_) {
        if (entry.state != ModuleState::INITIALIZED) {
            continue;
        }
        
        if (entry.health_score >= 80) {
            healthy_count++;
        } else if (entry.health_score >= 50) {
            degraded_count++;
            ESP_LOGW(TAG, "%s degraded (score: %d)", 
                     entry.module->get_name(), entry.health_score);
        } else {
            failed_count++;
            ESP_LOGE(TAG, "%s failing (score: %d)", 
                     entry.module->get_name(), entry.health_score);
        }
    }
    
    // Publish health report
    EventBus::publish("modules.health", {
        {"healthy", healthy_count},
        {"degraded", degraded_count},
        {"failed", failed_count},
        {"total", manager->modules_.size()}
    });
}

BaseModule* ModuleManager::get_module_by_name(const char* name) {
    auto* manager = get_instance();
    
    for (auto& entry : manager->modules_) {
        if (strcmp(entry.module->get_name(), name) == 0) {
            return entry.module;
        }
    }
    
    return nullptr;
}

std::vector<BaseModule*> ModuleManager::get_modules_by_priority(ModulePriority priority) {
    auto* manager = get_instance();
    std::vector<BaseModule*> result;
    
    for (const auto& entry : manager->modules_) {
        if (entry.priority == priority) {
            result.push_back(entry.module);
        }
    }
    
    return result;
}

bool ModuleManager::enable_module(const char* name) {
    auto* manager = get_instance();
    
    for (auto& entry : manager->modules_) {
        if (strcmp(entry.module->get_name(), name) == 0) {
            entry.enabled = true;
            ESP_LOGI(TAG, "Enabled module: %s", name);
            return true;
        }
    }
    
    return false;
}

bool ModuleManager::disable_module(const char* name) {
    auto* manager = get_instance();
    
    for (auto& entry : manager->modules_) {
        if (strcmp(entry.module->get_name(), name) == 0) {
            entry.enabled = false;
            ESP_LOGI(TAG, "Disabled module: %s", name);
            return true;
        }
    }
    
    return false;
}

size_t ModuleManager::get_module_count() {
    return get_instance()->modules_.size();
}

size_t ModuleManager::get_healthy_module_count() {
    auto* manager = get_instance();
    size_t count = 0;
    
    for (const auto& entry : manager->modules_) {
        if (entry.state == ModuleState::INITIALIZED && entry.health_score >= 80) {
            count++;
        }
    }
    
    return count;
}

ModuleHealthReport ModuleManager::get_health_report() {
    auto* manager = get_instance();
    ModuleHealthReport report;
    
    report.total_modules = manager->modules_.size();
    report.healthy_modules = 0;
    report.error_modules = 0;
    
    size_t i = 0;
    for (const auto& entry : manager->modules_) {
        if (i >= CONFIG_MAX_MODULES) break;
        
        auto& health = report.modules[i++];
        health.name = entry.module->get_name();
        health.state = entry.state;
        health.health_score = entry.health_score;
        health.update_time_us = 0;
        health.avg_update_time_us = entry.update_count > 0 ? 
            entry.total_update_time / entry.update_count : 0;
        health.max_update_time_us = entry.module->get_max_update_time_us();
        health.deadline_misses = entry.deadline_misses;
        health.total_errors = entry.error_count;
        health.last_error = entry.last_error;
        
        if (entry.health_score >= 80) {
            report.healthy_modules++;
        }
        if (entry.error_count > 0) {
            report.error_modules++;
        }
    }
    
    return report;
}

} // namespace ModuChill