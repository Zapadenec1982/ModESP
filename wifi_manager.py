import time

class WifiManager:
    def __init__(self, ssid, password):
        self.ssid = ssid
        self.password = password
        self.wlan = network.WLAN(network.STA_IF)

    def connect(self):
        if not self.wlan.isconnected():
            print('Connecting to network...', end='')
            self.wlan.active(True)
            self.wlan.connect(self.ssid, self.password)

            max_wait = 10
            while max_wait > 0:
                if self.wlan.status() < 0 or self.wlan.status() >= 3:
                    break
                max_wait -= 1
                print('.', end='')
                time.sleep(1)
            
            print()

        if self.wlan.isconnected():
            print('Network config:', self.wlan.ifconfig())
            return True 