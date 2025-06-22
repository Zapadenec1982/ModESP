# Поточні завдання для продовження розробки ModuChill

## Найближчі завдання (Тиждень 1-2)

### 🔥 Критичний пріоритет

#### 1. Валідація основних модулів
- [ ] **Тестування Core системи**
  - Перевірити EventBus під навантаженням
  - Валідувати SharedState thread safety
  - Тестувати Config збереження/відновлення
  - Замірити споживання пам'яті

- [ ] **Перевірка HAL драйверів**
  - Тестувати DS18B20 з множинними датчиками
  - Валідувати NTC калібрування
  - Перевірити Relay захисні таймери
  - Тестувати PWM soft start/stop

#### 2. Інтеграційне тестування
- [ ] **Створити тестову конфігурацію**
  - Налаштувати sensors.json з 3-5 датчиками
  - Конфігурувати actuators.json з реле та PWM
  - Протестувати повний цикл sensor→processing→actuator

- [ ] **Перевірити стабільність**
  - 24-годинний stress test
  - Моніторинг memory leaks
  - Валідація error recovery

### ⚡ Високий пріоритет

#### 3. Завершення UI системи
- [ ] **Веб-інтерфейс**
  - Створити базову HTML сторінку з real-time даними
  - Реалізувати WebSocket для оновлень
  - Додати форми для зміни конфігурації
  - Стилізувати адаптивний дизайн

- [ ] **API endpoints**
  - GET /api/sensors - список датчиків та їх значень
  - GET/POST /api/config - конфігурація системи
  - POST /api/actuators/{id}/command - команди актуаторам
  - GET /api/status - статус системи

#### 4. Оптимізація та документація
- [ ] **Оновлення документації**
  - Перевірити актуальність всіх .txt файлів
  - Додати приклади конфігурацій
  - Створити troubleshooting guide
  - Оновити API референси

## Завдання тижня 3-4

### 🛠️ Розширення функціональності

#### 5. Бізнес-логіка модулі
- [ ] **ClimateControl базова версія**
  - Simple on/off контроль температури
  - Hysteresis налаштування
  - Safety limits (min/max температури)
  - Basic logging подій

- [ ] **Alarm система**
  - Temperature out of range
  - Sensor disconnected/failed
  - Actuator malfunction
  - System errors (memory, tasks)

#### 6. Мережеві функції
- [ ] **WiFi Manager стабілізація**
  - Auto-reconnect при втраті зв'язку
  - Fallback до AP mode
  - WiFi credentials зберігання
  - Signal strength моніторинг

- [ ] **OTA базова версія**
  - HTTP OTA update
  - Version checking
  - Rollback на попередню версію
  - Progress reporting