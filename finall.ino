/**
 * ============================================
 * Productivity Bloom - Smart Productivity Cube
 * ============================================
 *
 * Architecture: Event-Driven Single-Core
 * ---------------------------------------
 * Uses the new architecture components:
 * - IntervalTimer: Clean timing (no manual millis())
 * - EventQueue: Push-based events (no polling)
 * - DisplayRenderer: All OLED drawing in one class
 * 
 * All networking runs on same core to avoid lwIP issues.
 * 
 * Flow: User Action → SystemState → EventQueue → Handlers
 */

#include <SPI.h>
#include <U8g2lib.h>
#include <math.h>
#include "config.h"
#include "IntervalTimer.h"
#include "EventQueue.h"
#include "SystemState.h"
#include "DisplayRenderer.h"
#include "WebServerHandler.h"
#include "Analytics.h"
#include "QRCodeGenerator.h"
#include "MPU6050Handler.h"
#include "BuzzerHandler.h"

// ============================================
// Global Objects
// ============================================

// Single Source of Truth (includes EventQueue)
SystemState systemState;

// Analytics (weekly stats tracking)
Analytics analytics;

// MPU-6050 Handler (flip detection)
MPU6050Handler mpuHandler;

// Buzzer Handler (audio feedback)
BuzzerHandler buzzer;

// OLED Display - Waveshare 1.5" SSD1327 128x128
// U8G2_R2 = 180 degree rotation so text is readable when cube is on table
U8G2_SSD1327_WS_128X128_F_4W_HW_SPI u8g2(U8G2_R2, OLED_CS, OLED_DC, OLED_RST);

// Display Renderer (new architecture)
DisplayRenderer display(u8g2);

// Web Server Handler
WebServerHandler* webServer = nullptr;

// ============================================
// Interval Timers (replaces manual millis())
// ============================================
IntervalTimer sensorTimer(SENSOR_READ_INTERVAL);  // 100ms default
IntervalTimer oledRefreshTimer(100);              // 10 FPS
IntervalTimer statsTimer(1000);                   // 1 second

// ============================================
// Timed Screen Overlays (congrats, revive)
// ============================================
OneShotTimer congratsTimer;
OneShotTimer reviveTimer;
bool showingCongrats = false;
bool showingRevive = false;

// ============================================
// State Tracking
// ============================================
bool showedWelcome = false;
bool displayFlipped = true;  // Start with 180° so text is readable
volatile bool oledNeedsRefresh = true;

// Track previous mode for session recording
SystemMode previousMode = MODE_IDLE;
uint32_t sessionStartTime = 0;
uint32_t accumulatedFocusMs = 0;  // Accumulated focus time in ms (for pause handling)
uint32_t accumulatedBreakMs = 0;  // Accumulated break time in ms

// ============================================
// Forward Declarations
// ============================================
void processEvents();
void handleMidnight();
void refreshOLED();

// ============================================
// Setup
// ============================================
void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(1000);

    DEBUG_PRINTLN("\n\n=================================");
    DEBUG_PRINTLN("   Productivity Bloom v3.0");
    DEBUG_PRINTLN("   Event-Driven Architecture");
    DEBUG_PRINTLN("=================================\n");

    // Initialize LDR pin
    pinMode(LDR_PIN, INPUT);

    // Initialize OLED Display
    DEBUG_PRINTLN("Initializing OLED...");
    
    pinMode(OLED_RST, OUTPUT);
    digitalWrite(OLED_RST, LOW);
    delay(50);
    digitalWrite(OLED_RST, HIGH);
    delay(100);
    
    SPI.begin(OLED_SCLK, -1, OLED_MOSI, OLED_CS);
    SPI.setFrequency(2000000);

    delay(100);

    u8g2.begin();
    delay(50);
    // Rotation is set in constructor (U8G2_R2 = 180°)
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x12_tr);
    u8g2.setDrawColor(1);
    u8g2.setContrast(200);

    // Show splash screen
    display.beginFrame();
    u8g2.setFont(u8g2_font_ncenB14_tr);
    const char* title = "Bloom v3.0";
    int16_t titleWidth = u8g2.getStrWidth(title);
    u8g2.drawStr((128 - titleWidth) / 2, 55, title);
    
    u8g2.setFont(u8g2_font_6x12_tr);
    const char* subtitle = "Connecting WiFi...";
    int16_t subWidth = u8g2.getStrWidth(subtitle);
    u8g2.drawStr((128 - subWidth) / 2, 75, subtitle);
    display.endFrame();

    DEBUG_PRINTLN("OLED initialized!");

    // Initialize SystemState
    systemState.begin();

    // Register callbacks (legacy support - they also push to EventQueue)
    systemState.onStateChanged([]() {
        oledNeedsRefresh = true;
    });
    systemState.onTimerTick([]() {
        oledNeedsRefresh = true;
    });
    systemState.onPlantChanged([]() {
        oledNeedsRefresh = true;
    });

    DEBUG_PRINTLN("SystemState initialized!");

    // Initialize Web Server
    DEBUG_PRINTLN("Initializing Web Server...");
    webServer = new WebServerHandler(&systemState);
    webServer->begin();

    // Update OLED to show ready state
    if (webServer->isAPMode()) {
        display.beginFrame();
        display.drawBorder();
        display.drawQRScreen();
        display.endFrame();
        DEBUG_PRINTLN("Showing AP mode QR code screen");
    } else {
        display.beginFrame();
        u8g2.setFont(u8g2_font_ncenB14_tr);
        u8g2.drawStr((128 - titleWidth) / 2, 55, title);
        
        u8g2.setFont(u8g2_font_6x12_tr);
        String ipMsg = "IP: " + webServer->getIP();
        int16_t ipWidth = u8g2.getStrWidth(ipMsg.c_str());
        u8g2.drawStr((128 - ipWidth) / 2, 75, ipMsg.c_str());
        display.endFrame();
        delay(2000);
    }
    
    // Initialize Analytics
    DEBUG_PRINTLN("Initializing Analytics...");
    analytics.begin();
    analytics.onMidnight(handleMidnight);

    // Initialize MPU-6050 (flip detection)
    DEBUG_PRINTLN("Initializing MPU-6050...");
    if (mpuHandler.begin()) {
        DEBUG_PRINTLN("MPU-6050 initialized successfully!");
        // Register flip callback
        mpuHandler.onFlip([](bool isFlipped) {
            DEBUG_PRINTF("MPU FLIP callback: isFlipped=%d\n", isFlipped);
            
            // Rotate display based on cube orientation
            // isFlipped=true means OLED facing down, need U8G2_R0
            // isFlipped=false means OLED facing up, need U8G2_R2
            if (isFlipped) {
                u8g2.setDisplayRotation(U8G2_R0);  // Normal when cube is flipped
            } else {
                u8g2.setDisplayRotation(U8G2_R2);  // 180° when cube is normal
            }
            
            systemState.handleFlip(isFlipped);
            oledNeedsRefresh = true;
            if (webServer) {
                webServer->broadcastStatus();
            }
        });
    } else {
        DEBUG_PRINTLN("WARNING: MPU-6050 not found! Flip control disabled.");
    }

    // Initialize Buzzer
    DEBUG_PRINTLN("Initializing Buzzer...");
    buzzer.begin();

    DEBUG_PRINTLN("Setup complete!");
    DEBUG_PRINTF("Access web interface at: http://%s\n", webServer->getIP().c_str());

    // Reset all timers
    sensorTimer.reset();
    oledRefreshTimer.reset();
    statsTimer.reset();
}

// ============================================
// Main Loop (Event-Driven)
// ============================================
void loop() {
    // Debug performance
    static uint32_t loopCount = 0;
    static uint32_t lastPrint = 0;
    static uint32_t totalSystemTime = 0, totalWebTime = 0, totalAnalyticsTime = 0, totalOledTime = 0;
    loopCount++;
    
    uint32_t startTime = micros();

    // 1. Update SystemState timer logic
    systemState.loop();
    totalSystemTime += (micros() - startTime);

    // 2. Handle Web Server
    startTime = micros();
    if (webServer) {
        webServer->loop();
    }
    totalWebTime += (micros() - startTime);
    
    // 3. Analytics loop (midnight check)
    startTime = micros();
    analytics.loop();
    totalAnalyticsTime += (micros() - startTime);

    // 4. Read sensors on interval
    if (sensorTimer.elapsed()) {
        int ldrValue = analogRead(LDR_PIN);
        systemState.handleLightSensor(ldrValue);
        
        // Update MPU-6050 (flip detection)
        mpuHandler.update();
        
        // Debug MPU every 2 seconds
        static uint32_t lastMpuDebug = 0;
        if (millis() - lastMpuDebug > 2000) {
            lastMpuDebug = millis();
            DEBUG_PRINTF("MPU Debug: initialized=%d, accelZ=%d, flipped=%d\n", 
                        mpuHandler.isInitialized(), mpuHandler.getAccelZ(), mpuHandler.getIsFlipped());
        }
    }

    // 5. Process event queue
    processEvents();

    // 6. Check overlay timers
    if (showingCongrats && congratsTimer.expired()) {
        showingCongrats = false;
        oledNeedsRefresh = true;
    }
    if (showingRevive && reviveTimer.expired()) {
        showingRevive = false;
        oledNeedsRefresh = true;
    }

    // 7. Refresh OLED when needed (rate limited)
    startTime = micros();
    if (oledNeedsRefresh && oledRefreshTimer.elapsed()) {
        refreshOLED();
        oledNeedsRefresh = false;
    }
    totalOledTime += (micros() - startTime);
    
    // 8. Broadcast WebSocket status once per second (not on every OLED refresh)
    static uint32_t lastWsBroadcast = 0;
    if (millis() - lastWsBroadcast >= 1000) {
        lastWsBroadcast = millis();
        if (webServer && (systemState.getMode() == MODE_FOCUSING || systemState.getMode() == MODE_BREAK)) {
            webServer->broadcastStatus();
        }
    }

    // 9. Print performance stats
    if (statsTimer.elapsed()) {
        uint32_t now = millis();
        if (now - lastPrint >= 1000) {
            DEBUG_PRINTF("Loops/sec: %lu | System: %luμs | Web: %luμs | Analytics: %luμs | OLED: %luμs\n", 
                        loopCount, totalSystemTime/loopCount, totalWebTime/loopCount, 
                        totalAnalyticsTime/loopCount, totalOledTime/loopCount);
            loopCount = 0;
            totalSystemTime = totalWebTime = totalAnalyticsTime = totalOledTime = 0;
            lastPrint = now;
        }
    }

    yield();
}

// ============================================
// Event Processing
// ============================================
void processEvents() {
    while (eventQueue.hasEvents()) {
        EventData event = eventQueue.popData();
        
        switch (event.type) {
            case Event::MIDNIGHT:
                // Already handled by callback, but can add extra logic here
                DEBUG_PRINTLN("Event: MIDNIGHT processed");
                break;
                
            case Event::PLANT_BLOOMED:
                if (!showingCongrats) {
                    showingCongrats = true;
                    congratsTimer.start(5000);
                    oledNeedsRefresh = true;
                    DEBUG_PRINTLN("Event: PLANT_BLOOMED - showing congrats!");
                }
                break;
                
            case Event::PLANT_REVIVED:
                if (!showingRevive) {
                    showingRevive = true;
                    reviveTimer.start(4000);
                    oledNeedsRefresh = true;
                    DEBUG_PRINTLN("Event: PLANT_REVIVED - showing celebration!");
                }
                // Broadcast to web clients
                if (webServer) {
                    webServer->broadcastPlant();
                    webServer->broadcastStatus();
                    webServer->broadcastRevive();  // Special revive message
                }
                break;
                
            case Event::PLANT_WATERED:
                analytics.recordTaskCompleted();
                DEBUG_PRINTLN("Event: PLANT_WATERED - recorded in analytics");
                break;
                
            case Event::STATE_CHANGED:
                handleStateChanged();
                if (webServer) {
                    webServer->broadcastTasks();
                }
                break;
                
            case Event::TIMER_TICK:
                // Check for countdown warning beeps (3, 2, 1 seconds left)
                {
                    uint16_t timeLeft = systemState.getTimeLeft();
                    SystemMode mode = systemState.getMode();
                    if ((mode == MODE_FOCUSING || mode == MODE_BREAK) && timeLeft <= 3 && timeLeft > 0) {
                        buzzer.playCountdownBeep(timeLeft);
                    }
                }
                break;
                
            case Event::WEB_BROADCAST:
                if (webServer) {
                    webServer->broadcastPlant();
                    webServer->broadcastStatus();
                }
                break;
                
            case Event::OLED_REFRESH:
                oledNeedsRefresh = true;
                break;
                
            default:
                // Unhandled event
                break;
        }
    }
}

// ============================================
// State Change Handler (for analytics)
// ============================================
void handleStateChanged() {
    SystemMode currentMode = systemState.getMode();
    
    // Helper lambda to round milliseconds to minutes (round up if >= 30 seconds)
    auto msToMins = [](uint32_t ms) -> uint32_t {
        uint32_t totalSeconds = ms / 1000;
        uint32_t mins = totalSeconds / 60;
        uint32_t remainingSeconds = totalSeconds % 60;
        if (remainingSeconds >= 30) mins++;  // Round up if >= 30 seconds
        return mins;
    };
    
    // When entering FOCUSING mode (fresh start, not resume from pause)
    if (currentMode == MODE_FOCUSING && previousMode != MODE_FOCUSING && previousMode != MODE_PAUSED) {
        sessionStartTime = millis();
        accumulatedFocusMs = 0;
        DEBUG_PRINTLN("Analytics: Starting new focus session");
    }
    
    // When entering BREAK mode (focus just completed)
    if (currentMode == MODE_BREAK && previousMode != MODE_BREAK) {
        // If coming from focusing, record the focus session first
        if (previousMode == MODE_FOCUSING || accumulatedFocusMs > 0) {
            uint32_t focusMs = accumulatedFocusMs;
            if (previousMode == MODE_FOCUSING && sessionStartTime > 0) {
                focusMs += (millis() - sessionStartTime);
            }
            uint32_t focusMins = msToMins(focusMs);
            if (focusMins > 0) {
                analytics.recordFocusSession(focusMins);
                DEBUG_PRINTF("Analytics: Recorded focus session: %lu min\n", focusMins);
            }
            accumulatedFocusMs = 0;
        }
        // Start break timer
        sessionStartTime = millis();
        accumulatedBreakMs = 0;
        DEBUG_PRINTLN("Analytics: Starting break session");
    }
    
    // When leaving FOCUSING to PAUSED (flip detected)
    if (previousMode == MODE_FOCUSING && currentMode == MODE_PAUSED) {
        // Accumulate the focus time so far
        if (sessionStartTime > 0) {
            accumulatedFocusMs += (millis() - sessionStartTime);
            DEBUG_PRINTF("Analytics: Paused focus, accumulated: %lu ms\n", accumulatedFocusMs);
        }
        sessionStartTime = 0;
    }
    
    // When resuming from PAUSED to FOCUSING
    if (previousMode == MODE_PAUSED && currentMode == MODE_FOCUSING) {
        sessionStartTime = millis();
        DEBUG_PRINTLN("Analytics: Resumed focus session");
    }
    
    // When going IDLE from anywhere (task completed or cancelled)
    if (currentMode == MODE_IDLE && previousMode != MODE_IDLE) {
        // Record any accumulated focus time
        if (previousMode == MODE_FOCUSING || previousMode == MODE_PAUSED) {
            uint32_t focusMs = accumulatedFocusMs;
            if (previousMode == MODE_FOCUSING && sessionStartTime > 0) {
                focusMs += (millis() - sessionStartTime);
            }
            uint32_t focusMins = msToMins(focusMs);
            if (focusMins > 0) {
                analytics.recordFocusSession(focusMins);
                DEBUG_PRINTF("Analytics: Recorded focus session (completed): %lu min\n", focusMins);
            }
        }
        // Record any accumulated break time
        if (previousMode == MODE_BREAK && sessionStartTime > 0) {
            uint32_t breakMs = accumulatedBreakMs + (millis() - sessionStartTime);
            uint32_t breakMins = msToMins(breakMs);
            if (breakMins > 0) {
                analytics.recordBreakSession(breakMins);
                DEBUG_PRINTF("Analytics: Recorded break session: %lu min\n", breakMins);
            }
        }
        
        // Reset everything
        accumulatedFocusMs = 0;
        accumulatedBreakMs = 0;
        sessionStartTime = 0;
    }
    
    previousMode = currentMode;
}

// ============================================
// Midnight Handler
// ============================================
void handleMidnight() {
    DEBUG_PRINTLN("Midnight! Checking if daily goals were met...");
    
    PlantInfo plant = systemState.getPlantInfo();
    
    if (plant.totalGoal > 0 && plant.wateredCount < plant.totalGoal) {
        DEBUG_PRINTF("Goals NOT met! (%d/%d) - Plant withers!\n", 
                     plant.wateredCount, plant.totalGoal);
        systemState.killPlant();
        
        if (webServer) {
            webServer->broadcastPlant();
        }
    } else if (plant.totalGoal > 0) {
        DEBUG_PRINTF("Goals met! (%d/%d) - Great job!\n", 
                     plant.wateredCount, plant.totalGoal);
    }
    
    systemState.resetForNewDay();
    oledNeedsRefresh = true;
}

// ============================================
// OLED Refresh
// ============================================
void refreshOLED() {
    SPI.setFrequency(2000000);
    
    display.beginFrame();
    display.drawBorder();
    
    // Clock in top-right
    int hour, minute;
    analytics.getCurrentTime(hour, minute);
    display.drawClock(hour, minute);

    // Check overlays first
    if (showingRevive) {
        display.drawReviveScreen();
        display.endFrame();
        delay(5);
        oledNeedsRefresh = true;  // Keep refreshing until timer expires
        return;
    }
    
    if (showingCongrats) {
        display.drawCongratsScreen();
        display.endFrame();
        delay(5);
        oledNeedsRefresh = true;  // Keep refreshing until timer expires
        return;
    }

    // Draw based on mode
    SystemMode mode = systemState.getMode();
    PlantInfo plant = systemState.getPlantInfo();
    const char* taskName = systemState.getCurrentTaskName();

    switch (mode) {
        case MODE_IDLE:
            display.drawIdleScreen(
                plant,
                webServer && webServer->isAPMode(),
                webServer && webServer->hasWebClient(),
                !showedWelcome
            );
            showedWelcome = true;
            break;

        case MODE_FOCUSING:
            display.drawFocusScreen(
                taskName,
                systemState.getTimeLeft(),
                systemState.getTotalTime()
            );
            break;

        case MODE_BREAK:
            display.drawBreakScreen(
                taskName,
                systemState.getTimeLeft(),
                systemState.getTotalTime()
            );
            break;

        case MODE_PAUSED:
            display.drawPausedScreen(
                taskName,
                systemState.getTimeLeft(),
                systemState.getTotalTime()
            );
            break;

        case MODE_WITHERED:
            display.drawWitheredScreen();
            break;
    }

    display.endFrame();
    delay(10);
}
