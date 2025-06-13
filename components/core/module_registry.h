/**
 * @file module_registry.h
 * @brief Module registration declarations
 */

#pragma once

#include <esp_err.h>

namespace ModuleRegistry {

/**
 * @brief Register all available modules with the ModuleManager
 * @return ESP_OK on success
 */
esp_err_t register_all_modules();

} // namespace ModuleRegistry 