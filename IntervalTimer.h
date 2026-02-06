#ifndef INTERVAL_TIMER_H
#define INTERVAL_TIMER_H

/**
 * ============================================
 * IntervalTimer - Clean timing utility
 * ============================================
 * 
 * Replaces repetitive `if (millis() - last >= interval)` patterns
 * with a clean, reusable abstraction.
 * 
 * Usage:
 *   IntervalTimer sensorTimer(100);  // 100ms interval
 *   if (sensorTimer.elapsed()) {
 *       handleSensors();
 *   }
 */

#include <Arduino.h>

class IntervalTimer {
public:
    // Create timer with interval in milliseconds
    explicit IntervalTimer(uint32_t intervalMs) 
        : interval(intervalMs), lastTime(0), enabled(true) {}
    
    // Check if interval has elapsed (auto-resets on true)
    bool elapsed() {
        if (!enabled) return false;
        
        uint32_t now = millis();
        if (now - lastTime >= interval) {
            lastTime = now;
            return true;
        }
        return false;
    }
    
    // Check without auto-reset
    bool check() const {
        if (!enabled) return false;
        return (millis() - lastTime >= interval);
    }
    
    // Manual reset
    void reset() {
        lastTime = millis();
    }
    
    // Force trigger on next check
    void trigger() {
        lastTime = 0;
    }
    
    // Enable/disable timer
    void setEnabled(bool en) { enabled = en; }
    bool isEnabled() const { return enabled; }
    
    // Change interval
    void setInterval(uint32_t ms) { interval = ms; }
    uint32_t getInterval() const { return interval; }
    
    // Get time remaining until next trigger
    uint32_t remaining() const {
        uint32_t elapsed = millis() - lastTime;
        if (elapsed >= interval) return 0;
        return interval - elapsed;
    }
    
    // Get elapsed time since last trigger
    uint32_t elapsedTime() const {
        return millis() - lastTime;
    }

private:
    uint32_t interval;
    uint32_t lastTime;
    bool enabled;
};

// ============================================
// OneShotTimer - Triggers once after delay
// ============================================
class OneShotTimer {
public:
    OneShotTimer() : startTime(0), duration(0), active(false), triggered(false) {}
    
    // Start the timer
    void start(uint32_t durationMs) {
        duration = durationMs;
        startTime = millis();
        active = true;
        triggered = false;
    }
    
    // Check if timer expired (returns true only once)
    bool expired() {
        if (!active || triggered) return false;
        
        if (millis() - startTime >= duration) {
            triggered = true;
            active = false;
            return true;
        }
        return false;
    }
    
    // Check if currently running
    bool isRunning() const {
        return active && !triggered && (millis() - startTime < duration);
    }
    
    // Cancel the timer
    void cancel() {
        active = false;
        triggered = false;
    }
    
    // Get progress (0.0 to 1.0)
    float progress() const {
        if (!active || duration == 0) return triggered ? 1.0f : 0.0f;
        
        uint32_t elapsed = millis() - startTime;
        if (elapsed >= duration) return 1.0f;
        return (float)elapsed / (float)duration;
    }
    
    // Get remaining time
    uint32_t remaining() const {
        if (!active) return 0;
        uint32_t elapsed = millis() - startTime;
        if (elapsed >= duration) return 0;
        return duration - elapsed;
    }

private:
    uint32_t startTime;
    uint32_t duration;
    bool active;
    bool triggered;
};

// ============================================
// Debouncer - For button/sensor debouncing
// ============================================
class Debouncer {
public:
    explicit Debouncer(uint32_t debounceMs = 50) 
        : debounceTime(debounceMs), lastChangeTime(0), lastState(false), stableState(false) {}
    
    // Update with new reading, returns true if stable state changed
    bool update(bool currentState) {
        uint32_t now = millis();
        
        if (currentState != lastState) {
            lastChangeTime = now;
            lastState = currentState;
        }
        
        if ((now - lastChangeTime) >= debounceTime) {
            if (stableState != lastState) {
                stableState = lastState;
                return true;  // State changed
            }
        }
        
        return false;
    }
    
    bool getState() const { return stableState; }
    
    // Detect rising edge (false -> true transition)
    bool rose() {
        static bool lastStable = false;
        bool rose = stableState && !lastStable;
        lastStable = stableState;
        return rose;
    }
    
    // Detect falling edge (true -> false transition)
    bool fell() {
        static bool lastStable = true;
        bool fell = !stableState && lastStable;
        lastStable = stableState;
        return fell;
    }

private:
    uint32_t debounceTime;
    uint32_t lastChangeTime;
    bool lastState;
    bool stableState;
};

#endif // INTERVAL_TIMER_H
