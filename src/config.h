#ifndef CONFIG_H
#define CONFIG_H

// ============================================
// WiFi Configuration
// ============================================
#define WIFI_SSID "DIGI-Dg9Y"
#define WIFI_PASSWORD "XFrgeUPa3P"

// Access Point fallback (if WiFi fails)
#define AP_SSID "ProductivityBloom"
#define AP_PASSWORD "bloom2024"

// ============================================
// NTP Configuration (Romania Timezone)
// ============================================
#define NTP_SERVER "pool.ntp.org"
#define GMT_OFFSET_SEC 7200      // UTC+2 (Romania standard time)
#define DAYLIGHT_OFFSET_SEC 3600 // +1 hour for DST (summer time)

// ============================================
// Pin Definitions
// ============================================
// OLED Display (SPI - Waveshare 1.5" SSD1327 128x128)
#define OLED_CS    5
#define OLED_DC    16
#define OLED_RST   4      // RST controlat de ESP32
#define OLED_MOSI  23
#define OLED_SCLK  18
#define OLED_WIDTH 128
#define OLED_HEIGHT 128

// MAX7219 Matrix (SPI) - Optional for hourglass animation
#define MATRIX_DIN 23
#define MATRIX_CLK 18
#define MATRIX_CS 5
#define MATRIX_DEVICES 1

// MPU6050 (I2C - shared with OLED)
#define MPU_ADDR 0x68

// LDR Sensor
#define LDR_PIN 34  // ADC pin

// Buzzer (optional)
#define BUZZER_PIN 25

// ============================================
// Game Logic Constants
// ============================================
#define MAX_TASKS 10
#define TASK_NAME_MAX_LENGTH 32

#define PLANT_STAGES 4  // 0: seed, 1: sprout, 2: growing, 3: bloomed

// LDR threshold for revive (higher = more light needed)
#define LDR_REVIVE_THRESHOLD 3000
#define LDR_REVIVE_DURATION 3000  // ms of light exposure needed

// Flip detection sensitivity
#define FLIP_THRESHOLD 8.0  // m/s² acceleration change

// ============================================
// Timing Constants
// ============================================
#define MIDNIGHT_CHECK_INTERVAL 60000    // Check every minute
#define SENSOR_READ_INTERVAL 100          // Read sensors every 100ms
#define WEBSOCKET_UPDATE_INTERVAL 1000    // Send updates every second
#define ANIMATION_FRAME_DELAY 50          // Animation speed

// ============================================
// NVS Keys (Persistent Storage)
// ============================================
#define NVS_NAMESPACE "bloom"
#define NVS_KEY_PLANT_STAGE "plantStage"
#define NVS_KEY_PLANT_WITHERED "plantWithered"
#define NVS_KEY_LAST_DATE "lastDate"
#define NVS_KEY_TASKS_DONE "tasksDone"
#define NVS_KEY_TASKS_TOTAL "tasksTotal"
#define NVS_KEY_FOCUS_MINUTES "focusMins"

// ============================================
// Debug
// ============================================
#define DEBUG_SERIAL true
#define SERIAL_BAUD 115200

#if DEBUG_SERIAL
    #define DEBUG_PRINT(x) Serial.print(x)
    #define DEBUG_PRINTLN(x) Serial.println(x)
    #define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
    #define DEBUG_PRINT(x)
    #define DEBUG_PRINTLN(x)
    #define DEBUG_PRINTF(...)
#endif

#endif // CONFIG_H
