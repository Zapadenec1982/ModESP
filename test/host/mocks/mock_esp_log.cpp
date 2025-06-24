// Mock esp_log implementation
#include "esp_log.h"
#include <stdarg.h>

void esp_log_write(esp_log_level_t level, const char* tag, const char* format, ...) {
    const char* level_str = "UNKNOWN";
    switch(level) {
        case ESP_LOG_ERROR: level_str = "ERROR"; break;
        case ESP_LOG_WARN: level_str = "WARN"; break;
        case ESP_LOG_INFO: level_str = "INFO"; break;
        case ESP_LOG_DEBUG: level_str = "DEBUG"; break;
        case ESP_LOG_VERBOSE: level_str = "VERBOSE"; break;
        default: break;
    }
    
    printf("[%s] %s: ", tag, level_str);
    
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    
    printf("\n");
}