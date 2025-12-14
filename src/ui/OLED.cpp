#include "OLED.h"

// Standard 5x7 font for basic text rendering (ASCII 32-126)
const uint8_t font5x7[][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, // (space)
    {0x00, 0x00, 0x5F, 0x00, 0x00}, // !
    {0x00, 0x07, 0x00, 0x07, 0x00}, // "
    {0x14, 0x7F, 0x14, 0x7F, 0x14}, // #
    {0x24, 0x2A, 0x7F, 0x2A, 0x12}, // $
    {0x23, 0x13, 0x08, 0x64, 0x62}, // %
    {0x36, 0x49, 0x55, 0x22, 0x50}, // &
    {0x00, 0x05, 0x03, 0x00, 0x00}, // '
    {0x00, 0x1C, 0x22, 0x41, 0x00}, // (
    {0x00, 0x41, 0x22, 0x1C, 0x00}, // )
    {0x08, 0x2A, 0x1C, 0x2A, 0x08}, // *
    {0x08, 0x08, 0x3E, 0x08, 0x08}, // +
    {0x00, 0x50, 0x30, 0x00, 0x00}, // ,
    {0x08, 0x08, 0x08, 0x08, 0x08}, // -
    {0x00, 0x60, 0x60, 0x00, 0x00}, // .
    {0x20, 0x10, 0x08, 0x04, 0x02}, // /
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9
    {0x00, 0x36, 0x36, 0x00, 0x00}, // :
    {0x00, 0x56, 0x36, 0x00, 0x00}, // ;
    {0x00, 0x08, 0x14, 0x22, 0x41}, // <
    {0x14, 0x14, 0x14, 0x14, 0x14}, // =
    {0x41, 0x22, 0x14, 0x08, 0x00}, // >
    {0x02, 0x01, 0x51, 0x09, 0x06}, // ?
    {0x32, 0x49, 0x79, 0x41, 0x3E}, // @
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, // A
    {0x7F, 0x49, 0x49, 0x49, 0x36}, // B
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // C
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, // D
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // E
    {0x7F, 0x09, 0x09, 0x01, 0x01}, // F
    {0x3E, 0x41, 0x41, 0x51, 0x32}, // G
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, // H
    {0x00, 0x41, 0x7F, 0x41, 0x00}, // I
    {0x20, 0x40, 0x41, 0x3F, 0x01}, // J
    {0x7F, 0x08, 0x14, 0x22, 0x41}, // K
    {0x7F, 0x40, 0x40, 0x40, 0x40}, // L
    {0x7F, 0x02, 0x04, 0x02, 0x7F}, // M
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, // N
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // O
    {0x7F, 0x09, 0x09, 0x09, 0x06}, // P
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, // Q
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // R
    {0x46, 0x49, 0x49, 0x49, 0x31}, // S
    {0x01, 0x01, 0x7F, 0x01, 0x01}, // T
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, // U
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, // V
    {0x7F, 0x20, 0x18, 0x20, 0x7F}, // W
    {0x63, 0x14, 0x08, 0x14, 0x63}, // X
    {0x03, 0x04, 0x78, 0x04, 0x03}, // Y
    {0x61, 0x51, 0x49, 0x45, 0x43}, // Z
    {0x00, 0x00, 0x7F, 0x41, 0x41}, // [
    {0x02, 0x04, 0x08, 0x10, 0x20}, // "\"
    {0x41, 0x41, 0x7F, 0x00, 0x00}, // ]
    {0x04, 0x02, 0x01, 0x02, 0x04}, // ^
    {0x40, 0x40, 0x40, 0x40, 0x40}, // _
    {0x00, 0x01, 0x02, 0x04, 0x00}, // `
    {0x20, 0x54, 0x54, 0x54, 0x78}, // a
    {0x7F, 0x48, 0x44, 0x44, 0x38}, // b
    {0x38, 0x44, 0x44, 0x44, 0x20}, // c
    {0x38, 0x44, 0x44, 0x48, 0x7F}, // d
    {0x38, 0x54, 0x54, 0x54, 0x18}, // e
    {0x08, 0x7E, 0x09, 0x01, 0x02}, // f
    {0x08, 0x14, 0x54, 0x54, 0x3C}, // g
    {0x7F, 0x08, 0x04, 0x04, 0x78}, // h
    {0x00, 0x44, 0x7D, 0x40, 0x00}, // i
    {0x20, 0x40, 0x44, 0x3D, 0x00}, // j
    {0x00, 0x7F, 0x10, 0x28, 0x44}, // k
    {0x00, 0x41, 0x7F, 0x40, 0x00}, // l
    {0x7C, 0x04, 0x18, 0x04, 0x78}, // m
    {0x7C, 0x08, 0x04, 0x04, 0x78}, // n
    {0x38, 0x44, 0x44, 0x44, 0x38}, // o
    {0x7C, 0x14, 0x14, 0x14, 0x08}, // p
    {0x08, 0x14, 0x14, 0x18, 0x7C}, // q
    {0x7C, 0x08, 0x04, 0x04, 0x08}, // r
    {0x48, 0x54, 0x54, 0x54, 0x20}, // s
    {0x04, 0x3F, 0x44, 0x40, 0x20}, // t
    {0x3C, 0x40, 0x40, 0x20, 0x7C}, // u
    {0x1C, 0x20, 0x40, 0x20, 0x1C}, // v
    {0x3C, 0x40, 0x30, 0x40, 0x3C}, // w
    {0x44, 0x28, 0x10, 0x28, 0x44}, // x
    {0x0C, 0x50, 0x50, 0x50, 0x3C}, // y
    {0x44, 0x64, 0x54, 0x4C, 0x44}, // z
    {0x00, 0x08, 0x36, 0x41, 0x00}, // {
    {0x00, 0x00, 0x7F, 0x00, 0x00}, // |
    {0x00, 0x41, 0x36, 0x08, 0x00}, // }
    {0x08, 0x08, 0x2A, 0x1C, 0x08}, // ->
    {0x08, 0x1C, 0x2A, 0x08, 0x08}  // <-
};

OLED::OLED() {
    memset(displayBuffer, 0, BUFFER_SIZE);
}

bool OLED::init() {
    // Step 1: Power control
    pinMode(VEXT_PIN, OUTPUT);
    digitalWrite(VEXT_PIN, LOW);  // Enable power
    delay(100);
    // Step 2: Reset sequence
    resetDisplay();
    // Step 3: Initialize I2C
    Wire.begin(SDA_PIN, SCL_PIN, 100000);
    //Wire.setClock(400000);
    delay(100);
    
    // Step 4: Check if device responds
    Wire.beginTransmission(I2C_ADDR);
    if (Wire.endTransmission() != 0) {
        return false;
    }
    
    // Step 5: Send initialization commands
    sendCommand(SSD1306_DISPLAYOFF);
    sendCommand(SSD1306_SETDISPLAYCLOCKDIV);
    sendCommand(0xF0);
    sendCommand(SSD1306_SETMULTIPLEX);
    sendCommand(SCREEN_HEIGHT - 1);
    sendCommand(SSD1306_SETDISPLAYOFFSET);
    sendCommand(0x00);
    sendCommand(SSD1306_SETSTARTLINE);
    sendCommand(SSD1306_CHARGEPUMP);
    sendCommand(0x14);
    sendCommand(SSD1306_MEMORYMODE);
    sendCommand(0x00);
    sendCommand(SSD1306_SEGREMAP | 0x01);
    sendCommand(SSD1306_COMSCANDEC);
    sendCommand(SSD1306_SETCOMPINS);
    sendCommand(0x12);
    sendCommand(SSD1306_SETCONTRAST);
    sendCommand(0xCF);
    sendCommand(SSD1306_SETPRECHARGE);
    sendCommand(0xF1);
    sendCommand(SSD1306_SETVCOMDETECT);
    sendCommand(0x40);
    sendCommand(SSD1306_DISPLAYALLON_RESUME);
    sendCommand(SSD1306_NORMALDISPLAY);
    sendCommand(0x2E); // Stop scroll
    sendCommand(SSD1306_DISPLAYON);
    clearDisplay();
    updateDisplay();
    return true;
}

void OLED::sendCommand(uint8_t cmd) {
    Wire.beginTransmission(I2C_ADDR);
    Wire.write(0x80); // Command mode
    Wire.write(cmd);
    Wire.endTransmission();
}

void OLED::resetDisplay() {
    if (RST_PIN != -1) {
        pinMode(RST_PIN, OUTPUT);
        digitalWrite(RST_PIN, HIGH);
        delay(1);
        digitalWrite(RST_PIN, LOW);
        delay(1);
        digitalWrite(RST_PIN, HIGH);
        delay(1);
    }
}

void OLED::displayOn() {
    sendCommand(SSD1306_DISPLAYON);
}

void OLED::displayOff() {
    sendCommand(SSD1306_DISPLAYOFF);
}

void OLED::drawLine(int x0, int y0, int x1, int y1) {
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;
    while (true) {
        setPixel(x0, y0);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void OLED::drawRect(int x, int y, int width, int height, bool fill) {
    if (fill) {
        fillRect(x, y, width, height);
    } else {
        drawHorizontalLine(x, y, width);
        drawHorizontalLine(x, y + height - 1, width);
        drawVerticalLine(x, y, height);
        drawVerticalLine(x + width - 1, y, height);
    }
}

void OLED::drawCircle(int x, int y, int radius, bool fill) {
    int f = 1 - radius;
    int ddF_x = 1;
    int ddF_y = -2 * radius;
    int px = 0;
    int py = radius;
    if (fill) {
        drawVerticalLine(x, y - radius, 2 * radius + 1);
        while (px < py) {
            if (f >= 0) {
                py--;
                ddF_y += 2;
                f += ddF_y;
            }
            px++;
            ddF_x += 2;
            f += ddF_x;
            drawVerticalLine(x + px, y - py, 2 * py + 1);
            drawVerticalLine(x - px, y - py, 2 * py + 1);
            drawVerticalLine(x + py, y - px, 2 * px + 1);
            drawVerticalLine(x - py, y - px, 2 * px + 1);
        }
    } 
    else {
        setPixel(x, y + radius);
        setPixel(x, y - radius);
        setPixel(x + radius, y);
        setPixel(x - radius, y);
        while (px < py) {
            if (f >= 0) {
                py--;
                ddF_y += 2;
                f += ddF_y;
            }
            px++;
            ddF_x += 2;
            f += ddF_x;
            setPixel(x + px, y + py);
            setPixel(x - px, y + py);
            setPixel(x + px, y - py);
            setPixel(x - px, y - py);
            setPixel(x + py, y + px);
            setPixel(x - py, y + px);
            setPixel(x + py, y - px);
            setPixel(x - py, y - px);
        }
    }
}

void OLED::drawHorizontalLine(int x, int y, int width) {
    for (int i = 0; i < width; i++) {
        setPixel(x + i, y);
    }
}

void OLED::drawVerticalLine(int x, int y, int height) {
    for (int i = 0; i < height; i++) {
        setPixel(x, y + i);
    }
}

void OLED::fillRect(int x, int y, int width, int height) {
    for (int i = 0; i < height; i++) {
        drawHorizontalLine(x, y + i, width);
    }
}

void OLED::clearDisplay() {
    memset(displayBuffer, 0, BUFFER_SIZE);
}

void OLED::setPixel(int x, int y, bool white) {
    if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT) {
        if (white) {
            displayBuffer[x + (y >> 3) * SCREEN_WIDTH] |= (1 << (y & 7));
        } else {
            displayBuffer[x + (y >> 3) * SCREEN_WIDTH] &= ~(1 << (y & 7));
        }
    }
}

void OLED::updateDisplay() {
    sendCommand(SSD1306_COLUMNADDR);
    sendCommand(0);
    sendCommand(SCREEN_WIDTH - 1);
    sendCommand(SSD1306_PAGEADDR);
    sendCommand(0);
    sendCommand((SCREEN_HEIGHT >> 3) - 1);
    for (uint16_t i = 0; i < BUFFER_SIZE; i += 16) {
        Wire.beginTransmission(I2C_ADDR);
        Wire.write(0x40); // Data mode
        for (uint8_t x = 0; x < 16 && (i + x) < BUFFER_SIZE; x++) {
            Wire.write(displayBuffer[i + x]);
        }
        Wire.endTransmission();
    }
}

void OLED::drawChar(int x, int y, char c) {
    // Allow full ASCII printable character range (32-126)
    if (c < 32 || c > 126) c = 32; // Default to space for unsupported chars
    const uint8_t* charData = font5x7[c - 32];
    for (int i = 0; i < 5; i++) {
        uint8_t line = charData[i];
        for (int j = 0; j < 8; j++) {
            if (line & (1 << j)) {
                setPixel(x + i, y + j);
            }
        }
    }
}

void OLED::drawString(int x, int y, const char* str) {
    int cursorX = x;
    while (*str) {
        drawChar(cursorX, y, *str);
        cursorX += 6; // 5 pixels wide + 1 pixel spacing
        str++;
    }
}

void OLED::drawString(int x, int y, String str) {
    drawString(x, y, str.c_str());
}