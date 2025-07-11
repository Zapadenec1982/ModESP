// lazy_component_loader.cpp
// Implementation of lazy loading system

#include "lazy_component_loader.h"
#include "esp_log.h"
#include <algorithm>

static const char* TAG = "LazyLoader";

namespace ModESP::UI {

// Static instance
LazyComponentLoader* LazyLoaderManager::instance = nullptr;

// LazyComponentLoader implementation
UIComponent* LazyComponentLoader::getComponent(const std::string& component_id) {
    // Check cache first
    auto cache_it = cache.find(component_id);
    if (cache_it != cache.end()) {
        // Update access time and count
        cache_it->second.last_access = std::chrono::steady_clock::now();
        cache_it->second.access_count++;
        
        // Update LRU order
        touchComponent(component_id);
        
        cache_hits++;
        ESP_LOGD(TAG, "Cache hit for component: %s", component_id.c_str());
        return cache_it->second.component.get();
    }
    
    // Cache miss - need to create
    cache_misses++;
    ESP_LOGD(TAG, "Cache miss for component: %s", component_id.c_str());
    
    // Check if factory exists
    if (!factory.hasFactory(component_id)) {
        ESP_LOGE(TAG, "No factory registered for component: %s", component_id.c_str());
        return nullptr;
    }
    
    // Create component
    auto component = factory.create(component_id);
    if (!component) {
        ESP_LOGE(TAG, "Failed to create component: %s", component_id.c_str());
        return nullptr;
    }
    
    // Check memory constraints
    size_t comp_size = component->getEstimatedSize();
    while (current_cache_size + comp_size > max_cache_size && !cache.empty()) {
        evictLRU();
    }
    
    // Add to cache
    CacheEntry entry;
    entry.component = std::move(component);
    entry.last_access = std::chrono::steady_clock::now();
    entry.access_count = 1;
    
    UIComponent* ptr = entry.component.get();
    cache[component_id] = std::move(entry);
    
    // Update LRU list
    lru_list.push_front(component_id);
    
    // Update cache size
    current_cache_size += comp_size;
    
    ESP_LOGI(TAG, "Loaded component: %s (size: %zu, total cache: %zu)", 
             component_id.c_str(), comp_size, current_cache_size);
    
    return ptr;
}

void LazyComponentLoader::evictLRU() {
    if (lru_list.empty()) return;
    
    // Get least recently used component
    std::string lru_id = lru_list.back();
    lru_list.pop_back();
    
    // Find in cache
    auto it = cache.find(lru_id);
    if (it != cache.end()) {
        size_t comp_size = it->second.component->getEstimatedSize();
        
        ESP_LOGI(TAG, "Evicting component: %s (size: %zu)", lru_id.c_str(), comp_size);
        
        // Remove from cache
        cache.erase(it);
        
        // Update cache size
        current_cache_size -= comp_size;
        evictions++;
    }
}

void LazyComponentLoader::touchComponent(const std::string& id) {
    // Remove from current position
    auto it = std::find(lru_list.begin(), lru_list.end(), id);
    if (it != lru_list.end()) {
        lru_list.erase(it);
    }
    
    // Add to front (most recently used)
    lru_list.push_front(id);
}

void LazyComponentLoader::preloadPriority() {
    ESP_LOGI(TAG, "Preloading %zu priority components...", priority_components.size());
    
    for (const auto& id : priority_components) {
        getComponent(id);
    }
    
    ESP_LOGI(TAG, "Preload complete. Cache size: %zu bytes", current_cache_size);
}

} // namespace ModESP::UI
