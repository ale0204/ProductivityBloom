#ifndef EVENT_QUEUE_H
#define EVENT_QUEUE_H

/**
 * ============================================
 * EventQueue - Push-based event system
 * ============================================
 * 
 * Replaces polling patterns with event-driven architecture.
 * Components push events, server/loop consumes them.
 * 
 * Benefits:
 * - No more constant polling (checkMidnight every 60s)
 * - Events fire exactly once
 * - Decoupled components
 * - Easy to add new event types
 * 
 * Usage:
 *   // Producer (Analytics, SystemState, etc.)
 *   eventQueue.push(Event::MIDNIGHT);
 *   eventQueue.push(Event::PLANT_CHANGED);
 *   
 *   // Consumer (main loop, web server)
 *   while (eventQueue.hasEvents()) {
 *       Event e = eventQueue.pop();
 *       handleEvent(e);
 *   }
 */

#include <Arduino.h>

// ============================================
// Event Types
// ============================================
enum class Event : uint8_t {
    NONE = 0,
    
    // Time events
    MIDNIGHT,           // Day changed, check goals
    TIMER_TICK,         // Every second during focus/break
    TIMER_COMPLETE,     // Focus/break timer finished
    
    // State events
    STATE_CHANGED,      // Mode changed (idle->focus, etc.)
    TASK_ADDED,         // New task added
    TASK_DELETED,       // Task removed
    TASK_STARTED,       // Task timer started
    TASK_COMPLETED,     // Task marked complete
    
    // Plant events
    PLANT_WATERED,      // Plant received water
    PLANT_WITHERED,     // Plant died
    PLANT_REVIVED,      // Plant brought back
    PLANT_BLOOMED,      // Plant reached stage 3
    
    // UI events
    OLED_REFRESH,       // Screen needs redraw
    WEB_BROADCAST,      // Send update to web clients
    
    // Sensor events
    LIGHT_DETECTED,     // LDR threshold crossed
    FLIP_DETECTED,      // MPU6050 detected flip
    FLIP_CONFIRM_NEEDED, // User flipped back, needs to confirm completion
    FLIP_RESUMED,       // User flipped back to resume (accidental flip)
    FLIP_CANCELLED,     // User confirmed flip was accidental, waiting for flip back
    
    // System events
    SAVE_STATE,         // Persist to NVS
    DAY_RESET,          // Reset for new day
    
    _EVENT_COUNT        // For array sizing
};

// ============================================
// Event with optional data
// ============================================
struct EventData {
    Event type;
    uint32_t timestamp;
    union {
        uint32_t value;      // Generic value
        uint32_t taskId;     // For task events
        uint8_t stage;       // For plant events
        struct {
            uint16_t param1;
            uint16_t param2;
        };
    };
    
    EventData() : type(Event::NONE), timestamp(0), value(0) {}
    EventData(Event e) : type(e), timestamp(millis()), value(0) {}
    EventData(Event e, uint32_t val) : type(e), timestamp(millis()), value(val) {}
};

// ============================================
// Circular Buffer Event Queue
// ============================================
template<size_t CAPACITY = 16>
class EventQueue {
public:
    EventQueue() : head(0), tail(0), count(0) {}
    
    // Push event to queue (returns false if full)
    bool push(Event event) {
        return pushData(EventData(event));
    }
    
    // Push event with value
    bool push(Event event, uint32_t value) {
        return pushData(EventData(event, value));
    }
    
    // Push full event data
    bool pushData(const EventData& data) {
        if (count >= CAPACITY) {
            // Queue full - drop oldest event
            head = (head + 1) % CAPACITY;
            count--;
        }
        
        buffer[tail] = data;
        tail = (tail + 1) % CAPACITY;
        count++;
        return true;
    }
    
    // Check if events are pending
    bool hasEvents() const {
        return count > 0;
    }
    
    // Get next event (simple)
    Event pop() {
        if (count == 0) return Event::NONE;
        
        Event e = buffer[head].type;
        head = (head + 1) % CAPACITY;
        count--;
        return e;
    }
    
    // Get next event with data
    EventData popData() {
        if (count == 0) return EventData();
        
        EventData data = buffer[head];
        head = (head + 1) % CAPACITY;
        count--;
        return data;
    }
    
    // Peek at next event without removing
    Event peek() const {
        if (count == 0) return Event::NONE;
        return buffer[head].type;
    }
    
    // Peek with data
    const EventData& peekData() const {
        static EventData empty;
        if (count == 0) return empty;
        return buffer[head];
    }
    
    // Check if specific event is pending
    bool hasEvent(Event event) const {
        for (size_t i = 0; i < count; i++) {
            size_t idx = (head + i) % CAPACITY;
            if (buffer[idx].type == event) return true;
        }
        return false;
    }
    
    // Remove all instances of specific event
    void remove(Event event) {
        // Rebuild queue without the specified event
        EventData temp[CAPACITY];
        size_t newCount = 0;
        
        for (size_t i = 0; i < count; i++) {
            size_t idx = (head + i) % CAPACITY;
            if (buffer[idx].type != event) {
                temp[newCount++] = buffer[idx];
            }
        }
        
        // Copy back
        for (size_t i = 0; i < newCount; i++) {
            buffer[i] = temp[i];
        }
        head = 0;
        tail = newCount;
        count = newCount;
    }
    
    // Clear all events
    void clear() {
        head = 0;
        tail = 0;
        count = 0;
    }
    
    // Get queue stats
    size_t size() const { return count; }
    size_t capacity() const { return CAPACITY; }
    bool isEmpty() const { return count == 0; }
    bool isFull() const { return count >= CAPACITY; }

private:
    EventData buffer[CAPACITY];
    size_t head;
    size_t tail;
    size_t count;
};

// ============================================
// Event Name Helper (for debugging)
// ============================================
inline const char* eventName(Event e) {
    switch (e) {
        case Event::NONE: return "NONE";
        case Event::MIDNIGHT: return "MIDNIGHT";
        case Event::TIMER_TICK: return "TIMER_TICK";
        case Event::TIMER_COMPLETE: return "TIMER_COMPLETE";
        case Event::STATE_CHANGED: return "STATE_CHANGED";
        case Event::TASK_ADDED: return "TASK_ADDED";
        case Event::TASK_DELETED: return "TASK_DELETED";
        case Event::TASK_STARTED: return "TASK_STARTED";
        case Event::TASK_COMPLETED: return "TASK_COMPLETED";
        case Event::PLANT_WATERED: return "PLANT_WATERED";
        case Event::PLANT_WITHERED: return "PLANT_WITHERED";
        case Event::PLANT_REVIVED: return "PLANT_REVIVED";
        case Event::PLANT_BLOOMED: return "PLANT_BLOOMED";
        case Event::OLED_REFRESH: return "OLED_REFRESH";
        case Event::WEB_BROADCAST: return "WEB_BROADCAST";
        case Event::LIGHT_DETECTED: return "LIGHT_DETECTED";
        case Event::FLIP_DETECTED: return "FLIP_DETECTED";
        case Event::SAVE_STATE: return "SAVE_STATE";
        case Event::DAY_RESET: return "DAY_RESET";
        default: return "UNKNOWN";
    }
}

// ============================================
// Global Event Queue Instance
// ============================================
extern EventQueue<32> eventQueue;

#endif // EVENT_QUEUE_H
