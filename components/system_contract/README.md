# System Contract Documentation

This directory contains the complete contract documentation for inter-module communication in ModESP.

## 📚 Contents

- **[events.md](events.md)** - All system events with payload structures
- **[states.md](states.md)** - All SharedState keys with types and update rates
- **[examples/](examples/)** - Usage examples for common scenarios

## 🎯 Quick Reference

### Using Events
```cpp
#include "generated_system_contract.h"

// Subscribe to sensor updates
EventBus::subscribe(Events::SENSOR_READING_UPDATED, handler);

// Publish error
EventBus::publish(Events::SENSOR_ERROR, {{"message", "Timeout"}});
```

### Using SharedState
```cpp
// Read temperature
float temp = SharedState::get<float>(States::TEMP_EVAPORATOR);

// Update state
SharedState::set(States::CLIMATE_MODE, "cooling");
```

Generated on: 2025-07-09 10:11:53
Total events: 3
Total modules: 1