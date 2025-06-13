#include <Arduino.h>
#include "wifi_manager.h" // Або як називається ваш header файл

// Замініть на ваші дані
const char* ssid = "your_ssid";
const char* password = "your_password";

// Створення об'єкту
WifiManager wifiManager(ssid, password);

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ; // очікування підключення серійного порту
  }

  // Запуск підключення до Wi-Fi
  wifiManager.connect();
}

void loop() {
  // Ваш основний код
} 