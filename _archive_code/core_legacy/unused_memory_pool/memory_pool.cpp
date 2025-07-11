/**
 * @file memory_pool.cpp
 * @brief Memory pool system implementation
 */

#include "memory_pool.h"
#include "esp_log.h"

namespace ModESP {
namespace Memory {

static const char* TAG = "MemoryPool";

// Explicit template instantiations
template class StaticMemoryPool<32, 128>;
template class StaticMemoryPool<64, 64>;
template class StaticMemoryPool<128, 32>;
template class StaticMemoryPool<256, 16>;
template class StaticMemoryPool<512, 8>;

} // namespace Memory
} // namespace ModESP
