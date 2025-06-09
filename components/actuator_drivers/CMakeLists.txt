# Base actuator drivers component providing interfaces and registry
idf_component_register(
    INCLUDE_DIRS "include"
    REQUIRES 
        ESPhal      # For HAL interfaces
        json        # For nlohmann/json
    PRIV_REQUIRES
        esp_common
)