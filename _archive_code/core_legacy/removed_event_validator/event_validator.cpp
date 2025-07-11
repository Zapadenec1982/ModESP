/**
 * @file event_validator.cpp
 * @brief Implementation of event validator
 */

#include "event_validator.h"
#include "generated_system_contract.h"
#include "manifest_reader.h"
#include "esp_log.h"
#include <algorithm>

static const char* TAG = "EventValidator";

namespace ModESP {

EventValidator& EventValidator::getInstance() {
    static EventValidator instance;
    return instance;
}

esp_err_t EventValidator::init() {
    if (m_initialized) {
        ESP_LOGW(TAG, "EventValidator already initialized");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Initializing EventValidator");
    
    // Add all generated event constants
    // Note: In a real implementation, this would be generated automatically
    m_validEvents.insert(Events::SENSOR_CALIBRATION_COMPLETE);
    m_validEvents.insert(Events::SENSOR_ERROR);
    m_validEvents.insert(Events::SENSOR_READING_UPDATED);
    m_validEvents.insert(Events::SYSTEM_HEALTH_WARNING);
    m_validEvents.insert(Events::SYSTEM_HEARTBEAT);
    
    // Also get events from ManifestReader
    // This would include any additional events declared in manifests
    auto& manifestReader = ManifestReader::getInstance();
    auto allManifests = manifestReader.getAllModuleManifests();
    
    for (const auto& manifest : allManifests) {
        // TODO: Add API to get events from manifest
        // For now, we just use the generated constants
    }
    
    // Add some common dynamic patterns
    allowDynamicPattern("debug.*");
    allowDynamicPattern("test.*");
    allowDynamicPattern("custom.*");
    
    ESP_LOGI(TAG, "Loaded %zu valid events", m_validEvents.size());
    
    m_initialized = true;
    return ESP_OK;
}

bool EventValidator::isValidEvent(const std::string& eventName) const {
    if (!m_validationEnabled) {
        return true;  // All events are valid when validation is disabled
    }
    
    // Check if event is in the valid set
    if (m_validEvents.find(eventName) != m_validEvents.end()) {
        return true;
    }
    
    // Check if it matches a dynamic pattern
    if (matchesDynamicPattern(eventName)) {
        return true;
    }
    
    ESP_LOGW(TAG, "Invalid event name: %s", eventName.c_str());
    return false;
}

std::vector<std::string> EventValidator::getAllEvents() const {
    std::vector<std::string> events;
    events.reserve(m_validEvents.size());
    
    for (const auto& event : m_validEvents) {
        events.push_back(event);
    }
    
    // Sort for consistent ordering
    std::sort(events.begin(), events.end());
    
    return events;
}

void EventValidator::allowDynamicPattern(const std::string& pattern) {
    m_dynamicPatterns.push_back(pattern);
    ESP_LOGI(TAG, "Added dynamic pattern: %s", pattern.c_str());
}

bool EventValidator::matchesDynamicPattern(const std::string& eventName) const {
    for (const auto& pattern : m_dynamicPatterns) {
        // Simple wildcard matching (ends with *)
        if (pattern.back() == '*') {
            std::string prefix = pattern.substr(0, pattern.length() - 1);
            if (eventName.substr(0, prefix.length()) == prefix) {
                return true;
            }
        }
        // Exact match
        else if (pattern == eventName) {
            return true;
        }
    }
    
    return false;
}

} // namespace ModESP
