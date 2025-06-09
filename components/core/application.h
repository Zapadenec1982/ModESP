/**
 * @file application.h
 * @brief Application lifecycle coordinator for ModuChill system
 * 
 * Manages initialization sequence, main loop execution, health monitoring,
 * and graceful shutdown. Ensures deterministic startup and stable runtime.
 */

#ifndef APPLICATION_H
#define APPLICATION_H

#include <stdint.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"

namespace Application {

/**
 * @brief Application states
 */
enum class State {
    BOOT,       // Hardware initialization (0-10ms)
    INIT,       // Service initialization (10-200ms)
    RUNNING,    // Normal operation
    ERROR,      // Recovery mode
    SHUTDOWN    // Graceful shutdown
};

/**
 * @brief Error severity levels
 */
enum class ErrorSeverity {
    WARNING,    // Non-critical, logged only
    ERROR,      // Degraded operation possible
    CRITICAL,   // System restart required
    FATAL       // Immediate restart
};

/**
 * @brief Initialize application
 * 
 * Performs one-time initialization:
 * 1. Hardware layer (NVS, GPIO, UART, clocks)
 * 2. Core services (EventBus, SharedState)
 * 3. Configuration loading
 * 4. Module manager setup
 * 5. Module initialization by priority
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t init();

/**
 * @brief Run main application loop
 * 
 * Starts the main loop at 100Hz. This function blocks forever.
 * Main loop structure:
 * - Update all modules (8ms budget)
 * - Process events (2ms budget)
 * - Health monitoring (1Hz)
 * - Watchdog feeding
 */
[[noreturn]] void run();

/**
 * @brief Request graceful shutdown
 * 
 * Initiates shutdown sequence:
 * 1. Stop accepting new operations
 * 2. Complete pending work
 * 3. Save persistent state
 * 4. Shutdown modules in reverse order
 * 5. Cleanup resources
 */
void shutdown();

/**
 * @brief Request system restart
 * 
 * @param delay_ms Delay before restart (default 1000ms)
 */
void restart(uint32_t delay_ms = 1000);

/**
 * @brief Get current application state
 * @return Current state
 */
State get_state();

/**
 * @brief Check if application is running
 * @return true if in RUNNING state
 */
bool is_running();

/**
 * @brief Report error from component
 * 
 * @param component Component name (e.g., "climate_control")
 * @param error Error code
 * @param severity Error severity
 * @param message Optional error message
 */
void report_error(const char* component, esp_err_t error, 
                 ErrorSeverity severity, const char* message = nullptr);

/**
 * @brief Run system-wide health check
 * 
 * Checks:
 * - Free heap memory
 * - Stack usage
 * - Module health scores
 * - Critical system resources
 * 
 * @return true if system is healthy
 */
bool check_health();

/**
 * @brief Get system uptime in milliseconds
 * @return Milliseconds since boot
 */
uint32_t get_uptime_ms();

/**
 * @brief Get free heap memory
 * @return Free heap in bytes
 */
size_t get_free_heap();

/**
 * @brief Get minimum free heap since boot
 * @return Minimum free heap in bytes
 */
size_t get_min_free_heap();

/**
 * @brief Get CPU usage percentage
 * @return CPU usage 0-100%
 */
uint8_t get_cpu_usage();

/**
 * @brief Get main task stack high water mark
 * @return Stack bytes remaining
 */
size_t get_stack_high_water_mark();

} // namespace Application

#endif // APPLICATION_H
