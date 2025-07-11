/**
 * @file event_validator.h
 * @brief Event validation using manifest-generated event names
 */

#ifndef EVENT_VALIDATOR_H
#define EVENT_VALIDATOR_H

#include <string>
#include <unordered_set>
#include <vector>
#include "esp_err.h"

namespace ModESP {

/**
 * @brief Validates event names against manifest-declared events
 * 
 * Uses generated event constants to ensure only valid events
 * are published/subscribed to.
 */
class EventValidator {
public:
    /**
     * @brief Get singleton instance
     */
    static EventValidator& getInstance();
    
    /**
     * @brief Initialize validator with generated event data
     * @return ESP_OK on success
     */
    esp_err_t init();
    
    /**
     * @brief Check if event name is valid
     * @param eventName Event name to validate
     * @return true if event is declared in manifests
     */
    bool isValidEvent(const std::string& eventName) const;
    
    /**
     * @brief Get all valid event names
     * @return Vector of all declared event names
     */
    std::vector<std::string> getAllEvents() const;
    
    /**
     * @brief Check if validation is enabled
     * @return true if validation is active
     */
    bool isValidationEnabled() const { return m_validationEnabled; }
    
    /**
     * @brief Enable/disable event validation
     * @param enabled true to enable validation
     */
    void setValidationEnabled(bool enabled) { m_validationEnabled = enabled; }
    
    /**
     * @brief Allow dynamic events (not in manifests)
     * @param pattern Wildcard pattern (e.g., "custom.*")
     */
    void allowDynamicPattern(const std::string& pattern);
    
    /**
     * @brief Check if event matches any dynamic pattern
     * @param eventName Event name to check
     * @return true if matches allowed dynamic pattern
     */
    bool matchesDynamicPattern(const std::string& eventName) const;
    
private:
    EventValidator() = default;
    ~EventValidator() = default;
    
    // Prevent copying
    EventValidator(const EventValidator&) = delete;
    EventValidator& operator=(const EventValidator&) = delete;
    
    bool m_initialized = false;
    bool m_validationEnabled = true;
    std::unordered_set<std::string> m_validEvents;
    std::vector<std::string> m_dynamicPatterns;
};

} // namespace ModESP

#endif // EVENT_VALIDATOR_H
