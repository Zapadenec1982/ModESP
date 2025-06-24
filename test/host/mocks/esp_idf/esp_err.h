// Mock esp_err.h for host testing
#pragma once

#include <stdint.h>

typedef int32_t esp_err_t;

#define ESP_OK          0
#define ESP_FAIL        -1
#define ESP_ERR_NO_MEM  0x101
#define ESP_ERR_INVALID_ARG  0x102
#define ESP_ERR_INVALID_STATE  0x103
#define ESP_ERR_INVALID_SIZE  0x104
#define ESP_ERR_NOT_FOUND  0x105
#define ESP_ERR_NOT_SUPPORTED  0x106
#define ESP_ERR_TIMEOUT  0x107

#define ESP_ERROR_CHECK(x) do {                                         \
        esp_err_t __err_rc = (x);                                       \
        if (__err_rc != ESP_OK) {                                       \
            abort();                                                    \
        }                                                               \
    } while(0)