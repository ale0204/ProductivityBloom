#ifndef SYSTEM_STATE_H
#define SYSTEM_STATE_H

#include <Arduino.h>
#include <functional>
#include <Preferences.h>
#include "config.h"
#include "EventQueue.h"
#include "IntervalTimer.h"

// Global event queue declaration
EventQueue<32> eventQueue;

// ============================================
// System Modes
// ============================================
enum SystemMode {
    MODE_IDLE,
    MODE_FOCUSING,
    MODE_BREAK,
    MODE_PAUSED,
    MODE_WITHERED
};

// ============================================
// Task Structure (simplified)
// ============================================
struct TaskInfo {
    uint32_t id;
    char name[TASK_NAME_MAX_LENGTH];
    uint16_t focusDuration;   // minutes
    uint16_t breakDuration;   // minutes
    bool completed;
    bool started;
};

// ============================================
// Plant Info
// ============================================
struct PlantInfo {
    uint8_t stage;        // 0-3
    bool isWithered;
    bool canWater;        // Based on completed tasks
    uint8_t wateredCount; // Tasks confirmed through watering
    uint8_t totalGoal;    // Total tasks needed (currentSessionGoal or taskCount)
};

// ============================================
// System State (Single Source of Truth)
// ============================================
class SystemState {
public:
    SystemState();

    // Lifecycle
    void begin();
    void loop();

    // State queries
    SystemMode getMode() const { return currentMode; }
    const char* getModeString() const;
    uint32_t getTimeLeft() const { return timeLeftSeconds; }
    uint32_t getTotalTime() const { return totalTimeSeconds; }
    const char* getCurrentTaskName() const;
    PlantInfo getPlantInfo() const;

    // Task management
    bool addTask(const char* name, uint16_t focusMins, uint16_t breakMins);
    bool deleteTask(uint32_t id);
    bool startTask(uint32_t id);
    bool toggleTaskComplete(uint32_t id);
    void clearAllTasks();  // Clear all tasks (used by demo kill)
    TaskInfo* getTask(uint32_t id);
    uint8_t getTaskCount() const { return taskCount; }
    TaskInfo* getTasks() { return tasks; }

    // Actions (these trigger state changes + notifications)
    void startFocus(uint32_t taskId);
    void pauseTimer();
    void resumeTimer();
    void stopTimer();
    
    // MPU Flip control
    void selectTaskForFlip(uint32_t taskId);  // Prepare task, wait for flip to start
    void handleFlip(bool isFlipped);          // Called when MPU detects flip
    uint32_t getSelectedTaskId() const { return selectedTaskId; }
    bool hasSelectedTask() const { return selectedTaskId != 0; }
    
    // Flip confirmation (when user flips back)
    void confirmTaskComplete();     // User confirms task is done
    void cancelTaskComplete();      // User says flip was accidental
    bool isWaitingForConfirmation() const;
    
    void waterPlant();
    void killPlant();     // Demo mode
    void revivePlant();   // LDR trigger
    void resetForNewDay(); // Midnight reset
    void restartDay();     // Manual restart for testing

    // Event-based notifications (push to EventQueue)
    // Legacy callback support (deprecated, use EventQueue instead)
    typedef std::function<void()> StateCallback;
    void onStateChanged(StateCallback callback) { stateChangedCallback = callback; }
    void onTimerTick(StateCallback callback) { timerTickCallback = callback; }
    void onPlantChanged(StateCallback callback) { plantChangedCallback = callback; }
    
    // Check if goals were met (for midnight check)
    bool checkDailyGoalsMet() const;
    
    // Sensor handling (moved from main sketch)
    void handleLightSensor(int ldrValue);
    bool isReviving() const { return reviving; }

    // Daily goal management
    void setDailyGoal(uint8_t goalTasks);
    uint8_t getDailyGoal() const { return dailyGoal; }
    uint8_t getCompletedCount() const;
    uint8_t getPendingWaterCount() const { return pendingWater; }

private:
    // Core state
    SystemMode currentMode;
    uint32_t activeTaskId;
    uint32_t selectedTaskId;  // Task selected, waiting for flip to start
    TaskInfo tasks[MAX_TASKS];
    uint8_t taskCount;

    // Timer state
    uint32_t timerStartMillis;
    uint32_t timeLeftSeconds;
    uint32_t totalTimeSeconds;
    uint32_t pausedTimeLeft;
    uint32_t lastTickMillis;
    bool waitingForConfirmation;  // True when flip-back detected, waiting for user confirmation

    // Plant state
    uint8_t plantStage;       // 0-3
    bool plantWithered;
    uint8_t pendingWater;     // Tasks completed but not watered yet
    uint8_t wateredCount;     // Tasks confirmed through watering

    // Daily goal
    uint8_t dailyGoal;
    uint8_t currentSessionGoal;

    // Observer callbacks (legacy, prefer EventQueue)
    StateCallback stateChangedCallback;
    StateCallback timerTickCallback;
    StateCallback plantChangedCallback;
    
    // Sensor state (moved from main sketch)
    bool reviving;
    uint32_t reviveStartTime;
    
    // Previous state tracking (for event detection)
    uint8_t lastWateredCount;
    bool wasWithered;
    bool congratsShown;

    // Persistence
    Preferences prefs;
    void saveState();
    void loadState();
    void saveTasks();
    void loadTasks();

    // Internal helpers
    void setMode(SystemMode newMode);
    void updateTimer();
    void handleTimerComplete();
    void updatePlantState();
    void notifyStateChanged();
    void notifyTimerTick();
    void notifyPlantChanged();
    int8_t findTaskIndex(uint32_t id);
};

// ============================================
// Implementation
// ============================================

SystemState::SystemState() {
    currentMode = MODE_IDLE;
    activeTaskId = 0;
    selectedTaskId = 0;  // No task selected for flip
    taskCount = 0;

    timerStartMillis = 0;
    timeLeftSeconds = 0;
    totalTimeSeconds = 0;
    pausedTimeLeft = 0;
    lastTickMillis = 0;
    waitingForConfirmation = false;

    plantStage = 0;
    plantWithered = false;
    pendingWater = 0;
    wateredCount = 0;

    dailyGoal = 0;
    currentSessionGoal = 0;

    // Initialize tasks
    for (int i = 0; i < MAX_TASKS; i++) {
        tasks[i].id = 0;
        tasks[i].name[0] = '\0';
        tasks[i].focusDuration = 0;
        tasks[i].breakDuration = 0;
        tasks[i].completed = false;
        tasks[i].started = false;
    }

    stateChangedCallback = nullptr;
    timerTickCallback = nullptr;
    plantChangedCallback = nullptr;
    
    // Sensor state init
    reviving = false;
    reviveStartTime = 0;
    lastWateredCount = 0;
    wasWithered = false;
    congratsShown = false;
}

void SystemState::begin() {
    DEBUG_PRINTLN("SystemState: Initializing...");
    
    // Load saved state from NVS
    loadState();
    loadTasks();
    
    // If plant was withered, set mode accordingly
    if (plantWithered) {
        currentMode = MODE_WITHERED;
    }
    
    // Initialize tracking variables
    lastWateredCount = wateredCount;
    wasWithered = plantWithered;
    
    lastTickMillis = millis();
    DEBUG_PRINTLN("SystemState: Ready (state restored from NVS)");
}

void SystemState::loop() {
    // Timer update (every second)
    if (currentMode == MODE_FOCUSING || currentMode == MODE_BREAK) {
        uint32_t now = millis();
        if (now - lastTickMillis >= 1000) {
            lastTickMillis = now;
            updateTimer();
        }
    }
}

const char* SystemState::getModeString() const {
    switch (currentMode) {
        case MODE_IDLE: return "idle";
        case MODE_FOCUSING: return "focusing";
        case MODE_BREAK: return "break";
        case MODE_PAUSED: return "paused";
        case MODE_WITHERED: return "withered";
        default: return "unknown";
    }
}

const char* SystemState::getCurrentTaskName() const {
    if (activeTaskId == 0) return nullptr;

    for (int i = 0; i < taskCount; i++) {
        if (tasks[i].id == activeTaskId) {
            return tasks[i].name;
        }
    }
    return nullptr;
}

PlantInfo SystemState::getPlantInfo() const {
    PlantInfo info;
    info.stage = plantWithered ? 0 : plantStage;
    info.isWithered = plantWithered;
    info.canWater = (pendingWater > 0 && !plantWithered && plantStage < 3);
    info.wateredCount = wateredCount;
    info.totalGoal = (currentSessionGoal > 0) ? currentSessionGoal : taskCount;
    return info;
}

bool SystemState::addTask(const char* name, uint16_t focusMins, uint16_t breakMins) {
    if (taskCount >= MAX_TASKS) {
        DEBUG_PRINTLN("SystemState: Task list full");
        return false;
    }

    tasks[taskCount].id = millis();
    strncpy(tasks[taskCount].name, name, TASK_NAME_MAX_LENGTH - 1);
    tasks[taskCount].name[TASK_NAME_MAX_LENGTH - 1] = '\0';
    tasks[taskCount].focusDuration = focusMins;
    tasks[taskCount].breakDuration = breakMins;
    tasks[taskCount].completed = false;
    tasks[taskCount].started = false;

    taskCount++;
    DEBUG_PRINTF("SystemState: Task added - %s (%d/%d min)\n", name, focusMins, breakMins);

    saveTasks();  // Persist to NVS
    notifyStateChanged();
    return true;
}

bool SystemState::deleteTask(uint32_t id) {
    int8_t index = findTaskIndex(id);
    if (index < 0) return false;

    // If active task is deleted, stop timer
    if (tasks[index].id == activeTaskId) {
        stopTimer();
    }

    // If completed task is deleted, adjust pending water
    if (tasks[index].completed && pendingWater > 0) {
        pendingWater--;
    }

    // Shift remaining tasks
    for (int i = index; i < taskCount - 1; i++) {
        tasks[i] = tasks[i + 1];
    }
    taskCount--;

    DEBUG_PRINTF("SystemState: Task deleted, remaining: %d\n", taskCount);
    saveTasks();  // Persist to NVS
    notifyStateChanged();
    return true;
}

void SystemState::clearAllTasks() {
    // Stop any active timer
    if (currentMode == MODE_FOCUSING || currentMode == MODE_BREAK) {
        stopTimer();
    }
    
    // Clear all tasks
    taskCount = 0;
    for (int i = 0; i < MAX_TASKS; i++) {
        tasks[i] = TaskInfo();
    }
    
    // Reset related state
    activeTaskId = 0;
    selectedTaskId = 0;
    pendingWater = 0;
    wateredCount = 0;
    
    DEBUG_PRINTLN("SystemState: All tasks cleared");
    saveTasks();
    notifyStateChanged();
}

bool SystemState::startTask(uint32_t id) {
    int8_t index = findTaskIndex(id);
    if (index < 0) return false;

    // Start focus mode
    activeTaskId = id;
    tasks[index].started = true;

    setMode(MODE_FOCUSING);
    totalTimeSeconds = tasks[index].focusDuration * 60;
    timeLeftSeconds = totalTimeSeconds;
    timerStartMillis = millis();
    lastTickMillis = millis();

    DEBUG_PRINTF("SystemState: Started task - %s (%d sec)\n", tasks[index].name, totalTimeSeconds);
    notifyStateChanged();
    return true;
}

bool SystemState::toggleTaskComplete(uint32_t id) {
    int8_t index = findTaskIndex(id);
    if (index < 0) return false;

    if (!tasks[index].started) {
        DEBUG_PRINTLN("SystemState: Cannot complete task that hasn't started");
        return false;
    }

    bool wasCompleted = tasks[index].completed;
    bool wasActive = (tasks[index].id == activeTaskId);
    tasks[index].completed = !tasks[index].completed;

    if (tasks[index].completed && !wasCompleted) {
        // Task newly completed - add pending water
        pendingWater++;
        DEBUG_PRINTF("SystemState: Task completed - pendingWater: %d\n", pendingWater);
        
        // If this was the active task, stop the timer
        if (wasActive) {
            DEBUG_PRINTLN("SystemState: Active task completed - stopping timer");
            activeTaskId = 0;
            timeLeftSeconds = 0;
            totalTimeSeconds = 0;
            setMode(MODE_IDLE);
        }
    } else if (!tasks[index].completed && wasCompleted) {
        // Task uncompleted - remove pending water
        if (pendingWater > 0) pendingWater--;
    }

    saveTasks();  // Persist task state
    saveState();  // Persist pending water count
    updatePlantState();
    notifyStateChanged();
    notifyPlantChanged();
    return true;
}

TaskInfo* SystemState::getTask(uint32_t id) {
    int8_t index = findTaskIndex(id);
    if (index < 0) return nullptr;
    return &tasks[index];
}

void SystemState::startFocus(uint32_t taskId) {
    startTask(taskId);
}

void SystemState::pauseTimer() {
    if (currentMode == MODE_FOCUSING || currentMode == MODE_BREAK) {
        pausedTimeLeft = timeLeftSeconds;
        setMode(MODE_PAUSED);
        DEBUG_PRINTLN("SystemState: Timer paused");
    }
}

void SystemState::resumeTimer() {
    if (currentMode == MODE_PAUSED && pausedTimeLeft > 0) {
        timeLeftSeconds = pausedTimeLeft;
        timerStartMillis = millis();
        lastTickMillis = millis();
        setMode(MODE_FOCUSING);
        DEBUG_PRINTLN("SystemState: Timer resumed");
    }
}

void SystemState::stopTimer() {
    activeTaskId = 0;
    selectedTaskId = 0;  // Clear selected task too
    timeLeftSeconds = 0;
    totalTimeSeconds = 0;
    pausedTimeLeft = 0;
    setMode(MODE_IDLE);
    DEBUG_PRINTLN("SystemState: Timer stopped");
}

// ============================================
// MPU Flip Control
// ============================================

void SystemState::selectTaskForFlip(uint32_t taskId) {
    int8_t index = findTaskIndex(taskId);
    if (index < 0) {
        DEBUG_PRINTLN("SystemState: Invalid task ID for flip selection");
        return;
    }
    
    if (tasks[index].completed) {
        DEBUG_PRINTLN("SystemState: Cannot select completed task");
        return;
    }
    
    selectedTaskId = taskId;
    tasks[index].started = true;  // Mark as started (shows in UI)
    
    DEBUG_PRINTF("SystemState: Task '%s' selected - flip to start timer!\n", tasks[index].name);
    saveTasks();
    notifyStateChanged();
}

void SystemState::handleFlip(bool isFlipped) {
    DEBUG_PRINTF("SystemState: handleFlip called, isFlipped=%d, mode=%d, selectedTask=%lu, activeTask=%lu\n",
                 isFlipped, currentMode, selectedTaskId, activeTaskId);
    
    // Logic: User flips cube down (OLED hidden) to START focusing
    //        User flips cube up (OLED visible) to PAUSE/STOP
    bool cubeReadyToStart = isFlipped;   // OLED facing down = focus mode
    bool cubePaused = !isFlipped;        // OLED facing up = paused/idle
    
    // Case 1: We have a selected task waiting to start, and user flipped OLED down
    if (selectedTaskId != 0 && currentMode == MODE_IDLE && cubeReadyToStart) {
        // Start the timer for selected task
        int8_t index = findTaskIndex(selectedTaskId);
        if (index >= 0) {
            activeTaskId = selectedTaskId;
            selectedTaskId = 0;  // Clear selection
            
            setMode(MODE_FOCUSING);
            totalTimeSeconds = tasks[index].focusDuration * 60;
            timeLeftSeconds = totalTimeSeconds;
            timerStartMillis = millis();
            lastTickMillis = millis();
            
            DEBUG_PRINTF("SystemState: FLIP START - Task '%s' timer started (%d sec)\n", 
                        tasks[index].name, totalTimeSeconds);
            
            // Push event for WebSocket broadcast
            eventQueue.push(Event::WEB_BROADCAST);
            notifyStateChanged();
        }
        return;
    }
    
    // Case 2: Timer is running (focusing), and user flipped OLED up (paused)
    if (currentMode == MODE_FOCUSING && cubePaused) {
        // Pause timer and wait for confirmation from web interface
        int8_t index = findTaskIndex(activeTaskId);
        if (index >= 0) {
            DEBUG_PRINTF("SystemState: FLIP PAUSE - Waiting for confirmation for '%s'\n", tasks[index].name);
            
            // Pause the timer (save remaining time)
            pausedTimeLeft = timeLeftSeconds;
            waitingForConfirmation = true;
            setMode(MODE_PAUSED);
            
            // Push event for WebSocket to show confirmation dialog
            eventQueue.push(Event::FLIP_CONFIRM_NEEDED);
            eventQueue.push(Event::WEB_BROADCAST);
        }
        return;
    }
    
    // Case 3: Timer is paused, flip OLED down resumes (user said it was accidental)
    if (currentMode == MODE_PAUSED && cubeReadyToStart) {
        if (waitingForConfirmation) {
            // User flipped back - they want to continue
            waitingForConfirmation = false;
            resumeTimer();
            DEBUG_PRINTLN("SystemState: FLIP RESUME - User continued, resuming timer");
            eventQueue.push(Event::FLIP_RESUMED);
            eventQueue.push(Event::WEB_BROADCAST);
        } else {
            resumeTimer();
            DEBUG_PRINTLN("SystemState: FLIP RESUME - Timer resumed");
        }
        return;
    }
}

// Called when user confirms task completion from web interface
void SystemState::confirmTaskComplete() {
    if (!waitingForConfirmation) {
        DEBUG_PRINTLN("SystemState: confirmTaskComplete called but not waiting for confirmation");
        return;
    }
    
    int8_t index = findTaskIndex(activeTaskId);
    if (index >= 0) {
        DEBUG_PRINTF("SystemState: Task '%s' confirmed complete!\n", tasks[index].name);
        
        // Mark task as completed (this adds pending water)
        tasks[index].completed = true;
        pendingWater++;
        
        // Stop timer
        uint32_t completedTaskId = activeTaskId;
        activeTaskId = 0;
        timeLeftSeconds = 0;
        totalTimeSeconds = 0;
        pausedTimeLeft = 0;
        waitingForConfirmation = false;
        setMode(MODE_IDLE);
        
        saveTasks();
        saveState();
        updatePlantState();
        notifyStateChanged();
        notifyPlantChanged();
        
        // Push events
        eventQueue.push(Event::WEB_BROADCAST);
    }
}

// Called when user says flip was accidental - they need to flip back to resume
void SystemState::cancelTaskComplete() {
    if (!waitingForConfirmation) {
        DEBUG_PRINTLN("SystemState: cancelTaskComplete called but not waiting for confirmation");
        return;
    }
    
    DEBUG_PRINTLN("SystemState: Task completion cancelled - waiting for flip back to resume");
    // Keep in paused state, user needs to flip back to resume
    // waitingForConfirmation stays true until they flip back
    eventQueue.push(Event::FLIP_CANCELLED);
    eventQueue.push(Event::WEB_BROADCAST);
}

bool SystemState::isWaitingForConfirmation() const {
    return waitingForConfirmation;
}

void SystemState::waterPlant() {
    if (plantWithered) {
        DEBUG_PRINTLN("SystemState: Cannot water withered plant");
        return;
    }

    if (pendingWater <= 0) {
        DEBUG_PRINTLN("SystemState: No pending water available");
        return;
    }

    if (plantStage >= 3) {
        DEBUG_PRINTLN("SystemState: Plant already fully grown");
        return;
    }

    // Consume one water
    pendingWater--;
    wateredCount++;

    // Calculate target stage based on progress
    uint8_t goalsToComplete = currentSessionGoal > 0 ? currentSessionGoal : taskCount;

    if (goalsToComplete == 0) {
        plantStage = 0;
    } else if (wateredCount >= goalsToComplete) {
        plantStage = 3; // Fully bloomed
    } else if (wateredCount >= 2) {
        plantStage = 2; // Growing
    } else if (wateredCount >= 1) {
        plantStage = 1; // Sprout
    } else {
        plantStage = 0; // Seed
    }

    DEBUG_PRINTF("SystemState: Plant watered - stage: %d, watered: %d/%d\n",
                 plantStage, wateredCount, goalsToComplete);

    saveState();  // Persist plant progress
    updatePlantState();
    notifyPlantChanged();
}

void SystemState::killPlant() {
    plantWithered = true;
    setMode(MODE_WITHERED);
    saveState();  // Persist withered state
    DEBUG_PRINTLN("SystemState: Plant withered (demo)");
    notifyPlantChanged();
}

void SystemState::revivePlant() {
    if (plantWithered) {
        plantWithered = false;
        plantStage = 0;
        pendingWater = 0;
        wateredCount = 0;
        setMode(MODE_IDLE);
        saveState();  // Persist revived state
        DEBUG_PRINTLN("SystemState: Plant revived!");
        notifyPlantChanged();
    }
}

void SystemState::resetForNewDay() {
    // Reset plant progress for new day (only if plant is alive)
    if (!plantWithered) {
        plantStage = 0;
        pendingWater = 0;
        wateredCount = 0;
        currentSessionGoal = dailyGoal;  // Reset session goal to daily goal
        
        DEBUG_PRINTLN("SystemState: Reset for new day - plant progress cleared");
        saveState();
        notifyPlantChanged();
    }
    
    // Clear all tasks for new day
    taskCount = 0;
    for (int i = 0; i < MAX_TASKS; i++) {
        tasks[i] = TaskInfo();
    }
    saveTasks();
    
    DEBUG_PRINTLN("SystemState: All tasks cleared for new day");
    notifyStateChanged();
}

void SystemState::restartDay() {
    // Full reset - plant, tasks, and goals
    plantWithered = false;
    plantStage = 0;
    pendingWater = 0;
    wateredCount = 0;
    dailyGoal = 0;
    currentSessionGoal = 0;
    
    // Clear all tasks
    taskCount = 0;
    for (int i = 0; i < MAX_TASKS; i++) {
        tasks[i] = TaskInfo();
    }
    
    // Reset mode
    currentMode = MODE_IDLE;
    activeTaskId = 0;
    
    // Save everything
    saveState();
    saveTasks();
    
    DEBUG_PRINTLN("SystemState: Day restarted - full reset!");
    notifyStateChanged();
    notifyPlantChanged();
}

void SystemState::setDailyGoal(uint8_t goalTasks) {
    uint8_t previousGoal = dailyGoal;
    dailyGoal = goalTasks;

    if (previousGoal > 0) {
        currentSessionGoal = goalTasks - previousGoal;
    } else {
        currentSessionGoal = goalTasks;
    }

    // Reset plant for new goal
    plantStage = 0;
    pendingWater = 0;
    wateredCount = 0;

    DEBUG_PRINTF("SystemState: Daily goal set - %d tasks (new: %d)\n",
                 dailyGoal, currentSessionGoal);

    saveState();  // Persist goal and plant reset
    updatePlantState();
    notifyStateChanged();
    notifyPlantChanged();
}

uint8_t SystemState::getCompletedCount() const {
    uint8_t count = 0;
    for (int i = 0; i < taskCount; i++) {
        if (tasks[i].completed) count++;
    }
    return count;
}

// ============================================
// Private Methods
// ============================================

void SystemState::setMode(SystemMode newMode) {
    if (currentMode != newMode) {
        currentMode = newMode;
        notifyStateChanged();
    }
}

void SystemState::updateTimer() {
    if (timeLeftSeconds > 0) {
        timeLeftSeconds--;
        notifyTimerTick();
    }
    if (timeLeftSeconds == 0) {
        handleTimerComplete();
    }
}

void SystemState::handleTimerComplete() {
    TaskInfo* activeTask = getTask(activeTaskId);

    if (currentMode == MODE_FOCUSING && activeTask != nullptr) {
        // Focus complete -> start break
        DEBUG_PRINTLN("SystemState: Focus complete, starting break");

        setMode(MODE_BREAK);
        totalTimeSeconds = activeTask->breakDuration * 60;
        timeLeftSeconds = totalTimeSeconds;
        timerStartMillis = millis();
        lastTickMillis = millis();

    } else if (currentMode == MODE_BREAK && activeTask != nullptr) {
        // Break complete -> restart focus
        DEBUG_PRINTLN("SystemState: Break complete, restarting focus");

        setMode(MODE_FOCUSING);
        totalTimeSeconds = activeTask->focusDuration * 60;
        timeLeftSeconds = totalTimeSeconds;
        timerStartMillis = millis();
        lastTickMillis = millis();

    } else {
        // No active task, go idle
        stopTimer();
    }
}

void SystemState::updatePlantState() {
    // Plant state is calculated in waterPlant()
    // This method can be extended for additional logic
}

int8_t SystemState::findTaskIndex(uint32_t id) {
    for (int i = 0; i < taskCount; i++) {
        if (tasks[i].id == id) {
            return i;
        }
    }
    return -1;
}

// ============================================
// Persistence Functions (NVS Storage)
// ============================================

void SystemState::saveState() {
    prefs.begin("bloomState", false);  // Read-write mode
    
    prefs.putUChar("plantStage", plantStage);
    prefs.putBool("plantWithered", plantWithered);
    prefs.putUChar("pendingWater", pendingWater);
    prefs.putUChar("wateredCount", wateredCount);
    prefs.putUChar("dailyGoal", dailyGoal);
    prefs.putUChar("sessionGoal", currentSessionGoal);
    prefs.putUChar("taskCount", taskCount);
    
    prefs.end();
    DEBUG_PRINTLN("SystemState: State saved to NVS");
}

void SystemState::loadState() {
    prefs.begin("bloomState", true);  // Read-only mode
    
    plantStage = prefs.getUChar("plantStage", 0);
    plantWithered = prefs.getBool("plantWithered", false);
    pendingWater = prefs.getUChar("pendingWater", 0);
    wateredCount = prefs.getUChar("wateredCount", 0);
    dailyGoal = prefs.getUChar("dailyGoal", 0);
    currentSessionGoal = prefs.getUChar("sessionGoal", 0);
    taskCount = prefs.getUChar("taskCount", 0);
    
    prefs.end();
    
    DEBUG_PRINTF("SystemState: Loaded - stage:%d, withered:%d, goal:%d, tasks:%d\n",
                 plantStage, plantWithered, dailyGoal, taskCount);
}

void SystemState::saveTasks() {
    prefs.begin("bloomTasks", false);
    
    prefs.putUChar("count", taskCount);
    
    for (int i = 0; i < taskCount && i < MAX_TASKS; i++) {
        char key[12];
        
        snprintf(key, sizeof(key), "t%d_id", i);
        prefs.putUInt(key, tasks[i].id);
        
        snprintf(key, sizeof(key), "t%d_name", i);
        prefs.putString(key, tasks[i].name);
        
        snprintf(key, sizeof(key), "t%d_focus", i);
        prefs.putUShort(key, tasks[i].focusDuration);
        
        snprintf(key, sizeof(key), "t%d_break", i);
        prefs.putUShort(key, tasks[i].breakDuration);
        
        snprintf(key, sizeof(key), "t%d_done", i);
        prefs.putBool(key, tasks[i].completed);
        
        snprintf(key, sizeof(key), "t%d_start", i);
        prefs.putBool(key, tasks[i].started);
    }
    
    prefs.end();
    DEBUG_PRINTF("SystemState: Saved %d tasks to NVS\n", taskCount);
}

void SystemState::loadTasks() {
    prefs.begin("bloomTasks", true);
    
    taskCount = prefs.getUChar("count", 0);
    if (taskCount > MAX_TASKS) taskCount = MAX_TASKS;
    
    for (int i = 0; i < taskCount; i++) {
        char key[12];
        
        snprintf(key, sizeof(key), "t%d_id", i);
        tasks[i].id = prefs.getUInt(key, 0);
        
        snprintf(key, sizeof(key), "t%d_name", i);
        String name = prefs.getString(key, "");
        strncpy(tasks[i].name, name.c_str(), TASK_NAME_MAX_LENGTH - 1);
        tasks[i].name[TASK_NAME_MAX_LENGTH - 1] = '\0';
        
        snprintf(key, sizeof(key), "t%d_focus", i);
        tasks[i].focusDuration = prefs.getUShort(key, 25);
        
        snprintf(key, sizeof(key), "t%d_break", i);
        tasks[i].breakDuration = prefs.getUShort(key, 5);
        
        snprintf(key, sizeof(key), "t%d_done", i);
        tasks[i].completed = prefs.getBool(key, false);
        
        snprintf(key, sizeof(key), "t%d_start", i);
        tasks[i].started = prefs.getBool(key, false);
    }
    
    prefs.end();
    DEBUG_PRINTF("SystemState: Loaded %d tasks from NVS\n", taskCount);
}

// ============================================
// New Methods - Goal Checking & Sensor Handling
// ============================================

bool SystemState::checkDailyGoalsMet() const {
    PlantInfo plant = getPlantInfo();
    
    // No goal set = no failure
    if (plant.totalGoal == 0) return true;
    
    // Check if watered count meets goal
    return plant.wateredCount >= plant.totalGoal;
}

void SystemState::handleLightSensor(int ldrValue) {
    // Only process if plant is withered
    if (!plantWithered) {
        reviving = false;
        return;
    }
    
    if (ldrValue >= LDR_REVIVE_THRESHOLD) {
        if (!reviving) {
            reviving = true;
            reviveStartTime = millis();
            DEBUG_PRINTLN("Light detected, starting revive...");
        } else if (millis() - reviveStartTime >= LDR_REVIVE_DURATION) {
            // Revive successful!
            revivePlant();  // This calls notifyPlantChanged which pushes PLANT_REVIVED
            reviving = false;
            DEBUG_PRINTLN("Plant revived by light!");
        }
    } else {
        // Light removed, reset
        reviving = false;
    }
}

// ============================================
// Enhanced notify methods with EventQueue
// ============================================

void SystemState::notifyStateChanged() {
    // Push to event queue
    eventQueue.push(Event::STATE_CHANGED);
    eventQueue.push(Event::OLED_REFRESH);
    
    // Legacy callback support
    if (stateChangedCallback) {
        stateChangedCallback();
    }
}

void SystemState::notifyTimerTick() {
    // Push to event queue
    eventQueue.push(Event::TIMER_TICK);
    eventQueue.push(Event::OLED_REFRESH);
    
    // Legacy callback support
    if (timerTickCallback) {
        timerTickCallback();
    }
}

void SystemState::notifyPlantChanged() {
    PlantInfo plant = getPlantInfo();
    
    // Detect revive (was withered, now alive)
    if (wasWithered && !plant.isWithered) {
        eventQueue.push(Event::PLANT_REVIVED);
        DEBUG_PRINTLN("Event: PLANT_REVIVED");
    }
    wasWithered = plant.isWithered;
    
    // Detect watering
    if (plant.wateredCount > lastWateredCount && !plant.isWithered) {
        eventQueue.push(Event::PLANT_WATERED);
        DEBUG_PRINTLN("Event: PLANT_WATERED");
    }
    lastWateredCount = plant.wateredCount;
    
    // Detect bloom
    if (plant.stage == 3 && !plant.isWithered && !congratsShown) {
        eventQueue.push(Event::PLANT_BLOOMED);
        congratsShown = true;
        DEBUG_PRINTLN("Event: PLANT_BLOOMED");
    }
    
    // Reset congratsShown if plant reset
    if (plant.stage < 3 || plant.isWithered) {
        congratsShown = false;
    }
    
    // Always refresh display and broadcast
    eventQueue.push(Event::OLED_REFRESH);
    eventQueue.push(Event::WEB_BROADCAST);
    
    // Legacy callback support
    if (plantChangedCallback) {
        plantChangedCallback();
    }
}

#endif // SYSTEM_STATE_H
