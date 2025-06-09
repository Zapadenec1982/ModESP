#pragma once

#include <esp_err.h>
#include <cstdint>
#include <functional>

namespace ModuChill {

/**
 * System states
 */
enum class SystemState : uint8_t {
    BOOT = 0,      // Hardware initialization (0-10ms)
    INIT,          // Service initialization (10-200ms)
    RUNNING,       // Normal operation
    ERROR,         // Recovery mode
    SHUTDOWN       // Graceful shutdown
};

/**
 * Error severity levels
 */
enum class ErrorSeverity : uint8_t {
    WARNING = 0,   // Log only
    MINOR,         // Degraded operation
    MAJOR,         // Component disabled
    CRITICAL,      // System restart required
    FATAL          // Immediate restart
};

/**
 * Component error information
 */
struct ComponentError {
    const char* component;
    esp_err_t error_code;
    ErrorSeverity severity;
    const char* message;
    uint32_t timestamp;
};

/**
 * System health status
 */
struct SystemHealth {
    SystemState state;
    uint8_t overall_score;     // 0-100
    uint32_t uptime_ms;
    uint32_t free_heap;
    uint8_t cpu_usage;
    uint32_t error_count;
    uint32_t restart_count;
};

/**
 * Application - Central lifecycle coordinator
 * 
 * Manages system initialization, main loop execution, health monitoring
 * and graceful shutdown. Ensures deterministic startup and stable operation.
 */
class Application {
private:
    SystemState state_;
    uint32_t boot_time_ms_;
    uint32_t cycle_count_;
    uint32_t error_count_;
    uint32_t last_health_check_ms_;
    
    // Performance metrics
    uint32_t min_cycle_time_us_;
    uint32_t max_cycle_time_us_;
    uint32_t total_cycle_time_us_;
    
    // Watchdog
    esp_timer_handle_t watchdog_timer_;
    
    // Singleton
    static Application* instance_;
    
    Application();
    ~Application() = default;
    
    // State transitions
    esp_err_t transition_to_init();
    esp_err_t transition_to_running();
    void transition_to_error(ErrorSeverity severity);
    void transition_to_shutdown();
    
    // Main loop tasks
    void main_loop_cycle();
    void check_system_health();
    void feed_watchdog();
    
    // Error handling
    void handle_critical_error(const ComponentError& error);
    void attempt_recovery();

public:
    // Lifecycle control
    static void init();
    static void run();
    static void shutdown();
    static void restart();
    
    // State management
    static SystemState get_state();
    static bool is_running();
    static bool is_error();
    
    // Error reporting
    static void report_error(const char* component, esp_err_t error, 
                           ErrorSeverity severity = ErrorSeverity::MINOR,
                           const char* message = nullptr);
    
    // Health monitoring
    static SystemHealth get_system_health();
    static void check_health();
    
    // Metrics
    static uint32_t get_uptime_ms();
    static uint32_t get_free_heap();
    static uint8_t get_cpu_usage();
    static uint32_t get_cycle_count();
    
    // Callbacks
    using StateChangeCallback = std::function<void(SystemState old_state, SystemState new_state)>;
    static void on_state_change(StateChangeCallback callback);
    
private:
    static Application* get_instance();
    static void watchdog_callback(void* arg);
};

} // namespace ModuChill