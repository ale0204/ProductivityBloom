#ifndef WEB_SERVER_HANDLER_H
#define WEB_SERVER_HANDLER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <DNSServer.h>  // For Captive Portal
#include <ArduinoJson.h>
#include <time.h>
#include <sys/time.h>   // For settimeofday
#include "config.h"
#include "SystemState.h"
#include "WebContent.h"  // Embedded HTML/CSS/JS
#include "Analytics.h"   // Weekly stats

// Forward declaration
extern Analytics analytics;

// DNS server for captive portal
const byte DNS_PORT = 53;

// ============================================
// Web Server Handler Class (Synchronous Version)
// Uses standard WebServer + WebSocketsServer
// Thread-safe for ESP32
// ============================================
class WebServerHandler {
public:
    WebServerHandler(SystemState* state);

    void begin();
    void loop();

    // WiFi
    bool connectWiFi();
    void setupAP();
    String getIP();
    bool isConnected();
    bool isAPMode() { return !wifiConnected; }
    bool hasWebClient() { return webClientConnected; }  // Someone accessed the web interface

    // NTP
    void syncTime();
    bool isTimeSynced();
    struct tm getLocalTime();
    bool isMidnight();

    // WebSocket broadcasts (called by SystemState callbacks)
    void broadcastStatus();
    void broadcastPlant();
    void broadcastTasks();
    void broadcastRevive();  // Special message when plant is revived

private:
    WebServer server;
    WebSocketsServer webSocket;
    DNSServer dnsServer;  // For captive portal

    SystemState* systemState;  // Single source of truth

    bool wifiConnected;
    bool webClientConnected;  // True when someone accessed the web interface
    bool timeSynced;
    uint8_t lastMinute;
    uint32_t lastBroadcast;

    // Route handlers
    void setupRoutes();
    void handleRoot();
    void handleApiStatus();
    void handleApiTasks();
    void handleApiAddTask();
    void handleApiDeleteTask();
    void handleApiStartTask();
    void handleApiToggleTask();
    void handleApiAction();
    void handleApiStats();
    void handleNotFound();

    // WebSocket handlers
    void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length);
    void handleWebSocketMessage(uint8_t num, uint8_t* payload, size_t length);
};

// ============================================
// Implementation
// ============================================

WebServerHandler::WebServerHandler(SystemState* state)
    : server(80), webSocket(81) {
    systemState = state;
    wifiConnected = false;
    webClientConnected = false;
    timeSynced = false;
    lastMinute = 255;
    lastBroadcast = 0;
}

void WebServerHandler::begin() {
    // No filesystem needed - using embedded content
    DEBUG_PRINTLN("Using embedded web content");

    // Connect to WiFi
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
}

void WebServerHandler::loop() {
    // Handle WebSocket first (critical for real-time updates)
    webSocket.loop();
    
    // Handle HTTP requests
    server.handleClient();
    
    // Process DNS requests frequently in AP mode (critical for captive portal!)
    // DNS must respond quickly or phone will timeout and show "no internet"
    if (!wifiConnected) {
        // Process DNS on every loop iteration for fast response
        dnsServer.processNextRequest();
    }

    // Periodic status broadcast
    if (millis() - lastBroadcast >= WEBSOCKET_UPDATE_INTERVAL) {
        lastBroadcast = millis();
        // Auto-broadcast handled by callbacks now
    }

    // Check for midnight (daily reset)
    if (timeSynced && isMidnight()) {
        DEBUG_PRINTLN("Midnight check triggered!");

        if (systemState->getTaskCount() > 0 &&
            systemState->getCompletedCount() < systemState->getTaskCount()) {
            systemState->killPlant();
            DEBUG_PRINTLN("Plant withered - tasks not completed!");
        }
    }
}

bool WebServerHandler::connectWiFi() {
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

void WebServerHandler::setupAP() {
    DEBUG_PRINTLN("Setting up Access Point with Captive Portal...");
    WiFi.mode(WIFI_AP);
    
    // Configure AP with specific settings for better captive portal detection
    WiFi.softAPConfig(
        IPAddress(192, 168, 4, 1),    // AP IP
        IPAddress(192, 168, 4, 1),    // Gateway (same as AP)
        IPAddress(255, 255, 255, 0)   // Subnet
    );
    
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    
    // Start DNS server for captive portal - redirects ALL domains to ESP32
    // The "*" pattern matches any domain name query
    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
    
    DEBUG_PRINTF("AP IP: %s\n", WiFi.softAPIP().toString().c_str());
    DEBUG_PRINTLN("Captive Portal active - all domains redirect to ESP32");
    DEBUG_PRINTLN("Connect to WiFi 'ProductivityBloom' password 'bloom2024'");
}

String WebServerHandler::getIP() {
    if (wifiConnected) {
        return WiFi.localIP().toString();
    }
    return WiFi.softAPIP().toString();
}

bool WebServerHandler::isConnected() {
    return wifiConnected && WiFi.status() == WL_CONNECTED;
}

void WebServerHandler::syncTime() {
    DEBUG_PRINTLN("Syncing time with NTP...");
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);

    struct tm timeinfo;
    if (::getLocalTime(&timeinfo, 5000)) {
        timeSynced = true;
        DEBUG_PRINTF("Time synced: %02d:%02d:%02d\n",
                     timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    } else {
        DEBUG_PRINTLN("Failed to sync time");
    }
}

bool WebServerHandler::isTimeSynced() {
    return timeSynced;
}

struct tm WebServerHandler::getLocalTime() {
    struct tm timeinfo;
    ::getLocalTime(&timeinfo);
    return timeinfo;
}

bool WebServerHandler::isMidnight() {
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

void WebServerHandler::setupRoutes() {
    // CORS headers for all responses
    server.enableCORS(true);

    // Serve embedded HTML (includes CSS and JS inline)
    server.on("/", HTTP_GET, [this]() { handleRoot(); });
    
    // ============================================
    // CAPTIVE PORTAL DETECTION ENDPOINTS
    // These trick the phone into thinking it has internet
    // so it doesn't show the annoying captive portal popup
    // ============================================
    
    // Apple iOS/macOS - redirect to main page so the captive portal shows our app
    // After user interacts, we'll respond with Success to keep connection stable
    server.on("/hotspot-detect.html", HTTP_GET, [this]() { 
        // If user already connected to web interface, return Success to keep connection
        if (webClientConnected) {
            server.send(200, "text/html", "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
        } else {
            // First time - redirect to our app so it opens in captive portal popup
            server.sendHeader("Location", "http://192.168.4.1/");
            server.send(302, "text/html", "");
        }
    });
    server.on("/library/test/success.html", HTTP_GET, [this]() { 
        if (webClientConnected) {
            server.send(200, "text/html", "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
        } else {
            server.sendHeader("Location", "http://192.168.4.1/");
            server.send(302, "text/html", "");
        }
    });
    server.on("/captive-portal/api/v1/status", HTTP_GET, [this]() { 
        server.send(200, "application/json", "{\"success\":true}");
        webClientConnected = true;
    });
    
    // Android - MUST return exactly 204 with empty body
    // Google connectivity check endpoints
    server.on("/generate_204", HTTP_GET, [this]() { 
        server.send(204, "", "");
        webClientConnected = true;
    });
    server.on("/gen_204", HTTP_GET, [this]() { 
        server.send(204, "", "");
        webClientConnected = true;
    });
    // Additional Android endpoints
    server.on("/mobile/status.php", HTTP_GET, [this]() { 
        server.send(204, "", "");
        webClientConnected = true;
    });
    server.on("/connectivity-check.html", HTTP_GET, [this]() { 
        server.send(204, "", "");
        webClientConnected = true;
    });
    server.on("/check_network_status.txt", HTTP_GET, [this]() { 
        server.send(204, "", "");
        webClientConnected = true;
    });
    
    // Windows NCSI - return expected responses
    server.on("/ncsi.txt", HTTP_GET, [this]() { 
        server.send(200, "text/plain", "Microsoft NCSI");
        webClientConnected = true;
    });
    server.on("/connecttest.txt", HTTP_GET, [this]() { 
        server.send(200, "text/plain", "Microsoft Connect Test");
        webClientConnected = true;
    });
    server.on("/redirect", HTTP_GET, [this]() { 
        server.send(200, "text/plain", "Microsoft NCSI");
        webClientConnected = true;
    });
    
    // Firefox
    server.on("/success.txt", HTTP_GET, [this]() { 
        server.send(200, "text/plain", "success");
        webClientConnected = true;
    });
    server.on("/canonical.html", HTTP_GET, [this]() { 
        server.send(200, "text/html", "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
        webClientConnected = true;
    });

    // API: Get status
    server.on("/api/status", HTTP_GET, [this]() { handleApiStatus(); });

    // API: Get tasks
    server.on("/api/tasks", HTTP_GET, [this]() { handleApiTasks(); });

    // API: Add task (POST)
    server.on("/api/tasks", HTTP_POST, [this]() { handleApiAddTask(); });

    // API: Action (POST) - water, kill, pause, resume, setGoal
    server.on("/api/action", HTTP_POST, [this]() { handleApiAction(); });

    // API: Stats
    server.on("/api/stats", HTTP_GET, [this]() { handleApiStats(); });

    // 404 handler
    server.onNotFound([this]() { handleNotFound(); });
}

void WebServerHandler::handleRoot() {
    // Mark that someone accessed the web interface
    webClientConnected = true;
    
    // Debug: show free heap
    DEBUG_PRINTF("handleRoot: Free heap = %d bytes\n", ESP.getFreeHeap());
    
    // IMPORTANT: No-cache headers to prevent Safari caching issues
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "0");
    
    // Add CORS headers for WebSocket connectivity in AP mode
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
    
    // Serve embedded HTML from WebContent.h (PROGMEM)
    // Use chunked transfer for large content
    size_t contentLen = strlen_P(INDEX_HTML);
    DEBUG_PRINTF("handleRoot: Sending %d bytes\n", contentLen);
    
    server.setContentLength(contentLen);
    server.send(200, "text/html", "");
    
    // Send in chunks to avoid memory issues
    const char* ptr = INDEX_HTML;
    size_t remaining = contentLen;
    const size_t CHUNK_SIZE = 1024;
    
    while (remaining > 0) {
        size_t toSend = (remaining > CHUNK_SIZE) ? CHUNK_SIZE : remaining;
        char chunk[CHUNK_SIZE + 1];
        memcpy_P(chunk, ptr, toSend);
        chunk[toSend] = '\0';
        server.sendContent(chunk);
        ptr += toSend;
        remaining -= toSend;
        yield();  // Let other tasks run
    }
    
    DEBUG_PRINTLN("handleRoot: Done sending");
}

void WebServerHandler::handleApiStatus() {
    DEBUG_PRINTLN("API: /api/status called");
    // CORS for AP mode
    server.sendHeader("Access-Control-Allow-Origin", "*");
    
    StaticJsonDocument<512> doc;
    JsonObject root = doc.to<JsonObject>();

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
    plantObj["wateredCount"] = plant.wateredCount;
    plantObj["totalGoal"] = plant.totalGoal;
    plantObj["pendingWater"] = systemState->getPendingWaterCount();
    plantObj["dailyGoal"] = systemState->getDailyGoal();

    JsonObject statsObj = root.createNestedObject("stats");
    statsObj["completed"] = systemState->getCompletedCount();
    statsObj["total"] = systemState->getTaskCount();

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void WebServerHandler::handleApiTasks() {
    StaticJsonDocument<1024> doc;
    JsonArray tasksArray = doc.createNestedArray("tasks");

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

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void WebServerHandler::handleApiAddTask() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"No body\"}");
        return;
    }

    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));

    if (error) {
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }

    const char* name = doc["name"] | "Untitled";
    uint16_t focus = doc["focusDuration"] | 25;
    uint16_t breakTime = doc["breakDuration"] | 5;

    if (systemState->addTask(name, focus, breakTime)) {
        broadcastTasks();
        server.send(200, "application/json", "{\"success\":true}");
    } else {
        server.send(400, "application/json", "{\"error\":\"Task list full\"}");
    }
}

void WebServerHandler::handleApiAction() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"No body\"}");
        return;
    }

    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, server.arg("plain"));

    if (error) {
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }

    const char* action = doc["action"];

    if (strcmp(action, "water") == 0) {
        systemState->waterPlant();
        server.send(200, "application/json", "{\"success\":true,\"action\":\"water\"}");
    }
    else if (strcmp(action, "kill") == 0) {
        systemState->killPlant();
        systemState->clearAllTasks();  // Clear tasks on demo kill
        analytics.forceDailyReset();   // Reset today's stats
        broadcastTasks();              // Notify clients
        broadcastStatus();
        server.send(200, "application/json", "{\"success\":true,\"action\":\"kill\"}");
    }
    else if (strcmp(action, "pause") == 0) {
        systemState->pauseTimer();
        server.send(200, "application/json", "{\"success\":true,\"action\":\"pause\"}");
    }
    else if (strcmp(action, "resume") == 0) {
        systemState->resumeTimer();
        server.send(200, "application/json", "{\"success\":true,\"action\":\"resume\"}");
    }
    else if (strcmp(action, "setGoal") == 0) {
        uint8_t goal = doc["goal"] | 0;
        systemState->setDailyGoal(goal);
        server.send(200, "application/json", "{\"success\":true,\"action\":\"setGoal\"}");
    }
    else if (strcmp(action, "restartDay") == 0) {
        systemState->restartDay();
        analytics.forceDailyReset();  // Reset today's statistics too
        broadcastTasks();   // Notify clients that tasks are cleared
        broadcastStatus();  // Update status
        broadcastPlant();   // Update plant state
        server.send(200, "application/json", "{\"success\":true,\"action\":\"restartDay\"}");
    }
    else if (strcmp(action, "revive") == 0) {
        systemState->revivePlant();
        server.send(200, "application/json", "{\"success\":true,\"action\":\"revive\"}");
    }
    else if (strcmp(action, "selectTask") == 0) {
        // Select task for flip control (prepare to start with MPU flip)
        uint32_t taskId = doc["taskId"] | 0;
        if (taskId > 0) {
            systemState->selectTaskForFlip(taskId);
            broadcastStatus();  // Notify clients
            server.send(200, "application/json", "{\"success\":true,\"action\":\"selectTask\"}");
        } else {
            server.send(400, "application/json", "{\"error\":\"Invalid taskId\"}");
        }
    }
    else if (strcmp(action, "confirmComplete") == 0) {
        // User confirms task completion after flip
        systemState->confirmTaskComplete();
        server.send(200, "application/json", "{\"success\":true,\"action\":\"confirmComplete\"}");
    }
    else if (strcmp(action, "cancelComplete") == 0) {
        // User says flip was accidental
        systemState->cancelTaskComplete();
        server.send(200, "application/json", "{\"success\":true,\"action\":\"cancelComplete\"}");
    }
    else {
        server.send(400, "application/json", "{\"error\":\"Unknown action\"}");
    }
}

void WebServerHandler::handleApiStats() {
    StaticJsonDocument<512> doc;
    
    // Today's stats
    DailyStats today = analytics.getTodayStats();
    doc["todayTasks"] = today.tasksCompleted;
    doc["todayFocus"] = today.focusMinutes;
    doc["todayBreak"] = today.breakMinutes;
    doc["todaySessions"] = today.sessionsCount;
    
    // Weekly report
    WeeklyReport week = analytics.getWeeklyReport();
    JsonObject weekly = doc.createNestedObject("weekly");
    weekly["totalTasks"] = week.totalTasks;
    weekly["totalFocus"] = week.totalFocusMinutes;
    weekly["totalBreak"] = week.totalBreakMinutes;
    weekly["totalSessions"] = week.totalSessions;
    weekly["avgTasksPerDay"] = week.avgTasksPerDay;
    weekly["avgFocusPerDay"] = week.avgFocusPerDay;
    weekly["mostProductiveDay"] = week.mostProductiveDay;
    weekly["mostProductiveTasks"] = week.mostProductiveTasks;
    weekly["daysRecorded"] = week.daysRecorded;
    weekly["hasFullWeek"] = week.hasFullWeek;
    
    // Day names for display
    const char* dayNames[] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    weekly["mostProductiveDayName"] = dayNames[week.mostProductiveDay];
    
    // Daily breakdown (last 7 days)
    JsonArray days = doc.createNestedArray("days");
    for (int i = 0; i < 7; i++) {
        DailyStats day = analytics.getDayStats(i);
        JsonObject dayObj = days.createNestedObject();
        dayObj["daysAgo"] = i;
        dayObj["tasks"] = day.tasksCompleted;
        dayObj["focus"] = day.focusMinutes;
        dayObj["valid"] = day.valid;
    }

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

void WebServerHandler::handleNotFound() {
    String uri = server.uri();
    String host = server.hostHeader();
    DEBUG_PRINTF("handleNotFound: URI=%s, Host=%s\n", uri.c_str(), host.c_str());
    
    // In AP mode, handle captive portal detection
    if (!wifiConnected) {
        // Check if this looks like a connectivity check request
        // These come from various domains like connectivitycheck.gstatic.com
        if (uri.indexOf("generate_204") >= 0 || uri.indexOf("gen_204") >= 0) {
            // Android connectivity check - MUST return 204
            server.send(204, "", "");
            webClientConnected = true;
            return;
        }
        
        // Apple captive portal detection - redirect to our page
        if (host.indexOf("apple.com") >= 0 || host.indexOf("captive") >= 0 ||
            uri.indexOf("hotspot-detect") >= 0 || uri.indexOf("library/test") >= 0) {
            if (webClientConnected) {
                // Already connected - return Success to maintain connection
                server.send(200, "text/html", "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
            } else {
                // First time - redirect to our app
                server.sendHeader("Location", "http://192.168.4.1/");
                server.send(302, "text/html", "");
            }
            return;
        }
        
        // Google connectivity check (Android)
        if (host.indexOf("google") >= 0 || host.indexOf("gstatic") >= 0 ||
            host.indexOf("connectivitycheck") >= 0) {
            server.send(204, "", "");
            webClientConnected = true;
            return;
        }
        
        if (uri.indexOf("ncsi") >= 0 || uri.indexOf("connecttest") >= 0) {
            // Windows connectivity check
            server.send(200, "text/plain", "Microsoft NCSI");
            webClientConnected = true;
            return;
        }
        
        if (uri.indexOf("success") >= 0) {
            // Generic success check
            server.send(200, "text/html", "<HTML><HEAD><TITLE>Success</TITLE></HEAD><BODY>Success</BODY></HTML>");
            webClientConnected = true;
            return;
        }
        
        // For all other unknown requests, serve the main page directly
        // (avoid redirect which can cause captive portal loops)
        webClientConnected = true;
        handleRoot();
        return;
    }
    server.send(404, "text/plain", "Not found");
}

void WebServerHandler::onWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_DISCONNECTED:
            DEBUG_PRINTF("WebSocket client #%u disconnected\n", num);
            break;

        case WStype_CONNECTED:
            DEBUG_PRINTF("WebSocket client #%u connected\n", num);
            // Send initial state to new client
            // Order matters: tasks first, then plant (plant display depends on tasks)
            broadcastTasks();
            broadcastPlant();
            broadcastStatus();
            break;

        case WStype_TEXT:
            handleWebSocketMessage(num, payload, length);
            break;

        default:
            break;
    }
}

void WebServerHandler::handleWebSocketMessage(uint8_t num, uint8_t* payload, size_t length) {
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, payload, length);

    if (error) {
        DEBUG_PRINTLN("Failed to parse WebSocket message");
        return;
    }

    const char* action = doc["action"];

    if (strcmp(action, "getStatus") == 0) {
        broadcastStatus();
    }
    else if (strcmp(action, "getTasks") == 0) {
        broadcastTasks();
    }
    else if (strcmp(action, "water") == 0) {
        systemState->waterPlant();
    }
    else if (strcmp(action, "kill") == 0) {
        systemState->killPlant();
        systemState->clearAllTasks();  // Clear tasks on demo kill
        analytics.forceDailyReset();   // Reset today's stats
        broadcastTasks();              // Notify clients
        broadcastStatus();
    }
    else if (strcmp(action, "pause") == 0) {
        systemState->pauseTimer();
    }
    else if (strcmp(action, "resume") == 0) {
        systemState->resumeTimer();
    }
    else if (strcmp(action, "addTask") == 0) {
        const char* name = doc["task"]["name"] | "Untitled";
        uint16_t focus = doc["task"]["focusDuration"] | 25;
        uint16_t breakTime = doc["task"]["breakDuration"] | 5;
        systemState->addTask(name, focus, breakTime);
    }
    else if (strcmp(action, "startTask") == 0) {
        uint32_t taskId = doc["taskId"];
        systemState->startTask(taskId);
    }
    else if (strcmp(action, "deleteTask") == 0) {
        uint32_t taskId = doc["taskId"];
        systemState->deleteTask(taskId);
    }
    else if (strcmp(action, "toggleTask") == 0) {
        uint32_t taskId = doc["taskId"];
        systemState->toggleTaskComplete(taskId);
    }
    else if (strcmp(action, "setGoal") == 0) {
        uint8_t goal = doc["goal"];
        systemState->setDailyGoal(goal);
    }
    else if (strcmp(action, "restartDay") == 0) {
        systemState->restartDay();
        analytics.forceDailyReset();  // Reset today's statistics too
    }
    else if (strcmp(action, "revive") == 0) {
        systemState->revivePlant();
    }
    else if (strcmp(action, "selectTask") == 0) {
        // Select task for flip control
        uint32_t taskId = doc["taskId"];
        systemState->selectTaskForFlip(taskId);
    }
    else if (strcmp(action, "confirmComplete") == 0) {
        // User confirms task completion after flip
        systemState->confirmTaskComplete();
    }
    else if (strcmp(action, "cancelComplete") == 0) {
        // User says flip was accidental
        systemState->cancelTaskComplete();
    }
    else if (strcmp(action, "setTime") == 0) {
        // Sync time from phone
        int hours = doc["hours"] | 0;
        int minutes = doc["minutes"] | 0;
        int seconds = doc["seconds"] | 0;
        int day = doc["day"] | 1;
        int month = doc["month"] | 1;
        int year = doc["year"] | 2024;
        
        struct tm timeinfo;
        timeinfo.tm_hour = hours;
        timeinfo.tm_min = minutes;
        timeinfo.tm_sec = seconds;
        timeinfo.tm_mday = day;
        timeinfo.tm_mon = month - 1;  // tm_mon is 0-11
        timeinfo.tm_year = year - 1900;  // tm_year is years since 1900
        timeinfo.tm_isdst = -1;
        
        time_t t = mktime(&timeinfo);
        struct timeval tv = { .tv_sec = t, .tv_usec = 0 };
        settimeofday(&tv, NULL);
        
        timeSynced = true;
        DEBUG_PRINTF("Time synced from phone: %02d:%02d:%02d\n", hours, minutes, seconds);
    }
}

void WebServerHandler::broadcastStatus() {
    StaticJsonDocument<256> doc;
    doc["type"] = "status";
    doc["state"] = systemState->getModeString();
    doc["timeLeft"] = systemState->getTimeLeft();
    doc["totalTime"] = systemState->getTotalTime();
    doc["waitingForConfirmation"] = systemState->isWaitingForConfirmation();

    const char* taskName = systemState->getCurrentTaskName();
    doc["taskName"] = taskName ? taskName : (char*)nullptr;

    String message;
    serializeJson(doc, message);
    webSocket.broadcastTXT(message);
}

void WebServerHandler::broadcastPlant() {
    StaticJsonDocument<192> doc;
    doc["type"] = "plant";

    PlantInfo plant = systemState->getPlantInfo();
    doc["stage"] = plant.stage;
    doc["isWithered"] = plant.isWithered;
    doc["wateredCount"] = plant.wateredCount;
    doc["totalGoal"] = plant.totalGoal;
    doc["pendingWater"] = systemState->getPendingWaterCount();
    doc["dailyGoal"] = systemState->getDailyGoal();

    String message;
    serializeJson(doc, message);
    
    DEBUG_PRINTF("broadcastPlant: stage=%d, watered=%d/%d, pending=%d\n", 
                 plant.stage, plant.wateredCount, plant.totalGoal, 
                 systemState->getPendingWaterCount());
    
    webSocket.broadcastTXT(message);
}

void WebServerHandler::broadcastTasks() {
    StaticJsonDocument<1024> doc;
    doc["type"] = "tasks";

    JsonArray tasksArray = doc.createNestedArray("tasks");
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

    String message;
    serializeJson(doc, message);
    webSocket.broadcastTXT(message);
}

void WebServerHandler::broadcastRevive() {
    StaticJsonDocument<128> doc;
    doc["type"] = "revive";
    doc["message"] = "Plant Revived! You can plant again!";
    
    String message;
    serializeJson(doc, message);
    webSocket.broadcastTXT(message);
    DEBUG_PRINTLN("WebSocket: Broadcast plant revive message");
}

#endif // WEB_SERVER_HANDLER_H
