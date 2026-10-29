#include "ScreenCtrl.h"
#include "I2cGuard.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>


static bool screenReady=false;
String line_1 = "";
String line_2 = "";
String line_3 = "";
String line_4 = "";

// Create an instance of the SSD1306 display
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1, 400000UL, 400000UL);

ScreenCtrl::ScreenCtrl() {
    // Constructor
}

void ScreenCtrl::init() {
    screenReady=false;
    I2cGuard guard;
    if (!guard.acquired) return;
    // SSD1306::begin checks allocation, not physical presence. Probe first so
    // an absent OLED never causes repeated frame writes / I2C timeouts.
    Wire.beginTransmission(0x3C);
    if (Wire.endTransmission()!=0) return;
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C, true, false)) { // Default I2C address is 0x3C
        // Initialization failed, handle error here (e.g., log or retry)
        return;
    }
    screenReady=true;
    display.setTextColor(SSD1306_WHITE);
    display.setTextWrap(false);
    display.clearDisplay();
    display.display();
}

bool ScreenCtrl::present() const { return screenReady; }

void ScreenCtrl::rotate(int rotateMode) {
    if(!screenReady)return;
    I2cGuard guard;
    if (!guard.acquired) return;
    display.setRotation(rotateMode);
}

void ScreenCtrl::updateScreen() {
    if(!screenReady)return;
    I2cGuard guard;
    if (!guard.acquired) return;
    display.clearDisplay();
    display.display();
}

// size = 1：ABCDEFGHIJK0123456789 in one line
void ScreenCtrl::displayText(String text, int16_t x, int16_t y, uint8_t size) {
    if(!screenReady)return;
    I2cGuard guard;
    if (!guard.acquired) return;
    display.setCursor(x, y);
    display.setTextSize(size); // Default text size
    display.print(text);
    display.display();
}

void ScreenCtrl::changeSingleLine(int lineNum, String text, bool updateFlag) {
    if(!screenReady)return;
    I2cGuard guard;
    if (!guard.acquired) return;
    switch (lineNum) {
        case 1:
            line_1 = text;
            break;
        case 2:
            line_2 = text;
            break;
        case 3:
            line_3 = text;
            break;
        case 4:
            line_4 = text;
            break;
    }
    if (updateFlag) {
        updateFrame();
    }
}

void ScreenCtrl::updateFrame() {
    if(!screenReady)return;
    I2cGuard guard;
    if (!guard.acquired) return;
    display.clearDisplay();
    display.setCursor(0, 0);
    display.setTextSize(1);
    display.print(line_1);
    display.setCursor(0, 8);
    display.print(line_2);
    display.setCursor(0, 16);
    display.print(line_3);
    display.setCursor(0, 24);
    display.print(line_4);
    display.display();
}

void ScreenCtrl::clearDisplay() {
    if(!screenReady)return;
    I2cGuard guard;
    if (!guard.acquired) return;
    display.clearDisplay();
    display.display();
}

// Background housekeeping never waits for IMU/I2C ownership.
bool ScreenCtrl::tryIdleFrame(const String& first,const String& second,const String& third,const String& fourth) {
    if(!screenReady)return false;
    I2cGuard guard(0);
    if(!guard.acquired)return false;
    line_1=first;line_2=second;line_3=third;line_4=fourth;
    updateFrame(); // Recursive guard succeeds immediately under the outer guard.
    return true;
}
