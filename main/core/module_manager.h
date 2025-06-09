#pragma once

#include "base_module.h"
#include <vector>
#include <unordered_map>
#include <string>
#include <memory>
#include <esp_timer.h>

namespace ModuChill {

/**
 * Module priority levels
 * Determines initialization order and execution priority
 */
enum class ModuleType : uint8_t {
    CRITICAL = 0,    // Safety & Protection (<100μs execution)
    HIGH = 1,        // Real-time I/O (<500μs execution)
    STANDARD = 2,    // Business Logic (<2ms execution)
    LOW = 3,         // User Interface (<5ms execution)
    BACKGROUND = 4   // Analytics (<10ms execution)
};

/**
 * Module state
 */
enum class ModuleState : uint8_t {
    UNINITIALIZED = 0,
    CONFIGURED,
    INITIALIZED,
    RUNNING,
    STOPPING,
    STOPPED,
    ERROR
};

/**
 * Module health information
 */
struct ModuleHealth {
    const char* name;
    ModuleState state;
    uint8_t health_score;
    uint32_t update_time_us;
    uint32_t avg_update_time_us;
    uint32_t max_update_time_us;
    uint32_t deadline_misses;
    uint32_t total_errors;
    esp_err_t last_error;
    int64_t last_update_timestamp;
};

/**
 * Module health report
 */
struct ModuleHealthReport {
    size_t total_modules;
    size_t healthy_modules;
    size_t error_modules;
    size_t disabled_modules;
    std::vector<ModuleHealth> modules;
};

/**
 * ModuleManager - Module lifecycle coordinator
 * 
 * Manages registration, initialization, execution and shutdown of all modules.
 * Provides priority-based execution, health monitoring and graceful degradation.
 */
class ModuleManager {
private:
    struct ModuleInfo {
        BaseModule* module;
        ModuleType type;
        ModuleState state;
        bool enabled;
        uint32_t error_count;
        uint32_t consecutive_errors;
        int64_t last_update_time;
        uint32_t total_update_time_us;
        uint32_t update_count;
        uint32_t max_update_time_us;
        uint32_t deadline_misses;
        esp_err_t last_error;
    };

    // Module storage by priority
    std::unordered_map<std::string, std::unique_ptr<ModuleInfo>> modules_;
    std::vector<ModuleInfo*> priority_lists_[5]; // One vector per priority
    
    // Performance tracking
    uint32_t total_tick_time_us_;
    uint32_t max_tick_time_us_;
    
    // Singleton instance
    static ModuleManager* instance_;
    
    ModuleManager() = default;
    ~ModuleManager() = default;

public:
    // Lifecycle management
    static void init();
    static esp_err_t register_module(BaseModule* module, ModuleType type = ModuleType::STANDARD);
    static void configure_modules(const json& config);
    static esp_err_t init_modules();
    static void tick_all();
    static void stop_all();
    
    // Module access
    static BaseModule* get_module_by_name(const char* name);
    static std::vector<BaseModule*> get_modules_by_type(ModuleType type);
    static const std::vector<BaseModule*> get_active_modules();
    
    // Runtime management
    static bool reload_module(const char* name);
    static bool enable_module(const char* name);
    static bool disable_module(const char* name);
    
    // Health and diagnostics
    static ModuleHealthReport get_health_report();
    static uint8_t get_system_health_score();
    static bool is_module_healthy(const char* name);
    
    // Performance metrics
    static uint32_t get_last_tick_time_us() { return instance_->total_tick_time_us_; }
    static uint32_t get_max_tick_time_us() { return instance_->max_tick_time_us_; }
    
private:
    static ModuleManager* get_instance();
    
    esp_err_t register_module_internal(BaseModule* module, ModuleType type);
    void tick_priority_level(ModuleType type, uint32_t time_budget_us);
    void update_module(ModuleInfo* info);
    void calculate_health_score(ModuleInfo* info);
    bool should_update_module(ModuleInfo* info);
};

} // namespace ModuChill