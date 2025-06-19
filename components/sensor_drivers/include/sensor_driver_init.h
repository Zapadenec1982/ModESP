/**
 * @file sensor_driver_init.h
 * @brief Initialization of built-in sensor drivers
 */

#pragma once

/**
 * @brief Initialize all built-in sensor drivers
 * 
 * This function registers all built-in sensor drivers with the registry.
 * Call this function during system initialization.
 */
void initialize_builtin_sensor_drivers();
