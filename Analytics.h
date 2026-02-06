#ifndef ANALYTICS_H
#define ANALYTICS_H

#include <Arduino.h>
#include <Preferences.h>
#include <time.h>
#include "config.h"
#include "EventQueue.h"
#include "IntervalTimer.h"

// ============================================
// Daily Stats Structure (compact for NVS storage)
// ============================================
struct DailyStats {
    uint8_t dayOfWeek;        // 0=Sunday, 1=Monday, etc.
    uint8_t tasksCompleted;
    uint16_t focusMinutes;
    uint16_t breakMinutes;
    uint8_t sessionsCount;
    bool valid;               // Is this entry valid?
};

// ============================================
// Weekly Report Structure
// ============================================
struct WeeklyReport {
    uint16_t totalTasks;
    uint16_t totalFocusMinutes;
    uint16_t totalBreakMinutes;
    uint16_t totalSessions;
    uint8_t avgTasksPerDay;
    uint16_t avgFocusPerDay;
    uint8_t mostProductiveDay;     // 0-6 (Sun-Sat)
    uint8_t mostProductiveTasks;   // Tasks on that day
    bool hasFullWeek;              // True if we have 7 days of data
    uint8_t daysRecorded;          // How many days we have
};

// ============================================
// Analytics Manager
// ============================================
class Analytics {
public:
    Analytics();
    
    void begin();
    void loop();  // Call in main loop for midnight checks
    
    // Recording events
    void recordTaskCompleted();
    void recordFocusSession(uint16_t minutes);
    void recordBreakSession(uint16_t minutes);
    
    // Queries
    DailyStats getTodayStats();
    DailyStats getDayStats(uint8_t daysAgo);  // 0=today, 1=yesterday, etc.
    WeeklyReport getWeeklyReport();
    
    // Time utilities
    bool isTimeValid();
    void getCurrentTime(int& hour, int& minute);
    int getCurrentDayOfWeek();  // 0=Sunday
    String getCurrentDateString();  // "YYYY-MM-DD"
    
    // Force daily reset (for testing)
    void forceDailyReset();
    
    // Midnight callback (called when day changes)
    typedef std::function<void()> MidnightCallback;
    void onMidnight(MidnightCallback callback) { midnightCallback = callback; }

private:
    Preferences prefs;
    
    // Current day tracking
    uint8_t currentDayOfWeek;
    String currentDateStr;
    
    // Today's live stats (in RAM, saved periodically)
    DailyStats todayStats;
    uint32_t lastSaveTime;
    IntervalTimer midnightCheckTimer;  // Check for midnight every 60s
    bool statsChanged;
    
    // Week history (7 days)
    DailyStats weekHistory[7];
    
    // Midnight callback
    MidnightCallback midnightCallback;
    bool pendingMidnightCallback;  // Set true if day changed at boot
    
    // Internal methods
    void loadFromNVS();
    void saveToNVS();
    void saveDayToHistory(DailyStats& stats);
    void loadWeekHistory();
    void checkMidnight();
    void performDailyReset();
    String getDayName(uint8_t day);
};

// ============================================
// Implementation
// ============================================

Analytics::Analytics() 
    : midnightCheckTimer(60000)  // Check every 60 seconds
{
    currentDayOfWeek = 0;
    lastSaveTime = 0;
    statsChanged = false;
    midnightCallback = nullptr;
    pendingMidnightCallback = false;
    
    // Initialize today's stats
    todayStats = {0, 0, 0, 0, 0, false};
    
    // Initialize week history
    for (int i = 0; i < 7; i++) {
        weekHistory[i] = {0, 0, 0, 0, 0, false};
    }
}

void Analytics::begin() {
    DEBUG_PRINTLN("Analytics: Initializing...");
    
    // Wait for time to sync (NTP should be configured before this)
    int attempts = 0;
    while (!isTimeValid() && attempts < 10) {
        delay(500);
        attempts++;
    }
    
    if (isTimeValid()) {
        struct tm timeinfo;
        getLocalTime(&timeinfo);
        currentDayOfWeek = timeinfo.tm_wday;
        
        char dateBuf[11];
        strftime(dateBuf, sizeof(dateBuf), "%Y-%m-%d", &timeinfo);
        currentDateStr = String(dateBuf);
        
        DEBUG_PRINTF("Analytics: Time synced - %s (day %d)\n", 
                     currentDateStr.c_str(), currentDayOfWeek);
    } else {
        DEBUG_PRINTLN("Analytics: Time not available, using defaults");
        currentDayOfWeek = 0;
        currentDateStr = "unknown";
    }
    
    // Load saved data
    loadFromNVS();
    loadWeekHistory();
    
    // Reset timer
    midnightCheckTimer.reset();
    
    DEBUG_PRINTLN("Analytics: Ready!");
}

void Analytics::loop() {
    uint32_t now = millis();
    
    // Check if we need to fire a pending midnight callback (day changed at boot)
    if (pendingMidnightCallback) {
        DEBUG_PRINTLN("Analytics: Day changed while offline - pushing MIDNIGHT event");
        eventQueue.push(Event::MIDNIGHT);
        
        // Also call legacy callback
        if (midnightCallback) {
            midnightCallback();
        }
        pendingMidnightCallback = false;
    }
    
    // Check for midnight using IntervalTimer
    if (midnightCheckTimer.elapsed()) {
        checkMidnight();
    }
    
    // Auto-save every 5 minutes if stats changed
    if (statsChanged && (now - lastSaveTime >= 300000)) {
        saveToNVS();
        statsChanged = false;
        lastSaveTime = now;
    }
}

void Analytics::recordTaskCompleted() {
    todayStats.tasksCompleted++;
    todayStats.valid = true;
    statsChanged = true;
    DEBUG_PRINTF("Analytics: Task completed (total today: %d)\n", todayStats.tasksCompleted);
}

void Analytics::recordFocusSession(uint16_t minutes) {
    todayStats.focusMinutes += minutes;
    todayStats.sessionsCount++;
    todayStats.valid = true;
    statsChanged = true;
    DEBUG_PRINTF("Analytics: Focus session +%d min (total: %d min)\n", 
                 minutes, todayStats.focusMinutes);
}

void Analytics::recordBreakSession(uint16_t minutes) {
    todayStats.breakMinutes += minutes;
    todayStats.valid = true;
    statsChanged = true;
}

DailyStats Analytics::getTodayStats() {
    todayStats.dayOfWeek = currentDayOfWeek;
    return todayStats;
}

DailyStats Analytics::getDayStats(uint8_t daysAgo) {
    if (daysAgo == 0) return getTodayStats();
    if (daysAgo > 7) return {0, 0, 0, 0, 0, false};
    
    // Calculate which day of week
    int targetDay = (currentDayOfWeek - daysAgo + 7) % 7;
    return weekHistory[targetDay];
}

WeeklyReport Analytics::getWeeklyReport() {
    WeeklyReport report = {0, 0, 0, 0, 0, 0, 0, 0, false, 0};
    
    uint8_t maxTasks = 0;
    uint8_t mostProductiveDay = 0;
    
    // Include today
    if (todayStats.valid) {
        report.totalTasks += todayStats.tasksCompleted;
        report.totalFocusMinutes += todayStats.focusMinutes;
        report.totalBreakMinutes += todayStats.breakMinutes;
        report.totalSessions += todayStats.sessionsCount;
        report.daysRecorded++;
        
        if (todayStats.tasksCompleted > maxTasks) {
            maxTasks = todayStats.tasksCompleted;
            mostProductiveDay = currentDayOfWeek;
        }
    }
    
    // Include history (last 6 days)
    for (int i = 0; i < 7; i++) {
        if (i == currentDayOfWeek) continue;  // Skip today, already counted
        
        if (weekHistory[i].valid) {
            report.totalTasks += weekHistory[i].tasksCompleted;
            report.totalFocusMinutes += weekHistory[i].focusMinutes;
            report.totalBreakMinutes += weekHistory[i].breakMinutes;
            report.totalSessions += weekHistory[i].sessionsCount;
            report.daysRecorded++;
            
            if (weekHistory[i].tasksCompleted > maxTasks) {
                maxTasks = weekHistory[i].tasksCompleted;
                mostProductiveDay = weekHistory[i].dayOfWeek;
            }
        }
    }
    
    // Calculate averages
    if (report.daysRecorded > 0) {
        report.avgTasksPerDay = report.totalTasks / report.daysRecorded;
        report.avgFocusPerDay = report.totalFocusMinutes / report.daysRecorded;
    }
    
    report.mostProductiveDay = mostProductiveDay;
    report.mostProductiveTasks = maxTasks;
    report.hasFullWeek = (report.daysRecorded >= 7);
    
    return report;
}

bool Analytics::isTimeValid() {
    struct tm timeinfo;
    return getLocalTime(&timeinfo);
}

void Analytics::getCurrentTime(int& hour, int& minute) {
    struct tm timeinfo;
    // Use 10ms timeout instead of default 5000ms!
    if (getLocalTime(&timeinfo, 10)) {
        hour = timeinfo.tm_hour;
        minute = timeinfo.tm_min;
    } else {
        hour = 0;
        minute = 0;
    }
}

int Analytics::getCurrentDayOfWeek() {
    return currentDayOfWeek;
}

String Analytics::getCurrentDateString() {
    return currentDateStr;
}

void Analytics::loadFromNVS() {
    prefs.begin(NVS_NAMESPACE, true);  // Read-only
    
    String savedDate = prefs.getString("statsDate", "");
    
    if (savedDate == currentDateStr) {
        // Same day, load today's stats
        todayStats.tasksCompleted = prefs.getUChar("sTasks", 0);
        todayStats.focusMinutes = prefs.getUShort("sFocus", 0);
        todayStats.breakMinutes = prefs.getUShort("sBreak", 0);
        todayStats.sessionsCount = prefs.getUChar("sSessions", 0);
        todayStats.dayOfWeek = currentDayOfWeek;
        todayStats.valid = true;
        
        DEBUG_PRINTF("Analytics: Loaded today's stats - %d tasks, %d min focus\n",
                     todayStats.tasksCompleted, todayStats.focusMinutes);
    } else if (savedDate.length() > 0) {
        // Different day - need to save old data to history and reset
        DEBUG_PRINTLN("Analytics: New day detected at boot, archiving previous stats");
        
        // Mark that we need to call midnight callback (will be done in loop after callback is registered)
        pendingMidnightCallback = true;
        
        // Load the old day's data temporarily
        DailyStats oldStats;
        oldStats.tasksCompleted = prefs.getUChar("sTasks", 0);
        oldStats.focusMinutes = prefs.getUShort("sFocus", 0);
        oldStats.breakMinutes = prefs.getUShort("sBreak", 0);
        oldStats.sessionsCount = prefs.getUChar("sSessions", 0);
        oldStats.dayOfWeek = prefs.getUChar("sDayOfWeek", 0);
        oldStats.valid = true;
        
        prefs.end();
        
        // Save old stats to history
        saveDayToHistory(oldStats);
        
        // Reset today's stats
        todayStats = {(uint8_t)currentDayOfWeek, 0, 0, 0, 0, true};
        saveToNVS();
        return;
    }
    
    prefs.end();
}

void Analytics::saveToNVS() {
    prefs.begin(NVS_NAMESPACE, false);  // Read-write
    
    prefs.putString("statsDate", currentDateStr);
    prefs.putUChar("sDayOfWeek", currentDayOfWeek);
    prefs.putUChar("sTasks", todayStats.tasksCompleted);
    prefs.putUShort("sFocus", todayStats.focusMinutes);
    prefs.putUShort("sBreak", todayStats.breakMinutes);
    prefs.putUChar("sSessions", todayStats.sessionsCount);
    
    prefs.end();
    
    DEBUG_PRINTLN("Analytics: Saved to NVS");
}

void Analytics::saveDayToHistory(DailyStats& stats) {
    prefs.begin(NVS_NAMESPACE, false);
    
    // Save to the appropriate day slot
    char key[12];
    uint8_t day = stats.dayOfWeek;
    
    snprintf(key, sizeof(key), "h%dTasks", day);
    prefs.putUChar(key, stats.tasksCompleted);
    
    snprintf(key, sizeof(key), "h%dFocus", day);
    prefs.putUShort(key, stats.focusMinutes);
    
    snprintf(key, sizeof(key), "h%dBreak", day);
    prefs.putUShort(key, stats.breakMinutes);
    
    snprintf(key, sizeof(key), "h%dSess", day);
    prefs.putUChar(key, stats.sessionsCount);
    
    snprintf(key, sizeof(key), "h%dValid", day);
    prefs.putBool(key, true);
    
    prefs.end();
    
    // Also update RAM
    weekHistory[day] = stats;
    
    DEBUG_PRINTF("Analytics: Saved day %d to history\n", day);
}

void Analytics::loadWeekHistory() {
    prefs.begin(NVS_NAMESPACE, true);
    
    for (int i = 0; i < 7; i++) {
        char key[12];
        
        snprintf(key, sizeof(key), "h%dValid", i);
        weekHistory[i].valid = prefs.getBool(key, false);
        
        if (weekHistory[i].valid) {
            snprintf(key, sizeof(key), "h%dTasks", i);
            weekHistory[i].tasksCompleted = prefs.getUChar(key, 0);
            
            snprintf(key, sizeof(key), "h%dFocus", i);
            weekHistory[i].focusMinutes = prefs.getUShort(key, 0);
            
            snprintf(key, sizeof(key), "h%dBreak", i);
            weekHistory[i].breakMinutes = prefs.getUShort(key, 0);
            
            snprintf(key, sizeof(key), "h%dSess", i);
            weekHistory[i].sessionsCount = prefs.getUChar(key, 0);
            
            weekHistory[i].dayOfWeek = i;
        }
    }
    
    prefs.end();
    
    DEBUG_PRINTLN("Analytics: Week history loaded");
}

void Analytics::checkMidnight() {
    if (!isTimeValid()) return;
    
    struct tm timeinfo;
    getLocalTime(&timeinfo);
    
    char dateBuf[11];
    strftime(dateBuf, sizeof(dateBuf), "%Y-%m-%d", &timeinfo);
    String newDate = String(dateBuf);
    
    if (newDate != currentDateStr) {
        DEBUG_PRINTLN("Analytics: Midnight crossed - pushing MIDNIGHT event");
        
        // Push MIDNIGHT event to queue (processed by main loop)
        eventQueue.push(Event::MIDNIGHT);
        
        // Also call legacy callback for backwards compatibility
        if (midnightCallback) {
            midnightCallback();
        }
        
        performDailyReset();
        
        currentDateStr = newDate;
        currentDayOfWeek = timeinfo.tm_wday;
    }
}

void Analytics::performDailyReset() {
    // Save today's stats to history before resetting
    if (todayStats.valid) {
        todayStats.dayOfWeek = currentDayOfWeek;
        saveDayToHistory(todayStats);
    }
    
    // Reset for new day
    todayStats = {0, 0, 0, 0, 0, false};
    statsChanged = true;
    saveToNVS();
}

void Analytics::forceDailyReset() {
    DEBUG_PRINTLN("Analytics: Force daily reset");
    performDailyReset();
}

String Analytics::getDayName(uint8_t day) {
    const char* days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    if (day < 7) return String(days[day]);
    return "???";
}

#endif // ANALYTICS_H
