// Auto-generated MQTT topic definitions
// Generated at: 2025-06-25T22:14:58.435832

#pragma once

#include <pgmspace.h>

// sensor_drivers topics
#define MQTT_TOPIC_SENSOR_DRIVERS_TEMPERATURE "modesp/sensor_drivers/temperature"
#define MQTT_TOPIC_SENSOR_DRIVERS_HUMIDITY "modesp/sensor_drivers/humidity"
#define MQTT_CMD_SENSOR_DRIVERS_TEMP_OFFSET "modesp/sensor_drivers/temp_offset/set"

// Telemetry mapping
struct MqttTelemetryMap {
    const char* topic;
    const char* state_key;
    uint16_t interval_s;
};

const MqttTelemetryMap MQTT_TELEMETRY_MAP[] PROGMEM = {
    {MQTT_TOPIC_SENSOR_DRIVERS_TEMPERATURE, "state.sensor.temperature", 60},
    {MQTT_TOPIC_SENSOR_DRIVERS_HUMIDITY, "state.sensor.humidity", 60},
};

const size_t MQTT_TELEMETRY_COUNT = sizeof(MQTT_TELEMETRY_MAP) / sizeof(MqttTelemetryMap);
