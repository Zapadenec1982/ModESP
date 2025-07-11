// lazy_component_loader.h
// Lazy loading system for UI components

#pragma once

#include "include/ui_component_base.h"
#include <unordered_map>
#include <memory>
#include <set>
#include <list>
#include <chrono>

namespace ModESP::UI {

/**
 * @brief Component factory registry
 */
class ComponentFactory {
private:
    using FactoryFunc = std::function<std::unique_ptr<UIComponent>()>;
    std::unordered_map<std::string, FactoryFunc> factories;
    
public:
    /**
     * @brief Register component factory
     */
    void registerFactory(const std::string& component_id, FactoryFunc factory) {
        factories[component_id] = factory;
    }
    
    /**
     * @brief Create component instance
     */
    std::unique_ptr<UIComponent> create(const std::string& component_id) {
        auto it = factories.find(component_id);
        if (it != factories.end()) {
            return it->second();
        }
        return nullptr;
    }
    
    /**
     * @brief Check if factory exists
     */
    bool hasFactory(const std::string& component_id) const {
        return factories.find(component_id) != factories.end();
    }
};

/**
 * @brief Lazy component loader with LRU cache
 */
class LazyComponentLoader {
private:
    struct CacheEntry {
        std::unique_ptr<UIComponent> component;
        std::chrono::steady_clock::time_point last_access;
        size_t access_count = 0;
    };
    
    ComponentFactory factory;
    std::unordered_map<std::string, CacheEntry> cache;
    std::list<std::string> lru_list;  // LRU order
    
    // Priority components to preload
    std::set<std::string> priority_components;
    
    // Memory management
    size_t max_cache_size = 10 * 1024;  // 10KB default
    size_t current_cache_size = 0;
    
    // Statistics
    mutable size_t cache_hits = 0;
    mutable size_t cache_misses = 0;
    mutable size_t evictions = 0;
    
    /**
     * @brief Evict least recently used component
     */
    void evictLRU();
    
    /**
     * @brief Update LRU order
     */
    void touchComponent(const std::string& id);
    
public:
    LazyComponentLoader() = default;
    
    /**
     * @brief Set maximum cache size
     */
    void setMaxCacheSize(size_t bytes) { max_cache_size = bytes; }
    
    /**
     * @brief Register component factory
     */
    void registerComponentFactory(const std::string& id, 
                                 std::function<std::unique_ptr<UIComponent>()> factory) {
        this->factory.registerFactory(id, factory);
    }
    
    /**
     * @brief Mark component as priority (will be preloaded)
     */
    void markPriority(const std::string& component_id) {
        priority_components.insert(component_id);
    }
    
    /**
     * @brief Get component (load if necessary)
     */
    UIComponent* getComponent(const std::string& component_id);
    
    /**
     * @brief Preload all priority components
     */
    void preloadPriority();
    
    /**
     * @brief Clear cache
     */
    void clearCache() {
        cache.clear();
        lru_list.clear();
        current_cache_size = 0;
    }
    
    /**
     * @brief Get loader statistics
     */
    struct LoaderStats {
        size_t components_loaded;
        size_t cache_size_bytes;
        size_t cache_hits;
        size_t cache_misses;
        size_t evictions;
        float hit_rate;
    };
    
    LoaderStats getStats() const {
        size_t total = cache_hits + cache_misses;
        return {
            .components_loaded = cache.size(),
            .cache_size_bytes = current_cache_size,
            .cache_hits = cache_hits,
            .cache_misses = cache_misses,
            .evictions = evictions,
            .hit_rate = total > 0 ? (float)cache_hits / total : 0.0f
        };
    }
};

/**
 * @brief Global lazy loader instance
 */
class LazyLoaderManager {
private:
    static LazyComponentLoader* instance;
    
public:
    static LazyComponentLoader& getInstance() {
        if (!instance) {
            instance = new LazyComponentLoader();
        }
        return *instance;
    }
    
    static void cleanup() {
        delete instance;
        instance = nullptr;
    }
};

} // namespace ModESP::UI
