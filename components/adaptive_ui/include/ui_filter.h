// ui_filter.h
// Smart filtering engine for Adaptive UI

#pragma once

#include "include/ui_component_base.h"
#include <vector>
#include <regex>
#include <unordered_map>

namespace ModESP::UI {

/**
 * @brief User role for access control
 */
using UserRole = AccessLevel;  // Reuse AccessLevel enum

/**
 * @brief Condition evaluator for dynamic filtering
 */
class ConditionEvaluator {
private:
    const nlohmann::json& config;
    UserRole current_role;
    std::unordered_map<std::string, bool> feature_flags;
    
    // Helper methods
    bool evaluateComparison(const std::string& left, 
                          const std::string& op, 
                          const std::string& right);
    
    std::string resolveConfigPath(const std::string& path);
    bool hasFeature(const std::string& feature);
    
public:
    ConditionEvaluator(const nlohmann::json& cfg, UserRole role)
        : config(cfg), current_role(role) {}
    
    /**
     * @brief Evaluate condition string
     * 
     * Examples:
     * - "always" -> true
     * - "never" -> false
     * - "config.sensor.type == 'DS18B20'"
     * - "role >= 'technician'"
     * - "has_feature('calibration')"
     * - "config.sensor.count > 0 && role == 'admin'"
     */
    bool evaluate(const std::string& condition);
    
    /**
     * @brief Set feature flags
     */
    void setFeatureFlag(const std::string& feature, bool enabled) {
        feature_flags[feature] = enabled;
    }
    
    /**
     * @brief Check if user role has access to component
     */
    bool checkAccess(AccessLevel min_access) const {
        return current_role >= min_access;
    }
};

/**
 * @brief UI Filter for adaptive component selection
 */
class UIFilter {
private:
    std::unique_ptr<ConditionEvaluator> evaluator;
    
    // Cache for performance
    mutable std::unordered_map<std::string, bool> condition_cache;
    
public:
    UIFilter() = default;
    
    /**
     * @brief Initialize filter with config and role
     */
    void init(const nlohmann::json& config, UserRole role) {
        evaluator = std::make_unique<ConditionEvaluator>(config, role);
        condition_cache.clear();
    }
    
    /**
     * @brief Filter components based on current state
     * 
     * @param all_components Array of all possible components
     * @param count Number of components
     * @return Vector of visible component metadata
     */
    std::vector<const ComponentMetadata*> filterComponents(
        const ComponentMetadata* all_components,
        size_t count
    );
    
    /**
     * @brief Check single component visibility
     */
    bool isComponentVisible(const ComponentMetadata& component);
    
    /**
     * @brief Get IDs of visible components
     */
    std::vector<std::string> getVisibleComponents() const;
    
    /**
     * @brief Clear condition cache (call on config change)
     */
    void clearCache() { condition_cache.clear(); }
    
    /**
     * @brief Get condition evaluator for feature flag setting
     */
    ConditionEvaluator* getEvaluator() { return evaluator.get(); }
    
    /**
     * @brief Get filter statistics
     */
    struct FilterStats {
        size_t total_components;
        size_t visible_components;
        size_t cache_hits;
        size_t cache_misses;
        uint32_t last_filter_time_us;
    };
    
    FilterStats getStats() const;
};

/**
 * @brief Filter criteria builder (fluent interface)
 */
class FilterCriteria {
private:
    nlohmann::json config;
    UserRole role = UserRole::USER;
    std::vector<std::string> required_features;
    std::vector<std::string> excluded_types;
    
public:
    FilterCriteria& withConfig(const nlohmann::json& cfg) {
        config = cfg;
        return *this;
    }
    
    FilterCriteria& withRole(UserRole r) {
        role = r;
        return *this;
    }
    
    FilterCriteria& requireFeature(const std::string& feature) {
        required_features.push_back(feature);
        return *this;
    }
    
    FilterCriteria& excludeType(ComponentType type) {
        excluded_types.push_back(std::to_string(static_cast<int>(type)));
        return *this;
    }
    
    std::unique_ptr<UIFilter> build() const;
};

} // namespace ModESP::UI
