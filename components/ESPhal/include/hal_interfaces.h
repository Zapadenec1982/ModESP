/**
 * @file hal_interfaces.h
 * @brief Hardware Abstraction Layer interfaces for ModuChill devices
 * 
 * This file defines the core interfaces that all HAL drivers must implement.
 * These interfaces provide a unified API for accessing different types of
 * hardware resources (GPIO, OneWire, ADC, etc.).
 */

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "esp_err.h"

/**
 * @brief Загальний результат операції, що може повернути помилку
 */
template <typename T>
struct HalResult {
    T value;
    esp_err_t error = ESP_OK;
    
    bool is_ok() const { return error == ESP_OK; }
    const T& operator*() const { return value; }
    T& operator*() { return value; }
};

/**
 * @brief Інтерфейс для керування цифровим виходом (реле, LED)
 */
class IGpioOutput {
public:
    virtual ~IGpioOutput() = default;
    
    /**
     * @brief Встановити стан виходу
     * @param is_on true для увімкнення, false для вимкнення
     * @return ESP_OK при успіху
     */
    virtual esp_err_t set_state(bool is_on) = 0;
    
    /**
     * @brief Отримати поточний стан виходу
     * @return true якщо увімкнено, false якщо вимкнено
     */
    virtual bool get_state() const = 0;
    
    /**
     * @brief Перемкнути стан виходу
     * @return ESP_OK при успіху
     */
    virtual esp_err_t toggle() = 0;
};

/**
 * @brief Інтерфейс для читання цифрового входу (кнопка, геркон)
 */
class IGpioInput {
public:
    virtual ~IGpioInput() = default;
    
    /**
     * @brief Прочитати стан входу
     * @return true якщо активний, false якщо неактивний
     */
    virtual bool get_state() = 0;
    
    // В майбутньому можна додати set_interrupt(), set_debounce() і т.д.
};

/**
 * @brief Інтерфейс для шини 1-Wire
 */
class IOneWireBus {
public:
    virtual ~IOneWireBus() = default;
    
    /**
     * @brief Шукає пристрої на шині і повертає їхні адреси
     * @return Вектор 64-бітних адрес знайдених пристроїв
     */
    virtual std::vector<uint64_t> search_devices() = 0;
    
    /**
     * @brief Відправляє команду на конвертацію температури всім пристроям
     * @return ESP_OK при успіху
     */
    virtual esp_err_t request_temperatures() = 0;
    
    /**
     * @brief Запускає конвертацію температури для конкретного пристрою
     * @param address 64-бітна адреса пристрою
     * @return ESP_OK при успіху
     */
    virtual esp_err_t start_temperature_conversion(uint64_t address) = 0;
    
    /**
     * @brief Читає температуру з конкретного пристрою
     * @param address 64-бітна адреса пристрою
     * @return Результат з температурою в градусах Цельсія
     */
    virtual HalResult<float> read_temperature(uint64_t address) = 0;
};

/**
 * @brief Інтерфейс для каналу АЦП
 */
class IAdcChannel {
public:
    virtual ~IAdcChannel() = default;
    
    /**
     * @brief Повертає сире значення з АЦП
     * @return Результат з сирим значенням АЦП
     */
    virtual HalResult<int> read_raw() = 0;
    
    /**
     * @brief Повертає напругу в мілівольтах
     * @return Результат з напругою в мВ
     */
    virtual HalResult<int> read_voltage_mv() = 0;
};
