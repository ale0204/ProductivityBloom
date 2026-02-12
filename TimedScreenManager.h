#ifndef TIMED_SCREEN_MANAGER_H
#define TIMED_SCREEN_MANAGER_H

/**
 * ============================================
 * Timed Screen Manager for OLED
 * ============================================
 * 
 * Abstractizare pentru ecrane temporare (congratulations, revive, etc.)
 * Elimină logica manuală cu millis() din loop principal
 */

#include <Arduino.h>
#include <functional>

// ============================================
// Screen Types Enum
// ============================================
enum class ScreenType {
    IDLE,           // Normal display (plant, status)
    CONGRATS,       // Congratulations screen
    REVIVE,         // Revive animation
    QR_CODE,        // QR code for AP mode
    CUSTOM          // User-defined screen
};

// ============================================
// Timed Callback Structure
// ============================================
struct TimedCallback {
    std::function<void()> onStart;      // Called when screen activates
    std::function<void()> onDraw;       // Called each frame
    std::function<void()> onEnd;        // Called when duration expires
    uint32_t duration;                  // Duration in ms (0 = permanent)
    bool loopDraw;                      // If true, onDraw is called repeatedly
};

// ============================================
// Screen Manager Class
// ============================================
class TimedScreenManager {
public:
    TimedScreenManager() {
        currentScreen = ScreenType::IDLE;
        screenStartTime = 0;
        screenDuration = 0;
        needsRefresh = true;
        isActive = false;
    }
    
    // Register a screen with its callbacks
    void registerScreen(ScreenType type, TimedCallback callback) {
        if (static_cast<int>(type) < MAX_SCREENS) {
            callbacks[static_cast<int>(type)] = callback;
        }
    }
    
    // Show a screen for a duration (0 = until manually changed)
    void showScreen(ScreenType type, uint32_t durationOverride = 0) {
        int idx = static_cast<int>(type);
        if (idx >= MAX_SCREENS) return;
        
        // End previous screen
        if (isActive && currentScreen != ScreenType::IDLE) {
            int prevIdx = static_cast<int>(currentScreen);
            if (callbacks[prevIdx].onEnd) {
                callbacks[prevIdx].onEnd();
            }
        }
        
        currentScreen = type;
        screenStartTime = millis();
        screenDuration = durationOverride > 0 ? durationOverride : callbacks[idx].duration;
        isActive = true;
        needsRefresh = true;
        
        // Call onStart
        if (callbacks[idx].onStart) {
            callbacks[idx].onStart();
        }
    }
    
    // Return to idle screen
    void showIdle() {
        showScreen(ScreenType::IDLE);
    }
    
    // Check if timed screen has expired and handle transitions
    // Call this in main loop
    void update() {
        if (!isActive || currentScreen == ScreenType::IDLE) {
            return;
        }
        
        int idx = static_cast<int>(currentScreen);
        
        // Check if duration expired
        if (screenDuration > 0 && (millis() - screenStartTime >= screenDuration)) {
            // Call onEnd
            if (callbacks[idx].onEnd) {
                callbacks[idx].onEnd();
            }
            
            // Return to idle
            currentScreen = ScreenType::IDLE;
            isActive = false;
            needsRefresh = true;
            return;
        }
        
        // If loopDraw is enabled, mark for continuous refresh
        if (callbacks[idx].loopDraw) {
            needsRefresh = true;
        }
    }
    
    // Draw current screen (call from OLED refresh)
    void draw() {
        int idx = static_cast<int>(currentScreen);
        if (callbacks[idx].onDraw) {
            callbacks[idx].onDraw();
        }
        needsRefresh = false;
    }
    
    // Getters
    ScreenType getCurrentScreen() const { return currentScreen; }
    bool isScreenActive(ScreenType type) const { return currentScreen == type && isActive; }
    bool needsRedraw() const { return needsRefresh; }
    void requestRefresh() { needsRefresh = true; }
    
    // Get elapsed time for animations
    uint32_t getElapsedTime() const {
        return millis() - screenStartTime;
    }
    
    // Get progress (0.0 to 1.0) for animations
    float getProgress() const {
        if (screenDuration == 0) return 0.0f;
        float progress = (float)(millis() - screenStartTime) / (float)screenDuration;
        return progress > 1.0f ? 1.0f : progress;
    }

private:
    static const int MAX_SCREENS = 8;
    
    TimedCallback callbacks[MAX_SCREENS];
    ScreenType currentScreen;
    uint32_t screenStartTime;
    uint32_t screenDuration;
    bool needsRefresh;
    bool isActive;
};

// ============================================
// Animation Helper Functions
// ============================================
namespace Animation {
    // Ease-in-out curve
    inline float easeInOut(float t) {
        return t < 0.5f ? 2.0f * t * t : 1.0f - (-2.0f * t + 2.0f) * (-2.0f * t + 2.0f) / 2.0f;
    }
    
    // Bounce effect
    inline float bounce(float t) {
        const float n1 = 7.5625f;
        const float d1 = 2.75f;
        
        if (t < 1.0f / d1) {
            return n1 * t * t;
        } else if (t < 2.0f / d1) {
            t -= 1.5f / d1;
            return n1 * t * t + 0.75f;
        } else if (t < 2.5f / d1) {
            t -= 2.25f / d1;
            return n1 * t * t + 0.9375f;
        } else {
            t -= 2.625f / d1;
            return n1 * t * t + 0.984375f;
        }
    }
    
    // Pulse effect (0 to 1 to 0)
    inline float pulse(float t) {
        return sin(t * 3.14159f);
    }
    
    // Oscillate between 0 and 1
    inline float oscillate(float t, float frequency = 1.0f) {
        return (sin(t * frequency * 6.28318f) + 1.0f) / 2.0f;
    }
}

// ============================================
// Simple State Machine for Complex Screens
// ============================================
template<typename StateEnum>
class ScreenStateMachine {
public:
    ScreenStateMachine(StateEnum initial) : currentState(initial), stateStartTime(0) {}
    
    void setState(StateEnum newState) {
        if (currentState != newState) {
            currentState = newState;
            stateStartTime = millis();
        }
    }
    
    StateEnum getState() const { return currentState; }
    
    uint32_t getStateElapsed() const {
        return millis() - stateStartTime;
    }
    
    bool isInState(StateEnum state) const {
        return currentState == state;
    }
    
    // Transition to next state after duration
    bool transitionAfter(StateEnum nextState, uint32_t duration) {
        if (getStateElapsed() >= duration) {
            setState(nextState);
            return true;
        }
        return false;
    }
    
private:
    StateEnum currentState;
    uint32_t stateStartTime;
};

#endif // TIMED_SCREEN_MANAGER_H
