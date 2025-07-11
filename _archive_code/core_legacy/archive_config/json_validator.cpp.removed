/**
 * @file json_validator.cpp
 * @brief Simplified JSON Schema validation implementation
 */

#include "json_validator.h"
#include "esp_log.h"
#include <sstream>
#include <algorithm>

namespace ModESP {

static const char* TAG = "JsonValidator";

JsonValidator& JsonValidator::instance() {
    static JsonValidator instance;
    return instance;
}

esp_err_t JsonValidator::load_schema(const std::string& schema_name, 
                                    const nlohmann::json& schema) {
    if (schema_name.empty()) {
        ESP_LOGE(TAG, "Schema name cannot be empty");
        return ESP_ERR_INVALID_ARG;
    }
    
    schemas_[schema_name] = schema;
    ESP_LOGI(TAG, "Loaded schema: %s", schema_name.c_str());
    return ESP_OK;
}

esp_err_t JsonValidator::load_schema(const std::string& schema_name,
                                    const std::string& schema_path) {
    // In embedded environment, schemas are typically compiled in
    // This is a placeholder for file-based loading    ESP_LOGW(TAG, "File-based schema loading not implemented");
    return ESP_ERR_NOT_SUPPORTED;
}

bool JsonValidator::validate(const nlohmann::json& json_data,
                           const std::string& schema_name,
                           std::vector<ValidationError>& errors) {
    errors.clear();
    
    auto it = schemas_.find(schema_name);
    if (it == schemas_.end()) {
        ESP_LOGE(TAG, "Schema not found: %s", schema_name.c_str());
        errors.push_back({"", "Schema not found: " + schema_name, ""});
        return false;
    }
    
    const auto& schema = it->second;
    return validate_type(json_data, schema, "", errors);
}

std::string JsonValidator::validate_with_details(const nlohmann::json& json_data,
                                               const std::string& schema_name) {
    std::vector<ValidationError> errors;
    if (validate(json_data, schema_name, errors)) {
        return "";  // Valid
    }
    
    std::stringstream ss;
    ss << "Validation failed for schema '" << schema_name << "':\n";
    for (const auto& error : errors) {
        ss << "  - ";        if (!error.path.empty()) {
            ss << error.path << ": ";
        }
        ss << error.message;
        if (!error.constraint.empty()) {
            ss << " (constraint: " << error.constraint << ")";
        }
        ss << "\n";
    }
    return ss.str();
}

bool JsonValidator::has_schema(const std::string& schema_name) const {
    return schemas_.find(schema_name) != schemas_.end();
}

std::vector<std::string> JsonValidator::get_loaded_schemas() const {
    std::vector<std::string> names;
    names.reserve(schemas_.size());
    for (const auto& pair : schemas_) {
        names.push_back(pair.first);
    }
    return names;
}

void JsonValidator::clear_schemas() {
    schemas_.clear();
}

bool JsonValidator::validate_type(const nlohmann::json& data,
                                const nlohmann::json& schema,
                                const std::string& path,                                std::vector<ValidationError>& errors) {
    // Handle $ref
    if (schema.contains("$ref")) {
        auto ref = schema["$ref"].get<std::string>();
        auto resolved = resolve_ref(schema, ref);
        if (resolved.is_null()) {
            errors.push_back({path, "Cannot resolve reference: " + ref, "$ref"});
            return false;
        }
        return validate_type(data, resolved, path, errors);
    }
    
    // Check type
    if (schema.contains("type")) {
        auto type = schema["type"].get<std::string>();
        bool type_valid = false;
        
        if (type == "object" && data.is_object()) type_valid = true;
        else if (type == "array" && data.is_array()) type_valid = true;
        else if (type == "string" && data.is_string()) type_valid = true;
        else if (type == "number" && data.is_number()) type_valid = true;
        else if (type == "integer" && data.is_number_integer()) type_valid = true;
        else if (type == "boolean" && data.is_boolean()) type_valid = true;
        else if (type == "null" && data.is_null()) type_valid = true;
        
        if (!type_valid) {
            errors.push_back({path, "Expected type '" + type + "' but got '" + 
                            data.type_name() + "'", "type"});
            return false;
        }
    }    
    // Type-specific validation
    if (data.is_object() && schema.contains("properties")) {
        if (!validate_properties(data, schema, path, errors)) {
            return false;
        }
    }
    
    if (data.is_array() && schema.contains("items")) {
        if (!validate_array(data, schema, path, errors)) {
            return false;
        }
    }
    
    // Validate constraints
    if (!validate_constraints(data, schema, path, errors)) {
        return false;
    }
    
    return errors.empty();
}

bool JsonValidator::validate_properties(const nlohmann::json& data,
                                      const nlohmann::json& schema,
                                      const std::string& path,
                                      std::vector<ValidationError>& errors) {
    const auto& properties = schema["properties"];
    
    // Check required properties
    if (schema.contains("required")) {
        for (const auto& req : schema["required"]) {
            auto req_name = req.get<std::string>();            if (!data.contains(req_name)) {
                errors.push_back({path, "Missing required property: " + req_name, "required"});
            }
        }
    }
    
    // Validate each property
    for (auto& [key, value] : data.items()) {
        std::string prop_path = path.empty() ? key : path + "." + key;
        
        if (properties.contains(key)) {
            validate_type(value, properties[key], prop_path, errors);
        } else if (schema.contains("additionalProperties") && 
                   !schema["additionalProperties"].get<bool>()) {
            errors.push_back({prop_path, "Additional property not allowed", "additionalProperties"});
        }
    }
    
    return true;
}

bool JsonValidator::validate_array(const nlohmann::json& data,
                                 const nlohmann::json& schema,
                                 const std::string& path,
                                 std::vector<ValidationError>& errors) {
    const auto& items = schema["items"];
    
    for (size_t i = 0; i < data.size(); ++i) {
        std::string item_path = path + "[" + std::to_string(i) + "]";
        validate_type(data[i], items, item_path, errors);
    }    
    return true;
}

bool JsonValidator::validate_constraints(const nlohmann::json& data,
                                       const nlohmann::json& schema,
                                       const std::string& path,
                                       std::vector<ValidationError>& errors) {
    // String constraints
    if (data.is_string()) {
        const auto& str = data.get<std::string>();
        
        if (schema.contains("minLength") && 
            str.length() < schema["minLength"].get<size_t>()) {
            errors.push_back({path, "String too short", "minLength"});
        }
        
        if (schema.contains("maxLength") && 
            str.length() > schema["maxLength"].get<size_t>()) {
            errors.push_back({path, "String too long", "maxLength"});
        }
        
        if (schema.contains("pattern")) {
            // Note: regex validation would require std::regex
            // which might be heavy for embedded use
            ESP_LOGW(TAG, "Pattern validation not implemented");
        }
        
        if (schema.contains("enum")) {
            bool found = false;
            for (const auto& enum_val : schema["enum"]) {                if (enum_val == data) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                errors.push_back({path, "Value not in enum", "enum"});
            }
        }
    }
    
    // Number constraints
    if (data.is_number()) {
        double val = data.get<double>();
        
        if (schema.contains("minimum") && val < schema["minimum"].get<double>()) {
            errors.push_back({path, "Value below minimum", "minimum"});
        }
        
        if (schema.contains("maximum") && val > schema["maximum"].get<double>()) {
            errors.push_back({path, "Value above maximum", "maximum"});
        }
    }
    
    // Array constraints
    if (data.is_array()) {
        size_t size = data.size();
        
        if (schema.contains("minItems") && size < schema["minItems"].get<size_t>()) {
            errors.push_back({path, "Array too small", "minItems"});
        }
                if (schema.contains("maxItems") && size > schema["maxItems"].get<size_t>()) {
            errors.push_back({path, "Array too large", "maxItems"});
        }
    }
    
    return true;
}

nlohmann::json JsonValidator::resolve_ref(const nlohmann::json& schema,
                                        const std::string& ref) {
    // Simple implementation for local refs only
    if (ref.find("#/definitions/") == 0) {
        std::string def_name = ref.substr(14);  // Skip "#/definitions/"
        
        if (schema.contains("definitions") && 
            schema["definitions"].contains(def_name)) {
            return schema["definitions"][def_name];
        }
    }
    
    return nullptr;
}

esp_err_t JsonValidator::init_default_schemas() {
    // Here we would load built-in schemas
    // For now, this is a placeholder
    ESP_LOGI(TAG, "Initializing default schemas");
    
    // Example: inline sensor schema
    nlohmann::json sensor_schema = R"({
        "type": "object",
        "properties": {
            "sensors": {
                "type": "array",
                "items": {
                    "type": "object",
                    "required": ["role", "type", "publish_key"],
                    "properties": {
                        "role": {"type": "string"},
                        "type": {"type": "string"},
                        "publish_key": {"type": "string"},
                        "config": {"type": "object"}
                    }
                }
            }
        }
    })"_json;
    
    load_schema("sensor_module", sensor_schema);
    
    return ESP_OK;
}

} // namespace ModESP