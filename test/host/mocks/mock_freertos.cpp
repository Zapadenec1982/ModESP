// Mock FreeRTOS implementation for host testing
#include "freertos/FreeRTOS.h"
#include <queue>
#include <mutex>
#include <map>
#include <cstring>

// Simple mock implementation using std::queue
struct MockQueue {
    std::queue<std::vector<uint8_t>> data;
    std::mutex mtx;
    size_t item_size;
    size_t max_items;
};

static std::map<QueueHandle_t, MockQueue*> queues;
static std::mutex queues_mutex;

QueueHandle_t xQueueCreate(UBaseType_t uxQueueLength, UBaseType_t uxItemSize) {
    auto* q = new MockQueue();
    q->item_size = uxItemSize;
    q->max_items = uxQueueLength;
    
    std::lock_guard<std::mutex> lock(queues_mutex);
    queues[(QueueHandle_t)q] = q;
    return (QueueHandle_t)q;
}

BaseType_t xQueueSend(QueueHandle_t xQueue, const void* pvItemToQueue, TickType_t xTicksToWait) {
    std::lock_guard<std::mutex> lock(queues_mutex);    auto it = queues.find(xQueue);
    if (it == queues.end()) return pdFAIL;
    
    MockQueue* q = it->second;
    std::lock_guard<std::mutex> qlock(q->mtx);
    
    if (q->data.size() >= q->max_items) {
        return pdFAIL;
    }
    
    std::vector<uint8_t> item(q->item_size);
    std::memcpy(item.data(), pvItemToQueue, q->item_size);
    q->data.push(item);
    
    return pdPASS;
}

BaseType_t xQueueSendFromISR(QueueHandle_t xQueue, const void* pvItemToQueue, 
                              BaseType_t* pxHigherPriorityTaskWoken) {
    if (pxHigherPriorityTaskWoken) *pxHigherPriorityTaskWoken = pdFALSE;
    return xQueueSend(xQueue, pvItemToQueue, 0);
}

BaseType_t xQueueReceive(QueueHandle_t xQueue, void* pvBuffer, TickType_t xTicksToWait) {
    std::lock_guard<std::mutex> lock(queues_mutex);
    auto it = queues.find(xQueue);
    if (it == queues.end()) return pdFAIL;
    
    MockQueue* q = it->second;    std::lock_guard<std::mutex> qlock(q->mtx);
    
    if (q->data.empty()) {
        return pdFAIL;
    }
    
    auto& item = q->data.front();
    std::memcpy(pvBuffer, item.data(), q->item_size);
    q->data.pop();
    
    return pdPASS;
}

UBaseType_t uxQueueMessagesWaiting(const QueueHandle_t xQueue) {
    std::lock_guard<std::mutex> lock(queues_mutex);
    auto it = queues.find(xQueue);
    if (it == queues.end()) return 0;
    
    MockQueue* q = it->second;
    std::lock_guard<std::mutex> qlock(q->mtx);
    return q->data.size();
}

UBaseType_t uxQueueSpacesAvailable(const QueueHandle_t xQueue) {
    std::lock_guard<std::mutex> lock(queues_mutex);
    auto it = queues.find(xQueue);
    if (it == queues.end()) return 0;
    
    MockQueue* q = it->second;
    std::lock_guard<std::mutex> qlock(q->mtx);
    return q->max_items - q->data.size();
}
void vQueueDelete(QueueHandle_t xQueue) {
    std::lock_guard<std::mutex> lock(queues_mutex);
    auto it = queues.find(xQueue);
    if (it != queues.end()) {
        delete it->second;
        queues.erase(it);
    }
}