#ifndef MULTI_CORE_WEB_SERVER_H
#define MULTI_CORE_WEB_SERVER_H

/**
 * ============================================
 * Multi-Core Web Server for ESP32
 * ============================================
 * 
 * Runs WebSocket and HTTP server on Core 0 (separate from Arduino loop)
 * Uses FreeRTOS tasks and mutexes for thread-safe state access
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <DNSServer.h>
#include <ArduinoJson.h>
#include <functional>
#include <map>
#include "config.h"
#include "SystemState.h"
#include "WebContent.h"
#include "Analytics.h"

// Forward declaration
extern Analytics analytics;

// ============================================
// Thread-Safe Mutex Wrapper for SystemState
// ============================================
class ThreadSafeState {
public:
    ThreadSafeState(SystemState* state) : systemState(state) {
        mutex = xSemaphoreCreateMutex();
    }
    
    // Execute a function while holding the mutex
    template<typename Func>
    auto withLock(Func&& func) -> decltype(func()) {
        xSemaphoreTake(mutex, portMAX_DELAY);
        auto result = func();
        xSemaphoreGive(mutex);
        return result;
    }
    
    // Execute a void function while holding the mutex
    template<typename Func>
    void withLockVoid(Func&& func) {
        xSemaphoreTake(mutex, portMAX_DELAY);
        func();
        xSemaphoreGive(mutex);
    }
    
    // Try to acquire lock (non-blocking)
    bool tryLock(TickType_t timeout = 0) {
        return xSemaphoreTake(mutex, timeout) == pdTRUE;
    }
    
    void unlock() {
        xSemaphoreGive(mutex);
    }
    
    SystemState* getState() { return systemState; }
    
private:
    SystemState* systemState;
    SemaphoreHandle_t mutex;
};

// ============================================
// Action Handler Map (replaces if/else chain)
// ============================================
using ActionHandler = std::function<void(JsonDocument&)>;
using ActionMap = std::map<String, ActionHandler>;

// ============================================
// Multi-Core Web Server Handler
// ============================================
class MultiCoreWebServer {
public:
    MultiCoreWebServer(SystemState* state);
    ~MultiCoreWebServer();
    
    void begin();
    
    // These run on Core 1 (main loop) - just for status checks
    bool isConnected() { return wifiConnected; }
    bool isAPMode() { return !wifiConnected; }
    bool hasWebClient() { return webClientConnected; }
    String getIP();
    
    // Thread-safe broadcast (can be called from any core)
    void broadcastStatus();
    void broadcastPlant();
    void broadcastTasks();
    
private:
    // Servers
    WebServer server;
    WebSocketsServer webSocket;
    DNSServer dnsServer;
    
    // Thread-safe state wrapper
    ThreadSafeState* safeState;
    SystemState* systemState;
    
    // Status flags (atomic operations)
    volatile bool wifiConnected;
    volatile bool webClientConnected;
    volatile bool timeSynced;
    volatile bool running;
    
    // Timing
    uint32_t lastDNSProcess;
    uint32_t lastBroadcast;
    uint8_t lastMinute;
    
    // FreeRTOS task handle
    TaskHandle_t webTaskHandle;
    
    // Action handler map for WebSocket messages
    ActionMap actionHandlers;
    
    // Queue for broadcasts from other cores
    QueueHandle_t broadcastQueue;
    
    // Private methods
    void setupActionHandlers();
    void setupRoutes();
    bool connectWiFi();
    void setupAP();
    void syncTime();
    
    // Route handlers
    void handleRoot();
    void handleApiStatus();
    void handleApiTasks();
    void handleApiAddTask();
    void handleApiAction();
    void handleApiStats();
    void handleNotFound();
    
    // WebSocket handlers
    void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length);
    void handleWebSocketMessage(uint8_t num, uint8_t* payload, size_t length);
    
    // Static task function for FreeRTOS
    static void webTaskFunction(void* parameter);
    
    // Main loop running on Core 0
    void webLoop();
    
    // Time helpers
    bool isMidnight();
    struct tm getLocalTime();
};

// ============================================
// Broadcast Message Types (for queue)
// ============================================
enum BroadcastType {
    BROADCAST_STATUS = 1,
    BROADCAST_PLANT = 2,
    BROADCAST_TASKS = 3
};

// ============================================
// Implementation
// ============================================

MultiCoreWebServer::MultiCoreWebServer(SystemState* state)
    : server(80), webSocket(81) {
    systemState = state;
    safeState = new ThreadSafeState(state);
    wifiConnected = false;
    webClientConnected = false;
    timeSynced = false;
    running = false;
    lastDNSProcess = 0;
    lastBroadcast = 0;
    lastMinute = 255;
    webTaskHandle = nullptr;
    
    // Create broadcast queue (10 items max)
    broadcastQueue = xQueueCreate(10, sizeof(BroadcastType));
}

MultiCoreWebServer::~MultiCoreWebServer() {
    running = false;
    if (webTaskHandle) {
        vTaskDelete(webTaskHandle);
    }
    delete safeState;
}

void MultiCoreWebServer::begin() {
    DEBUG_PRINTLN("MultiCoreWebServer: Initializing...");
    
    // Setup action handlers map
    setupActionHandlers();
    
    // Try WiFi, fallback to AP
    if (connectWiFi()) {
        syncTime();
    } else {
        setupAP();
    }
    
    // Setup HTTP routes
    setupRoutes();
    
    // Setup WebSocket
    webSocket.begin();
    webSocket.onEvent([this](uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
        this->onWebSocketEvent(num, type, payload, length);
    });
    
    // Start HTTP server
    server.begin();
    
    DEBUG_PRINTLN("HTTP server started on port 80");
    DEBUG_PRINTLN("WebSocket server started on port 81");
    DEBUG_PRINTF("Access at: http://%s\n", getIP().c_str());
    
    // Start web task on Core 0
    running = true;
    xTaskCreatePinnedToCore(
        webTaskFunction,    // Task function
        "WebServerTask",    // Task name
        8192,               // Stack size (bytes)
        this,               // Parameter passed to task
        1,                  // Priority (1 = low, configMAX_PRIORITIES-1 = high)
        &webTaskHandle,     // Task handle
        0                   // Core 0 (Web server)
    );
    
    DEBUG_PRINTLN("Web server task started on Core 0");
}

// Static task function wrapper
void MultiCoreWebServer::webTaskFunction(void* parameter) {
    MultiCoreWebServer* self = static_cast<MultiCoreWebServer*>(parameter);
    self->webLoop();
}

// Main web loop running on Core 0
void MultiCoreWebServer::webLoop() {
    DEBUG_PRINTF("WebLoop running on Core %d\n", xPortGetCoreID());
    
    while (running) {
        // Process WebSocket (highest priority)
        webSocket.loop();
        
        // Process HTTP requests
        server.handleClient();
        
        // Process DNS for captive portal (throttled)
        if (!wifiConnected) {
            uint32_t now = millis();
            uint32_t dnsInterval = webClientConnected ? 10 : 50;
            if (now - lastDNSProcess >= dnsInterval) {
                dnsServer.processNextRequest();
                lastDNSProcess = now;
            }
        }
        
        // Process broadcast queue (from other cores)
        BroadcastType msgType;
        while (xQueueReceive(broadcastQueue, &msgType, 0) == pdTRUE) {
            switch (msgType) {
                case BROADCAST_STATUS:
                    broadcastStatusInternal();
                    break;
                case BROADCAST_PLANT:
                    broadcastPlantInternal();
                    break;
                case BROADCAST_TASKS:
                    broadcastTasksInternal();
                    break;
            }
        }
        
        // Check for midnight
        if (timeSynced && isMidnight()) {
            safeState->withLockVoid([this]() {
                if (systemState->getTaskCount() > 0 &&
                    systemState->getCompletedCount() < systemState->getTaskCount()) {
                    systemState->killPlant();
                    DEBUG_PRINTLN("Plant withered - tasks not completed!");
                }
            });
        }
        
        // Small yield to prevent watchdog
        vTaskDelay(1);  // 1 tick delay
    }
    
    vTaskDelete(NULL);
}

void MultiCoreWebServer::setupActionHandlers() {
    // Map action names to handler functions
    actionHandlers["getStatus"] = [this](JsonDocument& doc) {
        broadcastStatus();
    };
    
    actionHandlers["getTasks"] = [this](JsonDocument& doc) {
        broadcastTasks();
    };
    
    actionHandlers["water"] = [this](JsonDocument& doc) {
        safeState->withLockVoid([this]() {
            systemState->waterPlant();
        });
    };
    
    actionHandlers["kill"] = [this](JsonDocument& doc) {
        safeState->withLockVoid([this]() {
            systemState->killPlant();
        });
    };
    
    actionHandlers["pause"] = [this](JsonDocument& doc) {
        safeState->withLockVoid([this]() {
            systemState->pauseTimer();
        });
    };
    
    actionHandlers["resume"] = [this](JsonDocument& doc) {
        safeState->withLockVoid([this]() {
            systemState->resumeTimer();
        });
    };
    
    actionHandlers["addTask"] = [this](JsonDocument& doc) {
        const char* name = doc["task"]["name"] | "Untitled";
        uint16_t focus = doc["task"]["focusDuration"] | 25;
        uint16_t breakTime = doc["task"]["breakDuration"] | 5;
        safeState->withLockVoid([this, name, focus, breakTime]() {
            systemState->addTask(name, focus, breakTime);
        });
    };
    
    actionHandlers["startTask"] = [this](JsonDocument& doc) {
        uint32_t taskId = doc["taskId"];
        safeState->withLockVoid([this, taskId]() {
            systemState->startTask(taskId);
        });
    };
    
    actionHandlers["deleteTask"] = [this](JsonDocument& doc) {
        uint32_t taskId = doc["taskId"];
        safeState->withLockVoid([this, taskId]() {
            systemState->deleteTask(taskId);
        });
    };
    
    actionHandlers["toggleTask"] = [this](JsonDocument& doc) {
        uint32_t taskId = doc["taskId"];
        safeState->withLockVoid([this, taskId]() {
            systemState->toggleTaskComplete(taskId);
        });
    };
    
    actionHandlers["setGoal"] = [this](JsonDocument& doc) {
        uint8_t goal = doc["goal"];
        safeState->withLockVoid([this, goal]() {
            systemState->setDailyGoal(goal);
        });
    };
    
    actionHandlers["restartDay"] = [this](JsonDocument& doc) {
        safeState->withLockVoid([this]() {
            systemState->restartDay();
        });
    };
    
    actionHandlers["revive"] = [this](JsonDocument& doc) {
        safeState->withLockVoid([this]() {
            systemState->revivePlant();
        });
    };
}

void MultiCoreWebServer::handleWebSocketMessage(uint8_t num, uint8_t* payload, size_t length) {
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, payload, length);
    
    if (error) {
        DEBUG_PRINTLN("Failed to parse WebSocket message");
        return;
    }
    
    const char* action = doc["action"];
    if (!action) return;
    
    // Lookup action in map (O(log n) instead of O(n) if/else chain)
    auto it = actionHandlers.find(String(action));
    if (it != actionHandlers.end()) {
        it->second(doc);  // Call the handler
    } else {
        DEBUG_PRINTF("Unknown WebSocket action: %s\n", action);
    }
}

void MultiCoreWebServer::onWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_DISCONNECTED:
            DEBUG_PRINTF("WebSocket client #%u disconnected\n", num);
            break;
            
        case WStype_CONNECTED: {
            DEBUG_PRINTF("WebSocket client #%u connected\n", num);
            webClientConnected = true;
            
            // Debounce: wait 100ms for TCP to stabilize before sending data
            vTaskDelay(pdMS_TO_TICKS(100));
            
            // Send initial state (thread-safe)
            safeState->withLockVoid([this]() {
                broadcastTasksInternal();
                broadcastPlantInternal();
                broadcastStatusInternal();
            });
            break;
        }
        
        case WStype_TEXT:
            handleWebSocketMessage(num, payload, length);
            break;
            
        default:
            break;
    }
}

// Thread-safe broadcast methods (can be called from any core)
void MultiCoreWebServer::broadcastStatus() {
    BroadcastType msg = BROADCAST_STATUS;
    xQueueSend(broadcastQueue, &msg, 0);
}

void MultiCoreWebServer::broadcastPlant() {
    BroadcastType msg = BROADCAST_PLANT;
    xQueueSend(broadcastQueue, &msg, 0);
}

void MultiCoreWebServer::broadcastTasks() {
    BroadcastType msg = BROADCAST_TASKS;
    xQueueSend(broadcastQueue, &msg, 0);
}

// Internal broadcast methods (run on Core 0)
void MultiCoreWebServer::broadcastStatusInternal() {
    StaticJsonDocument<256> doc;
    doc["type"] = "status";
    
    safeState->withLockVoid([this, &doc]() {
        doc["state"] = systemState->getModeString();
        doc["timeLeft"] = systemState->getTimeLeft();
        doc["totalTime"] = systemState->getTotalTime();
        const char* taskName = systemState->getCurrentTaskName();
        doc["taskName"] = taskName ? taskName : (char*)nullptr;
    });
    
    String message;
    serializeJson(doc, message);
    webSocket.broadcastTXT(message);
}

void MultiCoreWebServer::broadcastPlantInternal() {
    StaticJsonDocument<192> doc;
    doc["type"] = "plant";
    
    safeState->withLockVoid([this, &doc]() {
        PlantInfo plant = systemState->getPlantInfo();
        doc["stage"] = plant.stage;
        doc["isWithered"] = plant.isWithered;
        doc["wateredCount"] = plant.wateredCount;
        doc["totalGoal"] = plant.totalGoal;
        doc["pendingWater"] = systemState->getPendingWaterCount();
        doc["dailyGoal"] = systemState->getDailyGoal();
    });
    
    String message;
    serializeJson(doc, message);
    webSocket.broadcastTXT(message);
}

void MultiCoreWebServer::broadcastTasksInternal() {
    StaticJsonDocument<1024> doc;
    doc["type"] = "tasks";
    JsonArray tasksArray = doc.createNestedArray("tasks");
    
    safeState->withLockVoid([this, &tasksArray]() {
        TaskInfo* tasks = systemState->getTasks();
        for (int i = 0; i < systemState->getTaskCount(); i++) {
            JsonObject task = tasksArray.createNestedObject();
            task["id"] = tasks[i].id;
            task["name"] = tasks[i].name;
            task["focusDuration"] = tasks[i].focusDuration;
            task["breakDuration"] = tasks[i].breakDuration;
            task["completed"] = tasks[i].completed;
            task["started"] = tasks[i].started;
        }
    });
    
    String message;
    serializeJson(doc, message);
    webSocket.broadcastTXT(message);
}

bool MultiCoreWebServer::connectWiFi() {
    bool usingAP = false;
    if(usingAP) return false;

    DEBUG_PRINTF("Connecting to WiFi: %s\n", WIFI_SSID);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        DEBUG_PRINT(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        DEBUG_PRINTLN("\nWiFi connected!");
        DEBUG_PRINTF("IP address: %s\n", WiFi.localIP().toString().c_str());
        return true;
    }

    DEBUG_PRINTLN("\nWiFi connection failed!");
    return false;
}

void MultiCoreWebServer::setupAP() {
    DEBUG_PRINTLN("Setting up Access Point...");
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    dnsServer.start(53, "*", WiFi.softAPIP());
    DEBUG_PRINTF("AP IP: %s\n", WiFi.softAPIP().toString().c_str());
}

String MultiCoreWebServer::getIP() {
    if (wifiConnected) {
        return WiFi.localIP().toString();
    }
    return WiFi.softAPIP().toString();
}

void MultiCoreWebServer::syncTime() {
    DEBUG_PRINTLN("Syncing time with NTP...");
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
    
    struct tm timeinfo;
    if (::getLocalTime(&timeinfo, 5000)) {
        timeSynced = true;
        DEBUG_PRINTF("Time synced: %02d:%02d:%02d\n",
                     timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    }
}

struct tm MultiCoreWebServer::getLocalTime() {
    struct tm timeinfo;
    ::getLocalTime(&timeinfo);
    return timeinfo;
}

bool MultiCoreWebServer::isMidnight() {
    if (!timeSynced) return false;
    
    struct tm timeinfo = getLocalTime();
    if (timeinfo.tm_hour == 0 && timeinfo.tm_min == 0 && lastMinute != 0) {
        lastMinute = 0;
        return true;
    }
    if (timeinfo.tm_min != lastMinute) {
        lastMinute = timeinfo.tm_min;
    }
    return false;
}

void MultiCoreWebServer::setupRoutes() {
    server.enableCORS(true);
    
    // Main page with optimized chunked sending
    server.on("/", HTTP_GET, [this]() { handleRoot(); });
    
    // Captive portal endpoints
    server.on("/hotspot-detect.html", HTTP_GET, [this]() {
        server.send(200, "text/html", "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
        webClientConnected = true;
    });
    server.on("/generate_204", HTTP_GET, [this]() {
        server.send(204, "", "");
        webClientConnected = true;
    });
    server.on("/gen_204", HTTP_GET, [this]() {
        server.send(204, "", "");
        webClientConnected = true;
    });
    server.on("/ncsi.txt", HTTP_GET, [this]() {
        server.send(200, "text/plain", "Microsoft NCSI");
    });
    server.on("/connecttest.txt", HTTP_GET, [this]() {
        server.sendHeader("Location", "http://192.168.4.1/");
        server.send(302, "text/plain", "");
    });
    server.on("/success.txt", HTTP_GET, [this]() {
        server.send(200, "text/plain", "success");
    });
    
    // API endpoints
    server.on("/api/status", HTTP_GET, [this]() { handleApiStatus(); });
    server.on("/api/tasks", HTTP_GET, [this]() { handleApiTasks(); });
    server.on("/api/tasks", HTTP_POST, [this]() { handleApiAddTask(); });
    server.on("/api/action", HTTP_POST, [this]() { handleApiAction(); });
    server.on("/api/stats", HTTP_GET, [this]() { handleApiStats(); });
    
    server.onNotFound([this]() { handleNotFound(); });
}

// Optimized handleRoot with larger chunks
void MultiCoreWebServer::handleRoot() {
    webClientConnected = true;
    
    DEBUG_PRINTF("handleRoot: Free heap = %d bytes\n", ESP.getFreeHeap());
    
    // Set all CORS headers upfront
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
    server.sendHeader("Cache-Control", "no-cache");
    
    size_t contentLen = strlen_P(INDEX_HTML);
    DEBUG_PRINTF("handleRoot: Sending %d bytes\n", contentLen);
    
    // Use larger chunks for faster transfer (4KB instead of 1KB)
    server.setContentLength(contentLen);
    server.send(200, "text/html", "");
    
    const char* ptr = INDEX_HTML;
    size_t remaining = contentLen;
    const size_t CHUNK_SIZE = 4096;  // 4KB chunks
    
    // Stack buffer for chunk (careful with stack size on Core 0)
    static char chunk[4097];  // Static to avoid stack overflow
    
    while (remaining > 0) {
        size_t toSend = (remaining > CHUNK_SIZE) ? CHUNK_SIZE : remaining;
        memcpy_P(chunk, ptr, toSend);
        chunk[toSend] = '\0';
        server.sendContent(chunk);
        ptr += toSend;
        remaining -= toSend;
        
        // Yield every chunk to prevent watchdog
        vTaskDelay(1);
    }
    
    DEBUG_PRINTLN("handleRoot: Done");
}

void MultiCoreWebServer::handleApiStatus() {
    DEBUG_PRINTLN("API: /api/status called");
    server.sendHeader("Access-Control-Allow-Origin", "*");
    
    StaticJsonDocument<512> doc;
    JsonObject root = doc.to<JsonObject>();
    
    safeState->withLockVoid([this, &root]() {
        root["state"] = systemState->getModeString();
        root["timeLeft"] = systemState->getTimeLeft();
        root["totalTime"] = systemState->getTotalTime();
        
        const char* taskName = systemState->getCurrentTaskName();
        root["taskName"] = taskName ? taskName : (char*)nullptr;
        
        JsonObject plantObj = root.createNestedObject("plant");
        PlantInfo plant = systemState->getPlantInfo();
        plantObj["stage"] = plant.stage;
        plantObj["isWithered"] = plant.isWithered;
        plantObj["canWater"] = plant.canWater;
        
        JsonObject statsObj = root.createNestedObject("stats");
        statsObj["completed"] = systemState->getCompletedCount();
        statsObj["total"] = systemState->getTaskCount();
    });
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void MultiCoreWebServer::handleApiTasks() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    
    StaticJsonDocument<1024> doc;
    JsonArray tasksArray = doc.createNestedArray("tasks");
    
    safeState->withLockVoid([this, &tasksArray]() {
        TaskInfo* tasks = systemState->getTasks();
        for (int i = 0; i < systemState->getTaskCount(); i++) {
            JsonObject task = tasksArray.createNestedObject();
            task["id"] = tasks[i].id;
            task["name"] = tasks[i].name;
            task["focusDuration"] = tasks[i].focusDuration;
            task["breakDuration"] = tasks[i].breakDuration;
            task["completed"] = tasks[i].completed;
            task["started"] = tasks[i].started;
        }
    });
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void MultiCoreWebServer::handleApiAddTask() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"No body\"}");
        return;
    }
    
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, server.arg("plain"))) {
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }
    
    const char* name = doc["name"] | "Untitled";
    uint16_t focus = doc["focusDuration"] | 25;
    uint16_t breakTime = doc["breakDuration"] | 5;
    
    bool success = false;
    safeState->withLockVoid([this, &success, name, focus, breakTime]() {
        success = systemState->addTask(name, focus, breakTime);
    });
    
    if (success) {
        broadcastTasks();
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        server.send(400, "application/json", "{\"error\":\"Task list full\"}");
    }
}

void MultiCoreWebServer::handleApiAction() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"No body\"}");
        return;
    }
    
    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, server.arg("plain"))) {
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }
    
    const char* action = doc["action"];
    if (!action) {
        server.send(400, "application/json", "{\"error\":\"No action\"}");
        return;
    }
    
    // Use action handlers map
    auto it = actionHandlers.find(String(action));
    if (it != actionHandlers.end()) {
        it->second(doc);
        
        char response[64];
        snprintf(response, sizeof(response), "{\"success\":true,\"action\":\"%s\"}", action);
        server.send(200, "application/json", response);
    } else {
        server.send(400, "application/json", "{\"error\":\"Unknown action\"}");
    }
}

void MultiCoreWebServer::handleApiStats() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    
    StaticJsonDocument<512> doc;
    
    DailyStats today = analytics.getTodayStats();
    doc["todayTasks"] = today.tasksCompleted;
    doc["todayFocus"] = today.focusMinutes;
    doc["todayBreak"] = today.breakMinutes;
    doc["todaySessions"] = today.sessionsCount;
    
    WeeklyReport week = analytics.getWeeklyReport();
    JsonObject weekly = doc.createNestedObject("weekly");
    weekly["totalTasks"] = week.totalTasks;
    weekly["totalFocus"] = week.totalFocusMinutes;
    weekly["avgTasksPerDay"] = week.avgTasksPerDay;
    weekly["daysRecorded"] = week.daysRecorded;
    
    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void MultiCoreWebServer::handleNotFound() {
    if (!wifiConnected) {
        server.sendHeader("Location", "http://192.168.4.1/");
        server.send(302, "text/plain", "");
        return;
    }
    server.send(404, "text/plain", "Not found");
}

#endif // MULTI_CORE_WEB_SERVER_H
