# ModESP Testing Framework

## Overview
ModESP uses Unity Test Framework for unit testing directly on ESP32-S3 hardware. This approach ensures that tests run in the actual target environment with all hardware-specific features available.

## Test Structure

### Directory Layout
```
test/
├── target/              # Test component for ESP32
│   ├── CMakeLists.txt   # Component configuration
│   ├── test_runner.cpp  # Main test runner
│   ├── test_event_bus.cpp
│   ├── test_config_manager.cpp
│   ├── test_module_manager.cpp
│   ├── test_shared_state.cpp
│   └── test_json_validator.cpp
├── build_and_run.sh     # Linux/Mac build script
└── build_and_run.bat    # Windows build script
```

## Running Tests

### Quick Start

1. **Enable test mode** in `main/main.c`:
   ```c
   #define RUN_TESTS 1  // Set to 1 for tests, 0 for normal operation
   ```

2. **Build and flash** (Windows):
   ```bash
   cd test
   build_and_run.bat
   ```

   Or manually:
   ```bash
   idf.py set-target esp32s3
   idf.py build
   idf.py -p COM3 flash monitor
   ```

3. **View results** in serial monitor

### Test Output Example
```
I (1234) Main: ModuChill starting...
I (1236) Main: Running Unity tests...
I (3236) TestRunner: Starting EventBus tests...
[==========] Running 12 tests
[ RUN      ] test_eventbus_init
[ OK       ] test_eventbus_init
[ RUN      ] test_eventbus_publish_subscribe
[ OK       ] test_eventbus_publish_subscribe
...
[==========] 12 tests ran
[ PASSED   ] 12 tests
```

## Writing Tests

### Test File Template
```cpp
/**
 * @file test_component.cpp
 * @brief Unit tests for Component
 */

#include "unity.h"
#include "component.h"
#include "esp_log.h"

static const char* TAG = "TestComponent";

// Test fixtures
static void reset_test_state() {
    // Reset component state
}

// Test cases
void test_component_feature() {
    // Arrange
    reset_test_state();
    
    // Act
    auto result = Component::do_something();
    
    // Assert
    TEST_ASSERT_EQUAL(expected, result);
}

// Test group runner
void test_component_group(void) {
    RUN_TEST(test_component_feature);
}
```

### Adding New Tests

1. Create test file in `test/target/`
2. Add to `CMakeLists.txt` SRCS list
3. Declare test group in `test_runner.cpp`
4. Add test group call to `run_all_tests()`

## Test Categories

### Core System Tests
- **EventBus**: Pub/sub, priorities, filtering, concurrent access
- **ConfigManager**: Get/set, persistence, callbacks, validation
- **ModuleManager**: Registration, lifecycle, enable/disable
- **SharedState**: State sharing, subscriptions, thread safety
- **JsonValidator**: Schema validation, type checking

### Module Tests
- Individual module functionality
- Module interactions
- Error handling
- Performance benchmarks

## Best Practices

### Test Design
1. **Isolation**: Each test should be independent
2. **Repeatability**: Tests must produce consistent results
3. **Speed**: Keep individual tests under 100ms
4. **Coverage**: Test both success and failure paths

### Hardware Considerations
1. **Memory**: Monitor heap usage during tests
2. **Timing**: Use appropriate delays for async operations
3. **Resources**: Clean up after each test (close handles, free memory)
4. **Interrupts**: Test ISR-safe functions separately

### Assertions
Unity provides various assertion macros:
- `TEST_ASSERT_TRUE/FALSE(condition)`
- `TEST_ASSERT_EQUAL(expected, actual)`
- `TEST_ASSERT_EQUAL_STRING(expected, actual)`
- `TEST_ASSERT_NULL/NOT_NULL(pointer)`
- `TEST_ASSERT_EQUAL_MEMORY(expected, actual, len)`

## Continuous Integration

### Pre-commit Testing
Run core tests before committing:
```bash
cd test
build_and_run.bat
```

### Full Test Suite
For release validation, run with extended tests:
```c
#define RUN_EXTENDED_TESTS 1  // In test_runner.cpp
```

## Troubleshooting

### Common Issues

1. **Build Errors**
   - Ensure all test files are added to CMakeLists.txt
   - Check component dependencies (REQUIRES)
   - Verify Unity is included in components

2. **Runtime Failures**
   - Check stack sizes for test tasks
   - Monitor free heap during tests
   - Add delays for async operations

3. **Flashing Issues**
   - Verify correct COM port
   - Check USB drivers
   - Try lower baud rate: `idf.py -p COM3 -b 115200 flash`

## Performance Testing

### Measuring Execution Time
```cpp
uint64_t start = esp_timer_get_time();
// Operation to measure
uint64_t duration = esp_timer_get_time() - start;
ESP_LOGI(TAG, "Operation took %lld us", duration);
```

### Memory Profiling
```cpp
size_t heap_before = esp_get_free_heap_size();
// Operation
size_t heap_after = esp_get_free_heap_size();
ESP_LOGI(TAG, "Heap usage: %d bytes", heap_before - heap_after);
```

## Future Enhancements

1. **Mock Framework**: Add mocking support for hardware interfaces
2. **Coverage Analysis**: Integrate code coverage tools
3. **Automated Testing**: GitHub Actions for PR validation
4. **Performance Regression**: Track performance metrics over time
5. **Integration Tests**: Test complete scenarios end-to-end
