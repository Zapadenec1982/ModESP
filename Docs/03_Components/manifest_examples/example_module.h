/**
 * @file example_module.h
 * @brief Example module showing manifest integration
 * 
 * This is a template for creating new modules that integrate
 * with the manifest-driven architecture.
 */

#ifndef EXAMPLE_MODULE_H
#define EXAMPLE_MODULE_H

#include "base_module.h"
#include "nlohmann/json.hpp"

class ExampleModule : public BaseModule {
public:
    ExampleModule() = default;
    ~ExampleModule() override = default;
    
    // Required BaseModule interface
    const char* get_name() const override { return "ExampleModule"; }
    esp_err_t init() override;
    void update() override;
    void stop() override;
    
    // Optional configuration
    void configure(const nlohmann::json& config) override;
    
    // Health monitoring
    bool is_healthy() const override;
    uint8_t get_health_score() const override;
    
    // RPC support (optional)
    bool has_rpc() const override { return true; }
    void register_rpc(IJsonRpcRegistrar& registrar) override;
    
private:
    bool initialized_ = false;
    uint32_t update_interval_ms_ = 1000;
    uint32_t last_update_ms_ = 0;
    uint32_t update_count_ = 0;
    
    // Module-specific functionality
    void do_work();
    esp_err_t handle_rpc_status(const nlohmann::json& params, nlohmann::json& result);
};

#endif // EXAMPLE_MODULE_H
