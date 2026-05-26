/**
 * @copyright Copyright (c) 2024  Shenzhen Xin Yuan Electronic Technology Co., Ltd
 * @date      2024-04-05
 * @note      Arduino Setting
 *            Tools ->
 *                  Board:"ESP32S3 Dev Module"
 *                  USB CDC On Boot:"Enable"
 *                  USB DFU On Boot:"Disable"
 *                  Flash Size : "16MB(128Mb)"
 *                  Flash Mode"QIO 80MHz
 *                  Partition Scheme:"16M Flash(3M APP/9.9MB FATFS)"
 *                  PSRAM:"OPI PSRAM"
 *                  Upload Mode:"UART0/Hardware CDC"
 *                  USB Mode:"Hardware CDC and JTAG"
 *
 */

#ifndef BOARD_HAS_PSRAM
#error "Please enable PSRAM, Arduino IDE -> tools -> PSRAM -> OPI !!!"
#endif

#include <Arduino.h>
#include "epd_driver.h"
#include "firasans.h"
#include "esp_adc_cal.h"
#include <FS.h>
#include <SPI.h>
#include <SD.h>
#include "logo.h"
#include "Button2.h"            //Arduino IDE -> Library manager -> Install Button2
#include <Wire.h>
#include <TouchDrvGT911.hpp>    //Arduino IDE -> Library manager -> Install SensorLib v0.19     
#include <SensorPCF8563.hpp>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <esp_sntp.h>
#include "utilities.h"
#include "calc_key_icons.h"
#include "wifi_key_icons.h"
#include <math.h>
#include <time.h>

#ifndef WIFI_SSID
#define WIFI_SSID             "Your WiFi SSID"
#endif

#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD         "Your WiFi PASSWORD"
#endif


const char *ntpServer1 = "pool.ntp.org";
const char *ntpServer2 = "time.nist.gov";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;
const char *time_zone = "CST-8";  // TimeZone rule for Europe/Rome including daylight adjustment rules (optional)

Button2 btn(BUTTON_1);

SensorPCF8563 rtc;
TouchDrvGT911 touch;
Preferences wifiPrefs;
Preferences appPrefs;

uint8_t *framebuffer = NULL;
bool touchOnline = false;
uint32_t interval = 0;
int vref = 1100;
char buf[128];
uint32_t touch_loop_interval = 0;
bool found_rtc = false;
bool ntp_synced = false;
bool rtc_synced = false;
bool showingClock = false;
bool showingCalculator = false;
bool showingSettings = false;
bool showingContentSettings = false;
bool showingBookLibrary = false;
bool touchWasPressed = false;
bool lastWifiConnected = false;
volatile bool wifiStatusRefreshPending = false;
bool touchLatchActive = false;
int16_t latchedTouchX = 0;
int16_t latchedTouchY = 0;
uint32_t lastTouchReleaseTime = 0;

#define MAX_SCANNED_WIFI 10
char scanned_ssids[MAX_SCANNED_WIFI][33];
int scanned_count = 0;
bool show_password_prompt = false; // true if we clicked an SSID and are now entering the password
char wifi_ssid_input[33] = "";
char wifi_password_input[64] = "";
char saved_wifi_ssid[33] = "";
char saved_wifi_password[64] = "";
char saved_content_url[256] = "";
char content_url_input[256] = "";

#define MAX_BOOK_ITEMS 10
struct BookListItem {
    int32_t id;
    char title[80];
    char author[40];
    char category[40];
};
BookListItem book_items[MAX_BOOK_ITEMS];
int book_count = 0;
int book_total = 0;
char book_library_status[96] = "Tap book icon to load library";

enum KeyboardMode {
    KB_LOWERCASE,
    KB_UPPERCASE,
    KB_SYMBOLS
};
KeyboardMode kb_mode = KB_LOWERCASE;

const char keyboard_lowercase[3][10] = {
    {'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p'},
    {'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', '<'},
    {'z', 'x', 'c', 'v', 'b', 'n', 'm', '.', '_', '@'}
};

const char keyboard_uppercase[3][10] = {
    {'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P'},
    {'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', '<'},
    {'Z', 'X', 'C', 'V', 'B', 'N', 'M', '.', '_', '@'}
};

const char keyboard_symbols[3][10] = {
    {'-', '/', ':', ';', '(', ')', '$', '&', '*', '<'},
    {'+', '=', '%', '?', '!', '#', ',', '"', '\'', '\\'},
    {'~', '`', '|', '^', '[', ']', '{', '}', '.', '@'}
};
uint32_t clock_refresh_interval = 0;
uint32_t auto_refresh_interval = 0;
uint32_t wifi_reconnect_interval = 0;
char calcDisplay[24] = "0";
char calcExpression[64] = "0";
double calcStored = 0.0;
char calcPendingOp = 0;
bool calcNewInput = true;

struct CalcButton {
    const char *label;
    const uint8_t *icon;
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
};

static const CalcButton calcButtons[] = {
    {"C", calc_key_icon_c, 34, 270, 105, 76}, {"<-", calc_key_icon_delete, 153, 270, 105, 76}, {"/", calc_key_icon_solidus, 272, 270, 105, 76}, {"*", calc_key_icon_asterisk, 391, 270, 105, 76},
    {"7", calc_key_icon_7, 34, 362, 105, 76}, {"8", calc_key_icon_8, 153, 362, 105, 76}, {"9", calc_key_icon_9, 272, 362, 105, 76}, {"-", calc_key_icon_minus, 391, 362, 105, 76},
    {"4", calc_key_icon_4, 34, 454, 105, 76}, {"5", calc_key_icon_5, 153, 454, 105, 76}, {"6", calc_key_icon_6, 272, 454, 105, 76}, {"+", calc_key_icon_plus, 391, 454, 105, 76},
    {"1", calc_key_icon_1, 34, 546, 105, 76}, {"2", calc_key_icon_2, 153, 546, 105, 76}, {"3", calc_key_icon_3, 272, 546, 105, 76}, {"=", calc_key_icon_equal, 391, 546, 105, 168},
    {"0", calc_key_icon_0, 34, 638, 224, 76}, {".", calc_key_icon_dot, 272, 638, 105, 76},
};

static void drawPortraitHome();
static void drawAnalogClockScreen();
static void drawCalculatorScreen();
static void drawSettingsScreen();
static void drawContentSettingsScreen();
static void drawBookLibraryScreen();
static void drawBookLibraryLoadingScreen();
static void drawWifiScanningScreen();
static void drawBookIcon(int32_t x, int32_t y, int32_t w, int32_t h);
static void refreshDisplay(void (*drawFn)());
static void drawWifiPasswordInputBox();
static void drawContentUrlInputBox();
static void refreshWifiPasswordArea();
static void refreshContentUrlArea();
static void refreshWifiKeyboardArea();
static void refreshContentKeyboardArea();
static void refreshWifiStatusIconArea();
static bool pointInRect(int32_t px, int32_t py, int32_t rx, int32_t ry, int32_t rw, int32_t rh);
static bool portraitPointFromTouch(int16_t tx, int16_t ty, int32_t *px, int32_t *py, bool alternate);
static bool touchHitsPortraitRect(int16_t tx, int16_t ty, int32_t rx, int32_t ry, int32_t rw, int32_t rh);
static bool handleSettingsTouch(int16_t tx, int16_t ty);
static bool handleContentSettingsTouch(int16_t tx, int16_t ty);
static void processTouchRelease(int16_t x, int16_t y);
static bool touchHitsSettingsTile(int16_t tx, int16_t ty);
static bool touchHitsBookTile(int16_t tx, int16_t ty);
static bool touchHitsWifiStatusIcon(int16_t tx, int16_t ty);
static bool loadSavedWifiCredentials();
static void saveWifiCredentials(const char *ssid, const char *password);
static void loadContentUrl();
static void saveContentUrl();
static bool fetchBookLibrary();
static void buildBooksApiUrl(char *out, size_t outSize);

static const uint8_t clockIcon50x50[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x3F, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x01, 0xFF,
    0xFF, 0xE0, 0x00, 0x00, 0x00, 0x07, 0xFF, 0xFF, 0xF8, 0x00, 0x00, 0x00,
    0x0F, 0xE0, 0x01, 0xFC, 0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x3F, 0x00,
    0x00, 0x00, 0x7E, 0x00, 0xC0, 0x1F, 0x80, 0x00, 0x00, 0xF8, 0x00, 0xC0,
    0x07, 0xC0, 0x00, 0x01, 0xF0, 0x00, 0xC0, 0x03, 0xE0, 0x00, 0x03, 0xE0,
    0x00, 0xC0, 0x01, 0xF0, 0x00, 0x03, 0xC0, 0x00, 0xC0, 0x00, 0xF0, 0x00,
    0x07, 0x80, 0x00, 0xC0, 0x00, 0x78, 0x00, 0x0F, 0x00, 0x00, 0xC0, 0x00,
    0x3C, 0x00, 0x0F, 0x00, 0x00, 0xC0, 0x00, 0x3C, 0x00, 0x1E, 0x00, 0x00,
    0xC0, 0x00, 0x1E, 0x00, 0x1C, 0x00, 0x00, 0xC0, 0x00, 0x1E, 0x00, 0x1C,
    0x00, 0x00, 0xC0, 0x00, 0x0E, 0x00, 0x3C, 0x00, 0x00, 0xC0, 0x00, 0x0F,
    0x00, 0x38, 0x00, 0x00, 0xC0, 0x00, 0x07, 0x00, 0x38, 0x00, 0x00, 0xC0,
    0x00, 0x07, 0x00, 0x38, 0x00, 0x00, 0xC0, 0x00, 0x07, 0x00, 0x38, 0x00,
    0x03, 0xF0, 0x00, 0x07, 0x00, 0x38, 0x00, 0x03, 0xF0, 0x00, 0x07, 0x00,
    0x38, 0x00, 0x03, 0xF0, 0x00, 0x07, 0x00, 0x38, 0x00, 0x03, 0xF0, 0x00,
    0x07, 0x00, 0x38, 0x00, 0x07, 0xF0, 0x00, 0x07, 0x00, 0x38, 0x00, 0x0F,
    0xF0, 0x00, 0x07, 0x00, 0x38, 0x00, 0x1F, 0x00, 0x00, 0x07, 0x00, 0x38,
    0x00, 0x3E, 0x00, 0x00, 0x07, 0x00, 0x38, 0x00, 0x7C, 0x00, 0x00, 0x07,
    0x00, 0x3C, 0x00, 0xF8, 0x00, 0x00, 0x0F, 0x00, 0x1C, 0x00, 0xF0, 0x00,
    0x00, 0x0E, 0x00, 0x1C, 0x00, 0xE0, 0x00, 0x00, 0x1E, 0x00, 0x1E, 0x00,
    0x00, 0x00, 0x00, 0x1E, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x3C, 0x00,
    0x0F, 0x00, 0x00, 0x00, 0x00, 0x3C, 0x00, 0x07, 0x80, 0x00, 0x00, 0x00,
    0x78, 0x00, 0x03, 0xC0, 0x00, 0x00, 0x00, 0xF0, 0x00, 0x03, 0xE0, 0x00,
    0x00, 0x01, 0xF0, 0x00, 0x01, 0xF0, 0x00, 0x00, 0x03, 0xE0, 0x00, 0x00,
    0xF8, 0x00, 0x00, 0x07, 0xC0, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x1F, 0x80,
    0x00, 0x00, 0x3F, 0x00, 0x00, 0x7F, 0x00, 0x00, 0x00, 0x0F, 0xE0, 0x01,
    0xFC, 0x00, 0x00, 0x00, 0x07, 0xFF, 0xFF, 0xF8, 0x00, 0x00, 0x00, 0x01,
    0xFF, 0xFF, 0xE0, 0x00, 0x00, 0x00, 0x00, 0x3F, 0xFF, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00
};

// Portrait logical canvas. The EPD driver is fixed to the physical
// 960x540 panel, so portrait drawing is done by mapping 540x960 logical
// coordinates to the physical framebuffer rotated 90 degrees clockwise.
#define PORTRAIT_WIDTH  EPD_HEIGHT
#define PORTRAIT_HEIGHT EPD_WIDTH

static const int32_t HOME_ICON_SIZE = 118;
static const int32_t HOME_ICON_GAP = 52;
static const int32_t HOME_ICON_START_Y = 140;
static const int32_t CALC_DISPLAY_X = 24;
static const int32_t CALC_DISPLAY_Y = 88;
static const int32_t CALC_DISPLAY_W = PORTRAIT_WIDTH - 48;
static const int32_t CALC_DISPLAY_H = 126;
static const int32_t CALC_DIGITS_X = CALC_DISPLAY_X + 16;
static const int32_t CALC_DIGITS_Y = CALC_DISPLAY_Y + 8;
static const int32_t CALC_DIGITS_W = CALC_DISPLAY_W - 32;
static const int32_t CALC_DIGITS_H = CALC_DISPLAY_H - 16;
static const int32_t CALC_KEY_ICON_SCALE = 2;
static const int32_t CALC_DIGITS_RIGHT_PADDING = 16;
static const int32_t CALC_DIGITS_VERTICAL_OFFSET = 12;
static const int32_t CALC_DIGITS_REFRESH_MARGIN = 8;
static const int32_t WIFI_KBD_START_X = 10;
static const int32_t WIFI_KBD_START_Y = 330;
static const int32_t WIFI_KBD_KEY_W = 52;
static const int32_t WIFI_KBD_KEY_H = 105;
static const int32_t WIFI_KBD_GAP_Y = 5;
static const int32_t WIFI_KBD_NUMERIC_ROW = 0;
static const int32_t WIFI_KBD_LETTER_ROW_START = 1;
static const int32_t WIFI_KBD_ACTION_ROW = 4;
static const int32_t TOP_STATUS_BAR_H = 56;
static const int32_t WIFI_PASSWORD_BOX_X = 34;
static const int32_t WIFI_PASSWORD_BOX_Y = 150;
static const int32_t WIFI_PASSWORD_BOX_W = 472;
static const int32_t WIFI_PASSWORD_BOX_H = 60;
static const int32_t CONTENT_SETTINGS_TITLE_Y = 122;
static const int32_t CONTENT_URL_BOX_X = 34;
static const int32_t CONTENT_URL_BOX_Y = 150;
static const int32_t CONTENT_URL_BOX_W = 472;
static const int32_t CONTENT_URL_BOX_H = 60;

static const char wifi_keyboard_numbers[10] = {'1', '2', '3', '4', '5', '6', '7', '8', '9', '0'};

static inline int32_t homeIconStartX()
{
    return (PORTRAIT_WIDTH - (HOME_ICON_SIZE * 3 + HOME_ICON_GAP * 2)) / 2;
}

static inline int32_t wifiKeyboardRowY(int32_t row)
{
    return WIFI_KBD_START_Y + row * (WIFI_KBD_KEY_H + WIFI_KBD_GAP_Y);
}

static inline void portraitPixel(int32_t x, int32_t y, uint8_t color)
{
    if (!framebuffer || x < 0 || x >= PORTRAIT_WIDTH || y < 0 || y >= PORTRAIT_HEIGHT) {
        return;
    }
    epd_draw_pixel(y, PORTRAIT_WIDTH - 1 - x, color, framebuffer);
}

static void portraitFillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint8_t color)
{
    for (int32_t yy = y; yy < y + h; ++yy) {
        for (int32_t xx = x; xx < x + w; ++xx) {
            portraitPixel(xx, yy, color);
        }
    }
}

static void portraitDrawRect(int32_t x, int32_t y, int32_t w, int32_t h, uint8_t color)
{
    for (int32_t xx = x; xx < x + w; ++xx) {
        portraitPixel(xx, y, color);
        portraitPixel(xx, y + h - 1, color);
    }
    for (int32_t yy = y; yy < y + h; ++yy) {
        portraitPixel(x, yy, color);
        portraitPixel(x + w - 1, yy, color);
    }
}

static void portraitFillCircle(int32_t cx, int32_t cy, int32_t r, uint8_t color)
{
    int32_t rr = r * r;
    for (int32_t y = -r; y <= r; ++y) {
        for (int32_t x = -r; x <= r; ++x) {
            if (x * x + y * y <= rr) {
                portraitPixel(cx + x, cy + y, color);
            }
        }
    }
}

static void portraitDrawCircle(int32_t cx, int32_t cy, int32_t r, uint8_t color)
{
    int32_t x = 0;
    int32_t y = r;
    int32_t d = 3 - 2 * r;
    while (y >= x) {
        portraitPixel(cx + x, cy + y, color);
        portraitPixel(cx - x, cy + y, color);
        portraitPixel(cx + x, cy - y, color);
        portraitPixel(cx - x, cy - y, color);
        portraitPixel(cx + y, cy + x, color);
        portraitPixel(cx - y, cy + x, color);
        portraitPixel(cx + y, cy - x, color);
        portraitPixel(cx - y, cy - x, color);
        x++;
        if (d > 0) {
            y--;
            d += 4 * (x - y) + 10;
        } else {
            d += 4 * x + 6;
        }
    }
}

static void portraitDrawLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint8_t color)
{
    int32_t dx = abs(x1 - x0);
    int32_t sx = x0 < x1 ? 1 : -1;
    int32_t dy = -abs(y1 - y0);
    int32_t sy = y0 < y1 ? 1 : -1;
    int32_t err = dx + dy;

    while (true) {
        portraitPixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        int32_t e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static void drawThickPortraitLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t thickness, uint8_t color)
{
    int32_t radius = thickness / 2;
    for (int32_t oy = -radius; oy <= radius; ++oy) {
        for (int32_t ox = -radius; ox <= radius; ++ox) {
            portraitDrawLine(x0 + ox, y0 + oy, x1 + ox, y1 + oy, color);
        }
    }
}

static void drawHomeStatusIcon(int32_t x, int32_t y, uint8_t color)
{
    drawThickPortraitLine(x + 2, y + 18, x + 18, y + 4, 3, color);
    drawThickPortraitLine(x + 18, y + 4, x + 34, y + 18, 3, color);
    portraitFillRect(x + 8, y + 18, 21, 19, color);
    portraitFillRect(x + 16, y + 26, 6, 11, 0x00);
}

static void drawWifiStatusIcon(int32_t x, int32_t y, bool connected, uint8_t color)
{
    drawThickPortraitLine(x + 0, y + 10, x + 16, y + 0, 3, color);
    drawThickPortraitLine(x + 16, y + 0, x + 32, y + 10, 3, color);
    drawThickPortraitLine(x + 6, y + 20, x + 16, y + 13, 3, color);
    drawThickPortraitLine(x + 16, y + 13, x + 26, y + 20, 3, color);
    portraitFillCircle(x + 16, y + 30, 4, color);

    if (!connected) {
        drawThickPortraitLine(x + 26, y + 24, x + 40, y + 38, 3, color);
        drawThickPortraitLine(x + 40, y + 24, x + 26, y + 38, 3, color);
    }
}

static uint8_t getBatterySections()
{
    uint32_t batteryMv = analogReadMilliVolts(BATT_PIN) * 2;
    if (batteryMv >= 4100) {
        return 4;
    }
    if (batteryMv >= 3850) {
        return 3;
    }
    if (batteryMv >= 3600) {
        return 2;
    }
    if (batteryMv >= 3350) {
        return 1;
    }
    return 0;
}

static void drawBatteryStatusIcon(int32_t x, int32_t y, uint8_t sections, uint8_t color)
{
    portraitDrawRect(x, y, 44, 24, color);
    portraitDrawRect(x + 44, y + 7, 5, 10, color);

    for (uint8_t i = 0; i < 4; ++i) {
        int32_t sx = x + 4 + i * 10;
        if (i < sections) {
            portraitFillRect(sx, y + 4, 7, 16, color);
        } else {
            portraitDrawRect(sx, y + 4, 7, 16, color);
        }
    }
}

static void drawTopStatusBar()
{
    const int32_t barY = 0;
    const int32_t barH = TOP_STATUS_BAR_H;
    // Don't fill with black color; instead keep the background white/clear (0xFF)
    portraitFillRect(0, barY, PORTRAIT_WIDTH, barH, 0xFF);

    // Draw the home, wifi, and battery status icons in black color (0x00)
    drawHomeStatusIcon(22, barY + 9, 0x00);
    drawWifiStatusIcon(PORTRAIT_WIDTH - 136, barY + 10, WiFi.status() == WL_CONNECTED, 0x00);
    drawBatteryStatusIcon(PORTRAIT_WIDTH - 70, barY + 16, getBatterySections(), 0x00);
}

static void drawBitmapIcon1bpp(int32_t x, int32_t y, int32_t width, int32_t height, const uint8_t *bitmap, uint8_t color)
{
    int32_t stride = (width + 7) / 8;
    for (int32_t yy = 0; yy < height; ++yy) {
        for (int32_t xx = 0; xx < width; ++xx) {
            uint8_t packed = bitmap[yy * stride + xx / 8];
            if (packed & (0x80 >> (xx & 7))) {
                portraitPixel(x + xx, y + yy, color);
            }
        }
    }
}

static void drawCalcKeyIconCentered(const CalcButton &button)
{
    if (!button.icon) {
        return;
    }

    int32_t innerX = button.x + 3;
    int32_t innerY = button.y + 3;
    int32_t innerW = button.w - 6;
    int32_t innerH = button.h - 6;
    int32_t scaledW = CALC_KEY_ICON_W * CALC_KEY_ICON_SCALE;
    int32_t scaledH = CALC_KEY_ICON_H * CALC_KEY_ICON_SCALE;
    int32_t iconX = innerX + (innerW - scaledW) / 2;
    int32_t iconY = innerY + (innerH - scaledH) / 2;
    drawScaledBitmapIcon1bpp(iconX, iconY, CALC_KEY_ICON_W, CALC_KEY_ICON_H, button.icon, CALC_KEY_ICON_SCALE, 0x00);
}

static void drawScaledBitmapIcon1bpp(int32_t x, int32_t y, int32_t width, int32_t height, const uint8_t *bitmap, int32_t scale, uint8_t color)
{
    int32_t stride = (width + 7) / 8;
    for (int32_t yy = 0; yy < height; ++yy) {
        for (int32_t xx = 0; xx < width; ++xx) {
            uint8_t packed = bitmap[yy * stride + xx / 8];
            if (packed & (0x80 >> (xx & 7))) {
                portraitFillRect(x + xx * scale, y + yy * scale, scale, scale, color);
            }
        }
    }
}

static void drawPortraitTextCentered(const char *text, int32_t y, const GFXfont *font)
{
    const size_t textBufferSize = (EPD_WIDTH / 2) * PORTRAIT_HEIGHT;
    uint8_t *textBuffer = (uint8_t *)ps_calloc(sizeof(uint8_t), textBufferSize);
    if (!textBuffer) {
        return;
    }
    memset(textBuffer, 0xFF, textBufferSize);

    int32_t x = 0;
    int32_t baseY = 0;
    int32_t x1 = 0, y1 = 0, w = 0, h = 0;
    get_text_bounds(font, text, &x, &baseY, &x1, &y1, &w, &h, NULL);

    int32_t cursorX = (PORTRAIT_WIDTH - w) / 2 - x1;
    int32_t cursorY = y - y1;
    writeln(font, text, &cursorX, &cursorY, textBuffer);

    for (int32_t yy = 0; yy < PORTRAIT_HEIGHT; ++yy) {
        for (int32_t xx = 0; xx < PORTRAIT_WIDTH; ++xx) {
            uint8_t packed = textBuffer[yy * EPD_WIDTH / 2 + xx / 2];
            uint8_t color = (xx & 1) ? (packed >> 4) : (packed & 0x0F);
            // Thinnest width text: Only draw pixels that are very dark (color <= 2) to prevent bolding/antialiasing bloat
            if (color <= 2) {
                portraitPixel(xx, yy, 0x00);
            }
        }
    }

    free(textBuffer);
}

static void drawPortraitTextInRect(const char *text, int32_t rx, int32_t ry, int32_t rw, int32_t rh, const GFXfont *font)
{
    // Redirect to use drawPortraitTextInRectCentered which has mathematically perfect centering
    // that doesn't suffer from baseline/height offset issues!
    drawPortraitTextInRectCentered(text, rx, ry, rw, rh, font);
}

static void drawPortraitTextRightInRect(const char *text, int32_t rx, int32_t ry, int32_t rw, int32_t rh, const GFXfont *font)
{
    const size_t textBufferSize = (EPD_WIDTH / 2) * PORTRAIT_HEIGHT;
    uint8_t *textBuffer = (uint8_t *)ps_calloc(sizeof(uint8_t), textBufferSize);
    if (!textBuffer) {
        return;
    }
    memset(textBuffer, 0xFF, textBufferSize);

    int32_t x = 0;
    int32_t y = 0;
    int32_t x1 = 0, y1 = 0, w = 0, h = 0;
    get_text_bounds(font, text, &x, &y, &x1, &y1, &w, &h, NULL);

    int32_t cursorX = rx + rw - CALC_DIGITS_RIGHT_PADDING - w - x1;
    int32_t cursorY = ry + (rh - h) / 2 - y1 + CALC_DIGITS_VERTICAL_OFFSET;
    writeln(font, text, &cursorX, &cursorY, textBuffer);

    int32_t startY = ry < 0 ? 0 : ry;
    int32_t endY = ry + rh > PORTRAIT_HEIGHT ? PORTRAIT_HEIGHT : ry + rh;
    int32_t startX = rx < 0 ? 0 : rx;
    int32_t endX = rx + rw > PORTRAIT_WIDTH ? PORTRAIT_WIDTH : rx + rw;

    for (int32_t yy = startY; yy < endY; ++yy) {
        for (int32_t xx = startX; xx < endX; ++xx) {
            uint8_t packed = textBuffer[yy * EPD_WIDTH / 2 + xx / 2];
            uint8_t color = (xx & 1) ? (packed >> 4) : (packed & 0x0F);
            // Thinnest width text: Only draw pixels that are very dark (color <= 2) to prevent bolding/antialiasing bloat
            if (color <= 2) {
                portraitPixel(xx, yy, 0x00);
            }
        }
    }

    free(textBuffer);
}

static void drawPortraitTextInRectCentered(const char *text, int32_t rx, int32_t ry, int32_t rw, int32_t rh, const GFXfont *font)
{
    // EPD_HEIGHT is 540 and EPD_WIDTH is 960.
    // In low-level font.c, write_mode is hardcoded to clip any pixel drawn at yy >= EPD_HEIGHT (540).
    // In our portrait layout, yy is vertical and goes up to 960 (PORTRAIT_HEIGHT).
    // Therefore, any key/text with ry >= 540 gets completely clipped (drawn as empty/blank).
    // To bypass this low-level library constraint, we temporarily shift the vertical coordinate (ry)
    // up into the safe [0, 500] range, call writeln(), and then shift the pixels back down when copying!
    int32_t shiftY = 0;
    if (ry >= 400) {
        shiftY = ry - 200; // Shift up into safe bounds
    }

    const size_t textBufferSize = (EPD_WIDTH / 2) * PORTRAIT_HEIGHT;
    uint8_t *textBuffer = (uint8_t *)ps_calloc(sizeof(uint8_t), textBufferSize);
    if (!textBuffer) {
        return;
    }
    memset(textBuffer, 0xFF, textBufferSize);

    int32_t x = 0;
    int32_t y = 0;
    int32_t x1 = 0, y1 = 0, w = 0, h = 0;
    get_text_bounds(font, text, &x, &y, &x1, &y1, &w, &h, NULL);

    int32_t cursorX = rx + (rw - w) / 2 - x1;
    // Shift baseline cursorY down by +16px to compensate for low-level yy drawing offset (+height instead of -height)
    int32_t cursorY = (ry - shiftY) + (rh - h) / 2 - y1 + 16;
    writeln(font, text, &cursorX, &cursorY, textBuffer);

    // Expand the copying window vertically and horizontally by 20 pixels.
    int32_t startY = (ry - 20) < 0 ? 0 : (ry - 20);
    int32_t endY = (ry + rh + 20) > PORTRAIT_HEIGHT ? PORTRAIT_HEIGHT : (ry + rh + 20);
    int32_t startX = (rx - 20) < 0 ? 0 : (rx - 20);
    int32_t endX = (rx + rw + 20) > PORTRAIT_WIDTH ? PORTRAIT_WIDTH : (rx + rw + 20);

    for (int32_t yy = startY; yy < endY; ++yy) {
        int32_t srcY = yy - shiftY;
        if (srcY < 0 || srcY >= PORTRAIT_HEIGHT) continue;
        for (int32_t xx = startX; xx < endX; ++xx) {
            uint8_t packed = textBuffer[srcY * EPD_WIDTH / 2 + xx / 2];
            uint8_t color = (xx & 1) ? (packed >> 4) : (packed & 0x0F);
            // Thinnest width text: Only draw pixels that are very dark (color <= 2) to prevent bolding/antialiasing bloat
            if (color <= 2) {
                portraitPixel(xx, yy, 0x00);
            }
        }
    }

    free(textBuffer);
}

static void drawCalculatorDigits()
{
    portraitFillRect(CALC_DIGITS_X, CALC_DIGITS_Y, CALC_DIGITS_W, CALC_DIGITS_H, 0xFF);
    drawPortraitTextRightInRect(calcExpression, CALC_DIGITS_X, CALC_DIGITS_Y, CALC_DIGITS_W, CALC_DIGITS_H, (GFXfont *)&FiraSans);
}

static void drawSettingsIcon(int32_t x, int32_t y, int32_t size)
{
    int32_t cx = x + size / 2;
    int32_t cy = y + size / 2;

    const int32_t num_teeth = 8;
    const float R1 = 34.0f; // Base circle radius
    const float R2 = 45.0f; // Tip circle radius
    const float r_inner = 15.0f; // Inner circle radius

    // We have 32 points on the outer gear contour (8 teeth * 4 points/tooth)
    int32_t px[32];
    int32_t py[32];

    for (int i = 0; i < num_teeth; ++i) {
        float angle_center = (i * 45.0f) * DEG_TO_RAD;
        
        // 4 points per tooth to define the trapezoidal profile
        float a0 = angle_center - 13.5f * DEG_TO_RAD;
        float a1 = angle_center - 8.0f * DEG_TO_RAD;
        float a2 = angle_center + 8.0f * DEG_TO_RAD;
        float a3 = angle_center + 13.5f * DEG_TO_RAD;

        px[i * 4 + 0] = cx + (int32_t)roundf(cosf(a0) * R1);
        py[i * 4 + 0] = cy + (int32_t)roundf(sinf(a0) * R1);

        px[i * 4 + 1] = cx + (int32_t)roundf(cosf(a1) * R2);
        py[i * 4 + 1] = cy + (int32_t)roundf(sinf(a1) * R2);

        px[i * 4 + 2] = cx + (int32_t)roundf(cosf(a2) * R2);
        py[i * 4 + 2] = cy + (int32_t)roundf(sinf(a2) * R2);

        px[i * 4 + 3] = cx + (int32_t)roundf(cosf(a3) * R1);
        py[i * 4 + 3] = cy + (int32_t)roundf(sinf(a3) * R1);
    }

    // Connect the 32 points with thin lines to form a clean continuous gear outline
    for (int i = 0; i < 32; ++i) {
        int next = (i + 1) % 32;
        portraitDrawLine(px[i], py[i], px[next], py[next], 0x00);
    }

    // Draw the dotted circle outline (between r_inner and R1)
    const int32_t num_dots = 24;
    const float r_dotted = 24.5f;
    for (int i = 0; i < num_dots; ++i) {
        float angle = (i * 360.0f / (float)num_dots) * DEG_TO_RAD;
        int32_t dx = cx + (int32_t)roundf(cosf(angle) * r_dotted);
        int32_t dy = cy + (int32_t)roundf(sinf(angle) * r_dotted);
        portraitPixel(dx, dy, 0x00);
    }

    // Draw the inner circle outline
    portraitDrawCircle(cx, cy, (int32_t)r_inner, 0x00);
}

static void drawBookIcon(int32_t x, int32_t y, int32_t w, int32_t h)
{
    // Open-book icon, drawn with thin vector lines to match the home page style.
    const int32_t marginX = w / 8;
    const int32_t marginY = h / 5;
    const int32_t left = x + marginX;
    const int32_t right = x + w - marginX;
    const int32_t top = y + marginY;
    const int32_t bottom = y + h - marginY;
    const int32_t center = x + w / 2;
    const int32_t curve = 8;

    // Left cover/page outline
    portraitDrawLine(center, top + curve, left, top, 0x00);
    portraitDrawLine(left, top, left, bottom - curve, 0x00);
    portraitDrawLine(left, bottom - curve, center, bottom, 0x00);

    // Right cover/page outline
    portraitDrawLine(center, top + curve, right, top, 0x00);
    portraitDrawLine(right, top, right, bottom - curve, 0x00);
    portraitDrawLine(right, bottom - curve, center, bottom, 0x00);

    // Center spine
    portraitDrawLine(center, top + curve, center, bottom, 0x00);
    portraitDrawLine(center + 1, top + curve + 1, center + 1, bottom - 1, 0x00);

    // Page lines
    for (int32_t i = 0; i < 3; ++i) {
        int32_t yy = top + 10 + i * 9;
        portraitDrawLine(left + 9, yy, center - 9, yy + 4, 0x00);
        portraitDrawLine(center + 9, yy + 4, right - 9, yy, 0x00);
    }
}

static void drawPortraitStartup()
{
    memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);

    const size_t textBufferSize = (EPD_WIDTH / 2) * PORTRAIT_HEIGHT;
    uint8_t *textBuffer = (uint8_t *)ps_calloc(sizeof(uint8_t), textBufferSize);
    if (!textBuffer) {
        return;
    }
    memset(textBuffer, 0xFF, textBufferSize);

    int32_t x = 0, y = 0;
    int32_t x1 = 0, y1 = 0, w = 0, h = 0;
    get_text_bounds((GFXfont *)&FiraSans, "Asundar", &x, &y, &x1, &y1, &w, &h, NULL);

    int32_t cursorX = (PORTRAIT_WIDTH - w) / 2 - x1;
    int32_t cursorY = (PORTRAIT_HEIGHT - h) / 2 - y1;
    writeln((GFXfont *)&FiraSans, "Asundar", &cursorX, &cursorY, textBuffer);

    for (int32_t yy = 0; yy < PORTRAIT_HEIGHT; ++yy) {
        for (int32_t xx = 0; xx < PORTRAIT_WIDTH; ++xx) {
            uint8_t packed = textBuffer[yy * EPD_WIDTH / 2 + xx / 2];
            uint8_t color = (xx & 1) ? (packed >> 4) : (packed & 0x0F);
            if (color < 0x0F) {
                portraitPixel(xx, yy, color << 4);
            }
        }
    }

    free(textBuffer);
}

static void drawWifiScanningScreenSingleWidth(const char *text, int32_t y, const GFXfont *font)
{
    const size_t textBufferSize = (EPD_WIDTH / 2) * PORTRAIT_HEIGHT;
    uint8_t *textBuffer = (uint8_t *)ps_calloc(sizeof(uint8_t), textBufferSize);
    if (!textBuffer) {
        return;
    }
    memset(textBuffer, 0xFF, textBufferSize);

    int32_t x = 0;
    int32_t baseY = 0;
    int32_t x1 = 0, y1 = 0, w = 0, h = 0;
    get_text_bounds(font, text, &x, &baseY, &x1, &y1, &w, &h, NULL);

    int32_t cursorX = (PORTRAIT_WIDTH - w) / 2 - x1;
    int32_t cursorY = y - y1;
    writeln(font, text, &cursorX, &cursorY, textBuffer);

    for (int32_t yy = 0; yy < PORTRAIT_HEIGHT; ++yy) {
        for (int32_t xx = 0; xx < PORTRAIT_WIDTH; ++xx) {
            uint8_t packed = textBuffer[yy * EPD_WIDTH / 2 + xx / 2];
            uint8_t color = (xx & 1) ? (packed >> 4) : (packed & 0x0F);
            // Single width text: Only draw pixels that are very dark (color <= 2) to prevent bolding/antialiasing bloat
            if (color <= 2) {
                portraitPixel(xx, yy, 0x00);
            }
        }
    }

    free(textBuffer);
}

static void drawWifiScanningScreen()
{
    memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
    drawTopStatusBar();
    drawWifiScanningScreenSingleWidth("Scanning WiFi...", 400, (GFXfont *)&FiraSans);
}

static void drawWifiPasswordInputBox()
{
    portraitFillRect(WIFI_PASSWORD_BOX_X, WIFI_PASSWORD_BOX_Y, WIFI_PASSWORD_BOX_W, WIFI_PASSWORD_BOX_H, 0xFF);
    portraitDrawRect(WIFI_PASSWORD_BOX_X, WIFI_PASSWORD_BOX_Y, WIFI_PASSWORD_BOX_W, WIFI_PASSWORD_BOX_H, 0x00);
    portraitDrawRect(WIFI_PASSWORD_BOX_X + 1, WIFI_PASSWORD_BOX_Y + 1, WIFI_PASSWORD_BOX_W - 2, WIFI_PASSWORD_BOX_H - 2, 0x00);

    const char *passwordText = wifi_password_input[0] != '\0' ? wifi_password_input : "Enter Password...";
    drawPortraitTextInRect(passwordText,
                           WIFI_PASSWORD_BOX_X + 10,
                           WIFI_PASSWORD_BOX_Y + 12,
                           WIFI_PASSWORD_BOX_W - 20,
                           WIFI_PASSWORD_BOX_H,
                           (GFXfont *)&FiraSans);
}

static void drawPortraitTextInRectCenteredScaled(const char *text, int32_t rx, int32_t ry, int32_t rw, int32_t rh, const GFXfont *font, float scale)
{
    const size_t textBufferSize = (EPD_WIDTH / 2) * PORTRAIT_HEIGHT;
    uint8_t *textBuffer = (uint8_t *)ps_calloc(sizeof(uint8_t), textBufferSize);
    if (!textBuffer) {
        return;
    }
    memset(textBuffer, 0xFF, textBufferSize);

    int32_t x = 0;
    int32_t y = 0;
    int32_t x1 = 0, y1 = 0, w = 0, h = 0;
    get_text_bounds(font, text, &x, &y, &x1, &y1, &w, &h, NULL);

    // Draw full-scale text centered in a virtual bounding box scaled by 1/scale
    int32_t vrw = (int32_t)(rw / scale);
    int32_t vrh = (int32_t)(rh / scale);
    int32_t vrx = rx + (rw - vrw) / 2;
    int32_t vry = ry + (rh - vrh) / 2;

    int32_t cursorX = vrx + (vrw - w) / 2 - x1;
    
    // In Adafruit GFX, a character is drawn above the baseline. In write_mode/draw_char:
    // int32_t yy = cursor_y - glyph->top + y; (where glyph->top is negative, so yy = cursor_y + |glyph->top| + y).
    // Wait, the font drawing uses `yy = cursor_y - glyph->top + y` which is baseline + topOffset + y.
    // If we subtract y1 (which is negative, e.g. -32) from cursorY, then cursorY is baseline coordinate.
    // To completely prevent vertical clipping caused by y1 being mathematically slightly off or characters
    // drawing slightly lower than y1 bounds, we shift the vertical baseline down by 16 pixels relative to the virtual vrh.
    int32_t cursorY = vry + (vrh - h) / 2 - y1 + 16;
    writeln(font, text, &cursorX, &cursorY, textBuffer);

    int32_t cx = rx + rw / 2;
    int32_t cy = ry + rh / 2;

    // Expand the rendering scan loop horizontally and vertically (by 20px) to prevent vertical clipping.
    // Since we only draw dark pixels, this keeps the scaled characters completely unclipped.
    int32_t startY = (ry - 20) < 0 ? 0 : (ry - 20);
    int32_t endY = (ry + rh + 20) > PORTRAIT_HEIGHT ? PORTRAIT_HEIGHT : (ry + rh + 20);
    int32_t startX = (rx - 20) < 0 ? 0 : (rx - 20);
    int32_t endX = (rx + rw + 20) > PORTRAIT_WIDTH ? PORTRAIT_WIDTH : (rx + rw + 20);

    float invScale = 1.0f / scale;

    for (int32_t yy = startY; yy < endY; ++yy) {
        float srcYf = cy + (yy - cy) * invScale;
        int32_t srcY0 = (int32_t)floorf(srcYf);
        int32_t srcY1 = (int32_t)ceilf(srcYf);
        
        for (int32_t xx = startX; xx < endX; ++xx) {
            float srcXf = cx + (xx - cx) * invScale;
            int32_t srcX0 = (int32_t)floorf(srcXf);
            int32_t srcX1 = (int32_t)ceilf(srcXf);
            
            // To prevent segment loss during downscaling, we check adjacent pixels in the source buffer.
            // If any source pixel in the immediate scaled footprint is dark, we draw the destination pixel.
            bool isDark = false;
            for (int32_t sy = srcY0; sy <= srcY1; ++sy) {
                if (sy < 0 || sy >= PORTRAIT_HEIGHT) continue;
                for (int32_t sx = srcX0; sx <= srcX1; ++sx) {
                    if (sx < 0 || sx >= PORTRAIT_WIDTH) continue;
                    uint8_t packed = textBuffer[sy * EPD_WIDTH / 2 + sx / 2];
                    uint8_t color = (sx & 1) ? (packed >> 4) : (packed & 0x0F);
                    // Use a slightly more inclusive threshold (color <= 5) for sampling during scaling so thin segments are preserved
                    if (color <= 5) {
                        isDark = true;
                        break;
                    }
                }
                if (isDark) break;
            }
            
            if (isDark) {
                portraitPixel(xx, yy, 0x00);
            }
        }
    }

    free(textBuffer);
}

static void drawSettingsScreen()
{
    memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
    drawTopStatusBar();

    if (!show_password_prompt) {
        // Page 1: List scanned SSIDs
        drawPortraitTextCentered("Select WiFi Network", 100, (GFXfont *)&FiraSans);

        if (scanned_count <= 0) {
            drawPortraitTextCentered("No networks found", 400, (GFXfont *)&FiraSans);
            // Draw a neat "Retry" button (No frame)
            drawPortraitTextInRect("RETRY SCAN", 153, 500 + 15, 234, 60, (GFXfont *)&FiraSans);
        } else {
            // Draw list of networks (up to 10) (No frames, shifted down by 15px to prevent upper letters from being blocked)
            int num_items = scanned_count > 10 ? 10 : scanned_count;
            for (int i = 0; i < num_items; ++i) {
                int y = 140 + i * 65;
                // Reduce the SSID font size by 20% (scale = 0.8f)
                drawPortraitTextInRectCenteredScaled(scanned_ssids[i], 54, y + 15, 432, 55, (GFXfont *)&FiraSans, 0.8f);
            }
        }
    } else {
        // Page 2: Password entry window
        char ssid_label[64];
        snprintf(ssid_label, sizeof(ssid_label), "SSID: %s", wifi_ssid_input);
        drawPortraitTextCentered(ssid_label, 100, (GFXfont *)&FiraSans);

        drawWifiPasswordInputBox();

        // CONNECT button (Outline and thin lines only)
        portraitDrawRect(34, 230, 226, 60, 0x00);
        portraitDrawRect(37, 233, 220, 54, 0x00);
        drawPortraitTextInRectCentered("CONNECT", 34, 242, 226, 60, (GFXfont *)&FiraSans);

        // CANCEL button (Outline and thin lines only)
        portraitDrawRect(280, 230, 226, 60, 0x00);
        portraitDrawRect(283, 233, 220, 54, 0x00);
        drawPortraitTextInRectCentered("CANCEL", 280, 242, 226, 60, (GFXfont *)&FiraSans);

        // Draw keyboard in the lower half of the portrait screen.  The first
        // row is a dedicated number row, followed by three letter/symbol rows
        // and one action row; the shared constants keep drawing and touch
        // hit-testing aligned.
        int startX = WIFI_KBD_START_X;
        int keyW = WIFI_KBD_KEY_W;
        int keyH = WIFI_KBD_KEY_H;

        const char (*current_kb)[10];
        if (kb_mode == KB_LOWERCASE) {
            current_kb = keyboard_lowercase;
        } else if (kb_mode == KB_UPPERCASE) {
            current_kb = keyboard_uppercase;
        } else {
            current_kb = keyboard_symbols;
        }

        for (int c = 0; c < 10; ++c) {
            char ch = wifi_keyboard_numbers[c];
            int x = startX + c * keyW;
            int y = wifiKeyboardRowY(WIFI_KBD_NUMERIC_ROW);
            portraitDrawRect(x, y, keyW, keyH, 0x00);

            char label[2] = {ch, '\0'};
            drawPortraitTextInRectCentered(label, x, y, keyW, keyH, (GFXfont *)&FiraSans);
        }

        for (int r = 0; r < 3; ++r) {
            int y = wifiKeyboardRowY(WIFI_KBD_LETTER_ROW_START + r);
            for (int c = 0; c < 10; ++c) {
                char ch = current_kb[r][c];
                int x = startX + c * keyW;
                
                portraitDrawRect(x, y, keyW, keyH, 0x00);
                if (ch != '\0') {
                    char label[2] = {ch, '\0'};
                    if (ch == '<') {
                        drawPortraitTextInRectCentered("<-", x, y, keyW, keyH, (GFXfont *)&FiraSans);
                    } else {
                        drawPortraitTextInRectCentered(label, x, y, keyW, keyH, (GFXfont *)&FiraSans);
                    }
                }
            }
        }

        // Action row: Mode, Space, Clear
        int y3 = wifiKeyboardRowY(WIFI_KBD_ACTION_ROW);
        // Mode key (2 cols)
        portraitDrawRect(startX, y3, keyW * 2, keyH, 0x00);
        if (kb_mode == KB_LOWERCASE) {
            drawPortraitTextInRect("ABC", startX, y3, keyW * 2, keyH, (GFXfont *)&FiraSans);
        } else if (kb_mode == KB_UPPERCASE) {
            drawPortraitTextInRect("123", startX, y3, keyW * 2, keyH, (GFXfont *)&FiraSans);
        } else {
            drawPortraitTextInRect("abc", startX, y3, keyW * 2, keyH, (GFXfont *)&FiraSans);
        }

        // Space key (6 cols)
        portraitDrawRect(startX + keyW * 2, y3, keyW * 6, keyH, 0x00);
        drawPortraitTextInRect("SPACE", startX + keyW * 2, y3, keyW * 6, keyH, (GFXfont *)&FiraSans);

        // Clear key (2 cols)
        portraitDrawRect(startX + keyW * 8, y3, keyW * 2, keyH, 0x00);
        drawPortraitTextInRect("CLR", startX + keyW * 8, y3, keyW * 2, keyH, (GFXfont *)&FiraSans);
    }
}

static void drawContentUrlInputBox()
{
    portraitFillRect(CONTENT_URL_BOX_X, CONTENT_URL_BOX_Y, CONTENT_URL_BOX_W, CONTENT_URL_BOX_H, 0xFF);
    portraitDrawRect(CONTENT_URL_BOX_X, CONTENT_URL_BOX_Y, CONTENT_URL_BOX_W, CONTENT_URL_BOX_H, 0x00);
    portraitDrawRect(CONTENT_URL_BOX_X + 1, CONTENT_URL_BOX_Y + 1, CONTENT_URL_BOX_W - 2, CONTENT_URL_BOX_H - 2, 0x00);

    const char *urlText = content_url_input[0] != '\0' ? content_url_input : "https://";
    drawPortraitTextInRectCenteredScaled(urlText,
                                         CONTENT_URL_BOX_X + 10,
                                         CONTENT_URL_BOX_Y + 12,
                                         CONTENT_URL_BOX_W - 20,
                                         CONTENT_URL_BOX_H,
                                         (GFXfont *)&FiraSans,
                                         0.58f);
}

static void drawContentSettingsScreen()
{
    memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
    drawTopStatusBar();

    drawPortraitTextCentered("Content Settings", CONTENT_SETTINGS_TITLE_Y, (GFXfont *)&FiraSans);

    // First row: URL input box. Keep the page title separate so the URL label is not repeated.
    drawContentUrlInputBox();

    // SAVE and CLEAR controls above the shared on-screen keyboard.
    portraitDrawRect(34, 230, 226, 60, 0x00);
    portraitDrawRect(37, 233, 220, 54, 0x00);
    drawPortraitTextInRectCentered("SAVE", 34, 242, 226, 60, (GFXfont *)&FiraSans);

    portraitDrawRect(280, 230, 226, 60, 0x00);
    portraitDrawRect(283, 233, 220, 54, 0x00);
    drawPortraitTextInRectCentered("CLEAR", 280, 242, 226, 60, (GFXfont *)&FiraSans);

    int startX = WIFI_KBD_START_X;
    int keyW = WIFI_KBD_KEY_W;
    int keyH = WIFI_KBD_KEY_H;

    const char (*current_kb)[10];
    if (kb_mode == KB_LOWERCASE) {
        current_kb = keyboard_lowercase;
    } else if (kb_mode == KB_UPPERCASE) {
        current_kb = keyboard_uppercase;
    } else {
        current_kb = keyboard_symbols;
    }

    for (int c = 0; c < 10; ++c) {
        char ch = wifi_keyboard_numbers[c];
        int x = startX + c * keyW;
        int y = wifiKeyboardRowY(WIFI_KBD_NUMERIC_ROW);
        portraitDrawRect(x, y, keyW, keyH, 0x00);
        char label[2] = {ch, '\0'};
        drawPortraitTextInRectCentered(label, x, y, keyW, keyH, (GFXfont *)&FiraSans);
    }

    for (int r = 0; r < 3; ++r) {
        int y = wifiKeyboardRowY(WIFI_KBD_LETTER_ROW_START + r);
        for (int c = 0; c < 10; ++c) {
            char ch = current_kb[r][c];
            int x = startX + c * keyW;
            portraitDrawRect(x, y, keyW, keyH, 0x00);
            if (ch != '\0') {
                char label[2] = {ch, '\0'};
                if (ch == '<') {
                    drawPortraitTextInRectCentered("<-", x, y, keyW, keyH, (GFXfont *)&FiraSans);
                } else {
                    drawPortraitTextInRectCentered(label, x, y, keyW, keyH, (GFXfont *)&FiraSans);
                }
            }
        }
    }

    int y3 = wifiKeyboardRowY(WIFI_KBD_ACTION_ROW);
    portraitDrawRect(startX, y3, keyW * 2, keyH, 0x00);
    if (kb_mode == KB_LOWERCASE) {
        drawPortraitTextInRect("ABC", startX, y3, keyW * 2, keyH, (GFXfont *)&FiraSans);
    } else if (kb_mode == KB_UPPERCASE) {
        drawPortraitTextInRect("123", startX, y3, keyW * 2, keyH, (GFXfont *)&FiraSans);
    } else {
        drawPortraitTextInRect("abc", startX, y3, keyW * 2, keyH, (GFXfont *)&FiraSans);
    }

    portraitDrawRect(startX + keyW * 2, y3, keyW * 6, keyH, 0x00);
    drawPortraitTextInRect("SPACE", startX + keyW * 2, y3, keyW * 6, keyH, (GFXfont *)&FiraSans);

    portraitDrawRect(startX + keyW * 8, y3, keyW * 2, keyH, 0x00);
    drawPortraitTextInRect("CLR", startX + keyW * 8, y3, keyW * 2, keyH, (GFXfont *)&FiraSans);
}

static bool handleContentSettingsTouch(int16_t tx, int16_t ty)
{
    int32_t px = 0;
    int32_t py = 0;
    if (!portraitPointFromTouch(tx, ty, &px, &py, true)) {
        if (!portraitPointFromTouch(tx, ty, &px, &py, false)) {
            return false;
        }
    }

    if (pointInRect(px, py, 34, 230, 226, 60)) {
        saveContentUrl();
        drawContentUrlInputBox();
        refreshContentUrlArea();
        return true;
    }

    if (pointInRect(px, py, 280, 230, 226, 60)) {
        content_url_input[0] = '\0';
        refreshContentUrlArea();
        return true;
    }

    int startX = WIFI_KBD_START_X;
    int keyW = WIFI_KBD_KEY_W;
    int keyH = WIFI_KBD_KEY_H;

    int numberY = wifiKeyboardRowY(WIFI_KBD_NUMERIC_ROW);
    if (py >= numberY && py < numberY + keyH) {
        int c = (px - startX) / keyW;
        if (c >= 0 && c < 10) {
            size_t len = strlen(content_url_input);
            if (len < sizeof(content_url_input) - 1) {
                content_url_input[len] = wifi_keyboard_numbers[c];
                content_url_input[len + 1] = '\0';
            }
            refreshContentUrlArea();
            return true;
        }
    }

    int letterStartY = wifiKeyboardRowY(WIFI_KBD_LETTER_ROW_START);
    if (py >= letterStartY && py < letterStartY + 3 * (keyH + WIFI_KBD_GAP_Y)) {
        int r = (py - letterStartY) / (keyH + WIFI_KBD_GAP_Y);
        int c = (px - startX) / keyW;
        if (c >= 0 && c < 10) {
            const char (*current_kb)[10];
            if (kb_mode == KB_LOWERCASE) {
                current_kb = keyboard_lowercase;
            } else if (kb_mode == KB_UPPERCASE) {
                current_kb = keyboard_uppercase;
            } else {
                current_kb = keyboard_symbols;
            }
            char ch = current_kb[r][c];
            if (ch != '\0') {
                if (ch == '<') {
                    size_t len = strlen(content_url_input);
                    if (len > 0) {
                        content_url_input[len - 1] = '\0';
                    }
                } else {
                    size_t len = strlen(content_url_input);
                    if (len < sizeof(content_url_input) - 1) {
                        content_url_input[len] = ch;
                        content_url_input[len + 1] = '\0';
                    }
                }
                refreshContentUrlArea();
                return true;
            }
        }
    }

    int y3 = wifiKeyboardRowY(WIFI_KBD_ACTION_ROW);
    if (py >= y3 && py < y3 + keyH) {
        if (px >= startX && px < startX + keyW * 2) {
            if (kb_mode == KB_LOWERCASE) {
                kb_mode = KB_UPPERCASE;
            } else if (kb_mode == KB_UPPERCASE) {
                kb_mode = KB_SYMBOLS;
            } else {
                kb_mode = KB_LOWERCASE;
            }
            refreshContentKeyboardArea();
            return true;
        }
        if (px >= startX + keyW * 2 && px < startX + keyW * 8) {
            size_t len = strlen(content_url_input);
            if (len < sizeof(content_url_input) - 1) {
                content_url_input[len] = ' ';
                content_url_input[len + 1] = '\0';
            }
            refreshContentUrlArea();
            return true;
        }
        if (px >= startX + keyW * 8 && px < startX + keyW * 10) {
            content_url_input[0] = '\0';
            refreshContentUrlArea();
            return true;
        }
    }

    return false;
}

static bool handleSettingsTouch(int16_t tx, int16_t ty)
{
    int32_t px = 0;
    int32_t py = 0;
    if (!portraitPointFromTouch(tx, ty, &px, &py, true)) {
        if (!portraitPointFromTouch(tx, ty, &px, &py, false)) {
            return false;
        }
    }

    if (!show_password_prompt) {
        // Page 1: List selection
        if (scanned_count == 0) {
            // Retry scan button
            if (pointInRect(px, py, 153, 500, 234, 60)) {
                refreshDisplay(drawWifiScanningScreen);
                scanned_count = WiFi.scanNetworks();
                if (scanned_count > MAX_SCANNED_WIFI) {
                    scanned_count = MAX_SCANNED_WIFI;
                }
                for (int i = 0; i < scanned_count; ++i) {
                    strncpy(scanned_ssids[i], WiFi.SSID(i).c_str(), 32);
                    scanned_ssids[i][32] = '\0';
                }
                refreshDisplay(drawSettingsScreen);
                return true;
            }
        } else {
            int num_items = scanned_count > 10 ? 10 : scanned_count;
            for (int i = 0; i < num_items; ++i) {
                int y = 140 + i * 65;
                if (pointInRect(px, py, 34, y, 472, 55)) {
                    strncpy(wifi_ssid_input, scanned_ssids[i], sizeof(wifi_ssid_input) - 1);
                    wifi_ssid_input[sizeof(wifi_ssid_input) - 1] = '\0';
                    wifi_password_input[0] = '\0';
                    show_password_prompt = true;
                    refreshDisplay(drawSettingsScreen);
                    return true;
                }
            }
        }
    } else {
        // Page 2: Password Prompt & Keyboard

        // Cancel button click
        if (pointInRect(px, py, 280, 230, 226, 60)) {
            show_password_prompt = false;
            refreshDisplay(drawSettingsScreen);
            return true;
        }

        // Connect button click
        if (pointInRect(px, py, 34, 230, 226, 60)) {
            if (wifi_ssid_input[0] != '\0') {
                // Show connecting screen
                memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
                drawTopStatusBar();
                drawPortraitTextCentered("Connecting...", 400, (GFXfont *)&FiraSans);
                epd_poweron();
                epd_clear();
                epd_draw_grayscale_image(epd_full_screen(), framebuffer);
                epd_poweroff();

                // Always connect with the SSID selected from the scan list and
                // the password typed in the on-screen password box. Fully stop
                // the previous/default session first so ESP32 does not reuse
                // the compile-time WIFI_SSID/WIFI_PASSWORD connection.
                Serial.printf("Connecting to selected SSID: %s\n", wifi_ssid_input);
                WiFi.disconnect(true, true);
                delay(200);
                WiFi.mode(WIFI_STA);
                WiFi.setAutoReconnect(true);
                lastWifiConnected = false;
                WiFi.begin(wifi_ssid_input, wifi_password_input);
                
                // Wait up to 10 seconds
                int timeout = 0;
                while (WiFi.status() != WL_CONNECTED && timeout < 20) {
                    delay(500);
                    timeout++;
                }
                
                if (WiFi.status() == WL_CONNECTED) {
                    lastWifiConnected = true;
                    saveWifiCredentials(wifi_ssid_input, wifi_password_input);
                    memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
                    drawTopStatusBar();
                    drawPortraitTextCentered("Connected", 400, (GFXfont *)&FiraSans);
                    epd_poweron();
                    epd_clear();
                    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
                    epd_poweroff();
                    delay(1200);

                    showingSettings = false;
                    show_password_prompt = false;
                    refreshDisplay(drawPortraitHome);
                    refreshWifiStatusIconArea();
                } else {
                    lastWifiConnected = false;
                    // Connection failed screen
                    memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
                    drawTopStatusBar();
                    drawPortraitTextCentered("Connection Failed", 400, (GFXfont *)&FiraSans);
                    refreshDisplay(drawSettingsScreen);
                    delay(2000);
                    refreshDisplay(drawSettingsScreen);
                }
            }
            return true;
        }

        int startX = WIFI_KBD_START_X;
        int keyW = WIFI_KBD_KEY_W;
        int keyH = WIFI_KBD_KEY_H;

        int numberY = wifiKeyboardRowY(WIFI_KBD_NUMERIC_ROW);
        if (py >= numberY && py < numberY + keyH) {
            int c = (px - startX) / keyW;
            if (c >= 0 && c < 10) {
                size_t len = strlen(wifi_password_input);
                if (len < sizeof(wifi_password_input) - 1) {
                    wifi_password_input[len] = wifi_keyboard_numbers[c];
                    wifi_password_input[len + 1] = '\0';
                }
                refreshWifiPasswordArea();
                return true;
            }
        }

        int letterStartY = wifiKeyboardRowY(WIFI_KBD_LETTER_ROW_START);
        if (py >= letterStartY && py < letterStartY + 3 * (keyH + WIFI_KBD_GAP_Y)) {
            int r = (py - letterStartY) / (keyH + WIFI_KBD_GAP_Y);
            int c = (px - startX) / keyW;
            if (c >= 0 && c < 10) {
                const char (*current_kb)[10];
                if (kb_mode == KB_LOWERCASE) {
                    current_kb = keyboard_lowercase;
                } else if (kb_mode == KB_UPPERCASE) {
                    current_kb = keyboard_uppercase;
                } else {
                    current_kb = keyboard_symbols;
                }
                char ch = current_kb[r][c];
                if (ch != '\0') {
                    if (ch == '<') {
                        // Backspace
                        size_t len = strlen(wifi_password_input);
                        if (len > 0) {
                            wifi_password_input[len - 1] = '\0';
                        }
                    } else {
                        // Append char
                        size_t len = strlen(wifi_password_input);
                        if (len < sizeof(wifi_password_input) - 1) {
                            wifi_password_input[len] = ch;
                            wifi_password_input[len + 1] = '\0';
                        }
                    }
                    refreshWifiPasswordArea();
                    return true;
                }
            }
        }

        // Action row keys touch
        int y3 = wifiKeyboardRowY(WIFI_KBD_ACTION_ROW);
        if (py >= y3 && py < y3 + keyH) {
            // Mode key (0-104)
            if (px >= startX && px < startX + keyW * 2) {
                if (kb_mode == KB_LOWERCASE) {
                    kb_mode = KB_UPPERCASE;
                } else if (kb_mode == KB_UPPERCASE) {
                    kb_mode = KB_SYMBOLS;
                } else {
                    kb_mode = KB_LOWERCASE;
                }
                refreshWifiKeyboardArea();
                return true;
            }
            // Space key (104-416)
            if (px >= startX + keyW * 2 && px < startX + keyW * 8) {
                size_t len = strlen(wifi_password_input);
                if (len < sizeof(wifi_password_input) - 1) {
                    wifi_password_input[len] = ' ';
                    wifi_password_input[len + 1] = '\0';
                }
                refreshWifiPasswordArea();
                return true;
            }
            // Clear key (416-520)
            if (px >= startX + keyW * 8 && px < startX + keyW * 10) {
                wifi_password_input[0] = '\0';
                refreshWifiPasswordArea();
                return true;
            }
        }
    }

    return false;
}

static void drawPortraitHome()
{
    memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);

    portraitDrawRect(18, 18, PORTRAIT_WIDTH - 36, PORTRAIT_HEIGHT - 36, 0x00);
    drawTopStatusBar();

    const int32_t icon = HOME_ICON_SIZE;
    const int32_t gap = HOME_ICON_GAP;
    const int32_t startX = homeIconStartX();
    const int32_t startY = HOME_ICON_START_Y;

    // Simple recognisable portrait-mode icons: settings, calculator, clock (Thin outlines, no fill)
    int32_t sx = startX;
    int32_t sy = startY;
    drawSettingsIcon(sx, sy, icon);

    // Book icon placed directly below the settings icon.
    drawBookIcon(sx, sy + icon + gap, icon, icon);

    int32_t cx = startX + icon + gap;
    int32_t cy = startY;
    // Calculator Icon (Outline, no fill)
    portraitDrawRect(cx + 28, cy + 24, 62, 18, 0x00);
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            portraitDrawRect(cx + 28 + c * 24, cy + 54 + r * 18, 14, 10, 0x00);
        }
    }

    int32_t kx = startX + (icon + gap) * 2;
    int32_t ky = startY;
    // Clock Icon (Outline, thinnest line as possible, no fill)
    int32_t clock_cx = kx + 59;
    int32_t clock_cy = ky + 59;
    portraitDrawCircle(clock_cx, clock_cy, 45, 0x00);
    portraitDrawCircle(clock_cx, clock_cy, 2, 0x00);
    portraitDrawLine(clock_cx, clock_cy, clock_cx - 15, clock_cy - 12, 0x00); // Hour hand
    portraitDrawLine(clock_cx, clock_cy, clock_cx + 25, clock_cy - 15, 0x00); // Minute hand

}

static void drawBookLibraryLoadingScreen()
{
    memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
    drawTopStatusBar();
    drawPortraitTextCentered("Loading Books...", 360, (GFXfont *)&FiraSans);
}

static void drawBookLibraryScreen()
{
    memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
    drawTopStatusBar();

    drawPortraitTextCentered("Book Library", 102, (GFXfont *)&FiraSans);

    if (book_count <= 0) {
        drawPortraitTextInRectCenteredScaled(book_library_status,
                                             34,
                                             250,
                                             PORTRAIT_WIDTH - 68,
                                             80,
                                             (GFXfont *)&FiraSans,
                                             0.72f);
        return;
    }

    char summary[48];
    snprintf(summary, sizeof(summary), "%d of %d books", book_count, book_total);
    drawPortraitTextInRectCenteredScaled(summary, 54, 130, PORTRAIT_WIDTH - 108, 40, (GFXfont *)&FiraSans, 0.58f);

    const int32_t listX = 28;
    const int32_t listY = 185;
    const int32_t rowH = 68;
    const int32_t listW = PORTRAIT_WIDTH - 56;

    for (int i = 0; i < book_count; ++i) {
        const int32_t y = listY + i * rowH;
        portraitDrawRect(listX, y, listW, rowH - 8, 0x00);

        char titleLine[112];
        snprintf(titleLine, sizeof(titleLine), "%d. %s", i + 1, book_items[i].title);
        drawPortraitTextInRectCenteredScaled(titleLine,
                                             listX + 10,
                                             y + 4,
                                             listW - 20,
                                             30,
                                             (GFXfont *)&FiraSans,
                                             0.55f);

        char metaLine[96];
        if (book_items[i].category[0] != '\0' && book_items[i].author[0] != '\0') {
            snprintf(metaLine, sizeof(metaLine), "%s  %s", book_items[i].category, book_items[i].author);
        } else if (book_items[i].category[0] != '\0') {
            snprintf(metaLine, sizeof(metaLine), "%s", book_items[i].category);
        } else {
            snprintf(metaLine, sizeof(metaLine), "ID %ld", (long)book_items[i].id);
        }
        drawPortraitTextInRectCenteredScaled(metaLine,
                                             listX + 10,
                                             y + 32,
                                             listW - 20,
                                             24,
                                             (GFXfont *)&FiraSans,
                                             0.42f);
    }
}

static void drawAnalogClockScreen()
{
    memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
    drawTopStatusBar();

    time_t now = time(NULL);
    struct tm timeinfo;
    if (now < 100000 || !localtime_r(&now, &timeinfo)) {
        memset(&timeinfo, 0, sizeof(timeinfo));
        timeinfo.tm_hour = 10;
        timeinfo.tm_min = 10;
        timeinfo.tm_sec = 0;
    }

    const int32_t cx = PORTRAIT_WIDTH / 2;
    const int32_t cy = 360;
    const int32_t r = 180;
    portraitDrawCircle(cx, cy, r, 0x00);
    portraitDrawCircle(cx, cy, r - 1, 0x00);
    portraitDrawCircle(cx, cy, r - 2, 0x00);

    for (int i = 0; i < 60; ++i) {
        float a = (i * 6.0f - 90.0f) * DEG_TO_RAD;
        int32_t outerX = cx + (int32_t)(cosf(a) * (r - 10));
        int32_t outerY = cy + (int32_t)(sinf(a) * (r - 10));
        int32_t innerR = (i % 5 == 0) ? r - 34 : r - 22;
        int32_t innerX = cx + (int32_t)(cosf(a) * innerR);
        int32_t innerY = cy + (int32_t)(sinf(a) * innerR);
        drawThickPortraitLine(innerX, innerY, outerX, outerY, (i % 5 == 0) ? 4 : 2, 0x00);
    }

    float minuteAngle = ((timeinfo.tm_min + timeinfo.tm_sec / 60.0f) * 6.0f - 90.0f) * DEG_TO_RAD;
    float hourAngle = (((timeinfo.tm_hour % 12) + timeinfo.tm_min / 60.0f) * 30.0f - 90.0f) * DEG_TO_RAD;
    int32_t hourX = cx + (int32_t)(cosf(hourAngle) * 82);
    int32_t hourY = cy + (int32_t)(sinf(hourAngle) * 82);
    int32_t minuteX = cx + (int32_t)(cosf(minuteAngle) * 132);
    int32_t minuteY = cy + (int32_t)(sinf(minuteAngle) * 132);
    drawThickPortraitLine(cx, cy, hourX, hourY, 8, 0x00);
    drawThickPortraitLine(cx, cy, minuteX, minuteY, 5, 0x00);
    portraitFillCircle(cx, cy, 10, 0x00);

    char weatherLine[64];
    snprintf(weatherLine, sizeof(weatherLine), "24C  Sunny");
    drawPortraitTextCentered(weatherLine, cy + r + 80, (GFXfont *)&FiraSans);
}

static void drawCalculatorScreen()
{
    memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
    drawTopStatusBar();

    portraitDrawRect(CALC_DISPLAY_X, CALC_DISPLAY_Y, CALC_DISPLAY_W, CALC_DISPLAY_H, 0x00);
    portraitDrawRect(CALC_DISPLAY_X + 4, CALC_DISPLAY_Y + 4, CALC_DISPLAY_W - 8, CALC_DISPLAY_H - 8, 0x00);
    drawCalculatorDigits();

    for (size_t i = 0; i < sizeof(calcButtons) / sizeof(calcButtons[0]); ++i) {
        const CalcButton &b = calcButtons[i];
        portraitDrawRect(b.x, b.y, b.w, b.h, 0x00);
        portraitDrawRect(b.x + 3, b.y + 3, b.w - 6, b.h - 6, 0x00);
        // Draw crisp thinnest-possible vector text for all calculator keys
        drawPortraitTextInRectCentered(b.label, b.x, b.y, b.w, b.h, (GFXfont *)&FiraSans);
    }
}

static void drawCalculatorResultArea()
{
    drawCalculatorDigits();
}

static Rect_t portraitRectToPhysicalRect(int32_t x, int32_t y, int32_t w, int32_t h)
{
    Rect_t area;
    area.x = y;
    area.y = PORTRAIT_WIDTH - x - w;
    area.width = h;
    area.height = w;
    return area;
}

static uint8_t framebufferPixel(int32_t x, int32_t y)
{
    uint8_t packed = framebuffer[y * EPD_WIDTH / 2 + x / 2];
    return (x & 1) ? (packed >> 4) : (packed & 0x0F);
}

static void setPackedPixel(uint8_t *buffer, int32_t width, int32_t x, int32_t y, uint8_t color)
{
    int32_t stride = width / 2 + width % 2;
    uint8_t *packed = &buffer[y * stride + x / 2];
    if (x & 1) {
        *packed = (*packed & 0x0F) | (color << 4);
    } else {
        *packed = (*packed & 0xF0) | (color & 0x0F);
    }
}

static uint8_t *copyPhysicalAreaFromFramebuffer(Rect_t area)
{
    int32_t stride = area.width / 2 + area.width % 2;
    uint8_t *areaBuffer = (uint8_t *)ps_calloc(sizeof(uint8_t), stride * area.height);
    if (!areaBuffer) {
        return NULL;
    }
    memset(areaBuffer, 0xFF, stride * area.height);

    for (int32_t yy = 0; yy < area.height; ++yy) {
        int32_t srcY = area.y + yy;
        if (srcY < 0 || srcY >= EPD_HEIGHT) {
            continue;
        }
        for (int32_t xx = 0; xx < area.width; ++xx) {
            int32_t srcX = area.x + xx;
            if (srcX < 0 || srcX >= EPD_WIDTH) {
                continue;
            }
            setPackedPixel(areaBuffer, area.width, xx, yy, framebufferPixel(srcX, srcY));
        }
    }

    return areaBuffer;
}

static void refreshCalculatorResultArea()
{
    refreshCalculatorResultArea(calcExpression);
}

static void calcExpressionTextBounds(const char *text, int32_t *rx, int32_t *ry, int32_t *rw, int32_t *rh)
{
    int32_t x = 0;
    int32_t y = 0;
    int32_t x1 = 0, y1 = 0, w = 0, h = 0;
    get_text_bounds((GFXfont *)&FiraSans, text, &x, &y, &x1, &y1, &w, &h, NULL);

    int32_t cursorX = CALC_DIGITS_X + CALC_DIGITS_W - CALC_DIGITS_RIGHT_PADDING - w - x1;
    int32_t cursorY = CALC_DIGITS_Y + (CALC_DIGITS_H - h) / 2 - y1 + CALC_DIGITS_VERTICAL_OFFSET;

    *rx = cursorX + x1;
    *ry = cursorY + y1;
    *rw = w;
    *rh = h;
}

static void expandAndClipCalcRefreshRect(int32_t *x, int32_t *y, int32_t *w, int32_t *h)
{
    int32_t left = *x - CALC_DIGITS_REFRESH_MARGIN;
    int32_t top = *y - CALC_DIGITS_REFRESH_MARGIN;
    int32_t right = *x + *w + CALC_DIGITS_REFRESH_MARGIN;
    int32_t bottom = *y + *h + CALC_DIGITS_REFRESH_MARGIN;

    int32_t clipLeft = CALC_DIGITS_X;
    int32_t clipTop = CALC_DIGITS_Y;
    int32_t clipRight = CALC_DIGITS_X + CALC_DIGITS_W;
    int32_t clipBottom = CALC_DIGITS_Y + CALC_DIGITS_H;

    if (left < clipLeft) left = clipLeft;
    if (top < clipTop) top = clipTop;
    if (right > clipRight) right = clipRight;
    if (bottom > clipBottom) bottom = clipBottom;

    *x = left;
    *y = top;
    *w = right - left;
    *h = bottom - top;
}

static void refreshCalculatorResultArea(const char *previousExpression)
{
    (void)previousExpression;

    // Any calculator update redraws the whole result/digit area.  This is a
    // little larger than the old text-bounds-only refresh, but it guarantees
    // every changed value/operator/backspace result is visibly refreshed and
    // prevents stale digit remnants on the e-paper panel.
    const int32_t margin = CALC_DIGITS_REFRESH_MARGIN;
    int32_t refreshX = CALC_DIGITS_X - margin;
    int32_t refreshY = CALC_DIGITS_Y - margin;
    int32_t refreshW = CALC_DIGITS_W + margin * 2;
    int32_t refreshH = CALC_DIGITS_H + margin * 2;

    Rect_t area = portraitRectToPhysicalRect(refreshX, refreshY, refreshW, refreshH);

    epd_poweron();
    // First wipe the physical result area to pure white. This happens before
    // drawing/copying the new result so number/operator updates do not blend
    // with stale pixels from the previous calculator display value.
    epd_push_pixels(area, 80, 1);
    epd_push_pixels(area, 80, 1);

    drawCalculatorResultArea();
    uint8_t *areaBuffer = copyPhysicalAreaFromFramebuffer(area);
    if (!areaBuffer) {
        epd_poweroff();
        return;
    }

    // Now write the updated result framebuffer into the already-cleared area.
    epd_draw_grayscale_image(area, areaBuffer);
    epd_poweroff();
    free(areaBuffer);
}

static void formatCalcValue(double value)
{
    if (!isfinite(value)) {
        snprintf(calcDisplay, sizeof(calcDisplay), "Error");
        snprintf(calcExpression, sizeof(calcExpression), "Error");
        calcNewInput = true;
        return;
    }
    snprintf(calcDisplay, sizeof(calcDisplay), "%.10g", value);
    snprintf(calcExpression, sizeof(calcExpression), "%s", calcDisplay);
    calcNewInput = true;
}

static void appendCalcExpression(const char *label)
{
    if (strcmp(calcExpression, "0") == 0 || strcmp(calcExpression, "Error") == 0) {
        snprintf(calcExpression, sizeof(calcExpression), "%s", label);
        return;
    }
    if (strlen(calcExpression) + strlen(label) < sizeof(calcExpression) - 1) {
        strncat(calcExpression, label, sizeof(calcExpression) - strlen(calcExpression) - 1);
    }
}

static void appendCalcOperatorToExpression(char op)
{
    size_t len = strlen(calcExpression);
    if (len > 0 && strchr("+-*/", calcExpression[len - 1])) {
        calcExpression[len - 1] = op;
        return;
    }
    char opText[2] = {op, '\0'};
    appendCalcExpression(opText);
}

static double currentCalcValue()
{
    return atof(calcDisplay);
}

static double applyCalcOp(double left, double right, char op)
{
    switch (op) {
    case '+': return left + right;
    case '-': return left - right;
    case '*': return left * right;
    case '/': return right == 0.0 ? NAN : left / right;
    default: return right;
    }
}

static void resetCalculator()
{
    snprintf(calcDisplay, sizeof(calcDisplay), "0");
    snprintf(calcExpression, sizeof(calcExpression), "0");
    calcStored = 0.0;
    calcPendingOp = 0;
    calcNewInput = true;
}

static void appendCalcInput(const char *label)
{
    if (calcNewInput || strcmp(calcDisplay, "0") == 0 || strcmp(calcDisplay, "Error") == 0) {
        snprintf(calcDisplay, sizeof(calcDisplay), "%s", label);
        calcNewInput = false;
        return;
    }
    if (strlen(calcDisplay) + strlen(label) < sizeof(calcDisplay) - 1) {
        strncat(calcDisplay, label, sizeof(calcDisplay) - strlen(calcDisplay) - 1);
    }
}

static void handleCalcButton(const char *label)
{
    if (strlen(label) == 1 && label[0] >= '0' && label[0] <= '9') {
        if (calcNewInput && !calcPendingOp) {
            snprintf(calcExpression, sizeof(calcExpression), "0");
        }
        appendCalcExpression(label);
        appendCalcInput(label);
        return;
    }
    if (strcmp(label, ".") == 0) {
        if (calcNewInput) {
            if (!calcPendingOp) {
                snprintf(calcExpression, sizeof(calcExpression), "0");
            }
            appendCalcExpression("0.");
            snprintf(calcDisplay, sizeof(calcDisplay), "0.");
            calcNewInput = false;
        } else if (!strchr(calcDisplay, '.')) {
            appendCalcExpression(label);
            appendCalcInput(label);
        }
        return;
    }
    if (strcmp(label, "C") == 0) {
        resetCalculator();
        return;
    }
    if (strcmp(label, "<-") == 0) {
        size_t exprLen = strlen(calcExpression);
        if (exprLen > 1) {
            calcExpression[exprLen - 1] = '\0';
        } else {
            snprintf(calcExpression, sizeof(calcExpression), "0");
        }
        size_t len = strlen(calcDisplay);
        if (!calcNewInput && len > 1) {
            calcDisplay[len - 1] = '\0';
        } else {
            snprintf(calcDisplay, sizeof(calcDisplay), "0");
            calcNewInput = true;
        }
        return;
    }
    if (strcmp(label, "=") == 0) {
        if (calcPendingOp) {
            formatCalcValue(applyCalcOp(calcStored, currentCalcValue(), calcPendingOp));
            calcPendingOp = 0;
        }
        return;
    }
    char op = label[0];
    if (strchr("+-*/", op)) {
        double value = currentCalcValue();
        if (calcPendingOp && !calcNewInput) {
            value = applyCalcOp(calcStored, value, calcPendingOp);
            snprintf(calcDisplay, sizeof(calcDisplay), "%.10g", value);
        }
        calcStored = value;
        calcPendingOp = op;
        appendCalcOperatorToExpression(op);
        calcNewInput = true;
    }
}

static bool handleCalculatorTouch(int16_t tx, int16_t ty)
{
    int32_t px = 0;
    int32_t py = 0;

    if (portraitPointFromTouch(tx, ty, &px, &py, true)) {
        for (size_t i = 0; i < sizeof(calcButtons) / sizeof(calcButtons[0]); ++i) {
            const CalcButton &b = calcButtons[i];
            if (pointInRect(px, py, b.x, b.y, b.w, b.h)) {
                char previousExpression[sizeof(calcExpression)];
                snprintf(previousExpression, sizeof(previousExpression), "%s", calcExpression);
                handleCalcButton(b.label);
                refreshCalculatorResultArea(previousExpression);
                return true;
            }
        }
    }

    if (portraitPointFromTouch(tx, ty, &px, &py, false)) {
        for (size_t i = 0; i < sizeof(calcButtons) / sizeof(calcButtons[0]); ++i) {
            const CalcButton &b = calcButtons[i];
            if (pointInRect(px, py, b.x, b.y, b.w, b.h)) {
                char previousExpression[sizeof(calcExpression)];
                snprintf(previousExpression, sizeof(previousExpression), "%s", calcExpression);
                handleCalcButton(b.label);
                refreshCalculatorResultArea(previousExpression);
                return true;
            }
        }
    }

    return false;
}

static bool handleCalculatorTouchLegacy(int16_t tx, int16_t ty)
{
    for (size_t i = 0; i < sizeof(calcButtons) / sizeof(calcButtons[0]); ++i) {
        const CalcButton &b = calcButtons[i];
        if (touchHitsPortraitRect(tx, ty, b.x, b.y, b.w, b.h)) {
            char previousExpression[sizeof(calcExpression)];
            snprintf(previousExpression, sizeof(previousExpression), "%s", calcExpression);
            handleCalcButton(b.label);
            refreshCalculatorResultArea(previousExpression);
            return true;
        }
    }
    return false;
}

static void refreshDisplayExtended(void (*drawFn)(), bool use_black_refresh)
{
    epd_poweron();

    // Keep the top status bar untouched during refresh. Only wipe and redraw the
    // content area below it, first black and then white, to reduce ghosting
    // without blinking the top bar.
    Rect_t contentArea = portraitRectToPhysicalRect(0, TOP_STATUS_BAR_H, PORTRAIT_WIDTH, PORTRAIT_HEIGHT - TOP_STATUS_BAR_H);
    const int32_t refreshTime = use_black_refresh ? 60 : 120;
    const int32_t wipeCycles = use_black_refresh ? 1 : 2;
    for (int32_t i = 0; i < wipeCycles; ++i) {
        epd_push_pixels(contentArea, refreshTime, 0); // Black wipe below top bar
        epd_push_pixels(contentArea, refreshTime, 1); // White wipe below top bar
    }
    // Finish with extra white pulses so the content area is clean before the
    // next framebuffer image is drawn.
    epd_push_pixels(contentArea, refreshTime, 1);

    // Redraw the framebuffer, then update only the content area below the top bar.
    drawFn();

    uint8_t *contentBuffer = copyPhysicalAreaFromFramebuffer(contentArea);
    if (contentBuffer) {
        epd_draw_grayscale_image(contentArea, contentBuffer);
        free(contentBuffer);
    }

    epd_poweroff();
}

static void refreshDisplay(void (*drawFn)())
{
    refreshDisplayExtended(drawFn, false);
}

static void refreshWifiPasswordArea()
{
    // Redraw only the password input framebuffer region so typing does not
    // wipe the placeholder area, CONNECT button, CANCEL button, or keyboard.
    drawWifiPasswordInputBox();

    const int32_t margin = 4;
    Rect_t passwordArea = portraitRectToPhysicalRect(WIFI_PASSWORD_BOX_X - margin,
                                                     WIFI_PASSWORD_BOX_Y - margin,
                                                     WIFI_PASSWORD_BOX_W + margin * 2,
                                                     WIFI_PASSWORD_BOX_H + margin * 2);
    uint8_t *passwordBuffer = copyPhysicalAreaFromFramebuffer(passwordArea);
    if (!passwordBuffer) {
        return;
    }

    epd_poweron();
    // Partial e-paper updates can retain faint remnants of previous text
    // (especially the long "Enter Password..." placeholder). Use a small
    // black/white conditioning cycle, confined to the password box, before
    // drawing the updated framebuffer region.
    for (int32_t i = 0; i < 2; ++i) {
        epd_push_pixels(passwordArea, 60, 0); // Black wipe only the password box area
        epd_push_pixels(passwordArea, 60, 1); // White wipe only the password box area
    }
    epd_push_pixels(passwordArea, 60, 1); // Extra white pulse to clear residue
    epd_draw_grayscale_image(passwordArea, passwordBuffer);
    epd_poweroff();

    free(passwordBuffer);
}

static void refreshContentUrlArea()
{
    drawContentUrlInputBox();

    const int32_t margin = 4;
    Rect_t urlArea = portraitRectToPhysicalRect(CONTENT_URL_BOX_X - margin,
                                                CONTENT_URL_BOX_Y - margin,
                                                CONTENT_URL_BOX_W + margin * 2,
                                                CONTENT_URL_BOX_H + margin * 2);
    uint8_t *urlBuffer = copyPhysicalAreaFromFramebuffer(urlArea);
    if (!urlBuffer) {
        return;
    }

    epd_poweron();
    for (int32_t i = 0; i < 2; ++i) {
        epd_push_pixels(urlArea, 60, 0);
        epd_push_pixels(urlArea, 60, 1);
    }
    epd_push_pixels(urlArea, 60, 1);
    epd_draw_grayscale_image(urlArea, urlBuffer);
    epd_poweroff();

    free(urlBuffer);
}

static void refreshWifiKeyboardArea()
{
    // Redraw the full settings framebuffer in memory so keyboard labels match
    // the new kb_mode, but only wipe/update the keyboard rectangle on the EPD.
    drawSettingsScreen();

    // E-paper partial updates can leave visible remnants when changing dense
    // keyboard labels. Refresh a slightly larger band and use repeated
    // black/white pulses before drawing the new keyboard image.
    const int32_t keyboardMargin = 12;
    const int32_t keyboardY = WIFI_KBD_START_Y - keyboardMargin;
    const int32_t keyboardBottom = wifiKeyboardRowY(WIFI_KBD_ACTION_ROW) + WIFI_KBD_KEY_H + keyboardMargin;
    const int32_t keyboardH = keyboardBottom - keyboardY;
    Rect_t keyboardArea = portraitRectToPhysicalRect(0, keyboardY, PORTRAIT_WIDTH, keyboardH);
    uint8_t *keyboardBuffer = copyPhysicalAreaFromFramebuffer(keyboardArea);
    if (!keyboardBuffer) {
        return;
    }

    epd_poweron();
    for (int32_t i = 0; i < 3; ++i) {
        epd_push_pixels(keyboardArea, 80, 0); // Black wipe only over keyboard
        epd_push_pixels(keyboardArea, 80, 1); // White wipe only over keyboard
    }
    epd_push_pixels(keyboardArea, 80, 1); // Extra white pulse to clear residue
    epd_draw_grayscale_image(keyboardArea, keyboardBuffer);
    epd_poweroff();

    free(keyboardBuffer);
}

static void refreshContentKeyboardArea()
{
    // Redraw the full content settings framebuffer in memory so the keyboard
    // labels match the new kb_mode, but only wipe/update the keyboard rectangle
    // on the EPD. This preserves the entered URL plus SAVE/CLEAR controls.
    drawContentSettingsScreen();

    const int32_t keyboardMargin = 12;
    const int32_t keyboardY = WIFI_KBD_START_Y - keyboardMargin;
    const int32_t keyboardBottom = wifiKeyboardRowY(WIFI_KBD_ACTION_ROW) + WIFI_KBD_KEY_H + keyboardMargin;
    const int32_t keyboardH = keyboardBottom - keyboardY;
    Rect_t keyboardArea = portraitRectToPhysicalRect(0, keyboardY, PORTRAIT_WIDTH, keyboardH);
    uint8_t *keyboardBuffer = copyPhysicalAreaFromFramebuffer(keyboardArea);
    if (!keyboardBuffer) {
        return;
    }

    epd_poweron();
    for (int32_t i = 0; i < 3; ++i) {
        epd_push_pixels(keyboardArea, 80, 0);
        epd_push_pixels(keyboardArea, 80, 1);
    }
    epd_push_pixels(keyboardArea, 80, 1);
    epd_draw_grayscale_image(keyboardArea, keyboardBuffer);
    epd_poweroff();

    free(keyboardBuffer);
}

static void refreshWifiStatusIconArea()
{
    // Redraw the status bar in the framebuffer so the WiFi icon reflects the
    // current connection state, then update only the icon/tap region on the EPD.
    drawTopStatusBar();

    Rect_t wifiIconArea = portraitRectToPhysicalRect(PORTRAIT_WIDTH - 150, 0, 70, TOP_STATUS_BAR_H);
    uint8_t *wifiIconBuffer = copyPhysicalAreaFromFramebuffer(wifiIconArea);
    if (!wifiIconBuffer) {
        return;
    }

    epd_poweron();
    // Explicitly wipe the old WiFi icon before drawing the updated connected /
    // disconnected icon. Multiple white pulses help remove e-paper remnants.
    for (int32_t i = 0; i < 3; ++i) {
        epd_push_pixels(wifiIconArea, 70, 1); // Clear only the WiFi icon region
    }
    epd_draw_grayscale_image(wifiIconArea, wifiIconBuffer);
    epd_poweroff();

    free(wifiIconBuffer);
}

static bool pointInRect(int32_t px, int32_t py, int32_t rx, int32_t ry, int32_t rw, int32_t rh)
{
    return px >= rx && px < rx + rw && py >= ry && py < ry + rh;
}

static bool portraitPointFromTouch(int16_t tx, int16_t ty, int32_t *px, int32_t *py, bool alternate)
{
    if (alternate) {
        // Convert from the physical landscape coordinate space into the
        // portrait logical coordinate space used by portraitPixel().
        *px = PORTRAIT_WIDTH - 1 - ty;
        *py = tx;
    } else {
        // Some touch configurations already report the portrait logical space.
        *px = tx;
        *py = ty;
    }
    return *px >= 0 && *px < PORTRAIT_WIDTH && *py >= 0 && *py < PORTRAIT_HEIGHT;
}

static bool touchHitsPortraitRect(int16_t tx, int16_t ty, int32_t rx, int32_t ry, int32_t rw, int32_t rh)
{
    int32_t px = 0;
    int32_t py = 0;
    if (portraitPointFromTouch(tx, ty, &px, &py, false) && pointInRect(px, py, rx, ry, rw, rh)) {
        return true;
    }
    if (portraitPointFromTouch(tx, ty, &px, &py, true) && pointInRect(px, py, rx, ry, rw, rh)) {
        return true;
    }
    return false;
}

static bool touchHitsClockTile(int16_t tx, int16_t ty)
{
    const int32_t clockX = homeIconStartX() + (HOME_ICON_SIZE + HOME_ICON_GAP) * 2;
    return touchHitsPortraitRect(tx, ty, clockX, HOME_ICON_START_Y, HOME_ICON_SIZE, HOME_ICON_SIZE);
}

static bool touchHitsCalculatorTile(int16_t tx, int16_t ty)
{
    const int32_t calculatorX = homeIconStartX() + HOME_ICON_SIZE + HOME_ICON_GAP;
    return touchHitsPortraitRect(tx, ty, calculatorX, HOME_ICON_START_Y, HOME_ICON_SIZE, HOME_ICON_SIZE);
}

static bool touchHitsHomeStatusIcon(int16_t tx, int16_t ty)
{
    return touchHitsPortraitRect(tx, ty, 0, 0, 88, 56);
}

static bool touchHitsSettingsTile(int16_t tx, int16_t ty)
{
    const int32_t settingsX = homeIconStartX();
    return touchHitsPortraitRect(tx, ty, settingsX, HOME_ICON_START_Y, HOME_ICON_SIZE, HOME_ICON_SIZE);
}

static bool touchHitsBookTile(int16_t tx, int16_t ty)
{
    const int32_t bookX = homeIconStartX();
    const int32_t bookY = HOME_ICON_START_Y + HOME_ICON_SIZE + HOME_ICON_GAP;
    return touchHitsPortraitRect(tx, ty, bookX, bookY, HOME_ICON_SIZE, HOME_ICON_SIZE);
}

static bool touchHitsWifiStatusIcon(int16_t tx, int16_t ty)
{
    // Tap area centered on the top status bar's WiFi icon (PORTRAIT_WIDTH - 136, y = 10)
    return touchHitsPortraitRect(tx, ty, PORTRAIT_WIDTH - 150, 0, 70, 56);
}

struct _point {
    uint8_t buttonID;
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
} touchPoint[] = {
    {0, 10, 10, 80, 80},
    {1, EPD_WIDTH - 80, 10, 80, 80},
    {2, 10, EPD_HEIGHT - 80, 80, 80},
    {3, EPD_WIDTH - 80, EPD_HEIGHT - 80, 80, 80},
    {4, EPD_WIDTH / 2 - 60, EPD_HEIGHT - 80, 120, 80}
};

static void processTouchRelease(int16_t x, int16_t y)
{
    if ((showingClock || showingCalculator || showingSettings || showingContentSettings || showingBookLibrary) && touchHitsHomeStatusIcon(x, y)) {
        showingClock = false;
        showingCalculator = false;
        showingSettings = false;
        showingContentSettings = false;
        showingBookLibrary = false;
        show_password_prompt = false;
        refreshDisplay(drawPortraitHome);
        touch_loop_interval = millis() + 300;
        return;
    }

    if (showingCalculator && handleCalculatorTouch(x, y)) {
        touch_loop_interval = millis() + 300;
        return;
    }

    if (showingSettings && handleSettingsTouch(x, y)) {
        touch_loop_interval = millis() + 300;
        return;
    }

    if (showingContentSettings && handleContentSettingsTouch(x, y)) {
        touch_loop_interval = millis() + 300;
        return;
    }

    if (!showingClock && !showingCalculator && !showingSettings && !showingContentSettings && !showingBookLibrary && touchHitsSettingsTile(x, y)) {
        showingContentSettings = true;
        kb_mode = KB_LOWERCASE;
        refreshDisplay(drawContentSettingsScreen);
        touch_loop_interval = millis() + 300;
        return;
    }

    if (!showingClock && !showingCalculator && !showingSettings && !showingContentSettings && !showingBookLibrary && touchHitsBookTile(x, y)) {
        showingBookLibrary = true;
        book_count = 0;
        book_total = 0;
        snprintf(book_library_status, sizeof(book_library_status), "Loading books...");
        refreshDisplay(drawBookLibraryLoadingScreen);
        fetchBookLibrary();
        refreshDisplay(drawBookLibraryScreen);
        touch_loop_interval = millis() + 300;
        return;
    }

    if (!showingClock && !showingCalculator && !showingSettings && !showingContentSettings && !showingBookLibrary && touchHitsCalculatorTile(x, y)) {
        showingCalculator = true;
        refreshDisplay(drawCalculatorScreen);
        touch_loop_interval = millis() + 300;
        return;
    }

    if (!showingClock && !showingCalculator && !showingSettings && !showingContentSettings && !showingBookLibrary && touchHitsClockTile(x, y)) {
        showingClock = true;
        refreshDisplay(drawAnalogClockScreen);
        clock_refresh_interval = millis() + 60000;
        touch_loop_interval = millis() + 300;
        return;
    }

    if (!showingSettings && !showingContentSettings && touchHitsWifiStatusIcon(x, y)) {
        showingClock = false;
        showingCalculator = false;
        showingSettings = true;
        showingContentSettings = false;
        showingBookLibrary = false;
        show_password_prompt = false;
        kb_mode = KB_LOWERCASE;
        wifi_ssid_input[0] = '\0';
        wifi_password_input[0] = '\0';
        refreshDisplay(drawWifiScanningScreen);
        
        // Ensure WiFi is in STATION mode, disconnect first, then scan
        WiFi.mode(WIFI_STA);
        WiFi.disconnect();
        delay(100);
        
        scanned_count = WiFi.scanNetworks();
        if (scanned_count > MAX_SCANNED_WIFI) {
            scanned_count = MAX_SCANNED_WIFI;
        }
        if (scanned_count < 0) {
            scanned_count = 0;
        }
        for (int i = 0; i < scanned_count; ++i) {
            strncpy(scanned_ssids[i], WiFi.SSID(i).c_str(), 32);
            scanned_ssids[i][32] = '\0';
        }
        refreshDisplay(drawSettingsScreen);
        touch_loop_interval = millis() + 300;
        return;
    }

    for (int i = 0; i < sizeof(touchPoint) / sizeof(touchPoint[0]); ++i) {
        if ((x > touchPoint[i].x && x < (touchPoint[i].x + touchPoint[i].w))
                && (y > touchPoint[i].y && y < (touchPoint[i].y + touchPoint[i].h))) {

            if ( touchPoint[i].buttonID == 4) {

                Serial.println("Sleep !!!!!!");

                epd_clear();

                delay(1000);

                epd_poweroff_all();

                WiFi.disconnect(true);

                touch.sleep();

                delay(100);


                Wire.end();

                Serial.end();

                // Timer wakeup  + gpio wakeup = 388uA , see  https://github.com/Xinyuan-LilyGO/LilyGo-EPD47/issues/144
                esp_sleep_enable_timer_wakeup(30 * 1000000ULL);

                // BOOT(STR_IO0) Button wakeup 388uA
                esp_sleep_enable_ext1_wakeup(_BV(0), ESP_EXT1_WAKEUP_ANY_LOW);

                esp_deep_sleep_start();
            }
        }
    }

    touch_loop_interval = millis() + 300;
}

void WiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info)
{
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(IPAddress(info.got_ip.ip_info.ip.addr));
    wifiStatusRefreshPending = true;
}

void timeavailable(struct timeval *t)
{
    Serial.println("[WiFi]: Got time adjustment from NTP!");
    ntp_synced = true;
}

static bool loadSavedWifiCredentials()
{
    if (!wifiPrefs.begin("wifi", true)) {
        Serial.println("Failed to open WiFi preferences for reading");
        return false;
    }

    String ssid = wifiPrefs.getString("ssid", "");
    String password = wifiPrefs.getString("password", "");
    wifiPrefs.end();

    if (ssid.length() == 0) {
        saved_wifi_ssid[0] = '\0';
        saved_wifi_password[0] = '\0';
        return false;
    }

    snprintf(saved_wifi_ssid, sizeof(saved_wifi_ssid), "%s", ssid.c_str());
    snprintf(saved_wifi_password, sizeof(saved_wifi_password), "%s", password.c_str());
    return true;
}

static void saveWifiCredentials(const char *ssid, const char *password)
{
    if (!ssid || ssid[0] == '\0') {
        return;
    }

    if (!wifiPrefs.begin("wifi", false)) {
        Serial.println("Failed to open WiFi preferences for writing");
        return;
    }

    wifiPrefs.putString("ssid", ssid);
    wifiPrefs.putString("password", password ? password : "");
    wifiPrefs.end();

    snprintf(saved_wifi_ssid, sizeof(saved_wifi_ssid), "%s", ssid);
    snprintf(saved_wifi_password, sizeof(saved_wifi_password), "%s", password ? password : "");
    Serial.printf("Saved WiFi credentials for SSID: %s\n", saved_wifi_ssid);
}

static void loadContentUrl()
{
    if (!appPrefs.begin("app", true)) {
        Serial.println("Failed to open app preferences for reading");
        saved_content_url[0] = '\0';
        content_url_input[0] = '\0';
        return;
    }

    String url = appPrefs.getString("contentUrl", "");
    appPrefs.end();

    snprintf(saved_content_url, sizeof(saved_content_url), "%s", url.c_str());
    snprintf(content_url_input, sizeof(content_url_input), "%s", url.c_str());
}

static void saveContentUrl()
{
    if (!appPrefs.begin("app", false)) {
        Serial.println("Failed to open app preferences for writing");
        return;
    }

    appPrefs.putString("contentUrl", content_url_input);
    appPrefs.end();

    snprintf(saved_content_url, sizeof(saved_content_url), "%s", content_url_input);
    Serial.printf("Saved content URL: %s\n", content_url_input);
}

static void buildBooksApiUrl(char *out, size_t outSize)
{
    if (!out || outSize == 0) {
        return;
    }
    out[0] = '\0';

    const char *base = saved_content_url[0] != '\0' ? saved_content_url : content_url_input;
    if (!base || base[0] == '\0') {
        return;
    }

    char normalized[256];
    snprintf(normalized, sizeof(normalized), "%s", base);
    size_t len = strlen(normalized);
    while (len > 0 && normalized[len - 1] == '/') {
        normalized[len - 1] = '\0';
        --len;
    }

    if (strstr(normalized, "/api/books") != NULL) {
        char separator = strchr(normalized, '?') ? '&' : '?';
        snprintf(out, outSize, "%s%cpage=1&perPage=%d", normalized, separator, MAX_BOOK_ITEMS);
    } else {
        snprintf(out, outSize, "%s/api/books?page=1&perPage=%d", normalized, MAX_BOOK_ITEMS);
    }
}

static bool fetchBookLibrary()
{
    book_count = 0;
    book_total = 0;

    if (WiFi.status() != WL_CONNECTED) {
        snprintf(book_library_status, sizeof(book_library_status), "WiFi not connected");
        return false;
    }

    char url[320];
    buildBooksApiUrl(url, sizeof(url));
    if (url[0] == '\0') {
        snprintf(book_library_status, sizeof(book_library_status), "Set Content URL first");
        return false;
    }

    Serial.printf("Fetching book library: %s\n", url);

    HTTPClient http;
    http.setTimeout(10000);
    if (!http.begin(url)) {
        snprintf(book_library_status, sizeof(book_library_status), "Bad content URL");
        return false;
    }

    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        snprintf(book_library_status, sizeof(book_library_status), "HTTP error: %d", httpCode);
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        snprintf(book_library_status, sizeof(book_library_status), "JSON parse failed");
        Serial.printf("Book JSON parse failed: %s\n", err.c_str());
        return false;
    }

    JsonArray items = doc["items"].as<JsonArray>();
    book_total = doc["total"] | 0;
    if (items.isNull()) {
        snprintf(book_library_status, sizeof(book_library_status), "No items in JSON");
        return false;
    }

    for (JsonObject item : items) {
        if (book_count >= MAX_BOOK_ITEMS) {
            break;
        }
        BookListItem &book = book_items[book_count];
        book.id = item["id"] | 0;
        snprintf(book.title, sizeof(book.title), "%s", item["title"] | "Untitled");
        snprintf(book.author, sizeof(book.author), "%s", item["author"] | "");
        snprintf(book.category, sizeof(book.category), "%s", item["category"] | "");
        ++book_count;
    }

    if (book_count <= 0) {
        snprintf(book_library_status, sizeof(book_library_status), "No books found");
        return false;
    }

    snprintf(book_library_status, sizeof(book_library_status), "Loaded %d books", book_count);
    return true;
}


uint32_t pressed_cnt = 0;
void buttonPressed(Button2 &b)
{
    Serial.println("Button1 Pressed!");
    pressed_cnt++;
}



void setup()
{
    Serial.begin(115200);


    // Set WiFi to station mode and disconnect from an AP if it was previously connected
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    WiFi.onEvent(WiFiGotIP, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);

    if (loadSavedWifiCredentials()) {
        Serial.printf("Connecting to saved SSID: %s\n", saved_wifi_ssid);
        WiFi.begin(saved_wifi_ssid, saved_wifi_password);
    } else {
        Serial.printf("No saved WiFi credentials, connecting to default SSID: %s\n", WIFI_SSID);
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }

    loadContentUrl();

    // set notification call-back function
    sntp_set_time_sync_notification_cb( timeavailable );

    /**
     * This will set configured ntp servers and constant TimeZone/daylightOffset
     * should be OK if your time zone does not need to adjust daylightOffset twice a year,
     * in such a case time adjustment won't be handled automagicaly.
     */
    // configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2);

    configTzTime(time_zone, ntpServer1, ntpServer2);


    /**
    * SD Card test
    * Only as a test SdCard hardware, use example reference
    * https://github.com/espressif/arduino-esp32/tree/master/libraries/SD/examples
    */
    SPI.begin(SD_SCLK, SD_MISO, SD_MOSI);
    bool rlst = SD.begin(SD_CS, SPI);
    if (!rlst) {
        Serial.println("SD init failed");
        snprintf(buf, 128, "➸ No detected SdCard 😂");
    } else {
        Serial.println("SD init success");
        snprintf(buf, 128,
                 "➸ Detected SdCard insert:%.2f GB😀",
                 SD.cardSize() / 1024.0 / 1024.0 / 1024.0
                );
    }

    // Correct the ADC reference voltage
    esp_adc_cal_characteristics_t adc_chars;
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(
                                       ADC_UNIT_2,
                                       ADC_ATTEN_DB_12,
                                       ADC_WIDTH_BIT_12,
                                       1100,
                                       &adc_chars
                                   );

    if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        Serial.printf("eFuse Vref: %umV\r\n", adc_chars.vref);
        vref = adc_chars.vref;
    }

    framebuffer = (uint8_t *)ps_calloc(sizeof(uint8_t), EPD_WIDTH * EPD_HEIGHT / 2);
    if (!framebuffer) {
        Serial.println("alloc memory failed !!!");
        while (1);
    }
    memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);

    epd_init();

    // Startup screen showing "Asundar" for 3 seconds in portrait orientation
    epd_poweron();
    epd_clear();
    drawPortraitStartup();
    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    epd_poweroff();
    delay(3000);

    epd_poweron();
    // Wipe out "Asundar" with white color. To prevent/remove any ghost image, we do multiple white wipes (pulses) to thoroughly clear the screen without doing a harsh black flash.
    for (int i = 0; i < 3; i++) {
        epd_push_pixels(epd_full_screen(), 50, 1);
    }

#if defined(CONFIG_IDF_TARGET_ESP32S3)
    // Assuming that the previous touch was in sleep state, wake it up
    pinMode(TOUCH_INT, OUTPUT);
    digitalWrite(TOUCH_INT, HIGH);

    Wire.begin(BOARD_SDA, BOARD_SCL);

    rtc.begin(Wire);

    Wire.beginTransmission(0x51);
    found_rtc = Wire.endTransmission() == 0;
    // rtc.setDateTime(2022, 6, 30, 0, 0, 0);


    /*
    * The touch reset pin uses hardware pull-up,
    * and the function of setting the I2C device address cannot be used.
    * Use scanning to obtain the touch device address.*/
    uint8_t touchAddress = 0x14;

    Wire.beginTransmission(0x14);
    if (Wire.endTransmission() == 0) {
        touchAddress = 0x14;
    }
    Wire.beginTransmission(0x5D);
    if (Wire.endTransmission() == 0) {
        touchAddress = 0x5D;
    }

    touch.setPins(-1, TOUCH_INT);
    if (touch.begin(Wire, touchAddress, BOARD_SDA, BOARD_SCL )) {
        touch.setMaxCoordinates(EPD_WIDTH, EPD_HEIGHT);
        touch.setSwapXY(true);
        touch.setMirrorXY(false, true);
        touchOnline = true;
    }

#endif

    drawPortraitHome();
    epd_draw_grayscale_image(epd_full_screen(), framebuffer);
    lastWifiConnected = WiFi.status() == WL_CONNECTED;

    epd_poweroff();

    // Set the button callback function
    btn.setPressedHandler(buttonPressed);

    // Set the initial touch interval value
    touch_loop_interval = millis() + 300;

    // Set the initial auto page refresh timer (150s from startup)
    auto_refresh_interval = millis() + 150000;
}


void loop()
{
    // Check if NTP time is synced and RTC is not synced
    if (ntp_synced && !rtc_synced) {
        rtc_synced = true;
        // Sync RTC with NTP time
        rtc.hwClockWrite();
    }

    bool wifiConnected = WiFi.status() == WL_CONNECTED;
    if (wifiConnected != lastWifiConnected || wifiStatusRefreshPending) {
        wifiStatusRefreshPending = false;
        lastWifiConnected = wifiConnected;
        refreshWifiStatusIconArea();
    }

    if (!wifiConnected && saved_wifi_ssid[0] != '\0' && millis() > wifi_reconnect_interval) {
        wifi_reconnect_interval = millis() + 5000;
        Serial.printf("WiFi disconnected, retrying saved SSID: %s\n", saved_wifi_ssid);
        WiFi.disconnect();
        WiFi.mode(WIFI_STA);
        WiFi.begin(saved_wifi_ssid, saved_wifi_password);
    }

    // Auto page refresh timer that triggers every 150s (150000ms) to clear ghosting and refresh active view using black refresh
    if (millis() > auto_refresh_interval) {
        auto_refresh_interval = millis() + 150000;
        if (showingClock) {
            refreshDisplayExtended(drawAnalogClockScreen, true);
            clock_refresh_interval = millis() + 60000;
        } else if (showingCalculator) {
            refreshDisplayExtended(drawCalculatorScreen, true);
        } else if (showingSettings) {
            refreshDisplayExtended(drawSettingsScreen, true);
        } else {
            refreshDisplayExtended(drawPortraitHome, true);
        }
    }

    if (showingClock && !showingCalculator && millis() > clock_refresh_interval) {
        refreshDisplay(drawAnalogClockScreen);
        clock_refresh_interval = millis() + 60000;
    }

    if (touchOnline) {

        // Limit the touch detection interval and detect the touch status every 300ms
        // https://github.com/Xinyuan-LilyGO/LilyGo-EPD47/issues/143
        if (millis()  < touch_loop_interval) {
            return;
        }
        int16_t  x, y;

        uint8_t touched = touch.getPoint(&x, &y, 1);
        if (touched) {
            touchWasPressed = true;
            if (!touchLatchActive) {
                touchLatchActive = true;
                latchedTouchX = x;
                latchedTouchY = y;
            }

            // Keep polling while the finger is down, but do not update any key
            // yet. The latched key is processed once below, after release.
            touch_loop_interval = millis() + 40;
            return;
        } else {
            if (touchWasPressed && touchLatchActive) {
                touchWasPressed = false;
                touchLatchActive = false;

                // Debounce the release edge so GT911/noisy interrupt transitions
                // cannot produce two password characters for one physical tap.
                if (millis() - lastTouchReleaseTime > 120) {
                    lastTouchReleaseTime = millis();
                    processTouchRelease(latchedTouchX, latchedTouchY);
                } else {
                    touch_loop_interval = millis() + 120;
                }
                return;
            }
            touchWasPressed = false;
            touchLatchActive = false;
        }
        touch_loop_interval = millis() + 300;
    }

    btn.loop();

    delay(2);
}
