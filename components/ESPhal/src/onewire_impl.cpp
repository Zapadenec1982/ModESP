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
    
    // Configure data pin
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT_OD;  // Open-drain mode
    io_conf.pin_bit_mask = (1ULL << data_pin_);
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;  // Enable internal pull-up
    gpio_config(&io_conf);
    
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
    
    // Set bus high initially
    gpio_set_level(data_pin_, 1);
    
    ESP_LOGI(TAG, "OneWire bus initialized on pin %d", data_pin_);
}

OneWireBusImpl::~OneWireBusImpl() {
    if (power_pin_ != GPIO_NUM_NC) {
        gpio_set_level(power_pin_, 0);  // Turn off power
    }
}
std::vector<uint64_t> OneWireBusImpl::search_devices() {
    std::vector<uint64_t> devices;
    uint8_t address[8];
    bool found = false;
    
    ESP_LOGI(TAG, "Searching for devices on OneWire bus (pin %d)", data_pin_);
    
    // Test bus first
    ESP_LOGI(TAG, "Testing OneWire bus presence...");
    if (!reset()) {
        ESP_LOGW(TAG, "No presence pulse detected - check wiring and pull-up resistor");
        return devices;
    }
    ESP_LOGI(TAG, "Presence pulse detected - bus is working");
    
    // Reset search state
    last_discrepancy_ = 0;
    last_device_flag_ = false;
    
    while (true) {
        if (search_device(address)) {
            // Convert address to uint64_t
            uint64_t addr = 0;
            for (int i = 7; i >= 0; i--) {
                addr = (addr << 8) | address[i];
            }
            devices.push_back(addr);
            found = true;
            
            ESP_LOGI(TAG, "Found device: %02X%02X%02X%02X%02X%02X%02X%02X",
                     address[7], address[6], address[5], address[4],
                     address[3], address[2], address[1], address[0]);
        } else {
            break;
        }
    }
    
    ESP_LOGI(TAG, "Found %zu device(s) on OneWire bus", devices.size());
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

HalResult<float> OneWireBusImpl::read_temperature(uint64_t address) {
    HalResult<float> result;
    uint8_t addr_bytes[8];
    
    // Convert uint64_t address to byte array
    for (int i = 0; i < 8; i++) {
        addr_bytes[i] = (address >> (i * 8)) & 0xFF;
    }
    
    ESP_LOGV(TAG, "Reading temperature from device: %016llX", address);
    
    if (!reset()) {
        ESP_LOGW(TAG, "No devices found during reset");
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
    
    // Verify CRC
    if (!check_crc(data, 8, data[8])) {
        ESP_LOGW(TAG, "CRC check failed");
        result.error = ESP_ERR_INVALID_CRC;
        result.value = 0.0f;
        return result;
    }
    
    // Convert temperature data
    int16_t raw_temp = (data[1] << 8) | data[0];
    float temperature = (float)raw_temp / 16.0f;
    
    ESP_LOGV(TAG, "Raw temperature: %d, Converted: %.2f°C", raw_temp, temperature);
    
    result.error = ESP_OK;
    result.value = temperature;
    return result;
}
// Private low-level OneWire protocol methods
bool OneWireBusImpl::reset() {
    // Disable interrupts for precise timing
    portDISABLE_INTERRUPTS();
    
    gpio_set_level(data_pin_, 0);        // Pull line low
    ets_delay_us(480);                   // Hold for 480us minimum
    
    gpio_set_level(data_pin_, 1);        // Release line
    ets_delay_us(70);                    // Wait 70us for presence pulse
    
    bool presence = !gpio_get_level(data_pin_);  // Check for presence pulse (low)
    
    ets_delay_us(410);                   // Complete the reset cycle (480us total)
    
    portENABLE_INTERRUPTS();
    
    return presence;
}

void OneWireBusImpl::write_bit(bool bit) {
    portDISABLE_INTERRUPTS();
    
    if (bit) {
        // Write '1' bit
        gpio_set_level(data_pin_, 0);    // Pull low
        ets_delay_us(6);                 // Hold for 6us
        gpio_set_level(data_pin_, 1);    // Release
        ets_delay_us(64);                // Wait for rest of slot (70us total)
    } else {
        // Write '0' bit
        gpio_set_level(data_pin_, 0);    // Pull low
        ets_delay_us(60);                // Hold for 60us
        gpio_set_level(data_pin_, 1);    // Release
        ets_delay_us(10);                // Wait for recovery (70us total)
    }
    
    portENABLE_INTERRUPTS();
}

bool OneWireBusImpl::read_bit() {
    portDISABLE_INTERRUPTS();
    
    gpio_set_level(data_pin_, 0);        // Pull low
    ets_delay_us(3);                     // Hold for 3us (initiate read slot)
    gpio_set_level(data_pin_, 1);        // Release
    ets_delay_us(10);                    // Wait 10us before reading
    
    bool bit = gpio_get_level(data_pin_); // Read the bit
    
    ets_delay_us(53);                    // Wait for rest of slot (66us total)
    
    portENABLE_INTERRUPTS();
    
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
    
    ESP_LOGV(TAG, "Starting device search...");
    
    // Reset if this is the first search
    if (!last_device_flag_) {
        last_discrepancy_ = 0;
        last_device_flag_ = false;
        last_family_discrepancy_ = 0;
    }
    
    // Check if all devices have been found
    if (last_device_flag_) {
        ESP_LOGV(TAG, "All devices already found");
        return false;
    }
    
    if (!reset()) {
        // Reset failed, no devices present
        ESP_LOGD(TAG, "Reset failed during search - no devices present");
        last_discrepancy_ = 0;
        last_device_flag_ = false;
        last_family_discrepancy_ = 0;
        return false;
    }
    
    ESP_LOGV(TAG, "Reset OK, sending SEARCHROM command");
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
        
        ESP_LOGV(TAG, "Bit %d: id_bit=%d, cmp_id_bit=%d", id_bit_number, id_bit, cmp_id_bit);
        
        // Check for no devices on 1-wire
        if ((id_bit == 1) && (cmp_id_bit == 1)) {
            ESP_LOGD(TAG, "No devices found (both bits = 1)");
            break;
        } else {
            // All devices coupled have 0 or 1
            if (id_bit != cmp_id_bit) {
                search_direction = id_bit;  // Bit write value for search
            } else {
                // If this discrepancy is before the Last Discrepancy
                // on a previous next then pick the same as last time
                if (id_bit_number < last_discrepancy_) {
                    search_direction = ((address[rom_byte_number] & rom_byte_mask) > 0);
                } else {
                    // If equal to last pick 1, if not then pick 0
                    search_direction = (id_bit_number == last_discrepancy_);
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
                ESP_LOGV(TAG, "Completed byte %d: 0x%02X", rom_byte_number, address[rom_byte_number]);
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
        ESP_LOGV(TAG, "Search successful! Found device");
    }
    
    // If no device found then reset counters so next 'search' will be like a first
    if (!search_result || !address[0]) {
        ESP_LOGD(TAG, "Search failed, resetting state");
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