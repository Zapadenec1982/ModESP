/**
 * @file onewire_impl.cpp
 * @brief Implementation of OneWire bus for DS18B20 sensors
 */

#include "onewire_impl.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/portmacro.h>
#include <rom/ets_sys.h>
#include <driver/gpio.h>
#include <esp_timer.h>

static const char* TAG = "OneWireBus";

// DS18B20 specific commands
#define STARTCONVO      0x44
#define COPYSCRATCH     0x48
#define READSCRATCH     0xBE
#define WRITESCRATCH    0x4E
#define RECALLSCRATCH   0xB8
#define READPOWERSUPPLY 0xB4
#define SEARCHROM       0xF0
#define READROM         0x33
#define MATCHROM        0x55
#define SKIPROM         0xCC
#define ALARMSEARCH     0xEC

OneWireBusImpl::OneWireBusImpl(gpio_num_t data_pin, gpio_num_t power_pin) 
    : data_pin_(data_pin), power_pin_(power_pin) {
    
    ESP_LOGI(TAG, "Creating OneWire bus on pin %d", data_pin_);
    
    // Configure data pin - use INPUT_OUTPUT_OD for proper OneWire operation
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT_OUTPUT_OD;  // Open-drain input/output mode
    io_conf.pin_bit_mask = (1ULL << data_pin_);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;  // Enable internal pull-up
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO %d: %s", data_pin_, esp_err_to_name(ret));
    }
    
    // Configure power pin if specified
    if (power_pin_ != GPIO_NUM_NC) {
        gpio_config_t power_conf = {};
        power_conf.intr_type = GPIO_INTR_DISABLE;
        power_conf.mode = GPIO_MODE_OUTPUT;
        power_conf.pin_bit_mask = (1ULL << power_pin_);
        power_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        power_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        gpio_config(&power_conf);
        
        // Turn on power
        gpio_set_level(power_pin_, 1);
        vTaskDelay(pdMS_TO_TICKS(10)); // Wait for power to stabilize
    }
    
    // Set bus high initially and wait for stabilization
    gpio_set_level(data_pin_, 1);
    vTaskDelay(pdMS_TO_TICKS(100)); // Wait longer for line to stabilize
    
    ESP_LOGI(TAG, "OneWire bus initialized on pin %d", data_pin_);
}

OneWireBusImpl::~OneWireBusImpl() {
    if (power_pin_ != GPIO_NUM_NC) {
        gpio_set_level(power_pin_, 0);  // Turn off power
    }
}
std::vector<uint64_t> OneWireBusImpl::search_devices() {
    ESP_LOGI(TAG, "Searching for devices on OneWire bus (pin %d)", data_pin_);
    
    // Add debugging information
    ESP_LOGI(TAG, "Testing OneWire bus presence...");
    if (!reset()) {
        ESP_LOGW(TAG, "No presence pulse detected - check connections and pull-up resistor");
        return {};
    }
    ESP_LOGI(TAG, "Presence pulse detected - bus is working");
    
    std::vector<uint64_t> devices;
    
    // First try a simple communication test with SKIP ROM
    ESP_LOGI(TAG, "Testing basic communication with SKIP ROM + READ SCRATCHPAD...");
    if (reset()) {
        write_byte(SKIPROM);  // Skip ROM command (0xCC)
        write_byte(READSCRATCH);  // Read scratchpad (0xBE)
        
        ESP_LOGI(TAG, "Commands sent, reading 9 bytes of scratchpad data:");
        uint8_t scratchpad[9];
        for (int i = 0; i < 9; i++) {
            scratchpad[i] = read_byte();
            ESP_LOGI(TAG, "  Byte %d: 0x%02X", i, scratchpad[i]);
        }
        
        // If we got meaningful data (not all 0xFF), there's probably a working device
        bool has_data = false;
        for (int i = 0; i < 9; i++) {
            if (scratchpad[i] != 0xFF && scratchpad[i] != 0x00) {
                has_data = true;
                break;
            }
        }
        
        if (has_data) {
            ESP_LOGI(TAG, "Scratchpad contains meaningful data - device present but unknown address");
            ESP_LOGI(TAG, "Will proceed with device discovery methods...");
        } else {
            ESP_LOGI(TAG, "Scratchpad data is all 0xFF or 0x00 - communication may not be working");
        }
    }
    
    // First try READ ROM command (0x33) for single device detection
    ESP_LOGI(TAG, "Trying READ ROM command for single device detection...");
    if (reset()) {
        write_byte(READROM);  // Read ROM command (0x33)
        uint8_t single_address[8];
        
        // Read 8 bytes of ROM code
        for (int i = 0; i < 8; i++) {
            single_address[i] = read_byte();
        }
        
        ESP_LOGI(TAG, "READ ROM result: %02X %02X %02X %02X %02X %02X %02X %02X",
                single_address[0], single_address[1], single_address[2], single_address[3],
                single_address[4], single_address[5], single_address[6], single_address[7]);
        
        // Check if we got a valid family code (first byte should be 0x28 for DS18B20)
        if (single_address[0] == 0x28) {
            uint64_t device_addr = 0;
            for (int i = 7; i >= 0; i--) {
                device_addr = (device_addr << 8) | single_address[i];
            }
            devices.push_back(device_addr);
            ESP_LOGI(TAG, "Found single device via READ ROM: %02X%02X%02X%02X%02X%02X%02X%02X", 
                     single_address[7], single_address[6], single_address[5], single_address[4], 
                     single_address[3], single_address[2], single_address[1], single_address[0]);
        } else {
            ESP_LOGI(TAG, "READ ROM failed or multiple devices present (family code: 0x%02X)", single_address[0]);
        }
    }
    
    // If READ ROM didn't work, try SEARCH ROM algorithm
    if (devices.empty()) {
        ESP_LOGI(TAG, "Trying SEARCH ROM algorithm...");
        uint8_t address[8];
        last_discrepancy_ = 0;
        last_device_flag_ = false;
        last_family_discrepancy_ = 0;
        
        // Search for devices with detailed logging
        int search_attempts = 0;
        while (search_device(address) && search_attempts < 10) {
            search_attempts++;
            uint64_t device_addr = 0;
            for (int i = 7; i >= 0; i--) {
                device_addr = (device_addr << 8) | address[i];
            }
            devices.push_back(device_addr);
            ESP_LOGI(TAG, "Found device #%d: %02X%02X%02X%02X%02X%02X%02X%02X", 
                     search_attempts,
                     address[7], address[6], address[5], address[4], 
                     address[3], address[2], address[1], address[0]);
        }
        
        if (search_attempts == 0) {
            ESP_LOGI(TAG, "SEARCH ROM algorithm found no devices");
        }
    }
    
    ESP_LOGI(TAG, "Found %zu device(s) on OneWire bus", devices.size());
    
    // Add enhanced debugging
    if (devices.empty()) {
        ESP_LOGW(TAG, "No devices found. Troubleshooting tips:");
        ESP_LOGW(TAG, "1. Check DS18B20 wiring: VDD(red)->3.3V, GND(black)->GND, Data(yellow)->GPIO%d", data_pin_);
        ESP_LOGW(TAG, "2. Ensure 4.7kΩ pull-up resistor between Data and VDD");
        ESP_LOGW(TAG, "3. Check sensor power - try parasitic vs. external power");
        ESP_LOGW(TAG, "4. Test with shorter cables (< 50cm for initial testing)");
        
        // Perform additional diagnostics
        debug_line_states();
    }
    
    return devices;
}

esp_err_t OneWireBusImpl::request_temperatures() {
    ESP_LOGV(TAG, "Requesting temperature conversion");
    
    if (!reset()) {
        ESP_LOGW(TAG, "No devices found during reset");
        return ESP_ERR_NOT_FOUND;
    }
    
    write_byte(SKIPROM);        // Skip ROM command (address all devices)
    write_byte(STARTCONVO);     // Start temperature conversion
    
    return ESP_OK;
}

esp_err_t OneWireBusImpl::start_temperature_conversion(uint64_t address) {
    uint8_t addr_bytes[8];
    
    // Convert uint64_t address to byte array
    for (int i = 0; i < 8; i++) {
        addr_bytes[i] = (address >> (i * 8)) & 0xFF;
    }
    
    ESP_LOGV(TAG, "Starting temperature conversion for device: %016llX", address);
    
    if (!reset()) {
        ESP_LOGW(TAG, "No devices found during reset for conversion");
        return ESP_ERR_NOT_FOUND;
    }
    
    write_byte(MATCHROM);       // Match ROM command
    for (int i = 0; i < 8; i++) {
        write_byte(addr_bytes[i]);
    }
    
    write_byte(STARTCONVO);     // Start temperature conversion
    
    return ESP_OK;
}

HalResult<float> OneWireBusImpl::read_temperature(uint64_t address) {
    HalResult<float> result;
    uint8_t addr_bytes[8];
    
    // Convert uint64_t address to byte array
    for (int i = 0; i < 8; i++) {
        addr_bytes[i] = (address >> (i * 8)) & 0xFF;
    }
    
    ESP_LOGV(TAG, "Reading temperature from device: %016llX", address);
    
    // Read the scratchpad directly (assuming conversion already happened)
    if (!reset()) {
        ESP_LOGW(TAG, "No devices found during reset for reading");
        result.error = ESP_ERR_NOT_FOUND;
        result.value = 0.0f;
        return result;
    }
    
    write_byte(MATCHROM);       // Match ROM command
    for (int i = 0; i < 8; i++) {
        write_byte(addr_bytes[i]);
    }
    
    write_byte(READSCRATCH);    // Read scratchpad
    
    // Read 9 bytes of data
    uint8_t data[9];
    for (int i = 0; i < 9; i++) {
        data[i] = read_byte();
    }
    
    // Verify CRC - single attempt only for performance
    if (!check_crc(data, 8, data[8])) {
        ESP_LOGW(TAG, "CRC check failed for sensor %016llX", address);
        ESP_LOGW(TAG, "Scratchpad: %02X %02X %02X %02X %02X %02X %02X %02X (CRC: %02X)", 
                 data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7], data[8]);
        
        // Check for "frozen" scratchpad pattern that indicates sensor issues
        if (data[0] == 0x7E && data[1] == 0x01 && data[2] == 0x55 && data[3] == 0x05) {
            ESP_LOGW(TAG, "Detected potentially frozen scratchpad pattern - sensor may need reset");
            
            // Manual CRC calculation for debugging
            uint8_t manual_crc = 0;
            for (uint8_t i = 0; i < 8; i++) {
                uint8_t byte_data = data[i];
                for (uint8_t bit = 0; bit < 8; bit++) {
                    uint8_t mix = (manual_crc ^ byte_data) & 0x01;
                    manual_crc >>= 1;
                    if (mix) manual_crc ^= 0x8C;
                    byte_data >>= 1;
                }
            }
            ESP_LOGW(TAG, "Manual CRC calculation: received=0x%02X, calculated=0x%02X", data[8], manual_crc);
            
            // Try a hard reset sequence for this specific sensor
            ESP_LOGI(TAG, "Attempting sensor recovery sequence...");
            
            // Multiple reset attempts
            for (int i = 0; i < 3; i++) {
                if (reset()) {
                    ESP_LOGI(TAG, "Reset attempt %d successful", i+1);
                    break;
                } else {
                    ESP_LOGW(TAG, "Reset attempt %d failed", i+1);
                    vTaskDelay(pdMS_TO_TICKS(10)); // Small delay between attempts
                }
            }
        }
        
        result.error = ESP_ERR_INVALID_CRC;
        result.value = 0.0f;
        return result;
    }
    
    // Convert temperature data
    int16_t raw_temp = (data[1] << 8) | data[0];
    float temperature = (float)raw_temp / 16.0f;
    
    ESP_LOGI(TAG, "OneWire read successful: Raw=0x%04X (%d), Temperature=%.2f°C (sensor: %016llX)", 
             raw_temp, raw_temp, temperature, address);
    
    result.error = ESP_OK;
    result.value = temperature;
    return result;
}
// Private low-level OneWire protocol methods
bool OneWireBusImpl::reset() {
    gpio_set_direction(data_pin_, GPIO_MODE_OUTPUT_OD);
    gpio_set_level(data_pin_, 0);
    ets_delay_us(480);                // reset pulse 480 µs

    gpio_set_level(data_pin_, 1);
    gpio_set_direction(data_pin_, GPIO_MODE_INPUT);          // listen to line
    ets_delay_us(70);

    bool presence = (gpio_get_level(data_pin_) == 0);

    ets_delay_us(410);                // complete 960 µs cycle

    gpio_set_direction(data_pin_, GPIO_MODE_OUTPUT_OD);         // return to idle-high
    gpio_set_level(data_pin_, 1);

    ESP_LOGV(TAG, "Presence = %d", presence);
    return presence;
}

void OneWireBusImpl::write_bit(bool bit) {
    gpio_set_direction(data_pin_, GPIO_MODE_OUTPUT_OD);
    if (bit) {
        // write-1: pull-low 10 µs, release ≥55 µs (Medium recommendation)
        gpio_set_level(data_pin_, 0);
        ets_delay_us(10);
        gpio_set_level(data_pin_, 1);
        ets_delay_us(55);
    } else {
        // write-0: pull-low 65 µs, release ≥5 µs (Medium recommendation)
        gpio_set_level(data_pin_, 0);
        ets_delay_us(65);
        gpio_set_level(data_pin_, 1);
        ets_delay_us(5);
    }
}

bool OneWireBusImpl::read_bit() {
    gpio_set_direction(data_pin_, GPIO_MODE_OUTPUT_OD);
    gpio_set_level(data_pin_, 0);
    ets_delay_us(3);                  // t-init-read

    gpio_set_direction(data_pin_, GPIO_MODE_INPUT);  // Release line immediately
    ets_delay_us(10);                 // t-sample - Medium recommendation: 10us
    bool bit = gpio_get_level(data_pin_);

    ets_delay_us(50);                 // complete time-slot to ~63us total
    gpio_set_direction(data_pin_, GPIO_MODE_OUTPUT_OD);
    gpio_set_level(data_pin_, 1);
    return bit;
}

void OneWireBusImpl::write_byte(uint8_t byte) {
    for (int i = 0; i < 8; i++) {
        write_bit((byte >> i) & 1);
    }
}

uint8_t OneWireBusImpl::read_byte() {
    uint8_t byte = 0;
    for (int i = 0; i < 8; i++) {
        if (read_bit()) {
            byte |= (1 << i);
        }
    }
    return byte;
}
bool OneWireBusImpl::search_device(uint8_t* address) {
    uint8_t id_bit_number = 1;
    uint8_t last_zero = 0;
    uint8_t rom_byte_number = 0;
    bool search_result = false;
    uint8_t id_bit, cmp_id_bit;
    uint8_t rom_byte_mask = 1;
    uint8_t search_direction;
    
    ESP_LOGI(TAG, "Starting device search...");
    
    // Reset if this is the first search
    if (!last_device_flag_) {
        last_discrepancy_ = 0;
        last_device_flag_ = false;
        last_family_discrepancy_ = 0;
    }
    
    // Check if all devices have been found
    if (last_device_flag_) {
        ESP_LOGI(TAG, "All devices already found");
        return false;
    }
    
    if (!reset()) {
        // Reset failed, no devices present
        ESP_LOGI(TAG, "Reset failed during search - no devices present");
        last_discrepancy_ = 0;
        last_device_flag_ = false;
        last_family_discrepancy_ = 0;
        return false;
    }
    
    ESP_LOGI(TAG, "Reset OK, sending SEARCHROM command (0x%02X)", SEARCHROM);
    write_byte(SEARCHROM);  // Issue search ROM command
    
    // Clear address buffer
    for (int i = 0; i < 8; i++) {
        address[i] = 0;
    }
    
    // Loop through all ROM bytes 0-7
    do {
        // Read a bit and its complement
        id_bit = read_bit();
        cmp_id_bit = read_bit();
        
        if (id_bit_number <= 8) {  // Only log first few bits to avoid spam
            ESP_LOGI(TAG, "Bit %d: id_bit=%d, cmp_id_bit=%d", id_bit_number, id_bit, cmp_id_bit);
        }
        
        // Check for no devices on 1-wire
        if ((id_bit == 1) && (cmp_id_bit == 1)) {
            ESP_LOGI(TAG, "No devices found (both bits = 1) at bit %d", id_bit_number);
            break;
        } else {
            // All devices coupled have 0 or 1
            if (id_bit != cmp_id_bit) {
                search_direction = id_bit;  // Bit write value for search
                if (id_bit_number <= 8) {
                    ESP_LOGI(TAG, "Clear bit choice: direction=%d", search_direction);
                }
            } else {
                // If this discrepancy is before the Last Discrepancy
                // on a previous next then pick the same as last time
                if (id_bit_number < last_discrepancy_) {
                    search_direction = ((address[rom_byte_number] & rom_byte_mask) > 0);
                    if (id_bit_number <= 8) {
                        ESP_LOGI(TAG, "Previous choice: direction=%d", search_direction);
                    }
                } else {
                    // If equal to last pick 1, if not then pick 0
                    search_direction = (id_bit_number == last_discrepancy_);
                    if (id_bit_number <= 8) {
                        ESP_LOGI(TAG, "New choice: direction=%d", search_direction);
                    }
                }
                
                // If 0 was picked then record its position in LastZero
                if (search_direction == 0) {
                    last_zero = id_bit_number;
                    
                    // Check for Last discrepancy in family
                    if (last_zero < 9) {
                        last_family_discrepancy_ = last_zero;
                    }
                }
            }
            
            // Set or clear the bit in the ROM byte rom_byte_number
            // with mask rom_byte_mask
            if (search_direction == 1) {
                address[rom_byte_number] |= rom_byte_mask;
            } else {
                address[rom_byte_number] &= ~rom_byte_mask;
            }
            
            // Serial number search direction write bit
            write_bit(search_direction);
            
            // Increment the byte counter id_bit_number
            // and shift the mask rom_byte_mask
            id_bit_number++;
            rom_byte_mask <<= 1;
            
            // If the mask is 0 then go to new SerialNum byte rom_byte_number and reset mask
            if (rom_byte_mask == 0) {
                ESP_LOGI(TAG, "Completed byte %d: 0x%02X", rom_byte_number, address[rom_byte_number]);
                rom_byte_number++;
                rom_byte_mask = 1;
            }
        }
    } while (rom_byte_number < 8);  // Loop until through all ROM bytes 0-7
    
    // If the search was successful then
    if (!(id_bit_number < 65)) {
        // Search successful so set LastDiscrepancy,LastDeviceFlag,search_result
        last_discrepancy_ = last_zero;
        
        // Check for last device
        if (last_discrepancy_ == 0) {
            last_device_flag_ = true;
        }
        
        search_result = true;
        ESP_LOGI(TAG, "Search successful! Found device with %d bits processed", id_bit_number-1);
        
        // Print found address
        ESP_LOGI(TAG, "Device address: %02X %02X %02X %02X %02X %02X %02X %02X",
                address[0], address[1], address[2], address[3],
                address[4], address[5], address[6], address[7]);
    } else {
        ESP_LOGI(TAG, "Search incomplete - only %d bits processed", id_bit_number-1);
    }
    
    // If no device found then reset counters so next 'search' will be like a first
    if (!search_result || !address[0]) {
        ESP_LOGI(TAG, "Search failed, resetting state (result=%d, addr[0]=%02X)", search_result, address[0]);
        last_discrepancy_ = 0;
        last_device_flag_ = false;
        last_family_discrepancy_ = 0;
        search_result = false;
    }
    
    return search_result;
}
bool OneWireBusImpl::check_crc(const uint8_t* data, uint8_t len, uint8_t expected_crc) {
    uint8_t crc = 0;
    
    // CRC lookup table for faster computation
    static const uint8_t crc_table[256] = {
        0, 94, 188, 226, 97, 63, 221, 131, 194, 156, 126, 32, 163, 253, 31, 65,
        157, 195, 33, 127, 252, 162, 64, 30, 95, 1, 227, 189, 62, 96, 130, 220,
        35, 125, 159, 193, 66, 28, 254, 160, 225, 191, 93, 3, 128, 222, 60, 98,
        190, 224, 2, 92, 223, 129, 99, 61, 124, 34, 192, 158, 29, 67, 161, 255,
        70, 24, 250, 164, 39, 121, 155, 197, 132, 218, 56, 102, 229, 187, 89, 7,
        219, 133, 103, 57, 186, 228, 6, 88, 25, 71, 165, 251, 120, 38, 196, 154,
        101, 59, 217, 135, 4, 90, 184, 230, 167, 249, 27, 69, 198, 152, 122, 36,
        248, 166, 68, 26, 153, 199, 37, 123, 58, 100, 134, 216, 91, 5, 231, 185,
        140, 210, 48, 110, 237, 179, 81, 15, 78, 16, 242, 172, 47, 113, 147, 205,
        17, 79, 173, 243, 112, 46, 204, 146, 211, 141, 111, 49, 178, 236, 14, 80,
        175, 241, 19, 77, 206, 144, 114, 44, 109, 51, 209, 143, 12, 82, 176, 238,
        50, 108, 142, 208, 83, 13, 239, 177, 240, 174, 76, 18, 145, 207, 45, 115,
        202, 148, 118, 40, 171, 245, 23, 73, 8, 86, 180, 234, 105, 55, 213, 139,
        87, 9, 235, 181, 54, 104, 138, 212, 149, 203, 41, 119, 244, 170, 72, 22,
        233, 183, 85, 11, 136, 214, 52, 106, 43, 117, 151, 201, 74, 20, 246, 168,
        116, 42, 200, 150, 21, 75, 169, 247, 182, 232, 10, 84, 215, 137, 107, 53
    };
    
    for (uint8_t i = 0; i < len; i++) {
        crc = crc_table[crc ^ data[i]];
    }
    
    return crc == expected_crc;
}

void OneWireBusImpl::debug_line_states() {
    ESP_LOGI(TAG, "=== OneWire Line Diagnostics (pin %d) ===", data_pin_);
    
    // Test basic line control
    gpio_set_level(data_pin_, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    int high_read = gpio_get_level(data_pin_);
    
    gpio_set_level(data_pin_, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    int low_read = gpio_get_level(data_pin_);
    
    gpio_set_level(data_pin_, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    int final_read = gpio_get_level(data_pin_);
    
    ESP_LOGI(TAG, "Line states - Set HIGH/Read: %d, Set LOW/Read: %d, Final HIGH/Read: %d", 
             high_read, low_read, final_read);
    
    if (high_read != 1) {
        ESP_LOGW(TAG, "WARNING: Line doesn't pull high - check pull-up resistor or power");
    }
    if (low_read != 0) {
        ESP_LOGW(TAG, "WARNING: Line doesn't pull low - check GPIO configuration");
    }
    
    // Test different read bit timings
    ESP_LOGI(TAG, "Testing different read bit timings...");
    if (reset()) {
        ESP_LOGI(TAG, "Testing read timing variations:");
        
        // Test different timing variations
        struct TimingTest {
            int init_pulse_us;
            int sample_delay_us;
            const char* name;
        };
        
        TimingTest timings[] = {
            {2, 12, "Current (2us + 12us)"},
            {1, 15, "Old (1us + 15us)"},
            {3, 10, "Shorter sample (3us + 10us)"},
            {1, 8, "Early sample (1us + 8us)"},
            {1, 5, "Very early (1us + 5us)"},
            {5, 8, "Longer init (5us + 8us)"}
        };
        
        for (int t = 0; t < 6; t++) {
            if (!reset()) break;
            
            ESP_LOGI(TAG, "Testing %s:", timings[t].name);
            
            // Read 8 bits with this timing
            uint8_t result_bits = 0;
            for (int i = 0; i < 8; i++) {
                portDISABLE_INTERRUPTS();
                
                // Custom read bit with specific timing
                gpio_set_level(data_pin_, 0);
                ets_delay_us(timings[t].init_pulse_us);
                gpio_set_level(data_pin_, 1);
                ets_delay_us(timings[t].sample_delay_us);
                bool bit = gpio_get_level(data_pin_);
                ets_delay_us(60 - timings[t].init_pulse_us - timings[t].sample_delay_us);
                
                portENABLE_INTERRUPTS();
                
                if (bit) result_bits |= (1 << i);
            }
            
            ESP_LOGI(TAG, "  Result: 0x%02X (%s)", result_bits, 
                    (result_bits == 0xFF) ? "All 1s" : 
                    (result_bits == 0x00) ? "All 0s" : "Mixed");
        }
    }
    
    // Test manual line control during read operation
    ESP_LOGI(TAG, "Testing manual line control during read...");
    if (reset()) {
        ESP_LOGI(TAG, "Manual timing test:");
        
        portDISABLE_INTERRUPTS();
        
        // Pull low for 3us
        gpio_set_level(data_pin_, 0);
        int state_low = gpio_get_level(data_pin_);
        ets_delay_us(3);
        
        // Release and immediately read
        gpio_set_level(data_pin_, 1);
        int state_0us = gpio_get_level(data_pin_);
        ets_delay_us(5);
        int state_5us = gpio_get_level(data_pin_);
        ets_delay_us(5);
        int state_10us = gpio_get_level(data_pin_);
        ets_delay_us(5);
        int state_15us = gpio_get_level(data_pin_);
        ets_delay_us(35);
        int state_50us = gpio_get_level(data_pin_);
        
        portENABLE_INTERRUPTS();
        
        ESP_LOGI(TAG, "  Line states: LOW=%d, @0us=%d, @5us=%d, @10us=%d, @15us=%d, @50us=%d",
                state_low, state_0us, state_5us, state_10us, state_15us, state_50us);
    }
    
    // Test multiple reset attempts with detailed timing
    ESP_LOGI(TAG, "Testing multiple reset attempts with detailed timing:");
    for (int i = 0; i < 3; i++) {
        ESP_LOGI(TAG, "=== Reset attempt %d ===", i+1);
        
        portDISABLE_INTERRUPTS();
        
        // Pull line low
        gpio_set_level(data_pin_, 0);
        int pre_delay_state = gpio_get_level(data_pin_);
        ESP_LOGI(TAG, "  Line state after pulling low: %d", pre_delay_state);
        
        ets_delay_us(500);  // Reset pulse
        
        // Release line
        gpio_set_level(data_pin_, 1);
        ets_delay_us(15);   // Wait a bit
        int immediately_after = gpio_get_level(data_pin_);
        
        ets_delay_us(65);   // Wait for presence window (15+65=80us total)
        int presence_window = gpio_get_level(data_pin_);
        
        ets_delay_us(240);  // Wait more (total 320us)
        int late_read = gpio_get_level(data_pin_);
        
        ets_delay_us(200);  // Complete cycle (total 520us)
        int final_state = gpio_get_level(data_pin_);
        
        portENABLE_INTERRUPTS();
        
        bool presence = !presence_window;
        ESP_LOGI(TAG, "  Timing: immediately=%d, @80us=%d, @320us=%d, final=%d -> Presence: %s", 
                immediately_after, presence_window, late_read, final_state,
                presence ? "YES" : "NO");
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    ESP_LOGI(TAG, "=== End Line Diagnostics ===");
}