/**
 * @file web_ui_module.cpp
 * @brief Implementation of Web UI Module
 */

#include "web_ui_module.h"
#include "api_dispatcher.h"
#include "system_contract.h"
#include "shared_state.h"
#include "event_bus.h"
#include "esp_log.h"
#include <memory>

static const char* TAG = "WebUIModule";

using namespace ModespContract;

// Constructor
WebUIModule::WebUIModule() 
    : api_dispatcher_(std::make_unique<ApiDispatcher>()) {
}

// Destructor
WebUIModule::~WebUIModule() {
    stop();
}

// Initialize module
esp_err_t WebUIModule::init() {
    ESP_LOGI(TAG, "Initializing Web UI Module");
    
    // Initialize API dispatcher
    api_dispatcher_->configure_rest_mappings();
    
    // Publish initial status
    SharedState::set(State::ApiStatus, nlohmann::json{
        {"running", false},
        {"port", config_.port}
    });
    
    return ESP_OK;
}