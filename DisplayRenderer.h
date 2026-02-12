#ifndef DISPLAY_RENDERER_H
#define DISPLAY_RENDERER_H

/**
 * ============================================
 * DisplayRenderer - OLED Drawing Abstraction
 * ============================================
 * 
 * All OLED drawing functions in one place.
 * Takes U8G2 display reference via dependency injection.
 * 
 * Separates:
 * - Drawing logic (this class)
 * - Display hardware (U8G2)
 * - Application state (SystemState)
 */

#include <Arduino.h>
#include <U8g2lib.h>
#include <math.h>
#include "SystemState.h"
#include "QRCodeGenerator.h"

// Forward declaration for Analytics time
class Analytics;
extern Analytics analytics;

class DisplayRenderer {
public:
    // Inject display reference
    DisplayRenderer(U8G2& display) : u8g2(display) {}
    
    // ============================================
    // High-level screen drawing
    // ============================================
    
    void drawIdleScreen(const PlantInfo& plant, bool isAPMode, bool hasWebClient, bool showWelcome) {
        if (isAPMode && !hasWebClient) {
            drawQRScreen();
            return;
        }
        
        drawPlant(plant);
        
        if (isAPMode) {
            drawBottomText("192.168.4.1");
        } else if (showWelcome) {
            drawBottomText("Use web to start");
        }
    }
    
    void drawFocusScreen(const char* taskName, uint32_t timeLeft, uint32_t totalTime) {
        drawModeLabel("FOCUSING");
        drawTimer(timeLeft, totalTime);
        if (taskName) drawTaskName(taskName);
    }
    
    void drawBreakScreen(const char* taskName, uint32_t timeLeft, uint32_t totalTime) {
        drawModeLabel("BREAK");
        drawTimer(timeLeft, totalTime);
        drawBottomText("Take a break!");
    }
    
    void drawPausedScreen(const char* taskName, uint32_t timeLeft, uint32_t totalTime) {
        drawModeLabel("PAUSED");
        drawTimer(timeLeft, totalTime);
        if (taskName) drawTaskName(taskName);
    }
    
    void drawWitheredScreen() {
        drawWitheredPlant(64, 65);
        
        u8g2.setFont(u8g2_font_6x12_tr);
        centerText("Use light sensor", 95);
        centerText("to revive", 108);
    }
    
    void drawCongratsScreen() {
        u8g2.setFont(u8g2_font_ncenB12_tr);
        centerText("Congrats!", 35);
        
        // Flower icon
        drawFlowerIcon(64, 55);
        
        u8g2.setFont(u8g2_font_6x12_tr);
        centerText("All tasks done!", 85);
        centerText("Plant fully grown!", 100);
    }
    
    void drawReviveScreen() {
        u8g2.setFont(u8g2_font_ncenB12_tr);
        centerText("Revived!", 30);
        
        // Bloom flower
        drawFlowerIcon(64, 60, 12, 8);
        
        u8g2.setFont(u8g2_font_6x12_tr);
        centerText("Your plant lives!", 95);
        centerText("Start a new day!", 110);
    }
    
    void drawQRScreen() {
        u8g2.setFont(u8g2_font_6x12_tr);
        centerText("Scan to connect", 10);
        
        drawQRCode();
        
        u8g2.setFont(u8g2_font_5x7_tr);
        centerText("WiFi: ProductivityBloom", 98);
        centerText("Pass: bloom2024", 108);
        
        u8g2.setFont(u8g2_font_5x8_tr);
        centerText("or visit 192.168.4.1", 120);
    }
    
    // ============================================
    // Component drawing
    // ============================================
    
    void drawClock(int hour, int minute) {
        char timeStr[6];
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d", hour, minute);
        
        u8g2.setFont(u8g2_font_5x7_tr);
        u8g2.drawStr(98, 10, timeStr);
    }
    
    void drawBorder() {
        u8g2.drawFrame(0, 0, 128, 128);
    }
    
    void drawPlant(const PlantInfo& plant) {
        // Stage text at top
        u8g2.setFont(u8g2_font_6x12_tr);
        
        const char* stageNames[] = {"Seed", "Sprout", "Growing", "Bloom"};
        const char* stageName = plant.isWithered ? "Withered" : stageNames[plant.stage];
        
        char stageText[32];
        if (plant.totalGoal > 0) {
            snprintf(stageText, sizeof(stageText), "%s (%d/%d)", 
                     stageName, plant.wateredCount, plant.totalGoal);
        } else {
            snprintf(stageText, sizeof(stageText), "%s (0/0)", stageName);
        }
        centerText(stageText, 18);
        
        // Plant graphic
        int16_t cx = 64, baseY = 85;
        
        if (plant.isWithered) {
            drawWitheredPlant(cx, baseY);
        } else {
            switch (plant.stage) {
                case 0: drawSeedPlant(cx, baseY); break;
                case 1: drawSproutPlant(cx, baseY); break;
                case 2: drawGrowingPlant(cx, baseY); break;
                case 3: drawBloomPlant(cx, baseY); break;
                default: drawSeedPlant(cx, baseY); break;
            }
        }
    }
    
    void drawTimer(uint32_t timeLeft, uint32_t totalTime) {
        u8g2.setFont(u8g2_font_logisoso22_tn);
        
        uint32_t minutes = timeLeft / 60;
        uint32_t seconds = timeLeft % 60;
        
        char timeStr[8];
        snprintf(timeStr, sizeof(timeStr), "%02lu:%02lu", minutes, seconds);
        centerText(timeStr, 75);
        
        // Progress bar
        if (totalTime > 0) {
            const uint8_t barWidth = 100;
            const uint8_t barX = (128 - barWidth) / 2;
            const uint8_t barY = 85;
            const uint8_t barHeight = 6;
            
            u8g2.drawFrame(barX, barY, barWidth, barHeight);
            
            uint8_t progress = ((totalTime - timeLeft) * (barWidth - 2)) / totalTime;
            if (progress > 0) {
                u8g2.drawBox(barX + 1, barY + 1, progress, barHeight - 2);
            }
        }
    }
    
    // ============================================
    // Plant graphics (all stages)
    // ============================================
    
    void drawPot(int16_t cx, int16_t baseY) {
        // Pot body - trapezoid
        u8g2.drawLine(cx - 15, baseY, cx - 20, baseY + 18);
        u8g2.drawLine(cx + 15, baseY, cx + 20, baseY + 18);
        u8g2.drawLine(cx - 20, baseY + 18, cx + 20, baseY + 18);
        u8g2.drawLine(cx - 15, baseY, cx + 15, baseY);
        
        // Rim
        u8g2.drawLine(cx - 17, baseY - 2, cx + 17, baseY - 2);
        u8g2.drawLine(cx - 17, baseY - 2, cx - 15, baseY);
        u8g2.drawLine(cx + 17, baseY - 2, cx + 15, baseY);
        
        // Soil
        u8g2.drawLine(cx - 12, baseY + 3, cx + 12, baseY + 3);
    }
    
    void drawSeedPlant(int16_t cx, int16_t baseY) {
        drawPot(cx, baseY);
        u8g2.drawEllipse(cx, baseY - 5, 6, 4, U8G2_DRAW_ALL);
        u8g2.drawEllipse(cx, baseY - 5, 4, 2, U8G2_DRAW_ALL);
    }
    
    void drawSproutPlant(int16_t cx, int16_t baseY) {
        drawPot(cx, baseY);
        
        // Stem
        u8g2.drawLine(cx, baseY - 2, cx, baseY - 18);
        u8g2.drawLine(cx - 1, baseY - 2, cx - 1, baseY - 18);
        
        // Leaves
        u8g2.drawLine(cx - 1, baseY - 14, cx - 8, baseY - 20);
        u8g2.drawLine(cx - 8, baseY - 20, cx - 1, baseY - 17);
        u8g2.drawLine(cx + 1, baseY - 16, cx + 8, baseY - 22);
        u8g2.drawLine(cx + 8, baseY - 22, cx + 1, baseY - 19);
    }
    
    void drawGrowingPlant(int16_t cx, int16_t baseY) {
        drawPot(cx, baseY);
        
        // Tall stem
        u8g2.drawLine(cx, baseY - 2, cx, baseY - 35);
        u8g2.drawLine(cx - 1, baseY - 2, cx - 1, baseY - 35);
        u8g2.drawLine(cx + 1, baseY - 2, cx + 1, baseY - 35);
        
        // Leaves
        u8g2.drawTriangle(cx - 2, baseY - 10, cx - 14, baseY - 14, cx - 2, baseY - 16);
        u8g2.drawTriangle(cx + 2, baseY - 12, cx + 14, baseY - 16, cx + 2, baseY - 18);
        u8g2.drawTriangle(cx - 2, baseY - 20, cx - 12, baseY - 26, cx - 2, baseY - 26);
        u8g2.drawTriangle(cx + 2, baseY - 22, cx + 12, baseY - 28, cx + 2, baseY - 28);
        
        u8g2.drawLine(cx - 1, baseY - 30, cx - 6, baseY - 36);
        u8g2.drawLine(cx + 1, baseY - 30, cx + 6, baseY - 36);
    }
    
    void drawBloomPlant(int16_t cx, int16_t baseY) {
        drawPot(cx, baseY);
        
        // Stem
        u8g2.drawLine(cx, baseY - 2, cx, baseY - 35);
        u8g2.drawLine(cx - 1, baseY - 2, cx - 1, baseY - 35);
        u8g2.drawLine(cx + 1, baseY - 2, cx + 1, baseY - 35);
        
        // Stem leaves
        u8g2.drawTriangle(cx - 2, baseY - 10, cx - 10, baseY - 15, cx - 2, baseY - 17);
        u8g2.drawTriangle(cx + 2, baseY - 14, cx + 10, baseY - 19, cx + 2, baseY - 21);
        
        // Flower
        int16_t flowerY = baseY - 42;
        drawFlowerIcon(cx, flowerY, 11, 8);
    }
    
    void drawWitheredPlant(int16_t cx, int16_t baseY) {
        drawPot(cx, baseY);
        
        // Droopy stem
        u8g2.drawLine(cx, baseY - 2, cx - 5, baseY - 20);
        u8g2.drawLine(cx - 5, baseY - 20, cx - 15, baseY - 25);
        
        // Dead flower
        u8g2.drawCircle(cx - 18, baseY - 25, 5, U8G2_DRAW_ALL);
        
        // X eyes
        u8g2.drawLine(cx - 20, baseY - 27, cx - 18, baseY - 25);
        u8g2.drawLine(cx - 18, baseY - 27, cx - 20, baseY - 25);
        u8g2.drawLine(cx - 16, baseY - 27, cx - 14, baseY - 25);
        u8g2.drawLine(cx - 14, baseY - 27, cx - 16, baseY - 25);
    }
    
    void drawFlowerIcon(int16_t cx, int16_t cy, int16_t petalDist = 10, int numPetals = 6) {
        // Center
        u8g2.drawDisc(cx, cy, 5, U8G2_DRAW_ALL);
        
        // Petals
        for (int i = 0; i < numPetals; i++) {
            float angle = i * 3.14159f * 2.0f / numPetals;
            int16_t px = cx + cos(angle) * petalDist;
            int16_t py = cy + sin(angle) * petalDist;
            u8g2.drawDisc(px, py, 4, U8G2_DRAW_ALL);
        }
        
        // Center detail
        u8g2.setDrawColor(0);
        u8g2.drawDisc(cx, cy, 2, U8G2_DRAW_ALL);
        u8g2.setDrawColor(1);
        u8g2.drawCircle(cx, cy, 2, U8G2_DRAW_ALL);
    }
    
    void drawQRCode() {
        QRCodeGenerator qr;
        qr.generate("http://192.168.4.1");
        
        const int scale = 3;
        const int qrSize = qr.size * scale;
        const int offsetX = (128 - qrSize) / 2;
        const int offsetY = 14;
        
        // White border (quiet zone)
        u8g2.setDrawColor(0);
        u8g2.drawBox(offsetX - 4, offsetY - 4, qrSize + 8, qrSize + 8);
        u8g2.setDrawColor(1);
        
        // QR modules
        for (uint8_t y = 0; y < qr.size; y++) {
            for (uint8_t x = 0; x < qr.size; x++) {
                if (qr.getModule(x, y)) {
                    u8g2.drawBox(offsetX + x * scale, offsetY + y * scale, scale, scale);
                }
            }
        }
    }
    
    // ============================================
    // Utility functions
    // ============================================
    
    void centerText(const char* text, int16_t y) {
        int16_t width = u8g2.getStrWidth(text);
        u8g2.drawStr((128 - width) / 2, y, text);
    }
    
    void drawModeLabel(const char* mode) {
        u8g2.setFont(u8g2_font_6x12_tr);
        centerText(mode, 12);
    }
    
    void drawTaskName(const char* name) {
        u8g2.setFont(u8g2_font_6x12_tr);
        
        char display[22];
        strncpy(display, name, sizeof(display) - 1);
        display[sizeof(display) - 1] = '\0';
        
        centerText(display, 105);
    }
    
    void drawBottomText(const char* text) {
        u8g2.setFont(u8g2_font_5x7_tr);
        centerText(text, 120);
    }
    
    // Prepare buffer (call before drawing)
    void beginFrame() {
        u8g2.clearBuffer();
        u8g2.setDrawColor(1);
        u8g2.setFont(u8g2_font_6x12_tr);
    }
    
    // Send buffer to display (call after drawing)
    void endFrame() {
        u8g2.sendBuffer();
    }

private:
    U8G2& u8g2;
};

#endif // DISPLAY_RENDERER_H
