# RTC - Real Time Clock модуль

Система часу, календаря та розкладу.

## 📚 Документи
- **[rtc_module.md](rtc_module.md)** - Основна документація RTC
- **[implementation_summary.md](implementation_summary.md)** - Підсумок реалізації
- **[integration_examples.md](integration_examples.md)** - Приклади інтеграції

## 🕐 Функціональність
- **Hardware RTC** - зберігання часу при відключенні живлення
- **NTP синхронізація** - автоматичне оновлення через WiFi
- **Timezone підтримка** - часові пояси та DST
- **Alarm система** - планування подій

## 🚀 Використання
```cpp
// Отримання поточного часу
auto now = RTCModule::get_current_time();

// Встановлення alarm
RTCModule::set_alarm(target_time, callback);
```

---
*Документація оновлена: 2025-06-29*
