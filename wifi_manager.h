#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

class WifiManager {
public:
    WifiManager(const char* ssid, const char* password);
    void connect();

private:
    const char* _ssid;
    const char* _password;
};

#endif // WIFI_MANAGER_H 