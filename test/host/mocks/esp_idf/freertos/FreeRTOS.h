// Mock FreeRTOS.h for host testing
#pragma once

#include <stdint.h>
#include <stdbool.h>

// Basic FreeRTOS types
typedef long BaseType_t;
typedef unsigned long UBaseType_t;
typedef void* QueueHandle_t;
typedef void* SemaphoreHandle_t;
typedef uint32_t TickType_t;

#define pdTRUE  1
#define pdFALSE 0
#define pdPASS  pdTRUE
#define pdFAIL  pdFALSE

#define portMAX_DELAY  0xFFFFFFFF
#define configTICK_RATE_HZ  1000

// Mock queue functions
QueueHandle_t xQueueCreate(UBaseType_t uxQueueLength, UBaseType_t uxItemSize);
BaseType_t xQueueSend(QueueHandle_t xQueue, const void* pvItemToQueue, TickType_t xTicksToWait);
BaseType_t xQueueSendFromISR(QueueHandle_t xQueue, const void* pvItemToQueue, BaseType_t* pxHigherPriorityTaskWoken);
BaseType_t xQueueReceive(QueueHandle_t xQueue, void* pvBuffer, TickType_t xTicksToWait);
UBaseType_t uxQueueMessagesWaiting(const QueueHandle_t xQueue);
UBaseType_t uxQueueSpacesAvailable(const QueueHandle_t xQueue);
void vQueueDelete(QueueHandle_t xQueue);