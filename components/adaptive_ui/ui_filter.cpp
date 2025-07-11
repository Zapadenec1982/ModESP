// ui_filter.cpp
// Basic implementation of UI Filter for Phase 5

#include "ui_filter.h"
#include <regex>
#include <sstream>
#include "esp_log.h"

static const char* TAG = "UIFilter";

namespace ModESP::UI {

// ConditionEvaluator implementation
bool ConditionEvaluator::evaluate(const std::string& condition) {
    // Special cases
    if (condition == "always") return true;
    if (condition == "never") return false;
    
    // Simple equality check for now
    // Format: "config.path.to.value == 'expected'"
    
    // Extract parts using simple parsing
    if (condition.find("==") != std::string::npos) {
        auto pos = condition.find("==");
        std::string left = condition.substr(0, pos);
        std::string right = condition.substr(pos + 2);
        
        // Trim whitespace
        left.erase(0, left.find_first_not_of(" \t"));
        left.erase(left.find_last_not_of(" \t") + 1);
        right.erase(0, right.find_first_not_of(" \t"));
        right.erase(right.find_last_not_of(" \t") + 1);
        
        // Remove quotes from right side if present
        if (right.front() == '\'' && right.back() == '\'') {
            right = right.substr(1, right.length() - 2);
        }
        
        // Resolve config path
        std::string resolved = resolveConfigPath(left);
        
        ESP_LOGD(TAG, "Evaluating: %s == %s (resolved: %s)", 
                 left.c_str(), right.c_str(), resolved.c_str());
        
        return resolved == right;
    }
    
    // Check for role comparison
    if (condition.find("role") != std::string::npos) {
        // Simple implementation for now
        return true;
    }
    
    // Check for feature flags
    if (condition.find("has_feature") != std::string::npos) {
        auto start = condition.find("('");
        auto end = condition.find("')");
        if (start != std::string::npos && end != std::string::npos) {
            std::string feature = condition.substr(start + 2, end - start - 2);
            return hasFeature(feature);
        }
    }
    
    ESP_LOGW(TAG, "Unknown condition format: %s", condition.c_str());
    return false;
}

std::string ConditionEvaluator::resolveConfigPath(const std::string& path) {
    // Parse path like "config.sensor.type"
    std::vector<std::string> parts;
    std::stringstream ss(path);
    std::string part;
    
    while (std::getline(ss, part, '.')) {
        parts.push_back(part);
    }
    
    // Navigate JSON
    if (parts.empty() || parts[0] != "config") {
        return "";
    }
    
    nlohmann::json current = config;
    for (size_t i = 1; i < parts.size(); i++) {
        if (current.contains(parts[i])) {
            current = current[parts[i]];
        } else {
            return "";
        }
    }
    
    // Return as string
    if (current.is_string()) {
        return current.get<std::string>();
    } else {
        return current.dump();
    }
}

bool ConditionEvaluator::hasFeature(const std::string& feature) {
    auto it = feature_flags.find(feature);
    return it != feature_flags.end() && it->second;
}

// UIFilter implementation
std::vector<const ComponentMetadata*> UIFilter::filterComponents(
    const ComponentMetadata* all_components,
    size_t count
) {
    std::vector<const ComponentMetadata*> visible;
    
    if (!evaluator) {
        ESP_LOGE(TAG, "Filter not initialized!");
        return visible;
    }
    
    for (size_t i = 0; i < count; i++) {
        const auto& comp = all_components[i];
        
        if (isComponentVisible(comp)) {
            visible.push_back(&comp);
        }
    }
    
    ESP_LOGI(TAG, "Filtered %zu/%zu components", visible.size(), count);
    return visible;
}

bool UIFilter::isComponentVisible(const ComponentMetadata& component) {
    // Check cache first
    auto cache_it = condition_cache.find(component.id);
    if (cache_it != condition_cache.end()) {
        return cache_it->second;
    }
    
    // Evaluate condition
    bool condition_met = evaluator->evaluate(component.condition);
    
    // Check access level
    bool access_allowed = evaluator->checkAccess(component.min_access);
    
    bool visible = condition_met && access_allowed;
    
    // Cache result
    condition_cache[component.id] = visible;
    
    return visible;
}

std::vector<std::string> UIFilter::getVisibleComponents() const {
    std::vector<std::string> component_ids;
    
    // TODO: This should be properly implemented with actual component registry
    // For now, return some example components
    component_ids.push_back("sensor_temperature");
    component_ids.push_back("sensor_humidity");
    component_ids.push_back("control_setpoint");
    component_ids.push_back("status_display");
    
    return component_ids;
}

// FilterCriteria builder
std::unique_ptr<UIFilter> FilterCriteria::build() const {
    auto filter = std::make_unique<UIFilter>();
    filter->init(config, role);
    
    // Apply additional criteria
    for (const auto& feature : required_features) {
        filter->getEvaluator()->setFeatureFlag(feature, true);
    }
    
    return filter;
}

} // namespace ModESP::UI
