/**
 * @file system_contract.h
 * @brief Єдине джерело правди для всіх імен подій та ключів стану в системі ModESP
 * 
 * Цей файл визначає всі контракти для міжмодульної взаємодії через EventBus та SharedState.
 * Використання цих констант замість "магічних рядків" забезпечує безпеку типів та
 * полегшує рефакторинг.
 */

#ifndef SYSTEM_CONTRACT_H
#define SYSTEM_CONTRACT_H

#include <string_view>

namespace ModespContract {

/**
 * @brief Ключі для SharedState
 */
namespace State {
    // === Sensor States ===
    constexpr std::string_view SensorTemperature = "sensor.temperature";
    constexpr std::string_view SensorHumidity = "sensor.humidity";
    constexpr std::string_view SensorPressure = "sensor.pressure";
    constexpr std::string_view SensorDoorOpen = "sensor.door_open";
    
    // === Actuator States ===
    constexpr std::string_view ActuatorCompressor = "actuator.compressor";
    constexpr std::string_view ActuatorEvaporatorFan = "actuator.evaporator_fan";
    constexpr std::string_view ActuatorDefrostHeater = "actuator.defrost_heater";
    constexpr std::string_view ActuatorLight = "actuator.light";
    
    // === System States ===
    constexpr std::string_view SystemUptime = "system.uptime";
    constexpr std::string_view SystemFreeHeap = "system.free_heap";
    constexpr std::string_view SystemTime = "system.time";
    constexpr std::string_view SystemTimeValid = "system.time_valid";
    
    // === Climate Control States ===
    constexpr std::string_view ClimateSetpoint = "climate.setpoint";
    constexpr std::string_view ClimateMode = "climate.mode";
    constexpr std::string_view ClimateControlActive = "climate.control_active";
    
    // === Network States ===
    constexpr std::string_view NetworkWifiStatus = "network.wifi_status";
    constexpr std::string_view NetworkIpAddress = "network.ip_address";
    constexpr std::string_view NetworkMqttConnected = "network.mqtt_connected";
    
    // === API States ===
    constexpr std::string_view ApiStatus = "api.status";
    constexpr std::string_view ApiActiveConnections = "api.active_connections";
    constexpr std::string_view ApiMetrics = "api.metrics";
}

/**
 * @brief Імена подій для EventBus
 */
namespace Event {
    // === Sensor Events ===
    constexpr std::string_view SensorReading = "sensor.reading";
    constexpr std::string_view SensorError = "sensor.error";
    constexpr std::string_view SensorThresholdExceeded = "sensor.threshold_exceeded";
    
    // === Actuator Events ===
    constexpr std::string_view ActuatorCommand = "actuator.command";
    constexpr std::string_view ActuatorStateChanged = "actuator.state_changed";
    constexpr std::string_view ActuatorError = "actuator.error";
    
    // === System Events ===
    constexpr std::string_view SystemStartup = "system.startup";
    constexpr std::string_view SystemShutdown = "system.shutdown";
    constexpr std::string_view SystemError = "system.error";
    constexpr std::string_view SystemTimeSync = "system.time_sync";
    
    // === Climate Control Events ===
    constexpr std::string_view ClimateSetpointChanged = "climate.setpoint_changed";
    constexpr std::string_view ClimateModeChanged = "climate.mode_changed";
    constexpr std::string_view ClimateAlarm = "climate.alarm";
    
    // === Network Events ===
    constexpr std::string_view NetworkConnected = "network.connected";
    constexpr std::string_view NetworkDisconnected = "network.disconnected";
    constexpr std::string_view NetworkError = "network.error";
    
    // === API Events ===
    constexpr std::string_view ApiRequest = "api.request";
    constexpr std::string_view ApiResponse = "api.response";
    constexpr std::string_view ApiError = "api.error";
    constexpr std::string_view ApiClientConnected = "api.client_connected";
    constexpr std::string_view ApiClientDisconnected = "api.client_disconnected";
}

} // namespace ModespContract

#endif // SYSTEM_CONTRACT_H