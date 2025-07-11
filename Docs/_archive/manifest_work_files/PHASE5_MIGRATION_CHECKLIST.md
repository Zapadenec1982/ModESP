# Migration Checklist: Existing Modules to Phase 5

## 📋 Загальний план міграції

Цей документ містить покроковий план міграції існуючих модулів на Phase 5 Adaptive UI Architecture.

## 🎯 Модулі для міграції

### Priority 1 - Managers:
- [ ] SensorModule → SensorManager
- [ ] ActuatorModule → ActuatorManager
- [ ] ClimateControlModule → ClimateManager

### Priority 2 - Core:
- [ ] LoggerModule (додати adaptive UI)
- [ ] RTCModule (оновити маніфест)
- [ ] HeartbeatModule (приклад simple case)

### Priority 3 - Utilities:
- [ ] ConfigurationManager
- [ ] WebUIModule
- [ ] MQTTModule

## 📝 Checklist для кожного модуля

### 1. Оновити маніфест

#### Для Manager модулів:
```json
{
  "module": {
    "type": "MANAGER",  // Додати
    "driver_interface": "ISensorDriver"  // Додати
  },
  
  "ui": {
    "adaptive": {  // Нова секція
      "components": [...]
    }
  },
  
  "driver_registry": {  // Оновити
    "path": "sensor_drivers",
    "pattern": "*_driver_manifest.json",
    "interface": "ISensorDriver"
  }
}
```

#### Для звичайних модулів:
```json
{
  "ui": {
    "adaptive": {
      "components": [
        {
          "id": "module_status",
          "type": "text",
          "conditions": ["always"],
          "access_level": "user",
          "priority": "medium",
          "lazy_load": true
        }
      ]
    }
  }
}
```

### 2. Оновити клас модуля

#### Для Manager:
```cpp
class SensorManager : public BaseModule {
private:
    // Додати підтримку драйверів
    std::vector<std::unique_ptr<ISensorDriver>> drivers;
    
public:
    // Додати методи для драйверів
    esp_err_t registerDriver(std::unique_ptr<ISensorDriver> driver);
    std::vector<std::string> getAllUIComponents() const;
};
```

### 3. Створити/оновити драйвери

#### Структура драйвера:
```cpp
class DS18B20Driver : public ISensorDriver {
public:
    // Implement BaseDriver interface
    const char* get_name() const override;
    const char* get_type() const override;
    
    // Implement ISensorDriver
    esp_err_t read_value(float& value) override;
    
    // UI components
    std::vector<std::string> get_ui_components() const override {
        return {
            "ds18b20_resolution_slider",
            "ds18b20_parasite_toggle"
        };
    }
};
```

### 4. Оновити driver manifests

```json
{
  "driver": {
    "implements": "ISensorDriver"
  },
  
  "ui_extensions": {
    "inject_into": "sensor_config_panel",
    "components": [
      {
        "id": "ds18b20_resolution",
        "type": "slider",
        "condition": "config.sensor.type == 'DS18B20'",
        "access_level": "technician"
      }
    ]
  }
}
```

### 5. Тестування

- [ ] Компіляція без помилок
- [ ] Manifest validation passed
- [ ] UI components генеруються
- [ ] Filtering працює правильно
- [ ] Lazy loading функціонує
- [ ] Performance не погіршилась

## 🔄 Приклад міграції SensorModule

### Before (Phase 2):
```cpp
class SensorModule : public BaseModule {
    // Монолітний модуль з всіма драйверами
};
```

### After (Phase 5):
```cpp
class SensorManager : public BaseModule {
private:
    std::vector<std::unique_ptr<ISensorDriver>> drivers;
    
public:
    esp_err_t registerDriver(std::unique_ptr<ISensorDriver> driver) {
        drivers.push_back(std::move(driver));
        return ESP_OK;
    }
};

// Окремі драйвери
class DS18B20Driver : public ISensorDriver { ... };
class NTCDriver : public ISensorDriver { ... };
```

## 📊 Tracking Progress

| Module | Manifest | Code | Drivers | UI | Tests | Status |
|--------|----------|------|---------|----|----|--------|
| SensorModule | 🔄 | 🔄 | 🔄 | ⏳ | ⏳ | In Progress |
| ActuatorModule | ⏳ | ⏳ | ⏳ | ⏳ | ⏳ | Planned |
| ClimateControl | ⏳ | ⏳ | ⏳ | ⏳ | ⏳ | Planned |
| LoggerModule | ⏳ | ⏳ | N/A | ⏳ | ⏳ | Planned |

Legend: ✅ Done | 🔄 In Progress | ⏳ Planned | ❌ Blocked

## 🚨 Важливі моменти

### Backward Compatibility:
- Старі маніфести повинні працювати
- Використовувати feature flags для поступової міграції
- Тестувати в parallel mode

### Performance:
- Вимірювати RAM/Flash до і після
- Профілювати UI render time
- Оптимізувати lazy loading

### Testing:
- Unit tests для кожного компонента
- Integration tests для Manager-Driver
- Performance benchmarks

## 🎯 Success Criteria

Модуль вважається успішно мігрованим коли:
1. ✅ Всі тести проходять
2. ✅ RAM usage знизилось на 20%+
3. ✅ UI updates < 10ms
4. ✅ Всі features працюють
5. ✅ Документація оновлена

---

*Документ створено: 2025-01-27*  
*Для Phase 5 Migration Process*
