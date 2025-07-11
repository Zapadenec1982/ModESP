# ModESP - Модульна система керування

## Оновлення конфігурації плати

### Нова Custom Board конфігурація:

**GPIO розподіл:**
- Реле 1-4: GPIO 1-4
- Кнопки 1-5: GPIO 9-13 
- OLED дисплей: I2C (SCL: GPIO15, SDA: GPIO16)
- DS18B20 сенсори: GPIO 7-8

### Важливі зміни:

1. **Видалено старий блокуючий драйвер DS18B20**
   - Тепер використовується тільки асинхронний драйвер `DS18B20_Async`
   - Не блокує систему під час конверсії температури

2. **Як вибрати нову плату:**
   ```bash
   idf.py menuconfig
   # Component config → ESPhal Configuration → Board Type → Custom Board
   ```

3. **Приклад конфігурації сенсорів:**
   ```json
   {
     "type": "DS18B20_Async",
     "hal_id": "ONEWIRE_BUS_1",
     "address": "28FF123456789012"
   }
   ```

Детальна документація: [CUSTOM_BOARD_CONFIG.md](CUSTOM_BOARD_CONFIG.md)
