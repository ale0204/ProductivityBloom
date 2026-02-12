#ifndef QRCODE_GENERATOR_H
#define QRCODE_GENERATOR_H

/**
 * Valid QR Code for "http://192.168.4.1"
 * Generated with Python qrcode library
 * Version 2-L, 25x25 modules - VERIFIED SCANNABLE
 */

#include <Arduino.h>

#define QR_SIZE 25

// Valid QR code for http://192.168.4.1 - Version 2-L (25x25)
// Generated with Python qrcode library - VERIFIED CORRECT
static const PROGMEM uint8_t QR_BITMAP[25][4] = {
    {0xFE, 0x74, 0xBF, 0x80},  // 1111111001110100101111111
    {0x82, 0x18, 0x20, 0x80},  // 1000001000011000001000001
    {0xBA, 0x72, 0xAE, 0x80},  // 1011101001110010101011101
    {0xBA, 0xB3, 0xAE, 0x80},  // 1011101010110011101011101
    {0xBA, 0xAC, 0x2E, 0x80},  // 1011101010101100001011101
    {0x82, 0x66, 0x20, 0x80},  // 1000001001100110001000001
    {0xFE, 0xAA, 0xBF, 0x80},  // 1111111010101010101111111
    {0x00, 0x4C, 0x80, 0x00},  // 0000000001001100100000000
    {0xC7, 0x6B, 0x8C, 0x00},  // 1100011101101011100011000
    {0x21, 0xA3, 0xCF, 0x00},  // 0010000110100011110011110
    {0x46, 0xD9, 0x35, 0x80},  // 0100011011011001001101011
    {0x68, 0xB3, 0x8C, 0x80},  // 0110100010110011100011001
    {0xE6, 0x66, 0xE0, 0x80},  // 1110011001100110111000001
    {0xA0, 0x2F, 0x01, 0x00},  // 1010000000101111000000010
    {0x9E, 0x67, 0x55, 0x80},  // 1001111001100111010101011
    {0x9D, 0x8E, 0x8A, 0x80},  // 1001110110001110100010101
    {0xAF, 0x2A, 0xFA, 0x00},  // 1010111100101010111110100
    {0x00, 0xE1, 0x8A, 0x00},  // 0000000011100001100010100
    {0xFE, 0xEE, 0xAC, 0x80},  // 1111111011101110101011001
    {0x82, 0xF2, 0x88, 0x00},  // 1000001011110010100010000
    {0xBA, 0x51, 0xFE, 0x80},  // 1011101001010001111111101
    {0xBA, 0x2C, 0x35, 0x80},  // 1011101000101100001101011
    {0xBA, 0x26, 0x42, 0x80},  // 1011101000100110010000101
    {0x82, 0xAD, 0xB8, 0x80},  // 1000001010101101101110001
    {0xFE, 0x9D, 0xA4, 0x80},  // 1111111010011101101001001
};

class QRCodeGenerator {
public:
    uint8_t size;
    
    QRCodeGenerator() : size(QR_SIZE) {}
    
    void generate(const char* url) {
        // Pre-computed, nothing needed
    }
    
    bool getModule(uint8_t x, uint8_t y) {
        if (x >= QR_SIZE || y >= QR_SIZE) return false;
        
        uint8_t byteIdx = x / 8;
        uint8_t bitIdx = 7 - (x % 8);
        uint8_t byte = pgm_read_byte(&QR_BITMAP[y][byteIdx]);
        return (byte >> bitIdx) & 1;
    }
};

#endif // QRCODE_GENERATOR_H
