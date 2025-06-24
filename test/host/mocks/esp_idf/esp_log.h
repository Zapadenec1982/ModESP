// Mock esp_log.h for host testing
#pragma once

#include <stdio.h>
#include <stdarg.h>

typedef enum {
    ESP_LOG_NONE,
    ESP_LOG_ERROR,
    ESP_LOG_WARN,
    ESP_LOG_INFO,
    ESP_LOG_DEBUG,
    ESP_LOG_VERBOSE
} esp_log_level_t;

#define ESP_LOGE(tag, format, ...) printf("[%s] ERROR: " format "\n", tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, format, ...) printf("[%s] WARN: " format "\n", tag, ##__VA_ARGS__)
#define ESP_LOGI(tag, format, ...) printf("[%s] INFO: " format "\n", tag, ##__VA_ARGS__)
#define ESP_LOGD(tag, format, ...) printf("[%s] DEBUG: " format "\n", tag, ##__VA_ARGS__)
#define ESP_LOGV(tag, format, ...) printf("[%s] VERBOSE: " format "\n", tag, ##__VA_ARGS__)

// Mock implementation
void esp_log_write(esp_log_level_t level, const char* tag, const char* format, ...);