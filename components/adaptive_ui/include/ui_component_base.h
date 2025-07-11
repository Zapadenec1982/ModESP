// ui_component_base.h
// Base classes for Phase 5 Adaptive UI components

#pragma once

#include <string>
#include <memory>
#include <functional>
#include "nlohmann/json.hpp"

namespace ModESP::UI {

// Forward declarations
class LCDRenderer;
class WebRenderer;
class MQTTRenderer;
class TelegramRenderer;

/**
 * @brief Component types
 */
enum class ComponentType {
    TEXT,
    NUMBER,
    TOGGLE,
    BUTTON,
    SLIDER,
    DROPDOWN,
    CHART,
    LIST,
    COMPOSITE
};

/**
 * @brief Access levels
 */
enum class AccessLevel {
    USER = 0,
    OPERATOR = 1,
    TECHNICIAN = 2,
    SUPERVISOR = 3,
    ADMIN = 4
};

/**
 * @brief Component priority for loading
 */
enum class Priority {
    LOW,
    MEDIUM,
    HIGH,
    CRITICAL
};

/**
 * @brief Base UI Component interface
 * 
 * All UI components inherit from this
 */
class UIComponent {
protected:
    std::string id;
    std::string label;
    AccessLevel min_access;
    Priority priority;
    
public:
    UIComponent(const std::string& id, const std::string& label,
                AccessLevel access = AccessLevel::USER,
                Priority prio = Priority::MEDIUM)
        : id(id), label(label), min_access(access), priority(prio) {}
    
    virtual ~UIComponent() = default;
    
    // Getters
    const std::string& getId() const { return id; }
    const std::string& getLabel() const { return label; }
    AccessLevel getMinAccess() const { return min_access; }
    Priority getPriority() const { return priority; }
    
    // Pure virtual methods
    virtual ComponentType getType() const = 0;
    virtual size_t getEstimatedSize() const = 0;
    
    // Channel-specific rendering
    virtual void renderLCD(LCDRenderer& renderer) = 0;
    virtual void renderWeb(WebRenderer& renderer) = 0;
    virtual void renderMQTT(MQTTRenderer& renderer) = 0;
    virtual void renderTelegram(TelegramRenderer& renderer) {
        // Default: not all components need Telegram support
    }
    
    // Value binding
    virtual void bindValue(const std::string& state_key) {}
    virtual void updateValue(const nlohmann::json& value) {}
};

/**
 * @brief Text display component
 */
class TextComponent : public UIComponent {
private:
    std::string value;
    std::string state_key;
    
public:
    TextComponent(const std::string& id, const std::string& label)
        : UIComponent(id, label) {}
    
    ComponentType getType() const override { return ComponentType::TEXT; }
    size_t getEstimatedSize() const override { return 128; }
    
    void renderLCD(LCDRenderer& renderer) override;
    void renderWeb(WebRenderer& renderer) override;
    void renderMQTT(MQTTRenderer& renderer) override;
    
    void bindValue(const std::string& key) override { state_key = key; }
    void updateValue(const nlohmann::json& val) override {
        if (val.is_string()) value = val.get<std::string>();
        else value = val.dump();
    }
};

/**
 * @brief Slider component
 */
class SliderComponent : public UIComponent {
private:
    float value;
    float min_val;
    float max_val;
    float step;
    std::string state_key;
    std::function<void(float)> on_change;
    
public:
    SliderComponent(const std::string& id, const std::string& label,
                   float min, float max, float step = 1.0f)
        : UIComponent(id, label), min_val(min), max_val(max), step(step) {}
    
    ComponentType getType() const override { return ComponentType::SLIDER; }
    size_t getEstimatedSize() const override { return 256; }
    
    void renderLCD(LCDRenderer& renderer) override;
    void renderWeb(WebRenderer& renderer) override;
    void renderMQTT(MQTTRenderer& renderer) override;
    
    void onChange(std::function<void(float)> handler) { on_change = handler; }
};

/**
 * @brief Component metadata for filtering
 */
struct ComponentMetadata {
    std::string id;
    ComponentType type;
    std::string condition;      // e.g., "config.sensor.type == 'DS18B20'"
    AccessLevel min_access;
    Priority priority;
    bool lazy_loadable;
    size_t estimated_size;
    
    // Factory function to create the component
    std::function<std::unique_ptr<UIComponent>()> factory;
};

} // namespace ModESP::UI
