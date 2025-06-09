#include "module_manager.h"
#include "event_bus.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <algorithm>
#include <cctype>

static const char* TAG = "ModuleManager";

namespace ModuleManager {

// Module entry structure
struct ModuleEntry {
    std::unique_ptr<BaseModule> module;
    ModuleType type;
    ModuleState state = ModuleState::CREATED;  // ModuleManager керує станом
    bool enabled = true;
    
    // Performance metrics
    uint64_t total_update_time_us = 0;
    uint32_t update_count = 0;
    uint32_t last_update_time_us = 0;
    uint32_t max_update_time_us = 0;
    uint32_t deadline_misses = 0;
    
    // Health metrics
    uint8_t health_score = 100;
    uint32_t error_count = 0;
    esp_err_t last_error = ESP_OK;
    
    // Update interval control
    uint32_t update_interval_ms = 0;  // 0 = update every tick
    uint32_t last_update_time_ms = 0;
};

// Internal state
static std::vector<ModuleEntry> modules;
static bool initialized = false;

// Performance callbacks
static PerformanceCallback performance_callback = nullptr;
static ExecutionOrderCallback execution_order_callback = nullptr;

// Helper: конвертувати ім'я модуля в ім'я секції конфігурації
static std::string module_name_to_config_section(const char* module_name) {
    std::string result = module_name;
    
    // Видаляємо суфікс "Module" якщо є
    if (result.length() > 6 && result.substr(result.length() - 6) == "Module") {
        result = result.substr(0, result.length() - 6);
    }
    
    // Конвертуємо в нижній регістр
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    
    return result;
}

// Helper: розрахувати бал здоров'я модуля
static void update_health_score(ModuleEntry& entry) {
    uint8_t score = 100;
    
    // Базовий штраф за стан модуля
    if (!entry.module->is_healthy()) {
        score -= 10;
    }
    
    // Штраф за помилки (до 50 балів)
    if (entry.error_count > 0) {
        uint32_t error_penalty = std::min(50u, entry.error_count * 10);
        score = score > error_penalty ? score - error_penalty : 0;
    }
    
    // Штраф за пропуски дедлайнів (до 20 балів)
    if (entry.deadline_misses > 0 && entry.update_count > 0) {
        float miss_rate = (float)entry.deadline_misses / entry.update_count;
        if (miss_rate > 0.1f) { // > 10% пропусків
            uint32_t deadline_penalty = std::min(20u, (uint32_t)(miss_rate * 100));
            score = score > deadline_penalty ? score - deadline_penalty : 0;
        }
    }
    
    // Додатковий штраф від модуля через get_health_score()
    uint8_t module_score = entry.module->get_health_score();
    if (module_score < 100) {
        uint32_t module_penalty = 100 - module_score;
        score = score > module_penalty ? score - module_penalty : 0;
    }
    
    entry.health_score = score;
}

// Helper: отримати максимальний час оновлення для типу модуля
static uint32_t get_type_max_time_us(ModuleType type) {
    switch (type) {
        case ModuleType::CRITICAL:    return 100;   // 100μs
        case ModuleType::HIGH:        return 500;   // 500μs  
        case ModuleType::STANDARD:    return 2000;  // 2ms
        case ModuleType::LOW:         return 5000;  // 5ms
        case ModuleType::BACKGROUND:  return 10000; // 10ms
        default:                      return 2000;  // default 2ms
    }
}

esp_err_t init() {
    if (initialized) {
        ESP_LOGW(TAG, "ModuleManager already initialized");
        return ESP_OK;
    }
    
    modules.reserve(16); // Попереднє виділення для типової кількості модулів
    initialized = true;
    
    ESP_LOGI(TAG, "ModuleManager initialized");
    return ESP_OK;
}

esp_err_t register_module(std::unique_ptr<BaseModule> module, ModuleType type) {
    if (!initialized) {
        ESP_LOGE(TAG, "ModuleManager not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!module) {
        ESP_LOGE(TAG, "Cannot register null module");
        return ESP_ERR_INVALID_ARG;
    }
    
    const char* name = module->get_name();
    
    // Перевіряємо на дублікати
    for (const auto& entry : modules) {
        if (strcmp(entry.module->get_name(), name) == 0) {
            ESP_LOGE(TAG, "Module %s already registered", name);
            return ESP_ERR_INVALID_STATE;
        }
    }
    
    // Створюємо запис модуля
    ModuleEntry entry;
    entry.module = std::move(module);
    entry.type = type;
    entry.state = ModuleState::CREATED;  // ModuleManager керує станом
    
    modules.push_back(std::move(entry));
    
    // Сортуємо за пріоритетом (CRITICAL перший, BACKGROUND останній)
    std::sort(modules.begin(), modules.end(),
              [](const ModuleEntry& a, const ModuleEntry& b) {
                  return a.type < b.type;
              });
    
    ESP_LOGI(TAG, "Registered module: %s (type: %d)", name, (int)type);
    return ESP_OK;
}

void configure_all(const nlohmann::json& config) {
    ESP_LOGI(TAG, "Configuring %zu modules...", modules.size());
    
    for (auto& entry : modules) {
        const char* module_name = entry.module->get_name();
        
        // Застосовуємо угоду про іменування: "ClimateModule" -> "climate"
        std::string config_section = module_name_to_config_section(module_name);
        
        if (config.contains(config_section)) {
            ESP_LOGD(TAG, "Configuring %s with section '%s'", module_name, config_section.c_str());
            
            try {
                entry.module->configure(config[config_section]);
                entry.state = ModuleState::CONFIGURED;  // ModuleManager керує станом
                ESP_LOGD(TAG, "  [OK] %s configured", module_name);
                
            } catch (const std::exception& e) {
                ESP_LOGE(TAG, "  [FAIL] %s configuration error: %s", module_name, e.what());
                entry.error_count++;
                entry.last_error = ESP_ERR_INVALID_ARG;
            }
        } else {
            ESP_LOGW(TAG, "No config section '%s' found for module %s", 
                     config_section.c_str(), module_name);
        }
    }
    
    ESP_LOGI(TAG, "Module configuration completed");
}

esp_err_t init_all() {
    ESP_LOGI(TAG, "Initializing %zu modules by priority...", modules.size());
    
    bool critical_failed = false;
    
    for (auto& entry : modules) {
        const char* name = entry.module->get_name();
        
        // Пропускаємо модулі які не були сконфігуровані
        if (entry.state != ModuleState::CONFIGURED) {
            ESP_LOGW(TAG, "Skipping %s - not configured", name);
            continue;
        }
        
        ESP_LOGI(TAG, "Initializing %s (type: %d)...", name, (int)entry.type);
        
        esp_err_t ret = entry.module->init();
        if (ret == ESP_OK) {
            entry.state = ModuleState::INITIALIZED;  // ModuleManager керує станом
            ESP_LOGI(TAG, "  [OK] %s initialized", name);
        } else {
            entry.state = ModuleState::ERROR;  // ModuleManager керує станом
            entry.last_error = ret;
            entry.error_count++;
            ESP_LOGE(TAG, "  [FAIL] %s: %s", name, esp_err_to_name(ret));
            
            // Критичні модулі обов'язкові для роботи системи
            if (entry.type == ModuleType::CRITICAL) {
                critical_failed = true;
                ESP_LOGE(TAG, "Critical module %s failed - system cannot continue", name);
            }
        }
    }
    
    // Рахуємо статистику ініціалізації
    size_t initialized_count = 0;
    for (const auto& entry : modules) {
        if (entry.state == ModuleState::INITIALIZED) {
            initialized_count++;
        }
    }
    
    // Публікуємо подію завершення ініціалізації
    EventBus::publish("modules.initialized", {
        {"total", modules.size()},
        {"initialized", initialized_count},
        {"critical_failed", critical_failed}
    });
    
    ESP_LOGI(TAG, "Module initialization complete: %zu/%zu modules initialized", 
             initialized_count, modules.size());
    
    return critical_failed ? ESP_FAIL : ESP_OK;
}

void tick_all(uint32_t time_budget_ms) {
    uint64_t start_time_us = esp_timer_get_time();
    uint64_t budget_us = time_budget_ms * 1000;
    uint32_t current_time_ms = start_time_us / 1000;
    
    for (auto& entry : modules) {
        // Пропускаємо вимкнені модулі або модулі з помилками
        if (!entry.enabled || entry.state != ModuleState::INITIALIZED) {
            continue;
        }
        
        // Перевіряємо часовий бюджет
        uint64_t elapsed_us = esp_timer_get_time() - start_time_us;
        if (elapsed_us >= budget_us) {
            ESP_LOGW(TAG, "Module update time budget (%lums) exceeded", time_budget_ms);
            break;
        }
        
        // Перевіряємо інтервал оновлення модуля
        if (entry.update_interval_ms > 0) {
            if (current_time_ms - entry.last_update_time_ms < entry.update_interval_ms) {
                continue; // Пропускаємо цей цикл
            }
            entry.last_update_time_ms = current_time_ms;
        }
        
        // Оновлюємо модуль з відстеженням часу
        uint64_t update_start_us = esp_timer_get_time();
        
        try {
            entry.module->update();
            
            // Розраховуємо метрики продуктивності
            uint64_t update_time_us = esp_timer_get_time() - update_start_us;
            entry.last_update_time_us = (uint32_t)update_time_us;
            entry.total_update_time_us += update_time_us;
            entry.update_count++;
            
            if (update_time_us > entry.max_update_time_us) {
                entry.max_update_time_us = (uint32_t)update_time_us;
            }
            
            // Перевіряємо дотримання дедлайну
            uint32_t max_allowed_us = entry.module->get_max_update_time_us();
            if (max_allowed_us == 0) {
                max_allowed_us = get_type_max_time_us(entry.type);
            }
            
            if (update_time_us > max_allowed_us) {
                entry.deadline_misses++;
                ESP_LOGW(TAG, "%s exceeded deadline: %lluμs (max: %luμs)",
                        entry.module->get_name(), update_time_us, max_allowed_us);
            }
            
            // Викликаємо performance callback якщо зареєстрований
            if (performance_callback) {
                performance_callback(entry.module.get(), (uint32_t)update_time_us);
            }
            
        } catch (const std::exception& e) {
            ESP_LOGE(TAG, "Exception in %s::update(): %s",
                    entry.module->get_name(), e.what());
            entry.error_count++;
            entry.last_error = ESP_FAIL;
            entry.state = ModuleState::ERROR;  // ModuleManager керує станом
            
        } catch (...) {
            ESP_LOGE(TAG, "Unknown exception in %s::update()",
                    entry.module->get_name());
            entry.error_count++;
            entry.last_error = ESP_FAIL;
            entry.state = ModuleState::ERROR;  // ModuleManager керує станом
        }
        
        // Оновлюємо бал здоров'я
        update_health_score(entry);
    }
    
    // Застосовуємо кастомний порядок виконання якщо зареєстрований
    if (execution_order_callback) {
        std::vector<BaseModule*> module_pointers;
        for (auto& entry : modules) {
            module_pointers.push_back(entry.module.get());
        }
        execution_order_callback(module_pointers);
    }
}

void shutdown_all() {
    ESP_LOGI(TAG, "Shutting down all modules in reverse order...");
    
    // Зупиняємо модулі в зворотному порядку пріоритетів
    for (auto it = modules.rbegin(); it != modules.rend(); ++it) {
        if (it->state == ModuleState::INITIALIZED) {
            const char* name = it->module->get_name();
            ESP_LOGI(TAG, "Stopping %s...", name);
            
            try {
                it->module->stop();
                it->state = ModuleState::STOPPED;  // ModuleManager керує станом
                ESP_LOGD(TAG, "  [OK] %s stopped", name);
                
            } catch (const std::exception& e) {
                ESP_LOGE(TAG, "Exception stopping %s: %s", name, e.what());
            } catch (...) {
                ESP_LOGE(TAG, "Unknown exception stopping %s", name);
            }
        }
    }
    
    ESP_LOGI(TAG, "Module shutdown completed");
}

BaseModule* find_module(const char* name) {
    for (auto& entry : modules) {
        if (strcmp(entry.module->get_name(), name) == 0) {
            return entry.module.get();
        }
    }
    return nullptr;
}

esp_err_t get_module_state(const char* name, ModuleState& state) {
    for (const auto& entry : modules) {
        if (strcmp(entry.module->get_name(), name) == 0) {
            state = entry.state;
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_FOUND;
}

std::vector<BaseModule*> get_modules_by_type(ModuleType type) {
    std::vector<BaseModule*> result;
    
    for (auto& entry : modules) {
        if (entry.type == type) {
            result.push_back(entry.module.get());
        }
    }
    
    return result;
}

std::vector<BaseModule*> get_all_modules() {
    std::vector<BaseModule*> result;
    result.reserve(modules.size());
    
    for (auto& entry : modules) {
        result.push_back(entry.module.get());
    }
    
    return result;
}

esp_err_t enable_module(const char* name) {
    for (auto& entry : modules) {
        if (strcmp(entry.module->get_name(), name) == 0) {
            entry.enabled = true;
            ESP_LOGI(TAG, "Enabled module: %s", name);
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t disable_module(const char* name) {
    for (auto& entry : modules) {
        if (strcmp(entry.module->get_name(), name) == 0) {
            entry.enabled = false;
            ESP_LOGI(TAG, "Disabled module: %s", name);
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_FOUND;
}

bool is_module_enabled(const char* name) {
    for (const auto& entry : modules) {
        if (strcmp(entry.module->get_name(), name) == 0) {
            return entry.enabled;
        }
    }
    return false;
}

esp_err_t reload_module(const char* name, const nlohmann::json& config) {
    for (auto& entry : modules) {
        if (strcmp(entry.module->get_name(), name) == 0) {
            ESP_LOGI(TAG, "Reloading module: %s", name);
            
            // 1. Зупиняємо модуль
            if (entry.state == ModuleState::INITIALIZED) {
                entry.module->stop();
            }
            
            // 2. Конфігуруємо з новими параметрами (якщо надані)
            if (!config.empty()) {
                entry.module->configure(config);
            }
            entry.state = ModuleState::CONFIGURED;  // ModuleManager керує станом
            
            // 3. Ініціалізуємо знову
            esp_err_t ret = entry.module->init();
            if (ret == ESP_OK) {
                entry.state = ModuleState::INITIALIZED;  // ModuleManager керує станом
                ESP_LOGI(TAG, "Module %s reloaded successfully", name);
            } else {
                entry.state = ModuleState::ERROR;  // ModuleManager керує станом
                entry.last_error = ret;
                entry.error_count++;
                ESP_LOGE(TAG, "Failed to reload module %s: %s", name, esp_err_to_name(ret));
            }
            
            return ret;
        }
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t get_module_stats(const char* name, ModuleStats& stats) {
    for (const auto& entry : modules) {
        if (strcmp(entry.module->get_name(), name) == 0) {
            stats.name = entry.module->get_name();
            stats.state = entry.state;  // ModuleManager керує станом
            stats.type = entry.type;
            stats.health_score = entry.health_score;
            stats.update_count = entry.update_count;
            stats.error_count = entry.error_count;
            stats.last_update_time_us = entry.last_update_time_us;
            stats.avg_update_time_us = entry.update_count > 0 ? 
                (uint32_t)(entry.total_update_time_us / entry.update_count) : 0;
            stats.max_update_time_us = entry.max_update_time_us;
            stats.deadline_misses = entry.deadline_misses;
            stats.last_error = entry.last_error;
            stats.enabled = entry.enabled;
            
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_FOUND;
}

HealthReport get_health_report() {
    HealthReport report;
    report.total_modules = modules.size();
    report.healthy_modules = 0;
    report.degraded_modules = 0;
    report.error_modules = 0;
    report.disabled_modules = 0;
    
    uint32_t total_health_score = 0;
    
    for (const auto& entry : modules) {
        ModuleStats stats;
        stats.name = entry.module->get_name();
        stats.state = entry.state;  // ModuleManager керує станом
        stats.type = entry.type;
        stats.health_score = entry.health_score;
        stats.update_count = entry.update_count;
        stats.error_count = entry.error_count;
        stats.last_update_time_us = entry.last_update_time_us;
        stats.avg_update_time_us = entry.update_count > 0 ? 
            (uint32_t)(entry.total_update_time_us / entry.update_count) : 0;
        stats.max_update_time_us = entry.max_update_time_us;
        stats.deadline_misses = entry.deadline_misses;
        stats.last_error = entry.last_error;
        stats.enabled = entry.enabled;
        
        report.modules.push_back(stats);
        
        // Рахуємо категорії здоров'я
        if (!entry.enabled) {
            report.disabled_modules++;
        } else if (entry.state != ModuleState::INITIALIZED) {
            report.error_modules++;
        } else if (entry.health_score >= 80) {
            report.healthy_modules++;
        } else if (entry.health_score >= 50) {
            report.degraded_modules++;
        } else {
            report.error_modules++;
        }
        
        total_health_score += entry.health_score;
    }
    
    // Розраховуємо загальний бал здоров'я системи
    report.system_health_score = modules.empty() ? 100 : 
        (uint8_t)(total_health_score / modules.size());
    
    return report;
}

esp_err_t set_update_interval(const char* name, uint32_t interval_ms) {
    for (auto& entry : modules) {
        if (strcmp(entry.module->get_name(), name) == 0) {
            entry.update_interval_ms = interval_ms;
            ESP_LOGI(TAG, "Set update interval for %s: %lums", name, interval_ms);
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_FOUND;
}

void register_all_rpc(IJsonRpcRegistrar& registrar) {
    ESP_LOGI(TAG, "Registering RPC methods for %zu modules", modules.size());
    
    for (auto& entry : modules) {
        try {
            entry.module->register_rpc(registrar);
            ESP_LOGD(TAG, "Registered RPC for %s", entry.module->get_name());
        } catch (const std::exception& e) {
            ESP_LOGE(TAG, "Exception registering RPC for %s: %s", 
                    entry.module->get_name(), e.what());
        }
    }
}

void dump_modules(const char* log_tag) {
    const char* tag = log_tag ? log_tag : TAG;
    
    ESP_LOGI(tag, "=== MODULE MANAGER DUMP ===");
    ESP_LOGI(tag, "Total modules: %zu", modules.size());
    
    for (const auto& entry : modules) {
        const char* state_str = "UNKNOWN";
        switch (entry.state) {  // ModuleManager керує станом
            case ModuleState::CREATED:     state_str = "CREATED"; break;
            case ModuleState::CONFIGURED:  state_str = "CONFIGURED"; break;
            case ModuleState::INITIALIZED: state_str = "INITIALIZED"; break;
            case ModuleState::RUNNING:     state_str = "RUNNING"; break;
            case ModuleState::ERROR:       state_str = "ERROR"; break;
            case ModuleState::STOPPED:     state_str = "STOPPED"; break;
        }
        
        ESP_LOGI(tag, "  %s: type=%d, state=%s, enabled=%s, health=%d%%",
                entry.module->get_name(), (int)entry.type, state_str,
                entry.enabled ? "YES" : "NO", entry.health_score);
        
        if (entry.update_count > 0) {
            uint32_t avg_us = (uint32_t)(entry.total_update_time_us / entry.update_count);
            ESP_LOGI(tag, "    Updates: %lu, Avg: %luμs, Max: %luμs, Misses: %lu",
                    entry.update_count, avg_us, entry.max_update_time_us, 
                    entry.deadline_misses);
        }
        
        if (entry.error_count > 0) {
            ESP_LOGI(tag, "    Errors: %lu, Last: %s",
                    entry.error_count, esp_err_to_name(entry.last_error));
        }
    }
    
    ESP_LOGI(tag, "=== END MODULE DUMP ===");
}

void set_execution_order(ExecutionOrderCallback callback) {
    execution_order_callback = callback;
    ESP_LOGI(TAG, "Custom execution order callback registered");
}

void set_performance_callback(PerformanceCallback callback) {
    performance_callback = callback;
    ESP_LOGI(TAG, "Performance monitoring callback registered");
}

} // namespace ModuleManager