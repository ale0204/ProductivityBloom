# Productivity Bloom - System Architecture

## Overview

ESP32-based smart productivity cube with OLED display and web interface.
**Architecture:** Event-Driven Single-Core

```
┌─────────────────────────────────────────────────────────────┐
│                      User Actions                           │
│              (Web Interface / LDR Sensor)                   │
└──────────────────────────┬──────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│                     SystemState                              │
│              (Single Source of Truth)                        │
│  - Tasks, Timer, Plant state                                │
│  - Pushes events to EventQueue                              │
└──────────────────────────┬──────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│                     EventQueue<32>                           │
│              (Push-based Event System)                       │
│  - PLANT_BLOOMED, PLANT_REVIVED, MIDNIGHT, etc.             │
└──────────────────────────┬──────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│                    Main Loop Handlers                        │
│  - processEvents() → handle each event                      │
│  - refreshOLED() → DisplayRenderer                          │
│  - webServer→broadcast() → WebSocket clients                │
└─────────────────────────────────────────────────────────────┘
```

---

## Core Components

### 1. IntervalTimer (`IntervalTimer.h`)

Replaces repetitive `if (millis() - last >= interval)` patterns.

```cpp
// OLD WAY ❌
uint32_t lastSensorRead = 0;
if (millis() - lastSensorRead >= 100) {
    lastSensorRead = millis();
    handleSensors();
}

// NEW WAY ✓
IntervalTimer sensorTimer(100);
if (sensorTimer.elapsed()) {
    handleSensors();
}
```

**Classes:**
- `IntervalTimer` - Periodic timing (sensors, OLED refresh)
- `OneShotTimer` - One-time delays (congrats screen, revive animation)
- `Debouncer` - Button/sensor debouncing

### 2. EventQueue (`EventQueue.h`)

Push-based event system. Components push events, main loop consumes them.

```cpp
// Producer (in SystemState, Analytics, etc.)
eventQueue.push(Event::PLANT_BLOOMED);
eventQueue.push(Event::MIDNIGHT);

// Consumer (in main loop)
while (eventQueue.hasEvents()) {
    EventData event = eventQueue.popData();
    switch (event.type) {
        case Event::PLANT_BLOOMED:
            showingCongrats = true;
            congratsTimer.start(5000);
            break;
        case Event::MIDNIGHT:
            handleMidnight();
            break;
    }
}
```

**Event Types:**
| Event | Description |
|-------|-------------|
| `MIDNIGHT` | Day changed, check if goals met |
| `TIMER_TICK` | Every second during focus/break |
| `STATE_CHANGED` | Mode changed (idle→focus, etc.) |
| `PLANT_WATERED` | Plant received water |
| `PLANT_BLOOMED` | Plant reached stage 3 |
| `PLANT_REVIVED` | Plant brought back from withered |
| `OLED_REFRESH` | Screen needs redraw |
| `WEB_BROADCAST` | Send update to web clients |

### 3. DisplayRenderer (`DisplayRenderer.h`)

All OLED drawing in one class with dependency injection.

```cpp
// Inject U8G2 reference
DisplayRenderer display(u8g2);

// High-level screen methods
display.drawIdleScreen(plant, isAPMode, hasWebClient, showWelcome);
display.drawFocusScreen(taskName, timeLeft, totalTime);
display.drawCongratsScreen();
display.drawReviveScreen();

// Component methods
display.drawPlant(plant);
display.drawTimer(timeLeft, totalTime);
display.drawClock(hour, minute);
display.drawQRCode();
```

**Benefits:**
- All drawing logic centralized
- Easy to modify UI
- Testable (can mock U8G2)

### 4. SystemState (`SystemState.h`)

Single Source of Truth for application state.

**State:**
- Tasks (name, duration, completed)
- Timer (timeLeft, mode)
- Plant (stage, withered, wateredCount)
- Daily goal

**Events pushed:**
- `STATE_CHANGED` on mode change
- `TIMER_TICK` every second
- `PLANT_WATERED`, `PLANT_BLOOMED`, `PLANT_REVIVED` on plant changes

**Sensor handling:**
```cpp
// LDR sensor processing moved here
systemState.handleLightSensor(ldrValue);
```

### 5. Analytics (`Analytics.h`)

Weekly stats tracking with NTP time sync.

**Features:**
- Records focus sessions, breaks, tasks completed
- Stores 7-day history in NVS
- Midnight detection → pushes `Event::MIDNIGHT`
- Weekly report generation

---

## Main Loop Flow

```cpp
void loop() {
    // 1. Update core systems
    systemState.loop();      // Timer tick
    webServer->loop();       // Client cleanup
    analytics.loop();        // Midnight check
    
    // 2. Read sensors (rate limited)
    if (sensorTimer.elapsed()) {
        systemState.handleLightSensor(analogRead(LDR_PIN));
    }
    
    // 3. Process event queue
    processEvents();
    
    // 4. Check overlay timers
    if (showingCongrats && congratsTimer.expired()) {
        showingCongrats = false;
    }
    
    // 5. Refresh OLED (rate limited)
    if (oledNeedsRefresh && oledRefreshTimer.elapsed()) {
        refreshOLED();
        webServer->broadcastStatus();
    }
}
```

---

## File Structure

```
final/
├── final.ino              # Main sketch (event-driven loop)
├── config.h               # Pin definitions, constants
├── IntervalTimer.h        # Clean timing utilities
├── EventQueue.h           # Push-based event system
├── SystemState.h          # State management + sensor handling
├── DisplayRenderer.h      # OLED drawing abstraction
├── Analytics.h            # Weekly stats + midnight detection
├── WebServerHandler.h     # AsyncWebServer + WebSocket
├── QRCodeGenerator.h      # QR code for AP mode
├── WebContent.h           # Embedded HTML/CSS/JS
├── data/                  # Web files (source)
│   ├── index.html
│   ├── style.css
│   └── app.js
└── backup_single_core/    # Original implementation backup
```

---

## Key Improvements Over Original

| Aspect | Before | After |
|--------|--------|-------|
| Timing | `millis() - last >= interval` everywhere | `IntervalTimer.elapsed()` |
| Midnight | Polling every 60s | Push `Event::MIDNIGHT` once |
| OLED Drawing | Functions scattered in final.ino | `DisplayRenderer` class |
| Sensor Logic | In handleSensors() function | `SystemState.handleLightSensor()` |
| State Changes | Direct callbacks | EventQueue + callbacks (hybrid) |
| Overlays | Manual millis() tracking | `OneShotTimer` |

---

## Hardware

- **MCU:** ESP32-D0WD-V3 (4MB flash, dual-core)
- **Display:** Waveshare 1.5" SSD1327 128x128 OLED (SPI @ 2MHz)
- **Sensor:** LDR for plant revive
- **Power:** 3.7V LiPo battery

---

## Compilation

Use Arduino IDE or PlatformIO with ESP32 board package.

Required libraries:
- U8g2
- ESPAsyncWebServer
- AsyncTCP

The IntelliSense errors in VS Code (`cannot find Preferences.h`) are normal - 
these are ESP32-specific headers that exist only in the Arduino/PlatformIO build system.
