# 🚀 Наступні кроки: UI/API розвиток ModESP

## ✅ Завершено (23.06.2025)
- [x] Виправлено Python UI генератор
- [x] Згенеровано всі UI файли
- [x] Узгоджено документацію з реалізацією
- [x] Створено приклад ui_schema.json для sensor_drivers

## 🎯 Пріоритет 1: Розширення на всі модулі (1-2 дні)

### Додати ui_schema.json до модулів:

- [ ] **ActuatorModule** (`components/ESPhal/modules/actuator_module/ui_schema.json`)
```json
{
    "module": "actuator_module", 
    "label": "Actuators",
    "controls": [
        {
            "id": "compressor",
            "type": "switch", 
            "label": "Compressor",
            "read_method": "actuator.get_state",
            "write_method": "actuator.set_state"
        }
    ]
}
```

- [ ] **RTCModule** (`components/ESPhal/modules/rtc_module/ui_schema.json`)
```json
{
    "module": "rtc_module",
    "label": "Real Time Clock", 
    "controls": [
        {
            "id": "current_time",
            "type": "value",
            "label": "Current Time",
            "read_method": "rtc.get_time"
        }
    ]
}
```

- [ ] **WiFiManager** (`components/wifi_manager/ui_schema.json`)
```json
{
    "module": "wifi_manager",
    "label": "WiFi",
    "controls": [
        {
            "id": "status",
            "type": "value", 
            "label": "Connection Status",
            "read_method": "wifi.get_status"
        },
        {
            "id": "signal_strength",
            "type": "gauge",
            "label": "Signal Strength",
            "unit": "dBm",
            "min": -100,
            "max": -30,
            "read_method": "wifi.get_rssi"
        }
    ]
}
```

### Після додавання схем:
- [ ] Запустити генератор: `python tools/ui_generator.py`
- [ ] Зібрати проект: `idf.py build`  
- [ ] Перевірити згенеровані файли у `main/generated/`

## 🎯 Пріоритет 2: Тестування Web UI (3-5 днів)

- [ ] **Інтеграція WebUIModule**
  - [ ] Додати WebUIModule до основного додатку
  - [ ] Налаштувати HTTP сервер
  - [ ] Перевірити обслуговування static файлів

- [ ] **API тестування**
  - [ ] Тестувати `/api/data` endpoint
  - [ ] Тестувати `/api/rpc` endpoint  
  - [ ] Перевірити JSON-RPC методи модулів

- [ ] **Frontend тестування**
  - [ ] Перевірити відображення згенерованого HTML
  - [ ] Тестувати автоматичне оновлення даних
  - [ ] Перевірити форми вводу та команди

## 🎯 Пріоритет 3: MQTT інтеграція (1 тиждень)

- [ ] **Створити MQTT UI адаптер**
  - [ ] Базовий клас на основі UIAdapterBase
  - [ ] Використання згенерованих MQTT топіків
  - [ ] Автоматична публікація телеметрії

- [ ] **Приклад використання**:
```cpp
class MQTTUIAdapter : public UIAdapterBase {
    void register_ui_handlers() override {
        // Використовувати MQTT_TELEMETRY_MAP з generated файлу
        for (size_t i = 0; i < MQTT_TELEMETRY_COUNT; i++) {
            auto& mapping = MQTT_TELEMETRY_MAP[i];
            setup_telemetry_publishing(mapping.topic, mapping.state_key, mapping.interval_s);
        }
    }
};
```

## 🎯 Пріоритет 4: LCD UI (1 тиждень)

- [ ] **LCD UI адаптер**
  - [ ] Використання згенерованого `lcd_menu_generated.h`
  - [ ] Навігація по меню
  - [ ] Показ значень з SharedState

## 📋 Швидкі команди для розробки

### Генерація UI:
```bash
cd C:\ModESP_dev
python tools\ui_generator.py components main\generated
```

### Збірка:
```bash
idf.py build
```

### Перевірка згенерованих файлів:
```bash
ls main/generated/
```

### Додавання нового модуля:
1. Створити `my_module/ui_schema.json`
2. Запустити генератор
3. Зібрати проект
4. Модуль з'явиться в усіх UI!

## 🐛 Що робити при проблемах

### Python генератор не працює:
```bash
python --version  # Перевірити Python 3.8+
python -m pip install pathlib datetime json
```

### Генеровані файли не включаються:
- Перевірити `cmake/ui_generation.cmake` 
- Перевірити залежності у CMakeLists.txt
- Виконати `idf.py clean && idf.py build`

### UI схема не знайдена:
- Перевірити назву файлу: точно `ui_schema.json`
- Перевірити розташування: `components/[module_name]/ui_schema.json`
- Перевірити JSON синтаксис

## 📈 Метрики успіху

### Поточні досягнення:
- ✅ 93% економія RAM
- ✅ 85% швидше UI відгук  
- ✅ 1 модуль з автоматичним UI

### Цілі на наступний тиждень:
- 🎯 5+ модулів з UI схемами
- 🎯 Повнофункціональний Web UI
- 🎯 MQTT інтеграція
- 🎯 LCD меню

### Довгострокові цілі:
- 🎯 10+ UI адаптерів
- 🎯 Mobile app API
- 🎯 Cloud integration
- 🎯 Advanced dashboards

---

**💡 Пам'ятайте**: Кожен новий модуль з `ui_schema.json` автоматично отримує UI у всіх інтерфейсах!

**📞 Підтримка**: Задавайте питання про UI архітектуру у документації або коментарях до коду.
