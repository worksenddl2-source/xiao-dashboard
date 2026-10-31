// Copy this file to secrets.h and fill in your values.
// secrets.h is in .gitignore so your password and device key are never uploaded.
#pragma once

#define WIFI_SSID "your-wifi-name"
#define WIFI_PASS "your-wifi-password"

// Backend URL (Render) without a trailing slash
#define API_URL "https://your-service.onrender.com"

// Random device key. Register its SHA-256 hash in Supabase (see schema.sql)
#define DEVICE_KEY "your-device-key"
