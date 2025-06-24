# ModESP UI/API Documentation Index

## Core Documentation

### 📘 Main References
1. **[UI/API System Overview](UI_API_SYSTEM.md)** - Complete system documentation
   - Architecture, implementation, examples
   - Best practices and troubleshooting

2. **[Quick Start Guide](UI_API_QUICK_START.md)** - Get started in 3 steps
   - Simple examples for adding UI to modules
   - Common patterns and tips

3. **[Technical Reference](UI_API_TECHNICAL_REFERENCE.md)** - Implementation details
   - Memory layout, performance metrics
   - Build system, debugging, security

### 📋 Contracts and Schemas
4. **[API Contract](API_CONTRACT.md)** - System-wide contracts
   - SharedState keys and EventBus events
   - Data formats and conventions

5. **[System Contract Header](../components/core/include/system_contract.h)** - C++ definitions
   - Type-safe event and state names

### 🛠️ Tools and Generators
6. **[UI Generator](../tools/ui_generator.py)** - Build-time UI generation
   - Processes ui_schema.json files
   - Creates optimized C++ headers

7. **[Example Schema](../components/sensor_drivers/ui_schema.json)** - Schema template
   - Shows all available control types
   - Telemetry and alarm configuration

## Key Concepts

### UI Schema
Each module describes its UI once in JSON:
```json
{
    "module": "name",
    "controls": [...],  // UI elements
    "telemetry": {...}, // MQTT data
    "alarms": {...}     // Conditions
}
```

### Automatic Generation
Build system generates UI for all protocols:
- Web interface (HTML/JS/CSS)
- MQTT topics and discovery
- LCD menu structure
- Modbus register map

### Zero Configuration
New modules automatically appear in all UIs without code changes.

## Quick Links

- **Add UI to module**: See [Quick Start](UI_API_QUICK_START.md)
- **Understand architecture**: See [System Overview](UI_API_SYSTEM.md)
- **Debug issues**: See [Technical Reference](UI_API_TECHNICAL_REFERENCE.md#debugging)
- **View example**: See [Example Module](../components/sensor_drivers/ui_schema.json)

## Deprecated Documentation

The following documents are superseded by the above:
- ~~API_UI_ARCHITECTURE_ANALYSIS.md~~ → See Architecture in System Overview
- ~~UI_EXTENSIBILITY_ARCHITECTURE.md~~ → See UI Adapters in System Overview  
- ~~UI_COMPILE_TIME_GENERATION.md~~ → See Implementation in System Overview
- ~~UI_RESOURCE_COMPARISON.md~~ → See Resource Usage in Technical Reference
- ~~EXAMPLE_NEW_MODULE_WITH_AUTO_UI.md~~ → See Examples in System Overview

---

*Last updated: [Date]*
*For the latest information, check the source code and build system.*