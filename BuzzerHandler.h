/**
 * BuzzerHandler.h
 * GPIO25 ──[ 220Ω ]── Piezo ── GND
 */

#ifndef BUZZER_HANDLER_H
#define BUZZER_HANDLER_H

#include <Arduino.h>
#include "config.h"

// Note frequencies (Hz)
#define NOTE_C4  262
#define NOTE_D4  294
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_G4  392
#define NOTE_A4  440
#define NOTE_B4  494
#define NOTE_C5  523
#define NOTE_D5  587
#define NOTE_E5  659
#define NOTE_F5  698
#define NOTE_G5  784
#define NOTE_A5  880
#define NOTE_B5  988
#define NOTE_C6  1047
#define NOTE_E6  1319

// PWM channel for buzzer
#define BUZZER_CHANNEL 0

class BuzzerHandler {
public:
    BuzzerHandler() : initialized(false), lastWarningTime(0) {}
    
    void begin() {
        // Setup PWM for passive buzzer (new ESP32 Arduino core API)
        ledcAttach(BUZZER_PIN, 2000, 8);  // pin, freq, resolution
        ledcWrite(BUZZER_PIN, 0);  // Start silent
        initialized = true;
        DEBUG_PRINTLN("Buzzer: Initialized on GPIO25");
    }
    
    // Play a tone for specified duration
    void tone(uint16_t frequency, uint16_t duration) {
        if (!initialized) return;
        ledcWriteTone(BUZZER_PIN, frequency);
        delay(duration);
        ledcWriteTone(BUZZER_PIN, 0);
    }
    
    // Non-blocking tone (needs update() to stop)
    void toneStart(uint16_t frequency) {
        if (!initialized) return;
        ledcWriteTone(BUZZER_PIN, frequency);
    }
    
    void toneStop() {
        if (!initialized) return;
        ledcWriteTone(BUZZER_PIN, 0);
    }
    
    // ============================================
    // Melodii predefinite
    // ============================================
    
    // Warning beeps - 3 secunde înainte de terminare
    void playWarningBeeps() {
        // 3 beep-uri rapide și plăcute
        tone(NOTE_E5, 100);
        delay(80);
        tone(NOTE_E5, 100);
        delay(80);
        tone(NOTE_G5, 150);
    }
    
    // Focus session complete - melodie de succes
    void playFocusComplete() {
        // Melodie ascendentă - success!
        tone(NOTE_C5, 120);
        delay(30);
        tone(NOTE_E5, 120);
        delay(30);
        tone(NOTE_G5, 120);
        delay(30);
        tone(NOTE_C6, 200);
    }
    
    // Break complete - melodie soft de trezire
    void playBreakComplete() {
        // Două tonuri gentle
        tone(NOTE_G4, 150);
        delay(100);
        tone(NOTE_C5, 200);
    }
    
    // Plant revived - melodie fericită
    void playRevive() {
        tone(NOTE_C5, 100);
        delay(50);
        tone(NOTE_E5, 100);
        delay(50);
        tone(NOTE_G5, 100);
        delay(50);
        tone(NOTE_C6, 100);
        delay(50);
        tone(NOTE_E6, 200);
    }
    
    // Plant withered - melodie tristă
    void playWithered() {
        tone(NOTE_E4, 200);
        delay(100);
        tone(NOTE_D4, 200);
        delay(100);
        tone(NOTE_C4, 300);
    }
    
    // Task completed - beep scurt pozitiv
    void playTaskComplete() {
        tone(NOTE_E5, 80);
        delay(50);
        tone(NOTE_G5, 120);
    }
    
    // Error/Cancel beep
    void playError() {
        tone(NOTE_A4, 150);
        delay(50);
        tone(NOTE_A4, 150);
    }
    
    // Check if should play warning (called every timer tick)
    // Returns true if warning should play at this second
    bool shouldPlayWarning(uint16_t secondsLeft) {
        // Play warning at 3, 2, 1 seconds
        if (secondsLeft <= 3 && secondsLeft > 0) {
            unsigned long now = millis();
            // Prevent multiple plays in same second
            if (now - lastWarningTime > 800) {
                lastWarningTime = now;
                return true;
            }
        }
        return false;
    }
    
    // Single warning beep for countdown
    void playCountdownBeep(uint16_t secondsLeft) {
        if (secondsLeft == 3) {
            tone(NOTE_E5, 80);
        } else if (secondsLeft == 2) {
            tone(NOTE_E5, 80);
        } else if (secondsLeft == 1) {
            tone(NOTE_G5, 120);  // Higher pitch for last second
        }
    }
    
private:
    bool initialized;
    unsigned long lastWarningTime;
};

#endif // BUZZER_HANDLER_H
