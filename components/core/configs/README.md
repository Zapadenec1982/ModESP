# ModuChill Configuration Files

This directory contains default configuration files for different types of refrigeration equipment.

## Available Configurations

### default_config.json
- Standard refrigerator configuration
- Temperature range: -10°C to +10°C
- Single-stage control
- Basic defrost cycle

### freezer_config.json
- Commercial freezer configuration
- Temperature range: -30°C to -10°C
- Extended defrost cycles
- Lower compressor cycling

### ripening_config.json
- Ripening chamber configuration
- Temperature range: +10°C to +20°C
- Humidity control (85% ± 3%)
- CO2 monitoring and control
- Pre-programmed ripening curves

## Adding Custom Configurations

1. Create a new JSON file in this directory
2. Follow the structure of existing configs
3. Add the filename to CMakeLists.txt
4. Rebuild the project

## Configuration Structure

```json
{
    "version": 1,                    // Config version for migration
    "system": {                      // System settings
        "name": "Equipment Name",
        "type": "equipment_type"
    },
    "climate": {                     // Temperature control
        "setpoint": 4.0,
        "hysteresis": 0.5
    },
    "compressor": {                  // Compressor protection
        "min_off_time": 180,
        "min_on_time": 120
    },
    // ... other sections
}
```

## Usage

Configurations are embedded into the firmware at compile time. To use a specific configuration:

```cpp
// Load freezer configuration
ConfigManager::load_type("freezer");

// Get current type
std::string type = ConfigManager::get_type();

// Reset to factory defaults for current type
ConfigManager::reset_to_defaults();
```

## Best Practices

1. **Version your configs** - Increment version when making breaking changes
2. **Document changes** - Add comments for non-obvious values
3. **Test thoroughly** - Validate ranges and dependencies
4. **Keep it simple** - Don't add features that aren't implemented
5. **Use sensible defaults** - Safe values that won't damage equipment

## Validation

Configurations are validated at:
- Compile time (JSON syntax)
- Load time (required fields)
- Runtime (value ranges)

## Migration

When updating configuration structure:
1. Increment the version number
2. Add migration logic in config_manager.cpp
3. Document the changes in this README