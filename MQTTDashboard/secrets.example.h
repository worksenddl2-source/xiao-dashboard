// Copy this file to secrets.h and fill in your Wi-Fi (2.4 GHz) and broker settings.
// secrets.h is in .gitignore so your password is never uploaded.
#pragma once

#define WIFI_SSID "your-wifi-name"
#define WIFI_PASS "your-wifi-password"

// IP address of the PC running Mosquitto (ipconfig -> IPv4 of the Wi-Fi adapter)
#define MQTT_HOST "192.168.0.10"
#define MQTT_PORT 1884
