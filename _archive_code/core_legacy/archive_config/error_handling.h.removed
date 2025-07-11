#pragma once

#include <esp_err.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <string>
#include <functional>
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace ModESP {

// Розширені коди помилок для модульної системи
#define MOD_ERR_BASE           0x10000
#define MOD_ERR_INVALID_STATE  (MOD_ERR_BASE + 1)
#define MOD_ERR_TIMEOUT        (MOD_ERR_BASE + 2)
#define MOD_ERR_HANDLER_FAIL   (MOD_ERR_BASE + 3)
#define MOD_ERR_ALLOC_FAIL     (MOD_ERR_BASE + 4)

// Результат з додатковою інформацією
template<typename T>
class Result {
private:
    bool m_valid;
    esp_err_t m_error;
    T m_value;
    std::string m_error_msg;

public:
    Result(T value) : m_valid(true), m_error(ESP_OK), m_value(std::move(value)) {}
    Result(esp_err_t err, const std::string& msg = "") 
        : m_valid(false), m_error(err), m_error_msg(msg) {}

    bool is_ok() const { return m_valid; }
    bool is_err() const { return !m_valid; }
    
    T& value() { return m_value; }
    const T& value() const { return m_value; }
    
    esp_err_t error() const { return m_error; }
    const std::string& error_msg() const { return m_error_msg; }
    
    // Зручні оператори
    operator bool() const { return m_valid; }
    T& operator*() { return m_value; }
    const T& operator*() const { return m_value; }
    T* operator->() { return &m_value; }
    const T* operator->() const { return &m_value; }
};

// Спеціалізація для void
template<>
class Result<void> {
private:
    esp_err_t m_error;
    std::string m_error_msg;

public:
    Result() : m_error(ESP_OK) {}
    Result(esp_err_t err, const std::string& msg = "") 
        : m_error(err), m_error_msg(msg) {}

    bool is_ok() const { return m_error == ESP_OK; }
    bool is_err() const { return m_error != ESP_OK; }
    
    esp_err_t error() const { return m_error; }
    const std::string& error_msg() const { return m_error_msg; }
    
    operator bool() const { return is_ok(); }
};

// Макроси для зручної обробки помилок
#define MOD_CHECK(x) do {                                    \
        esp_err_t err_rc_ = (x);                            \
        if (err_rc_ != ESP_OK) {                            \
            ESP_LOGE(TAG, "Check failed: %s", #x);          \
            return err_rc_;                                 \
        }                                                   \
    } while(0)

#define MOD_CHECK_RETURN(x, ret) do {                       \
        esp_err_t err_rc_ = (x);                            \
        if (err_rc_ != ESP_OK) {                            \
            ESP_LOGE(TAG, "Check failed: %s", #x);          \
            return ret;                                     \
        }                                                   \
    } while(0)

// Безпечне виконання з обробкою помилок
template<typename Func>
esp_err_t safe_execute(const char* context, Func&& func) {
    static const char* TAG = "SafeExecute";
    
    // Перевірка стеку
    UBaseType_t stack_free = uxTaskGetStackHighWaterMark(NULL);
    if (stack_free < 512) {
        ESP_LOGW(TAG, "Low stack in %s: %d bytes", context, stack_free);
    }
    
    // Виконання функції
    esp_err_t result = func();
    
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Error in %s: %s (0x%x)", 
                 context, esp_err_to_name(result), result);
    }
    
    return result;
}

// Клас для накопичення помилок
class ErrorCollector {
private:
    static const size_t MAX_ERRORS = 10;
    struct ErrorInfo {
        esp_err_t code;
        std::string context;
        int64_t timestamp;
    };
    
    std::vector<ErrorInfo> m_errors;
    size_t m_total_count = 0;

public:
    void add(esp_err_t err, const std::string& context) {
        if (err == ESP_OK) return;
        
        if (m_errors.size() >= MAX_ERRORS) {
            m_errors.erase(m_errors.begin());
        }
        
        m_errors.push_back({err, context, esp_timer_get_time()});
        m_total_count++;
    }
    
    bool has_errors() const { return !m_errors.empty(); }
    size_t error_count() const { return m_total_count; }
    
    void clear() {
        m_errors.clear();
        m_total_count = 0;
    }
    
    void log_all(const char* TAG) const {
        for (const auto& err : m_errors) {
            ESP_LOGE(TAG, "[%lld] %s: %s (0x%x)", 
                     err.timestamp, err.context.c_str(), 
                     esp_err_to_name(err.code), err.code);
        }
    }
};

} // namespace ModESP
