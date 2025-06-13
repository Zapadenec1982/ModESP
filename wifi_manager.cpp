#include "wifi_manager.h"
#include <Arduino.h>
#include <WiFi.h>

WifiManager::WifiManager(const char* ssid, const char* password)
: _ssid(ssid), _password(password) {
}

void WifiManager::connect() {
    Serial.print("Connecting to network: ");
    Serial.println(_ssid);

    WiFi.begin(_ssid, _password);

    int max_wait = 20; // Час очікування підключення в секундах (20 * 500ms = 10s)
    while (WiFi.status() != WL_CONNECTED && max_wait > 0) {
        delay(500);
        Serial.print(".");
        max_wait--;
    }

    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("WiFi connected!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("WiFi connection failed!");
    }
} 