/**
 * @file actuator_module.h
 * @brief ActuatorModule - HAL module for actuator control
 * 
 * ActuatorModule uses a modular driver system where each actuator type is a 
 * self-contained driver. It listens to commands from SharedState, creates 
 * driver instances through the registry, and manages actuator states.
 * 
 * Key features:
 * - Dynamic driver loading through registry
 * - Type-agnostic actuator management  
 * - Configuration-driven actuator creation
 * - Command routing from SharedState
 * - Status feedback to SharedState
 */

#pragma once

#include "base_module.h"
#include "esphal.h"
#include "shared_state.h"
#include "actuator_driver_interface.h"
#include "nlohmann/json.hpp"
#include <vector>
#include <memory>
#include <string>
#include <functional>

/**
 * @brief Configuration for a single actuator instance
 */
struct ActuatorConfig {
    std::string role;           // Logical role (e.g., "compressor")
    std::string type;           // Driver type (e.g., "RELAY", "PWM")
    std::string command_key;    // SharedState key to listen for commands
    std::string status_key;     // SharedState key to publish status
    nlohmann::json config;      // Driver-specific configuration
};
/**
 * @brief ActuatorModule - HAL module for actuator management
 * 
 * Manages all actuators in the system by:
 * - Creating driver instances from configuration
 * - Subscribing to command keys in SharedState
 * - Routing commands to appropriate drivers
 * - Publishing status updates
 * - Handling emergency stops
 */
class ActuatorModule : public BaseModule {
public:
    /**
     * @brief Constructor with Dependency Injection
     * @param hal Reference to ESPhal instance for hardware access
     */
    explicit ActuatorModule(ESPhal& hal);
    
    /**
     * @brief Destructor
     */
    ~ActuatorModule() override = default;
    
    // Non-copyable, non-movable
    ActuatorModule(const ActuatorModule&) = delete;
    ActuatorModule& operator=(const ActuatorModule&) = delete;
    ActuatorModule(ActuatorModule&&) = delete;
    ActuatorModule& operator=(ActuatorModule&&) = delete;

    // === BaseModule interface ===
    
    const char* get_name() const override {
        return "ActuatorModule";  // Config section: "actuators"
    }
    
    esp_err_t init() override;
    void update() override;
    void stop() override;
    void configure(const nlohmann::json& config) override;
    bool is_healthy() const override;
    uint8_t get_health_score() const override;
    uint32_t get_max_update_time_us() const override;    
    // === ActuatorModule specific methods ===
    
    /**
     * @brief Get actuator status by role
     * @param role Actuator role (e.g., "compressor")
     * @return Status or nullopt if actuator not found
     */
    std::optional<ActuatorStatus> get_actuator_status(const std::string& role) const;
    
    /**
     * @brief Get actuator configuration by role
     * @param role Actuator role
     * @return Config or nullopt if not found
     */
    std::optional<ActuatorConfig> get_actuator_config(const std::string& role) const;
    
    /**
     * @brief Execute command immediately (bypass SharedState)
     * @param role Actuator role
     * @param command Command to execute
     * @return ESP_OK on success
     */
    esp_err_t execute_command(const std::string& role, const nlohmann::json& command);
    
    /**
     * @brief Emergency stop all actuators
     * @return ESP_OK on success
     */
    esp_err_t emergency_stop_all();
    
    /**
     * @brief Get available actuator driver types
     * @return List of registered driver type identifiers
     */
    std::vector<std::string> get_available_drivers() const;

private:
    // Structure to hold actuator instance with its configuration
    struct ActuatorInstance {
        std::unique_ptr<IActuatorDriver> driver;
        ActuatorConfig config;
        std::string subscription_id;     // SharedState subscription ID
        uint32_t command_count = 0;
        uint32_t error_count = 0;
    };    
    // Reference to HAL for hardware access
    ESPhal& hal_;
    
    // All active actuators
    std::vector<ActuatorInstance> actuators_;
    
    // Module state
    bool initialized_ = false;
    uint32_t update_count_ = 0;
    uint32_t total_commands_ = 0;
    uint32_t total_errors_ = 0;
    
    // Configuration
    uint32_t update_interval_ms_ = 100;  // Update interval for time-based drivers
    bool publish_on_error_ = true;       // Publish error states
    
    // Helper methods
    esp_err_t create_actuator_from_config(const nlohmann::json& actuator_config);
    void handle_command(const std::string& role, const nlohmann::json& value);
    void publish_actuator_status(const ActuatorInstance& actuator);
    std::unique_ptr<IActuatorDriver> create_driver(const std::string& type);
};

#endif // ACTUATOR_MODULE_H