/**
 * MPU6050Handler.h
 * Flip detection pentru controlul timer-ului cu MPU-6050
 * 
 * Conexiuni:
 * VCC -> 3.3V
 * GND -> GND
 * SDA -> GPIO21
 * SCL -> GPIO22
 * INT -> GPIO27 (optional)
 */

#ifndef MPU6050_HANDLER_H
#define MPU6050_HANDLER_H

#include <Wire.h>
#include "config.h"

// MPU-6050 I2C address
#define MPU6050_ADDR 0x68

// Thresholds for flip detection
#define FLIP_THRESHOLD_HIGH  10000   // AccelZ > this = NORMAL (face up)
#define FLIP_THRESHOLD_LOW  -10000   // AccelZ < this = FLIPPED (face down)

// Debounce time to avoid false triggers
#define FLIP_DEBOUNCE_MS 500

// Callback type for flip events
typedef void (*FlipCallback)(bool isFlipped);

class MPU6050Handler {
public:
    MPU6050Handler() : 
        initialized(false),
        isFlipped(false),
        wasFlipped(false),
        lastFlipTime(0),
        flipCallback(nullptr),
        accelZ(0) {}
    
    bool begin() {
        DEBUG_PRINTLN("MPU6050: Initializing...");
        
        // Initialize I2C
        Wire.begin(21, 22);  // SDA=GPIO21, SCL=GPIO22
        
        // Check if MPU-6050 is responding
        Wire.beginTransmission(MPU6050_ADDR);
        Wire.write(0x75);  // WHO_AM_I register
        Wire.endTransmission(false);
        Wire.requestFrom(MPU6050_ADDR, 1, true);
        
        if (Wire.available()) {
            uint8_t whoAmI = Wire.read();
            DEBUG_PRINTF("MPU6050: WHO_AM_I = 0x%02X\n", whoAmI);
            if (whoAmI != 0x68 && whoAmI != 0x98) {
                DEBUG_PRINTLN("MPU6050: Wrong device ID!");
                return false;
            }
        } else {
            DEBUG_PRINTLN("MPU6050: No response from device!");
            return false;
        }
        
        // Wake up MPU-6050 (it starts in sleep mode)
        Wire.beginTransmission(MPU6050_ADDR);
        Wire.write(0x6B);  // PWR_MGMT_1 register
        Wire.write(0x00);  // Clear sleep bit
        if (Wire.endTransmission() != 0) {
            DEBUG_PRINTLN("MPU6050: Failed to wake up!");
            return false;
        }
        
        // Configure accelerometer to ±2g (most sensitive)
        Wire.beginTransmission(MPU6050_ADDR);
        Wire.write(0x1C);  // ACCEL_CONFIG register
        Wire.write(0x00);  // ±2g range
        Wire.endTransmission();
        
        // Small delay for sensor to stabilize
        delay(100);
        
        // Read initial state
        readAccelZ();
        isFlipped = (accelZ < FLIP_THRESHOLD_LOW);
        wasFlipped = isFlipped;
        
        initialized = true;
        DEBUG_PRINTF("MPU6050: Initialized! Initial state: %s\n", 
                     isFlipped ? "FLIPPED" : "NORMAL");
        
        return true;
    }
    
    void update() {
        if (!initialized) return;
        
        readAccelZ();
        
        // Store previous state
        wasFlipped = isFlipped;
        
        // Update state with hysteresis to avoid flickering
        if (accelZ < FLIP_THRESHOLD_LOW) {
            isFlipped = true;
        } else if (accelZ > FLIP_THRESHOLD_HIGH) {
            isFlipped = false;
        }
        // Between thresholds: keep previous state (hysteresis)
        
        // Detect state change with debouncing
        if (isFlipped != wasFlipped) {
            unsigned long now = millis();
            if (now - lastFlipTime > FLIP_DEBOUNCE_MS) {
                lastFlipTime = now;
                
                DEBUG_PRINTF("MPU6050: FLIP detected! Now: %s (accelZ=%d)\n",
                             isFlipped ? "FLIPPED" : "NORMAL", accelZ);
                
                // Call callback if registered
                if (flipCallback) {
                    flipCallback(isFlipped);
                }
            } else {
                // Debounce: revert to previous state
                isFlipped = wasFlipped;
            }
        }
    }
    
    void onFlip(FlipCallback callback) {
        flipCallback = callback;
    }
    
    bool getIsFlipped() const {
        return isFlipped;
    }
    
    int16_t getAccelZ() const {
        return accelZ;
    }
    
    bool isInitialized() const {
        return initialized;
    }
    
private:
    void readAccelZ() {
        // We only need AccelZ for flip detection
        // AccelZ is at registers 0x3F (high) and 0x40 (low)
        Wire.beginTransmission(MPU6050_ADDR);
        Wire.write(0x3F);  // ACCEL_ZOUT_H
        Wire.endTransmission(false);
        Wire.requestFrom(MPU6050_ADDR, 2, true);
        
        if (Wire.available() >= 2) {
            accelZ = (Wire.read() << 8) | Wire.read();
        }
    }
    
    bool initialized;
    bool isFlipped;
    bool wasFlipped;
    unsigned long lastFlipTime;
    FlipCallback flipCallback;
    int16_t accelZ;
};

#endif // MPU6050_HANDLER_H
