/**
 * @file application_wrapper.cpp
 * @brief C wrapper for C++ Application namespace
 * 
 * Provides C-compatible interface for the C++ Application class
 * so it can be called from main.c
 */

#include "application.h"
#include <esp_log.h>

extern "C" {

/**
 * @brief Launch the C++ application
 * 
 * This is a C wrapper function that initializes and runs the C++ application.
 * Called from main.c app_main() function.
 */
void launch_application() {
    // Initialize the application
    esp_err_t ret = Application::init();
    if (ret != ESP_OK) {
        ESP_LOGE("ApplicationWrapper", "Failed to initialize application: %s", esp_err_to_name(ret));
        return;
    }
    
    // Run the main application loop (this will block forever)
    Application::run();
}

} // extern "C" 