/**
 * @file json_validator.h
 * @brief JSON Schema validation for ModESP configurations
 */

#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include "nlohmann/json.hpp"
#include "esp_err.h"

namespace ModESP {

/**
 * @brief Validation error information
 */
struct ValidationError {
    std::string path;        // JSON path where error occurred
    std::string message;     // Error description
    std::string constraint;  // Schema constraint that failed
};

/**
 * @brief JSON Schema validator
 * 
 * Provides configuration validation against JSON schemas
 * to catch errors before they reach modules.
 */
class JsonValidator {
public:
    /**
     * @brief Get singleton instance
     */
    static JsonValidator& instance();    
    /**
     * @brief Load schema from file
     * 
     * @param schema_name Name to register schema as
     * @param schema_path Path to JSON schema file
     * @return ESP_OK on success
     */
    esp_err_t load_schema(const std::string& schema_name, 
                         const std::string& schema_path);
    
    /**
     * @brief Load schema from JSON object
     * 
     * @param schema_name Name to register schema as
     * @param schema JSON schema object
     * @return ESP_OK on success
     */
    esp_err_t load_schema(const std::string& schema_name,
                         const nlohmann::json& schema);
    
    /**
     * @brief Validate JSON against schema
     * 
     * @param json_data Data to validate
     * @param schema_name Name of schema to validate against
     * @param errors Output validation errors
     * @return true if valid, false if validation failed
     */
    bool validate(const nlohmann::json& json_data,
                 const std::string& schema_name,
                 std::vector<ValidationError>& errors);    
    /**
     * @brief Validate with detailed error reporting
     * 
     * @param json_data Data to validate
     * @param schema_name Name of schema to validate against
     * @return Formatted error string or empty if valid
     */
    std::string validate_with_details(const nlohmann::json& json_data,
                                     const std::string& schema_name);
    
    /**
     * @brief Check if schema is loaded
     * 
     * @param schema_name Schema name to check
     * @return true if schema is loaded
     */
    bool has_schema(const std::string& schema_name) const;
    
    /**
     * @brief Get list of loaded schemas
     * 
     * @return Vector of schema names
     */
    std::vector<std::string> get_loaded_schemas() const;
    
    /**
     * @brief Clear all loaded schemas
     */
    void clear_schemas();
    
    /**
     * @brief Initialize with default schemas
     * 
     * Loads built-in schemas for standard modules
     * @return ESP_OK on success
     */
    esp_err_t init_default_schemas();
private:
    JsonValidator() = default;
    ~JsonValidator() = default;
    
    // Non-copyable singleton
    JsonValidator(const JsonValidator&) = delete;
    JsonValidator& operator=(const JsonValidator&) = delete;
    
    // Schema storage
    std::unordered_map<std::string, nlohmann::json> schemas_;
    
    // Helper methods
    bool validate_type(const nlohmann::json& data, 
                      const nlohmann::json& schema,
                      const std::string& path,
                      std::vector<ValidationError>& errors);
    
    bool validate_properties(const nlohmann::json& data,
                           const nlohmann::json& schema,
                           const std::string& path,
                           std::vector<ValidationError>& errors);
    
    bool validate_array(const nlohmann::json& data,
                       const nlohmann::json& schema,
                       const std::string& path,
                       std::vector<ValidationError>& errors);
    
    bool validate_constraints(const nlohmann::json& data,
                            const nlohmann::json& schema,
                            const std::string& path,
                            std::vector<ValidationError>& errors);
    
    nlohmann::json resolve_ref(const nlohmann::json& schema,
                              const std::string& ref);
};

} // namespace ModESP