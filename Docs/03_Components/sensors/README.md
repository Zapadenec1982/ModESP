# Sensors - Модулі датчиків

Системи збору даних з підтримкою різних типів датчиків.

## 📚 Документи

### Основна документація
- **[sensor_module.txt](sensor_module.txt)** - Архітектура модуля датчиків
- **[async_optimization.md](async_optimization.md)** - Асинхронна оптимізація
- **[ds18b20_async.md](ds18b20_async.md)** - DS18B20 драйвер

### Детальна технічна документація
- **[execution_details.md](execution_details.md)** - 🆕 Деталі виконання sensor системи
- **[optimization_guide.md](optimization_guide.md)** - 🆕 Специфічні оптимізації sensor модулів
- **[architecture_summary.md](architecture_summary.md)** - 🆕 Підсумок модульної архітектури HAL
- **[sensor_drivers_readme.md](sensor_drivers_readme.md)** - 🆕 Базовий огляд sensor_drivers

## 🔧 Підтримувані типи
- **DS18B20** - цифрові датчики OneWire
- **NTC** - аналогові термістори
- **Pressure** - датчики тиску 4-20мА
- **GPIO** - дискретні входи

## 🚀 Додавання нового датчика
1. Реалізуйте `ISensorDriver`
2. Автореєстрація через `SensorDriverRegistrar`
3. Конфігурація в `sensors.json`

---
*Документація оновлена: 2025-06-29*
