#pragma once

#include <vector>
#include <mutex>

namespace ModESP {

/**
 * @brief Потокобезпечний кільцевий буфер
 */
template<typename T>
class RingBuffer {
public:
    explicit RingBuffer(size_t capacity) 
        : m_buffer(capacity), m_capacity(capacity), m_head(0), m_tail(0), m_size(0) {
    }
    
    bool push(const T& item) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (m_size == m_capacity) {
            // Буфер повний, перезаписуємо найстаріший елемент
            m_tail = (m_tail + 1) % m_capacity;
        } else {
            m_size++;
        }
        
        m_buffer[m_head] = item;
        m_head = (m_head + 1) % m_capacity;
        
        return true;
    }
    
    bool pop(T& item) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (m_size == 0) {
            return false;
        }
        
        item = m_buffer[m_tail];
        m_tail = (m_tail + 1) % m_capacity;
        m_size--;
        
        return true;
    }    
    size_t size() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_size;
    }
    
    bool empty() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_size == 0;
    }
    
    bool full() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_size == m_capacity;
    }
    
    void clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_head = 0;
        m_tail = 0;
        m_size = 0;
    }
    
    std::vector<T> getAll() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<T> result;
        result.reserve(m_size);
        
        size_t idx = m_tail;
        for (size_t i = 0; i < m_size; i++) {
            result.push_back(m_buffer[idx]);
            idx = (idx + 1) % m_capacity;
        }
        
        return result;
    }
    
private:
    mutable std::mutex m_mutex;
    std::vector<T> m_buffer;
    size_t m_capacity;
    size_t m_head;
    size_t m_tail;
    size_t m_size;
};

} // namespace ModESP
