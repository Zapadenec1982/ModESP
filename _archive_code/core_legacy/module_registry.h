/**
 * @file module_registry.h
 * @brief Module registration interface
 */

#ifndef MODULE_REGISTRY_H
#define MODULE_REGISTRY_H

#include "esp_err.h"

namespace ModuleRegistry {

/**
 * @brief Register all available modules with ModuleManager
 * 
 * This function registers all system modules in the correct order
 * and with appropriate priorities.
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t register_all_modules();

} // namespace ModuleRegistry

#endif // MODULE_REGISTRY_H 