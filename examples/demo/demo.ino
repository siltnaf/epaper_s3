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
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <esp_sntp.h>
#include "utilities.h"
#include "calc_key_icons.h"
#include "wifi_key_icons.h"
#include "book_nav_icons.h"
#include "chinese_font.h"
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
bool showingSettingsMenu = false;
bool showingContentSettings = false;
bool showingBookLibrary = false;
bool showingBookReader = false;
bool showingSdMenu = false;
bool showingSdFolder = false;
char sd_status_message[96] = "";
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
int32_t book_current_page = 1;
char book_library_status[96] = "Tap book icon to load library";
int32_t selected_book_id = 0;
char selected_book_title[80] = "";
char selected_book_author[40] = "";
char selected_book_category[40] = "";
char book_reader_status[96] = "Select a book";
String selected_book_content;
int32_t book_reader_page = 0;
int32_t book_reader_total_pages = 1;

struct ClockWeatherInfo {
    char city[64];
    char timezone[64];
    char temp[16];
    char desc[48];
    char humidity[16];
    char wind[16];
    char status[96];
    bool loaded;
};

ClockWeatherInfo clock_weather = {{0}, {0}, {0}, {0}, {0}, {0}, "Tap clock to sync", false};

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
static void refreshClockTimeArea();
static void drawCalculatorScreen();
static void drawSettingsMenuScreen();
static void drawSettingsScreen();
static void drawContentSettingsScreen();
static void drawBookLibraryScreen();
static void drawBookReaderScreen();
static void drawBookLibraryLoadingScreen();
static void drawSdMenuScreen();
static void drawSdFolderScreen();
static void formatSdCard();
static void drawSdStatusArea();
static void refreshDisplayWhiteOnly(void (*drawFn)());
static void refreshSdStatusArea();
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
static void refreshBookLibraryListArea();
static void refreshBookReaderContentArea();
static uint8_t *copyPhysicalAreaFromFramebuffer(Rect_t area);
static bool fetchSelectedBook(int32_t bookId);
static void buildBookDetailApiUrl(char *out, size_t outSize, int32_t bookId);
static void buildContentApiUrl(char *out, size_t outSize, const char *endpoint, const char *query = NULL);
static bool httpGetString(const char *url, String &payload, char *status, size_t statusSize, uint32_t timeoutMs = 10000);
static bool fetchClockWeatherInfo();
static void updateBookReaderPagination();
static void copyJsonString(char *dest, size_t destSize, JsonObject item, const char *key, const char *fallback);
static void copyBookTitle(char *dest, size_t destSize, JsonObject item);
static void decodeJsonUnicodeEscapes(char *text);
static void drawUtf8ChineseTextInRectSingleWidth(const char *text, int32_t rx, int32_t ry, int32_t rw, int32_t rh);
static void drawUtf8ChineseTextLeftAligned(const char *text, int32_t rx, int32_t ry, int32_t rh);
static void drawUtf8ChineseTextLeftAlignedClipped(const char *text, int32_t rx, int32_t ry, int32_t rw, int32_t rh);
static void drawUtf8ChineseTextLeftAlignedClippedNarrow(const char *text, int32_t rx, int32_t ry, int32_t rw, int32_t rh);
static void drawAsciiSingleWidthCharReader(char ch, int32_t x, int32_t y);
static void drawUtf8ChineseTextRightAligned(const char *text, int32_t rx_end, int32_t ry, int32_t rh);
static bool pointInRect(int32_t px, int32_t py, int32_t rx, int32_t ry, int32_t rw, int32_t rh);
static bool portraitPointFromTouch(int16_t tx, int16_t ty, int32_t *px, int32_t *py, bool alternate);
static bool touchHitsPortraitRect(int16_t tx, int16_t ty, int32_t rx, int32_t ry, int32_t rw, int32_t rh);
static bool handleSettingsTouch(int16_t tx, int16_t ty);
static bool handleSettingsMenuTouch(int16_t tx, int16_t ty);
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
static const int32_t BOOK_NAV_ICON_SIZE = 64;
static const int32_t BOOK_NAV_UP_X = 350;
static const int32_t BOOK_NAV_UP_Y = 80;
static const int32_t BOOK_NAV_DOWN_X = 430;
static const int32_t BOOK_NAV_DOWN_Y = 80;
static const int32_t BOOK_LIST_X = 22;
static const int32_t BOOK_LIST_Y = 178;
static const int32_t BOOK_LIST_ROW_H = 74;
static const int32_t BOOK_LIST_ROW_BOX_H = 60;
static const int32_t BOOK_LIST_W = PORTRAIT_WIDTH - 44;
static const int32_t BOOK_LIST_REFRESH_Y = 168;
static const int32_t BOOK_LIST_REFRESH_BOTTOM_MARGIN = 8;
static const int32_t BOOK_READER_CONTENT_X = 24;
static const int32_t BOOK_READER_CONTENT_Y = 187;
static const int32_t BOOK_READER_CONTENT_W = PORTRAIT_WIDTH - 48;
static const int32_t BOOK_READER_LINE_H = 50;
static const int32_t BOOK_READER_CHARS_PER_LINE = 14;
static const float BOOK_LIST_FONT_SCALE = 1.21f;
static const float BOOK_READER_FONT_SCALE = 1.61f;
static const float BOOK_READER_FONT_X_SCALE = 0.88f; // squared, e-reader-like width
static const int32_t SETTINGS_MENU_ITEM_X = 54;
static const int32_t SETTINGS_MENU_ITEM_W = PORTRAIT_WIDTH - 108;
static const int32_t SETTINGS_MENU_ITEM_H = 86;
static const int32_t SETTINGS_MENU_ITEM_GAP = 28;
static const int32_t SETTINGS_MENU_FIRST_Y = 190;

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

    // Draw full-scale text centered in a virtual bounding box scaled by 1/scale
    int32_t vrw = (int32_t)(rw / scale);
    int32_t vrh = (int32_t)(rh / scale);
    int32_t vrx = rx + (rw - vrw) / 2;
    int32_t vry = (ry - shiftY) + (rh - vrh) / 2;

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
        int32_t srcY = yy - shiftY;
        if (srcY < 0 || srcY >= PORTRAIT_HEIGHT) continue;
        float srcYf = (cy - shiftY) + (srcY - (cy - shiftY)) * invScale;
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

static bool utf8NextCodepoint(const char **cursor, uint32_t *codepoint)
{
    const uint8_t *s = (const uint8_t *)(*cursor);
    if (!s || *s == 0) {
        return false;
    }

    if (s[0] < 0x80) {
        *codepoint = s[0];
        *cursor += 1;
        return true;
    }
    if ((s[0] & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) {
        *codepoint = ((uint32_t)(s[0] & 0x1F) << 6) | (uint32_t)(s[1] & 0x3F);
        *cursor += 2;
        return true;
    }
    if ((s[0] & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) {
        *codepoint = ((uint32_t)(s[0] & 0x0F) << 12) | ((uint32_t)(s[1] & 0x3F) << 6) | (uint32_t)(s[2] & 0x3F);
        *cursor += 3;
        return true;
    }
    if ((s[0] & 0xF8) == 0xF0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80 && (s[3] & 0xC0) == 0x80) {
        *codepoint = ((uint32_t)(s[0] & 0x07) << 18) | ((uint32_t)(s[1] & 0x3F) << 12) | ((uint32_t)(s[2] & 0x3F) << 6) | (uint32_t)(s[3] & 0x3F);
        *cursor += 4;
        return true;
    }

    *codepoint = '?';
    *cursor += 1;
    return true;
}

static const ChineseGlyph *findChineseGlyph(uint32_t codepoint)
{
    int32_t low = 0;
    int32_t high = ChineseFontGlyphCount - 1;
    while (low <= high) {
        int32_t mid = low + (high - low) / 2;
        uint32_t midCode = ChineseFontGlyphs[mid].codepoint;
        if (midCode == codepoint) {
            return &ChineseFontGlyphs[mid];
        }
        if (midCode < codepoint) {
            low = mid + 1;
        } else {
            high = mid - 1;
        }
    }
    return NULL;
}

static int32_t singleWidthTextWidth(const char *text)
{
    int32_t width = 0;
    const char *p = text;
    uint32_t cp = 0;
    while (utf8NextCodepoint(&p, &cp)) {
        if (cp < 0x80) {
            width += (int32_t)ceilf(((cp == ' ') ? 8.0f : 12.0f) * BOOK_LIST_FONT_SCALE);
        } else {
            const ChineseGlyph *glyph = findChineseGlyph(cp);
            width += glyph ? ((int32_t)ceilf(glyph->width * BOOK_LIST_FONT_SCALE) + (int32_t)ceilf(2.0f * BOOK_LIST_FONT_SCALE)) : (int32_t)ceilf(14.0f * BOOK_LIST_FONT_SCALE);
        }
    }
    return width;
}

static void drawAsciiSingleWidthChar(char ch, int32_t x, int32_t y)
{
    // Use the existing vector font path for ASCII, but keep it thin by only
    // copying the darkest pixels. This is used only for small prefixes like
    // "1. "; Chinese glyphs below are always drawn pixel-for-pixel.
    char label[2] = {ch, '\0'};
    drawPortraitTextInRectCenteredScaled(label,
                                         x,
                                         y - (int32_t)ceilf(18.0f * BOOK_LIST_FONT_SCALE),
                                         (int32_t)ceilf(12.0f * BOOK_LIST_FONT_SCALE),
                                         (int32_t)ceilf(36.0f * BOOK_LIST_FONT_SCALE),
                                         (GFXfont *)&FiraSans,
                                         BOOK_LIST_FONT_SCALE);
}

static void drawAsciiSingleWidthCharReader(char ch, int32_t x, int32_t y)
{
    char label[2] = {ch, '\0'};
    drawPortraitTextInRectCenteredScaled(label,
                                         x,
                                         y - (int32_t)ceilf(18.0f * BOOK_READER_FONT_SCALE),
                                         (int32_t)ceilf(12.0f * BOOK_READER_FONT_SCALE),
                                         (int32_t)ceilf(36.0f * BOOK_READER_FONT_SCALE),
                                         (GFXfont *)&FiraSans,
                                         BOOK_READER_FONT_SCALE);
}

static char punctuationFallbackChar(uint32_t cp)
{
    switch (cp) {
    case 0x3002: return '.';  // 。
    case 0xFF0C: return ',';  // ，
    case 0x3001: return ',';  // 、
    case 0xFF1A: return ':';  // ：
    case 0xFF1B: return ';';  // ；
    case 0xFF01: return '!';  // ！
    case 0xFF1F: return '?';  // ？
    case 0x201C: return '"'; // “
    case 0x201D: return '"'; // ”
    case 0x2018: return '\''; // ‘
    case 0x2019: return '\''; // ’
    case 0xFF08: return '(';  // （
    case 0xFF09: return ')';  // ）
    case 0x300A: return '<';  // 《
    case 0x300B: return '>';  // 》
    case 0x2014: return '-';  // —
    case 0x2026: return '.';  // …
    default: return '\0';
    }
}

static int32_t chineseTextCodepointAdvance(uint32_t cp)
{
    if (cp == '\r' || cp == '\n') {
        return 0;
    }
    if (cp < 0x80) {
        return (int32_t)ceilf(((cp == ' ') ? 8.0f : 12.0f) * BOOK_LIST_FONT_SCALE);
    }

    const ChineseGlyph *glyph = findChineseGlyph(cp);
    if (glyph) {
        return (int32_t)ceilf(glyph->width * BOOK_LIST_FONT_SCALE) + (int32_t)ceilf(2.0f * BOOK_LIST_FONT_SCALE);
    }

    char fallback = punctuationFallbackChar(cp);
    return (int32_t)ceilf((fallback ? 10.0f : 14.0f) * BOOK_LIST_FONT_SCALE);
}

static int32_t chineseTextCodepointAdvanceReader(uint32_t cp)
{
    if (cp == '\r' || cp == '\n') {
        return 0;
    }
    if (cp < 0x80) {
        return (int32_t)ceilf(((cp == ' ') ? 8.0f : 12.0f) * BOOK_READER_FONT_SCALE);
    }

    const ChineseGlyph *glyph = findChineseGlyph(cp);
    if (glyph) {
        return (int32_t)ceilf(glyph->width * BOOK_READER_FONT_SCALE * BOOK_READER_FONT_X_SCALE) + (int32_t)ceilf(2.0f * BOOK_READER_FONT_SCALE);
    }

    char fallback = punctuationFallbackChar(cp);
    return (int32_t)ceilf((fallback ? 10.0f : 14.0f) * BOOK_READER_FONT_SCALE);
}

static int32_t chineseTextCodepointAdvanceNarrow(uint32_t cp)
{
    return chineseTextCodepointAdvanceReader(cp);
}

static void drawChineseGlyphScaled(const ChineseGlyph *glyph, int32_t x, int32_t y, int32_t clipRight, int32_t clipBottom)
{
    if (!glyph) {
        return;
    }

    const int32_t scaledW = (int32_t)ceilf(glyph->width * BOOK_LIST_FONT_SCALE);
    const int32_t scaledH = (int32_t)ceilf(glyph->height * BOOK_LIST_FONT_SCALE);
    for (int32_t dy = 0; dy < scaledH && y + dy < clipBottom; ++dy) {
        int32_t srcY = (int32_t)floorf(dy / BOOK_LIST_FONT_SCALE);
        if (srcY < 0) srcY = 0;
        if (srcY >= glyph->height) srcY = glyph->height - 1;
        for (int32_t dx = 0; dx < scaledW && x + dx < clipRight; ++dx) {
            int32_t srcX = (int32_t)floorf(dx / BOOK_LIST_FONT_SCALE);
            if (srcX < 0) srcX = 0;
            if (srcX >= glyph->width) srcX = glyph->width - 1;
            uint8_t packed = ChineseFontBitmap[glyph->offset + srcY * glyph->rowBytes + srcX / 8];
            if (packed & (0x80 >> (srcX & 7))) {
                portraitPixel(x + dx, y + dy, 0x00);
            }
        }
    }
}

static void drawChineseGlyphScaledX(const ChineseGlyph *glyph, int32_t x, int32_t y, int32_t clipRight, int32_t clipBottom, float xScale)
{
    if (!glyph) {
        return;
    }

    const int32_t scaledW = max((int32_t)1, (int32_t)ceilf(glyph->width * BOOK_READER_FONT_SCALE * xScale));
    const int32_t scaledH = (int32_t)ceilf(glyph->height * BOOK_READER_FONT_SCALE);
    for (int32_t dy = 0; dy < scaledH && y + dy < clipBottom; ++dy) {
        int32_t srcY = (int32_t)floorf(dy / BOOK_READER_FONT_SCALE);
        if (srcY < 0) srcY = 0;
        if (srcY >= glyph->height) srcY = glyph->height - 1;
        for (int32_t dx = 0; dx < scaledW && x + dx < clipRight; ++dx) {
            int32_t srcX = (int32_t)floorf(dx / (BOOK_READER_FONT_SCALE * xScale));
            if (srcX < 0) srcX = 0;
            if (srcX >= glyph->width) srcX = glyph->width - 1;
            uint8_t packed = ChineseFontBitmap[glyph->offset + srcY * glyph->rowBytes + srcX / 8];
            if (packed & (0x80 >> (srcX & 7))) {
                portraitPixel(x + dx, y + dy, 0x00);
            }
        }
    }
}

static void drawUtf8ChineseTextInRectSingleWidth(const char *text, int32_t rx, int32_t ry, int32_t rw, int32_t rh)
{
    if (!text || text[0] == '\0') {
        return;
    }

    int32_t textW = singleWidthTextWidth(text);
    int32_t x = rx + (rw - textW) / 2;
    if (x < rx + 2) {
        x = rx + 2;
    }
    int32_t scaledFontHeight = (int32_t)ceilf(ChineseFontHeight * BOOK_LIST_FONT_SCALE);
    int32_t y = ry + (rh - scaledFontHeight) / 2;
    if (y < ry + 1) {
        y = ry + 1;
    }

    const char *p = text;
    uint32_t cp = 0;
    while (utf8NextCodepoint(&p, &cp) && x < rx + rw - 2) {
        if (cp < 0x80) {
            drawAsciiSingleWidthChar((char)cp, x, y + scaledFontHeight / 2);
            x += (int32_t)ceilf(((cp == ' ') ? 8.0f : 12.0f) * BOOK_LIST_FONT_SCALE);
            continue;
        }

        const ChineseGlyph *glyph = findChineseGlyph(cp);
        if (!glyph) {
            x += (int32_t)ceilf(14.0f * BOOK_LIST_FONT_SCALE);
            continue;
        }

        drawChineseGlyphScaled(glyph, x, y, rx + rw - 2, ry + rh - 1);
        x += (int32_t)ceilf(glyph->width * BOOK_LIST_FONT_SCALE) + (int32_t)ceilf(2.0f * BOOK_LIST_FONT_SCALE);
    }
}

static void drawUtf8ChineseTextLeftAligned(const char *text, int32_t rx, int32_t ry, int32_t rh)
{
    if (!text || text[0] == '\0') {
        return;
    }

    int32_t x = rx;
    int32_t scaledFontHeight = (int32_t)ceilf(ChineseFontHeight * BOOK_LIST_FONT_SCALE);
    int32_t y = ry + (rh - scaledFontHeight) / 2;
    if (y < ry + 1) {
        y = ry + 1;
    }

    const char *p = text;
    uint32_t cp = 0;
    while (utf8NextCodepoint(&p, &cp) && x < PORTRAIT_WIDTH - 2) {
        if (cp < 0x80) {
            drawAsciiSingleWidthChar((char)cp, x, y + scaledFontHeight / 2);
            x += (int32_t)ceilf(((cp == ' ') ? 8.0f : 12.0f) * BOOK_LIST_FONT_SCALE);
            continue;
        }

        const ChineseGlyph *glyph = findChineseGlyph(cp);
        if (!glyph) {
            x += (int32_t)ceilf(14.0f * BOOK_LIST_FONT_SCALE);
            continue;
        }

        drawChineseGlyphScaled(glyph, x, y, PORTRAIT_WIDTH - 2, ry + rh - 1);
        x += (int32_t)ceilf(glyph->width * BOOK_LIST_FONT_SCALE) + (int32_t)ceilf(2.0f * BOOK_LIST_FONT_SCALE);
    }
}

static void drawUtf8ChineseTextLeftAlignedClipped(const char *text, int32_t rx, int32_t ry, int32_t rw, int32_t rh)
{
    if (!text || text[0] == '\0' || rw <= 0 || rh <= 0) {
        return;
    }

    const int32_t clipRight = min(PORTRAIT_WIDTH - 2, rx + rw);
    const int32_t clipBottom = min(PORTRAIT_HEIGHT - 1, ry + rh);
    int32_t x = max((int32_t)0, rx);
    int32_t scaledFontHeight = (int32_t)ceilf(ChineseFontHeight * BOOK_LIST_FONT_SCALE);
    int32_t y = ry + (rh - scaledFontHeight) / 2;
    if (y < ry + 1) {
        y = ry + 1;
    }

    const char *p = text;
    uint32_t cp = 0;
    while (utf8NextCodepoint(&p, &cp) && x < clipRight) {
        int32_t advance = 0;
        if (cp < 0x80) {
            advance = (int32_t)ceilf(((cp == ' ') ? 8.0f : 12.0f) * BOOK_LIST_FONT_SCALE);
            if (x + advance > clipRight) {
                break;
            }
            drawAsciiSingleWidthChar((char)cp, x, y + scaledFontHeight / 2);
            x += advance;
            continue;
        }

        const ChineseGlyph *glyph = findChineseGlyph(cp);
        if (!glyph) {
            char fallback = punctuationFallbackChar(cp);
            advance = (int32_t)ceilf((fallback ? 10.0f : 14.0f) * BOOK_LIST_FONT_SCALE);
            if (x + advance > clipRight) {
                break;
            }
            if (fallback) {
                drawAsciiSingleWidthChar(fallback, x, y + scaledFontHeight / 2);
            }
            x += advance;
            continue;
        }

        advance = (int32_t)ceilf(glyph->width * BOOK_LIST_FONT_SCALE) + (int32_t)ceilf(2.0f * BOOK_LIST_FONT_SCALE);
        if (x + advance > clipRight) {
            break;
        }
        drawChineseGlyphScaled(glyph, x, y, clipRight, clipBottom);
        x += advance;
    }
}

static void drawUtf8ChineseTextLeftAlignedClippedNarrow(const char *text, int32_t rx, int32_t ry, int32_t rw, int32_t rh)
{
    if (!text || text[0] == '\0' || rw <= 0 || rh <= 0) {
        return;
    }

    const int32_t clipRight = min(PORTRAIT_WIDTH - 2, rx + rw);
    const int32_t clipBottom = min(PORTRAIT_HEIGHT - 1, ry + rh);
    int32_t x = max((int32_t)0, rx);
    int32_t scaledFontHeight = (int32_t)ceilf(ChineseFontHeight * BOOK_READER_FONT_SCALE);
    int32_t y = ry + (rh - scaledFontHeight) / 2;
    if (y < ry + 1) {
        y = ry + 1;
    }

    const char *p = text;
    uint32_t cp = 0;
    while (utf8NextCodepoint(&p, &cp) && x < clipRight) {
        int32_t advance = chineseTextCodepointAdvanceNarrow(cp);
        if (x + advance > clipRight) {
            break;
        }

        if (cp < 0x80) {
            drawAsciiSingleWidthCharReader((char)cp, x, y + scaledFontHeight / 2);
            x += advance;
            continue;
        }

        const ChineseGlyph *glyph = findChineseGlyph(cp);
        if (!glyph) {
            char fallback = punctuationFallbackChar(cp);
            if (fallback) {
                drawAsciiSingleWidthCharReader(fallback, x, y + scaledFontHeight / 2);
            }
            x += advance;
            continue;
        }

        drawChineseGlyphScaledX(glyph, x, y, clipRight, clipBottom, BOOK_READER_FONT_X_SCALE);
        x += advance;
    }
}


static void drawUtf8ChineseTextRightAligned(const char *text, int32_t rx_end, int32_t ry, int32_t rh)
{
    if (!text || text[0] == '\0') {
        return;
    }

    int32_t textW = singleWidthTextWidth(text);
    int32_t x = rx_end - textW;
    if (x < 2) {
        x = 2;
    }
    int32_t scaledFontHeight = (int32_t)ceilf(ChineseFontHeight * BOOK_LIST_FONT_SCALE);
    int32_t y = ry + (rh - scaledFontHeight) / 2;
    if (y < ry + 1) {
        y = ry + 1;
    }

    const char *p = text;
    uint32_t cp = 0;
    while (utf8NextCodepoint(&p, &cp) && x < rx_end) {
        if (cp < 0x80) {
            drawAsciiSingleWidthChar((char)cp, x, y + scaledFontHeight / 2);
            x += (int32_t)ceilf(((cp == ' ') ? 8.0f : 12.0f) * BOOK_LIST_FONT_SCALE);
            continue;
        }

        const ChineseGlyph *glyph = findChineseGlyph(cp);
        if (!glyph) {
            x += (int32_t)ceilf(14.0f * BOOK_LIST_FONT_SCALE);
            continue;
        }

        drawChineseGlyphScaled(glyph, x, y, rx_end, ry + rh - 1);
        x += (int32_t)ceilf(glyph->width * BOOK_LIST_FONT_SCALE) + (int32_t)ceilf(2.0f * BOOK_LIST_FONT_SCALE);
    }
}

static int hexNibble(char ch)
{
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}

static bool appendUtf8(char *out, size_t outSize, size_t *outLen, uint32_t cp)
{
    if (!out || !outLen || outSize == 0) {
        return false;
    }
    if (cp <= 0x7F) {
        if (*outLen + 1 >= outSize) return false;
        out[(*outLen)++] = (char)cp;
    } else if (cp <= 0x7FF) {
        if (*outLen + 2 >= outSize) return false;
        out[(*outLen)++] = (char)(0xC0 | (cp >> 6));
        out[(*outLen)++] = (char)(0x80 | (cp & 0x3F));
    } else if (cp <= 0xFFFF) {
        if (*outLen + 3 >= outSize) return false;
        out[(*outLen)++] = (char)(0xE0 | (cp >> 12));
        out[(*outLen)++] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[(*outLen)++] = (char)(0x80 | (cp & 0x3F));
    } else {
        if (*outLen + 4 >= outSize) return false;
        out[(*outLen)++] = (char)(0xF0 | (cp >> 18));
        out[(*outLen)++] = (char)(0x80 | ((cp >> 12) & 0x3F));
        out[(*outLen)++] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[(*outLen)++] = (char)(0x80 | (cp & 0x3F));
    }
    out[*outLen] = '\0';
    return true;
}

static void decodeJsonUnicodeEscapes(char *text)
{
    if (!text || !strstr(text, "\\u")) {
        return;
    }

    char decoded[96];
    size_t outLen = 0;
    decoded[0] = '\0';

    for (size_t i = 0; text[i] != '\0' && outLen < sizeof(decoded) - 1;) {
        if (text[i] == '\\' && text[i + 1] == 'u') {
            int h0 = hexNibble(text[i + 2]);
            int h1 = hexNibble(text[i + 3]);
            int h2 = hexNibble(text[i + 4]);
            int h3 = hexNibble(text[i + 5]);
            if (h0 >= 0 && h1 >= 0 && h2 >= 0 && h3 >= 0) {
                uint32_t cp = ((uint32_t)h0 << 12) | ((uint32_t)h1 << 8) | ((uint32_t)h2 << 4) | (uint32_t)h3;
                i += 6;

                // Decode UTF-16 surrogate pairs if they appear in JSON strings.
                if (cp >= 0xD800 && cp <= 0xDBFF && text[i] == '\\' && text[i + 1] == 'u') {
                    int l0 = hexNibble(text[i + 2]);
                    int l1 = hexNibble(text[i + 3]);
                    int l2 = hexNibble(text[i + 4]);
                    int l3 = hexNibble(text[i + 5]);
                    if (l0 >= 0 && l1 >= 0 && l2 >= 0 && l3 >= 0) {
                        uint32_t low = ((uint32_t)l0 << 12) | ((uint32_t)l1 << 8) | ((uint32_t)l2 << 4) | (uint32_t)l3;
                        if (low >= 0xDC00 && low <= 0xDFFF) {
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                            i += 6;
                        }
                    }
                }

                appendUtf8(decoded, sizeof(decoded), &outLen, cp);
                continue;
            }
        }

        decoded[outLen++] = text[i++];
        decoded[outLen] = '\0';
    }

    snprintf(text, 80, "%s", decoded);
}

static void copyJsonString(char *dest, size_t destSize, JsonObject item, const char *key, const char *fallback)
{
    if (!dest || destSize == 0) {
        return;
    }
    const char *value = nullptr;
    if (key) {
        JsonVariant var = item[key];
        if (!var.isNull()) {
            value = var.as<const char*>();
        }
    }
    snprintf(dest, destSize, "%s", (value && value[0] != '\0') ? value : (fallback ? fallback : ""));
    decodeJsonUnicodeEscapes(dest);
}

static void copyBookTitle(char *dest, size_t destSize, JsonObject item)
{
    if (!dest || destSize == 0) {
        return;
    }

    // Since JsonObject::operator[] returns a JsonVariant (which can convert to const char*),
    // we must retrieve the value as a JsonVariant first to check if the key exists in ArduinoJson.
    // reference/server.js returns /api/books items as:
    // { id, title, author, category, created_at }. The Chinese book name is `title`.
    const char *keys[] = {"title", "name", "chineseName", "chinese_name", "chineseTitle", "chinese_title", "titleZh", "title_zh"};
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i) {
        JsonVariant value = item[keys[i]];
        if (!value.isNull()) {
            const char* str = value.as<const char*>();
            if (str && str[0] != '\0') {
                snprintf(dest, destSize, "%s", str);
                decodeJsonUnicodeEscapes(dest);
                return;
            }
        }
    }

    snprintf(dest, destSize, "Untitled");
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

static void drawSettingsMenuScreen()
{
    memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
    drawTopStatusBar();

    drawPortraitTextCentered("Settings", 100, (GFXfont *)&FiraSans);

    // Menu Item 1: Content URL
    int item1Y = 190;
    portraitFillRect(34, item1Y, 472, 90, 0xFF);
    portraitDrawRect(34, item1Y, 472, 90, 0x00);
    portraitDrawRect(36, item1Y + 2, 468, 86, 0x00);
    drawPortraitTextInRectCentered("Content URL", 34, item1Y, 340, 90, (GFXfont *)&FiraSans);
    // Right arrow indicator
    drawPortraitTextInRectCentered(">", 370, item1Y, 80, 90, (GFXfont *)&FiraSans);

    // Menu Item 2: SD Card
    int item2Y = 304;
    portraitFillRect(34, item2Y, 472, 90, 0xFF);
    portraitDrawRect(34, item2Y, 472, 90, 0x00);
    portraitDrawRect(36, item2Y + 2, 468, 86, 0x00);
    drawPortraitTextInRectCentered("SD Card", 34, item2Y, 340, 90, (GFXfont *)&FiraSans);
    drawPortraitTextInRectCentered(">", 370, item2Y, 80, 90, (GFXfont *)&FiraSans);

    // Menu Item 3: Volume
    int item3Y = 418;
    portraitFillRect(34, item3Y, 472, 90, 0xFF);
    portraitDrawRect(34, item3Y, 472, 90, 0x00);
    portraitDrawRect(36, item3Y + 2, 468, 86, 0x00);
    drawPortraitTextInRectCentered("Volume", 34, item3Y, 340, 90, (GFXfont *)&FiraSans);
    drawPortraitTextInRectCentered(">", 370, item3Y, 80, 90, (GFXfont *)&FiraSans);
}

static bool ensureSdReady()
{
    if (SD.cardType() != CARD_NONE) {
        return true;
    }
    SD.end();
    SPI.begin(SD_SCLK, SD_MISO, SD_MOSI);
    return SD.begin(SD_CS, SPI) && SD.cardType() != CARD_NONE;
}

static bool clearSdDirectory(File dir)
{
    bool ok = true;
    File entry = dir.openNextFile();
    while (entry) {
        char path[192];
        snprintf(path, sizeof(path), "%s", entry.path());
        bool isDir = entry.isDirectory();
        if (isDir) {
            ok = clearSdDirectory(entry) && ok;
        }
        entry.close();
        if (isDir) {
            ok = SD.rmdir(path) && ok;
        } else {
            ok = SD.remove(path) && ok;
        }
        entry = dir.openNextFile();
    }
    return ok;
}

static void drawSdMenuScreen()
{
    memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
    drawTopStatusBar();
    drawPortraitTextCentered("SD Card", 100, (GFXfont *)&FiraSans);

    bool ready = ensureSdReady();
    char status[96];
    if (ready) {
        uint64_t mb = SD.cardSize() / (1024ULL * 1024ULL);
        snprintf(status, sizeof(status), "Connected  %llu MB", (unsigned long long)mb);
    } else {
        snprintf(status, sizeof(status), "No SD card detected");
    }
    drawPortraitTextInRectCenteredScaled(status, 34, 145, 472, 42, (GFXfont *)&FiraSans, 0.62f);

    portraitDrawRect(34, 240, 472, 90, 0x00);
    portraitDrawRect(36, 242, 468, 86, 0x00);
    drawPortraitTextInRectCentered("Folder", 34, 240, 472, 90, (GFXfont *)&FiraSans);

    portraitDrawRect(34, 370, 472, 90, 0x00);
    portraitDrawRect(36, 372, 468, 86, 0x00);
    drawPortraitTextInRectCentered("Format SD", 34, 370, 472, 90, (GFXfont *)&FiraSans);

    if (sd_status_message[0] != '\0') {
        drawSdStatusArea();
    }
}

static void drawSdStatusArea()
{
    portraitFillRect(34, PORTRAIT_HEIGHT - 115, 472, 70, 0xFF);
    drawPortraitTextInRectCenteredScaled(sd_status_message, 34, PORTRAIT_HEIGHT - 115, 472, 70, (GFXfont *)&FiraSans, 0.72f);
}

static void drawSdFolderScreen()
{
    memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
    drawTopStatusBar();
    drawPortraitTextCentered("SD Folder", 100, (GFXfont *)&FiraSans);

    if (!ensureSdReady()) {
        drawPortraitTextInRectCenteredScaled("No SD card detected", 34, 260, 472, 80, (GFXfont *)&FiraSans, 0.72f);
        return;
    }

    File root = SD.open("/");
    if (!root || !root.isDirectory()) {
        drawPortraitTextInRectCenteredScaled("Cannot open root folder", 34, 260, 472, 80, (GFXfont *)&FiraSans, 0.72f);
        return;
    }

    int y = 160;
    int count = 0;
    File entry = root.openNextFile();
    while (entry && count < 15) {
        char line[96];
        snprintf(line, sizeof(line), "%s %s", entry.isDirectory() ? "[D]" : "[F]", entry.name());
        drawPortraitTextInRectCenteredScaled(line, 34, y, 472, 38, (GFXfont *)&FiraSans, 0.52f);
        y += 46;
        ++count;
        entry.close();
        entry = root.openNextFile();
    }
    if (count == 0) {
        drawPortraitTextInRectCenteredScaled("SD card is empty", 34, 260, 472, 80, (GFXfont *)&FiraSans, 0.72f);
    }
    root.close();
}

static void formatSdCard()
{
    snprintf(sd_status_message, sizeof(sd_status_message), "Format SD");
    refreshSdStatusArea();

    bool ok = false;
    if (ensureSdReady()) {
        File root = SD.open("/");
        ok = root && root.isDirectory() && clearSdDirectory(root);
        if (root) root.close();
    }

    (void)ok;
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

static bool handleSettingsMenuTouch(int16_t tx, int16_t ty)
{
    int32_t px = 0;
    int32_t py = 0;
    if (!portraitPointFromTouch(tx, ty, &px, &py, true)) {
        if (!portraitPointFromTouch(tx, ty, &px, &py, false)) {
            return false;
        }
    }

    // Menu Item 1: Content URL (Y=190)
    if (pointInRect(px, py, 34, 190, 472, 90)) {
        showingSettingsMenu = false;
        showingContentSettings = true;
        kb_mode = KB_LOWERCASE;
        refreshDisplay(drawContentSettingsScreen);
        return true;
    }

    // Menu Item 2: SD Card (Y=304)
    if (pointInRect(px, py, 34, 304, 472, 90)) {
        showingSettingsMenu = false;
        showingSdMenu = true;
        showingSdFolder = false;
        refreshDisplay(drawSdMenuScreen);
        return true;
    }

    // Menu Item 3: Volume (Y=418) - placeholder for now
    if (pointInRect(px, py, 34, 418, 472, 90)) {
        // Placeholder: stay on the menu for now
        return true;
    }

    return false;
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

static void drawBookLibraryRowsArea()
{
    // The page-navigation partial refresh owns only this lower content band.
    // Keep the in-memory header/count/icons untouched so the copied region
    // exactly matches the physical area updated on the e-paper.
    portraitFillRect(0,
                     BOOK_LIST_REFRESH_Y,
                     PORTRAIT_WIDTH,
                     PORTRAIT_HEIGHT - BOOK_LIST_REFRESH_Y - BOOK_LIST_REFRESH_BOTTOM_MARGIN,
                     0xFF);

    if (book_count > 0) {
        // Re-draw the counter summary in the partial-refresh rows area to ensure it updates instantly!
        char summary[32];
        int displayedCount = (int)((book_current_page - 1) * MAX_BOOK_ITEMS) + book_count;
        if (book_total > 0 && displayedCount > book_total) {
            displayedCount = book_total;
        }
        snprintf(summary, sizeof(summary), "%d/%d", displayedCount, book_total);
        drawPortraitTextInRectCenteredScaled(summary,
                                             54,
                                             132,
                                             PORTRAIT_WIDTH - 108,
                                             28,
                                             (GFXfont *)&FiraSans,
                                             0.46f);
    }

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

    for (int i = 0; i < book_count; ++i) {
        const int32_t y = BOOK_LIST_Y + i * BOOK_LIST_ROW_H;
        if (y + BOOK_LIST_ROW_BOX_H > PORTRAIT_HEIGHT - 14) {
            break;
        }
        portraitDrawRect(BOOK_LIST_X, y, BOOK_LIST_W, BOOK_LIST_ROW_BOX_H, 0x00);

        // Display ONLY the Chinese book title (no book numbers and no writer/author)
        // Indented cleanly on the left, vertically centered inside the row box.
        drawUtf8ChineseTextLeftAligned(book_items[i].title, BOOK_LIST_X + 16, y, BOOK_LIST_ROW_BOX_H);

        // Show category on the right side of the row (right-aligned, vertically centered).
        if (book_items[i].category[0] != '\0') {
            drawUtf8ChineseTextRightAligned(book_items[i].category, BOOK_LIST_X + BOOK_LIST_W - 16, y, BOOK_LIST_ROW_BOX_H);
        }
    }
}

static void drawBookLibraryScreen()
{
    memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
    drawTopStatusBar();

    // draw "书库" in Chinese instead of "Book Library"
    drawUtf8ChineseTextInRectSingleWidth("书库", 0, 84, PORTRAIT_WIDTH, 40);

    // Draw Up and Down navigation icons next to each other
    drawBitmapIcon1bpp(BOOK_NAV_UP_X, BOOK_NAV_UP_Y, BOOK_NAV_ICON_SIZE, BOOK_NAV_ICON_SIZE, book_nav_up_icon_64x64, 0x00);
    drawBitmapIcon1bpp(BOOK_NAV_DOWN_X, BOOK_NAV_DOWN_Y, BOOK_NAV_ICON_SIZE, BOOK_NAV_ICON_SIZE, book_nav_down_icon_64x64, 0x00);

    drawBookLibraryRowsArea();
}

static int32_t utf8CodepointCount(const char *text)
{
    int32_t count = 0;
    const char *p = text;
    uint32_t cp = 0;
    while (utf8NextCodepoint(&p, &cp)) {
        ++count;
    }
    return count;
}

static int32_t countBookReaderPagesByPixelWrap(const char *text)
{
    if (!text || text[0] == '\0') {
        return 1;
    }

    const int32_t linesPerPage = max((int32_t)1, (PORTRAIT_HEIGHT - BOOK_READER_CONTENT_Y - 30) / BOOK_READER_LINE_H);
    int32_t pageCount = 1;
    int32_t lineCount = 1;
    int32_t lineWidth = 0;
    bool lineHasContent = false;
    const int32_t maxLineWidth = BOOK_READER_CONTENT_W - 4;

    const char *p = text;
    uint32_t cp = 0;
    while (utf8NextCodepoint(&p, &cp)) {
        if (cp == '\r') {
            continue;
        }
        if (cp == '\n') {
            lineWidth = 0;
            lineHasContent = false;
            ++lineCount;
        } else {
            int32_t advance = chineseTextCodepointAdvanceNarrow(cp);
            if (lineHasContent && lineWidth + advance > maxLineWidth) {
                lineWidth = 0;
                lineHasContent = false;
                ++lineCount;
            }
            lineWidth += advance;
            lineHasContent = true;
        }

        if (lineCount > linesPerPage) {
            ++pageCount;
            lineCount = 1;
            lineWidth = (cp == '\n') ? 0 : lineWidth;
            lineHasContent = (cp != '\n') && lineHasContent;
        }
    }

    return max((int32_t)1, pageCount);
}

static void appendUtf8CodepointToBuffer(char *line, size_t lineSize, size_t *lineLen, const char *start, const char *end)
{
    if (!line || !lineLen || !start || !end || end <= start) {
        return;
    }
    size_t bytes = (size_t)(end - start);
    if (*lineLen + bytes >= lineSize) {
        return;
    }
    memcpy(line + *lineLen, start, bytes);
    *lineLen += bytes;
    line[*lineLen] = '\0';
}

static void drawBookReaderContentPage()
{
    portraitFillRect(0,
                     BOOK_READER_CONTENT_Y,
                     PORTRAIT_WIDTH,
                     PORTRAIT_HEIGHT - BOOK_READER_CONTENT_Y - BOOK_LIST_REFRESH_BOTTOM_MARGIN,
                     0xFF);

    if (selected_book_content.length() == 0) {
        drawPortraitTextInRectCenteredScaled(book_reader_status,
                                             34,
                                             250,
                                             PORTRAIT_WIDTH - 68,
                                             80,
                                             (GFXfont *)&FiraSans,
                                             0.72f);
        return;
    }

    const int32_t linesPerPage = max((int32_t)1, (PORTRAIT_HEIGHT - BOOK_READER_CONTENT_Y - 30) / BOOK_READER_LINE_H);
    const int32_t targetPage = max((int32_t)0, book_reader_page);
    const int32_t maxLineWidth = BOOK_READER_CONTENT_W - 4;

    const char *p = selected_book_content.c_str();
    uint32_t cp = 0;
    int32_t pageIndex = 0;
    int32_t lineIndex = 0;
    int32_t lineWidth = 0;
    bool lineHasContent = false;
    char line[160];
    size_t lineLen = 0;
    line[0] = '\0';

    while (*p != '\0' && lineIndex < linesPerPage) {
        const char *cpStart = p;
        if (!utf8NextCodepoint(&p, &cp)) {
            break;
        }
        const char *cpEnd = p;

        if (cp == '\r') {
            continue;
        }

        if (cp == '\n') {
            if (pageIndex == targetPage && lineLen > 0) {
                drawUtf8ChineseTextLeftAlignedClippedNarrow(line, BOOK_READER_CONTENT_X, BOOK_READER_CONTENT_Y + lineIndex * BOOK_READER_LINE_H, BOOK_READER_CONTENT_W, BOOK_READER_LINE_H);
            }
            if (pageIndex == targetPage) {
                ++lineIndex;
                if (lineIndex >= linesPerPage) {
                    break;
                }
            } else if (++lineIndex >= linesPerPage) {
                ++pageIndex;
                lineIndex = 0;
            }

            lineLen = 0;
            lineWidth = 0;
            lineHasContent = false;
            line[0] = '\0';
            continue;
        }

        const int32_t advance = chineseTextCodepointAdvanceNarrow(cp);
        if (lineHasContent && lineWidth + advance > maxLineWidth) {
            if (pageIndex == targetPage) {
                drawUtf8ChineseTextLeftAlignedClippedNarrow(line, BOOK_READER_CONTENT_X, BOOK_READER_CONTENT_Y + lineIndex * BOOK_READER_LINE_H, BOOK_READER_CONTENT_W, BOOK_READER_LINE_H);
                ++lineIndex;
                if (lineIndex >= linesPerPage) {
                    break;
                }
            } else if (++lineIndex >= linesPerPage) {
                ++pageIndex;
                lineIndex = 0;
            }

            lineLen = 0;
            lineWidth = 0;
            lineHasContent = false;
            line[0] = '\0';
        }

        if (pageIndex == targetPage) {
            appendUtf8CodepointToBuffer(line, sizeof(line), &lineLen, cpStart, cpEnd);
        }
        lineWidth += advance;
        lineHasContent = true;
    }

    if (lineLen > 0 && pageIndex == targetPage && lineIndex < linesPerPage) {
        drawUtf8ChineseTextLeftAlignedClippedNarrow(line, BOOK_READER_CONTENT_X, BOOK_READER_CONTENT_Y + lineIndex * BOOK_READER_LINE_H, BOOK_READER_CONTENT_W, BOOK_READER_LINE_H);
    }
}

static void drawBookReaderScreen()
{
    memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
    drawTopStatusBar();

    const char *title = selected_book_title[0] != '\0' ? selected_book_title : book_reader_status;
    drawUtf8ChineseTextLeftAlignedClipped(title, 24, 84, BOOK_NAV_UP_X - 48, 40);

    // Reuse the existing book-library up/down icons for reader page navigation.
    drawBitmapIcon1bpp(BOOK_NAV_UP_X, BOOK_NAV_UP_Y, BOOK_NAV_ICON_SIZE, BOOK_NAV_ICON_SIZE, book_nav_up_icon_64x64, 0x00);
    drawBitmapIcon1bpp(BOOK_NAV_DOWN_X, BOOK_NAV_DOWN_Y, BOOK_NAV_ICON_SIZE, BOOK_NAV_ICON_SIZE, book_nav_down_icon_64x64, 0x00);

    char summary[32];
    snprintf(summary, sizeof(summary), "%ld/%ld", (long)(book_reader_page + 1), (long)book_reader_total_pages);
    drawPortraitTextInRectCenteredScaled(summary,
                                         54,
                                         132,
                                         PORTRAIT_WIDTH - 108,
                                         28,
                                         (GFXfont *)&FiraSans,
                                         0.46f);

    drawBookReaderContentPage();
}

// Chinese weekday names
static const char *weekdayZh[] = {"星期日", "星期一", "星期二", "星期三", "星期四", "星期五", "星期六"};

// Translate English weather description to Chinese
static const char *translateWeatherDescZh(const char *en)
{
    if (!en || en[0] == '\0') return "天气";
    // Check common English weather descriptions from wttr.in
    if (strstr(en, "Sunny") || strstr(en, "Clear")) return "晴";
    if (strstr(en, "Partly cloudy")) return "多云";
    if (strstr(en, "Cloudy") || strstr(en, "Overcast")) return "阴";
    if (strstr(en, "Light rain") || strstr(en, "Drizzle")) return "小雨";
    if (strstr(en, "Heavy rain")) return "大雨";
    if (strstr(en, "Rain")) return "雨";
    if (strstr(en, "Thunderstorm") || strstr(en, "Storm")) return "雷暴";
    if (strstr(en, "Light snow")) return "小雪";
    if (strstr(en, "Heavy snow")) return "大雪";
    if (strstr(en, "Snow") || strstr(en, "Ice")) return "雪";
    if (strstr(en, "Fog") || strstr(en, "Haze")) return "雾";
    if (strstr(en, "Mist")) return "薄雾";
    if (strstr(en, "Windy")) return "大风";
    if (strstr(en, "Hot")) return "炎热";
    if (strstr(en, "Cold")) return "寒冷";
    if (strstr(en, "Warm")) return "温暖";
    // Chinese descriptions pass through unchanged
    if (en[0] >= 0x80) return en;
    return en; // fallback: show original English
}

// Translate English city name to Chinese
static const char *translateCityZh(const char *en)
{
    if (!en || en[0] == '\0') return "深圳";
    // Chinese names pass through unchanged (check UTF-8 lead byte)
    if ((unsigned char)en[0] >= 0x80) return en;
    // Build lowercase version for case-insensitive comparison
    char lowerEn[64];
    strncpy(lowerEn, en, sizeof(lowerEn) - 1);
    lowerEn[sizeof(lowerEn) - 1] = '\0';
    for (int i = 0; lowerEn[i]; i++) lowerEn[i] = tolower((unsigned char)lowerEn[i]);
    
    // Common city translations - case insensitive
    if (strcmp(lowerEn, "shenzhen") == 0) return "深圳";
    if (strcmp(lowerEn, "shanghai") == 0) return "上海";
    if (strcmp(lowerEn, "beijing") == 0) return "北京";
    if (strcmp(lowerEn, "guangzhou") == 0) return "广州";
    if (strcmp(lowerEn, "hangzhou") == 0) return "杭州";
    if (strcmp(lowerEn, "chengdu") == 0) return "成都";
    if (strcmp(lowerEn, "wuhan") == 0) return "武汉";
    if (strcmp(lowerEn, "nanjing") == 0) return "南京";
    if (strcmp(lowerEn, "tianjin") == 0) return "天津";
    if (strcmp(lowerEn, "chongqing") == 0) return "重庆";
    if (strcmp(lowerEn, "suzhou") == 0) return "苏州";
    if (strcmp(lowerEn, "xiamen") == 0) return "厦门";
    if (strcmp(lowerEn, "qingdao") == 0) return "青岛";
    if (strcmp(lowerEn, "harbin") == 0) return "哈尔滨";
    if (strcmp(lowerEn, "hong kong") == 0 || strcmp(lowerEn, "hongkong") == 0) return "香港";
    if (strcmp(lowerEn, "macau") == 0) return "澳门";
    if (strcmp(lowerEn, "taipei") == 0) return "台北";
    if (strcmp(lowerEn, "sanya") == 0) return "三亚";
    if (strcmp(lowerEn, "haikou") == 0) return "海口";
    if (strcmp(lowerEn, "dalian") == 0) return "大连";
    if (strcmp(lowerEn, "kunming") == 0) return "昆明";
    if (strcmp(lowerEn, "fuzhou") == 0) return "福州";
    if (strcmp(lowerEn, "hefei") == 0) return "合肥";
    if (strcmp(lowerEn, "jinan") == 0) return "济南";
    if (strcmp(lowerEn, "lanzhou") == 0) return "兰州";
    if (strcmp(lowerEn, "guiyang") == 0) return "贵阳";
    if (strcmp(lowerEn, "nanning") == 0) return "南宁";
    if (strcmp(lowerEn, "changsha") == 0) return "长沙";
    if (strcmp(lowerEn, "zhengzhou") == 0) return "郑州";
    if (strcmp(lowerEn, "shenyang") == 0) return "沈阳";
    // Ma Tso Lung variants from wttr.in / local API
    if (strstr(lowerEn, "matsolurg") || strstr(lowerEn, "matsolung") || strstr(lowerEn, "ma tso") || strstr(lowerEn, "matso") || strstr(lowerEn, "matslung") || strstr(lowerEn, "mats lung")) return "马草垄";
    return en; // fallback: show original English name
}

// Clock layout constants - bigger clock, weather pushed lower
static const int32_t CLOCK_CENTER_Y = 260;
static const int32_t CLOCK_RADIUS = 175;
static const int32_t CLOCK_TIME_Y = 455;
static const int32_t CLOCK_DATE_Y = 505;
static const int32_t CLOCK_LOC_TITLE_Y = 560;
static const int32_t CLOCK_LOC_CITY_Y = 600;
static const int32_t CLOCK_WEATHER_TITLE_Y = 660;
static const int32_t CLOCK_WEATHER_BOX_Y = 700;
static const int32_t CLOCK_WEATHER_BOX_H = 210;
// Partial refresh area: time + date region (below clock face, above location)
static const int32_t CLOCK_TIME_AREA_Y = CLOCK_TIME_Y - 10;
static const int32_t CLOCK_TIME_AREA_H = 90;

static void drawClockChineseDateLine(const struct tm &timeinfo)
{
    // Keep Chinese date format, but draw the numeric portions with a smaller
    // ASCII font. Drawing the full mixed string through the Chinese renderer
    // makes ASCII digits too large and causes overlap on the e-paper screen.
    const int32_t y = CLOCK_DATE_Y;
    char yearBuf[8];
    char monthBuf[4];
    char dayBuf[4];
    snprintf(yearBuf, sizeof(yearBuf), "%04d", timeinfo.tm_year + 1900);
    snprintf(monthBuf, sizeof(monthBuf), "%02d", timeinfo.tm_mon + 1);
    snprintf(dayBuf, sizeof(dayBuf), "%02d", timeinfo.tm_mday);

    drawPortraitTextInRectCenteredScaled(yearBuf, 88, y, 112, 38, (GFXfont *)&FiraSans, 0.50f);
    drawUtf8ChineseTextInRectSingleWidth("年", 188, y, 34, 38);
    drawPortraitTextInRectCenteredScaled(monthBuf, 218, y, 58, 38, (GFXfont *)&FiraSans, 0.50f);
    drawUtf8ChineseTextInRectSingleWidth("月", 272, y, 34, 38);
    drawPortraitTextInRectCenteredScaled(dayBuf, 302, y, 58, 38, (GFXfont *)&FiraSans, 0.50f);
    drawUtf8ChineseTextInRectSingleWidth("日", 356, y, 34, 38);
    drawUtf8ChineseTextInRectSingleWidth(weekdayZh[timeinfo.tm_wday], 398, y, 120, 38);
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

    // Section 1: 本地时钟
    // Larger analog clock face
    const int32_t cx = PORTRAIT_WIDTH / 2;
    const int32_t cy = CLOCK_CENTER_Y;
    const int32_t r = CLOCK_RADIUS;
    portraitDrawCircle(cx, cy, r, 0x00);
    portraitDrawCircle(cx, cy, r - 1, 0x00);

    for (int i = 0; i < 60; ++i) {
        float a = (i * 6.0f - 90.0f) * DEG_TO_RAD;
        int32_t outerX = cx + (int32_t)(cosf(a) * (r - 12));
        int32_t outerY = cy + (int32_t)(sinf(a) * (r - 12));
        int32_t innerR = (i % 5 == 0) ? r - 32 : r - 20;
        int32_t innerX = cx + (int32_t)(cosf(a) * innerR);
        int32_t innerY = cy + (int32_t)(sinf(a) * innerR);
        drawThickPortraitLine(innerX, innerY, outerX, outerY, (i % 5 == 0) ? 3 : 1, 0x00);
    }

    for (int hour = 1; hour <= 12; ++hour) {
        float a = (hour * 30.0f - 90.0f) * DEG_TO_RAD;
        int32_t tx = cx + (int32_t)(cosf(a) * (r - 48));
        int32_t ty = cy + (int32_t)(sinf(a) * (r - 48));
        char hourLabel[3];
        snprintf(hourLabel, sizeof(hourLabel), "%d", hour);
        drawPortraitTextInRectCenteredScaled(hourLabel, tx - 16, ty - 16, 32, 32, (GFXfont *)&FiraSans, 0.38f);
    }

    float minuteAngle = ((timeinfo.tm_min + timeinfo.tm_sec / 60.0f) * 6.0f - 90.0f) * DEG_TO_RAD;
    float hourAngle = (((timeinfo.tm_hour % 12) + timeinfo.tm_min / 60.0f) * 30.0f - 90.0f) * DEG_TO_RAD;
    int32_t hourX = cx + (int32_t)(cosf(hourAngle) * 80);
    int32_t hourY = cy + (int32_t)(sinf(hourAngle) * 80);
    int32_t minuteX = cx + (int32_t)(cosf(minuteAngle) * 125);
    int32_t minuteY = cy + (int32_t)(sinf(minuteAngle) * 125);
    drawThickPortraitLine(cx, cy, hourX, hourY, 7, 0x00);
    drawThickPortraitLine(cx, cy, minuteX, minuteY, 5, 0x00);
    portraitFillCircle(cx, cy, 10, 0x00);

    // Digital time display - ASCII only, no Chinese mixing
    char timeLine[16];
    snprintf(timeLine, sizeof(timeLine), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
    drawPortraitTextInRectCenteredScaled(timeLine, 34, CLOCK_TIME_Y, PORTRAIT_WIDTH - 68, 48, (GFXfont *)&FiraSans, 0.80f);

    // Date display - Chinese format with smaller numeric glyphs.
    drawClockChineseDateLine(timeinfo);

    // Separator line between clock and location
    portraitDrawLine(34, CLOCK_LOC_TITLE_Y - 8, PORTRAIT_WIDTH - 34, CLOCK_LOC_TITLE_Y - 8, 0x00);

    // Section 2: Current location - translate to Chinese. Title intentionally removed.
    const char *cityZh = translateCityZh(clock_weather.city[0] != '\0' ? clock_weather.city : "Shenzhen");
    drawUtf8ChineseTextInRectSingleWidth(cityZh, 34, CLOCK_LOC_TITLE_Y + 14, PORTRAIT_WIDTH - 68, 46);

    // Separator line between location and weather
    portraitDrawLine(34, CLOCK_WEATHER_TITLE_Y - 8, PORTRAIT_WIDTH - 34, CLOCK_WEATHER_TITLE_Y - 8, 0x00);

    // Section 3: Weather. Title intentionally removed.
    const int32_t weatherBoxY = CLOCK_WEATHER_TITLE_Y + 16;
    const int32_t weatherBoxH = 220;
    portraitDrawRect(34, weatherBoxY, PORTRAIT_WIDTH - 68, weatherBoxH, 0x00);
    portraitDrawRect(38, weatherBoxY + 4, PORTRAIT_WIDTH - 76, weatherBoxH - 8, 0x00);

    if (clock_weather.loaded) {
        const char *descZh = translateWeatherDescZh(clock_weather.desc);

        // Temperature on left, weather desc on right
        char tempBuf[16];
        snprintf(tempBuf, sizeof(tempBuf), "%s", clock_weather.temp[0] ? clock_weather.temp : "--");
        drawPortraitTextInRectCenteredScaled(tempBuf, 52, weatherBoxY + 22, 70, 38, (GFXfont *)&FiraSans, 0.50f);
        // Degree symbol and C
        drawUtf8ChineseTextInRectSingleWidth("度", 116, weatherBoxY + 20, 44, 40);

        // Weather description
        drawUtf8ChineseTextInRectSingleWidth(descZh, 174, weatherBoxY + 20, 170, 40);

        // Humidity and wind use separate Chinese labels + ASCII values to keep one consistent size and avoid overlap.
        drawUtf8ChineseTextInRectSingleWidth("湿度", 70, weatherBoxY + 86, 90, 30);
        drawPortraitTextInRectCenteredScaled(clock_weather.humidity[0] ? clock_weather.humidity : "--", 178, weatherBoxY + 84, 70, 34, (GFXfont *)&FiraSans, 0.38f);
        drawPortraitTextInRectCenteredScaled("%", 240, weatherBoxY + 84, 36, 34, (GFXfont *)&FiraSans, 0.34f);

        drawUtf8ChineseTextInRectSingleWidth("风速", 70, weatherBoxY + 136, 90, 30);
        drawPortraitTextInRectCenteredScaled(clock_weather.wind[0] ? clock_weather.wind : "--", 178, weatherBoxY + 134, 70, 34, (GFXfont *)&FiraSans, 0.38f);
        drawUtf8ChineseTextInRectSingleWidth("公里/时", 252, weatherBoxY + 136, 150, 30);
    } else {
        drawUtf8ChineseTextInRectSingleWidth(clock_weather.status, 48, weatherBoxY + 50, PORTRAIT_WIDTH - 96, 60);
    }
}

// Partial refresh for the clock time/date area only (avoids full screen redraw)
static void refreshClockTimeArea()
{
    // Clear the time/date region in framebuffer
    portraitFillRect(34, CLOCK_TIME_AREA_Y, PORTRAIT_WIDTH - 68, CLOCK_TIME_AREA_H, 0xFF);

    // Redraw time and date in the cleared region
    time_t now = time(NULL);
    struct tm timeinfo;
    if (now < 100000 || !localtime_r(&now, &timeinfo)) {
        memset(&timeinfo, 0, sizeof(timeinfo));
        timeinfo.tm_hour = 10;
        timeinfo.tm_min = 10;
        timeinfo.tm_sec = 0;
    }

    char timeLine[16];
    snprintf(timeLine, sizeof(timeLine), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
    drawPortraitTextInRectCenteredScaled(timeLine, 34, CLOCK_TIME_Y, PORTRAIT_WIDTH - 68, 50, (GFXfont *)&FiraSans, 0.85f);

    drawClockChineseDateLine(timeinfo);

    // Also redraw the analog clock hands (only the area inside the clock face)
    const int32_t cx = PORTRAIT_WIDTH / 2;
    const int32_t cy = CLOCK_CENTER_Y;
    const int32_t r = CLOCK_RADIUS;
    // Clear the inner clock face area for hand redraw
    portraitFillRect(cx - r + 10, cy - r + 10, 2 * r - 20, 2 * r - 20, 0xFF);
    // Redraw the clock circle outline
    portraitDrawCircle(cx, cy, r, 0x00);
    portraitDrawCircle(cx, cy, r - 1, 0x00);
    // Redraw tick marks
    for (int i = 0; i < 60; ++i) {
        float a = (i * 6.0f - 90.0f) * DEG_TO_RAD;
        int32_t outerX = cx + (int32_t)(cosf(a) * (r - 12));
        int32_t outerY = cy + (int32_t)(sinf(a) * (r - 12));
        int32_t innerR = (i % 5 == 0) ? r - 32 : r - 20;
        int32_t innerX = cx + (int32_t)(cosf(a) * innerR);
        int32_t innerY = cy + (int32_t)(sinf(a) * innerR);
        drawThickPortraitLine(innerX, innerY, outerX, outerY, (i % 5 == 0) ? 3 : 1, 0x00);
    }
    // Redraw hour numbers
    for (int hour = 1; hour <= 12; ++hour) {
        float a = (hour * 30.0f - 90.0f) * DEG_TO_RAD;
        int32_t tx = cx + (int32_t)(cosf(a) * (r - 48));
        int32_t ty = cy + (int32_t)(sinf(a) * (r - 48));
        char hourLabel[3];
        snprintf(hourLabel, sizeof(hourLabel), "%d", hour);
        drawPortraitTextInRectCenteredScaled(hourLabel, tx - 16, ty - 16, 32, 32, (GFXfont *)&FiraSans, 0.38f);
    }
    // Redraw hands
    float minuteAngle = ((timeinfo.tm_min + timeinfo.tm_sec / 60.0f) * 6.0f - 90.0f) * DEG_TO_RAD;
    float hourAngle = (((timeinfo.tm_hour % 12) + timeinfo.tm_min / 60.0f) * 30.0f - 90.0f) * DEG_TO_RAD;
    int32_t hourX = cx + (int32_t)(cosf(hourAngle) * 80);
    int32_t hourY = cy + (int32_t)(sinf(hourAngle) * 80);
    int32_t minuteX = cx + (int32_t)(cosf(minuteAngle) * 125);
    int32_t minuteY = cy + (int32_t)(sinf(minuteAngle) * 125);
    drawThickPortraitLine(cx, cy, hourX, hourY, 7, 0x00);
    drawThickPortraitLine(cx, cy, minuteX, minuteY, 5, 0x00);
    portraitFillCircle(cx, cy, 10, 0x00);

    // Refresh the combined area (clock face + time/date) on the EPD
    const int32_t refreshY = cy - r - 5;
    const int32_t refreshH = CLOCK_TIME_AREA_Y + CLOCK_TIME_AREA_H - refreshY;
    Rect_t area = portraitRectToPhysicalRect(10, refreshY, PORTRAIT_WIDTH - 20, refreshH);
    uint8_t *areaBuffer = copyPhysicalAreaFromFramebuffer(area);
    if (!areaBuffer) return;

    epd_poweron();
    for (int32_t i = 0; i < 2; ++i) {
        epd_push_pixels(area, 70, 0);
        epd_push_pixels(area, 70, 1);
    }
    epd_push_pixels(area, 70, 1);
    epd_draw_grayscale_image(area, areaBuffer);
    epd_poweroff();
    free(areaBuffer);
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

static void refreshDisplayWhiteOnly(void (*drawFn)())
{
    epd_poweron();

    epd_push_pixels(epd_full_screen(), 80, 1);
    drawFn();
    epd_draw_grayscale_image(epd_full_screen(), framebuffer);

    epd_poweroff();
}

static void refreshSdStatusArea()
{
    drawSdStatusArea();

    Rect_t statusArea = portraitRectToPhysicalRect(34, PORTRAIT_HEIGHT - 115, 472, 70);
    uint8_t *statusBuffer = copyPhysicalAreaFromFramebuffer(statusArea);
    if (!statusBuffer) {
        return;
    }

    epd_poweron();
    epd_draw_grayscale_image(statusArea, statusBuffer);
    epd_poweroff();

    free(statusBuffer);
}

static void refreshBookLibraryListArea()
{
    // For page up/down navigation, keep the header area unchanged on the EPD:
    // home/status bar, "书库", count, and up/down icons are not refreshed.
    // Only the rows below the icon/header band are wiped and replaced.
    drawBookLibraryRowsArea();

    const int32_t listRefreshH = PORTRAIT_HEIGHT - BOOK_LIST_REFRESH_Y - BOOK_LIST_REFRESH_BOTTOM_MARGIN;
    Rect_t listArea = portraitRectToPhysicalRect(0, BOOK_LIST_REFRESH_Y, PORTRAIT_WIDTH, listRefreshH);
    uint8_t *listBuffer = copyPhysicalAreaFromFramebuffer(listArea);
    if (!listBuffer) {
        return;
    }

    epd_poweron();
    for (int32_t i = 0; i < 2; ++i) {
        epd_push_pixels(listArea, 70, 0);
        epd_push_pixels(listArea, 70, 1);
    }
    epd_push_pixels(listArea, 70, 1);
    epd_draw_grayscale_image(listArea, listBuffer);
    epd_poweroff();

    free(listBuffer);
}

static void refreshBookReaderContentArea()
{
    updateBookReaderPagination();
    drawBookReaderScreen();

    const int32_t readerRefreshH = PORTRAIT_HEIGHT - BOOK_READER_CONTENT_Y - BOOK_LIST_REFRESH_BOTTOM_MARGIN;
    Rect_t readerArea = portraitRectToPhysicalRect(0, BOOK_READER_CONTENT_Y, PORTRAIT_WIDTH, readerRefreshH);
    uint8_t *readerBuffer = copyPhysicalAreaFromFramebuffer(readerArea);
    if (!readerBuffer) {
        return;
    }

    epd_poweron();
    for (int32_t i = 0; i < 2; ++i) {
        epd_push_pixels(readerArea, 70, 0);
        epd_push_pixels(readerArea, 70, 1);
    }
    epd_push_pixels(readerArea, 70, 1);
    epd_draw_grayscale_image(readerArea, readerBuffer);
    epd_poweroff();

    free(readerBuffer);
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
    if ((showingClock || showingCalculator || showingSettings || showingSettingsMenu || showingContentSettings || showingBookLibrary || showingBookReader || showingSdMenu || showingSdFolder) && touchHitsHomeStatusIcon(x, y)) {
        showingClock = false;
        showingCalculator = false;
        showingSettings = false;
        showingSettingsMenu = false;
        showingContentSettings = false;
        showingBookLibrary = false;
        showingBookReader = false;
        showingSdMenu = false;
        showingSdFolder = false;
        show_password_prompt = false;
        refreshDisplay(drawPortraitHome);
        touch_loop_interval = millis() + 300;
        return;
    }

    if (showingSdMenu) {
        if (touchHitsPortraitRect(x, y, 34, 240, 472, 90)) {
            showingSdMenu = false;
            showingSdFolder = true;
            refreshDisplayWhiteOnly(drawSdFolderScreen);
            touch_loop_interval = millis() + 300;
            return;
        }
        if (touchHitsPortraitRect(x, y, 34, 370, 472, 90)) {
            formatSdCard();
            touch_loop_interval = millis() + 300;
            return;
        }
    }

    if (showingBookLibrary) {
        // Check Up navigation button
        if (touchHitsPortraitRect(x, y, BOOK_NAV_UP_X, BOOK_NAV_UP_Y, BOOK_NAV_ICON_SIZE, BOOK_NAV_ICON_SIZE)) {
            if (book_current_page > 1) {
                int32_t previousPage = book_current_page;
                book_current_page--;
                if (!fetchBookLibrary()) {
                    book_current_page = previousPage;
                } else {
                    refreshBookLibraryListArea();
                }
            }
            touch_loop_interval = millis() + 300;
            return;
        }
        // Check Down navigation button
        if (touchHitsPortraitRect(x, y, BOOK_NAV_DOWN_X, BOOK_NAV_DOWN_Y, BOOK_NAV_ICON_SIZE, BOOK_NAV_ICON_SIZE)) {
            if (book_current_page * MAX_BOOK_ITEMS < book_total) {
                int32_t previousPage = book_current_page;
                book_current_page++;
                if (!fetchBookLibrary()) {
                    book_current_page = previousPage;
                } else {
                    refreshBookLibraryListArea();
                }
            }
            touch_loop_interval = millis() + 300;
            return;
        }

        for (int i = 0; i < book_count; ++i) {
            const int32_t rowY = BOOK_LIST_Y + i * BOOK_LIST_ROW_H;
            if (touchHitsPortraitRect(x, y, BOOK_LIST_X, rowY, BOOK_LIST_W, BOOK_LIST_ROW_BOX_H)) {
                const int32_t tappedBookId = book_items[i].id;
                const bool canUseCachedBook = (selected_book_id == tappedBookId && selected_book_content.length() > 0);
                selected_book_id = tappedBookId;
                snprintf(selected_book_title, sizeof(selected_book_title), "%s", book_items[i].title);
                snprintf(selected_book_author, sizeof(selected_book_author), "%s", book_items[i].author);
                snprintf(selected_book_category, sizeof(selected_book_category), "%s", book_items[i].category);
                book_reader_page = 0;
                if (canUseCachedBook) {
                    updateBookReaderPagination();
                    snprintf(book_reader_status, sizeof(book_reader_status), "Loaded book");
                } else {
                    selected_book_content = "";
                    book_reader_total_pages = 1;
                    snprintf(book_reader_status, sizeof(book_reader_status), "Loading book...");

                    refreshDisplay(drawBookReaderScreen);
                    if (!fetchSelectedBook(selected_book_id)) {
                        selected_book_content = "";
                    }
                }
                showingBookLibrary = false;
                showingBookReader = true;
                refreshDisplay(drawBookReaderScreen);
                touch_loop_interval = millis() + 300;
                return;
            }
        }
    }

    if (showingBookReader) {
        if (touchHitsPortraitRect(x, y, BOOK_NAV_UP_X, BOOK_NAV_UP_Y, BOOK_NAV_ICON_SIZE, BOOK_NAV_ICON_SIZE)) {
            if (book_reader_page > 0) {
                book_reader_page--;
                refreshBookReaderContentArea();
            } else {
                showingBookReader = false;
                showingBookLibrary = true;
                refreshDisplay(drawBookLibraryScreen);
            }
            touch_loop_interval = millis() + 300;
            return;
        }
        if (touchHitsPortraitRect(x, y, BOOK_NAV_DOWN_X, BOOK_NAV_DOWN_Y, BOOK_NAV_ICON_SIZE, BOOK_NAV_ICON_SIZE)) {
            if (book_reader_page + 1 < book_reader_total_pages) {
                book_reader_page++;
                refreshBookReaderContentArea();
            }
            touch_loop_interval = millis() + 300;
            return;
        }
    }

    if (showingCalculator && handleCalculatorTouch(x, y)) {
        touch_loop_interval = millis() + 300;
        return;
    }

    if (showingSettings && handleSettingsTouch(x, y)) {
        touch_loop_interval = millis() + 300;
        return;
    }

    if (showingSettingsMenu && handleSettingsMenuTouch(x, y)) {
        touch_loop_interval = millis() + 300;
        return;
    }

    if (showingContentSettings && handleContentSettingsTouch(x, y)) {
        touch_loop_interval = millis() + 300;
        return;
    }

    if (!showingClock && !showingCalculator && !showingSettings && !showingSettingsMenu && !showingContentSettings && !showingBookLibrary && !showingBookReader && !showingSdMenu && !showingSdFolder && touchHitsSettingsTile(x, y)) {
        showingSettingsMenu = true;
        refreshDisplay(drawSettingsMenuScreen);
        touch_loop_interval = millis() + 300;
        return;
    }

    if (!showingClock && !showingCalculator && !showingSettings && !showingSettingsMenu && !showingContentSettings && !showingBookLibrary && !showingBookReader && !showingSdMenu && !showingSdFolder && touchHitsBookTile(x, y)) {
        showingBookLibrary = true;
        book_count = 0;
        book_total = 0;
        book_current_page = 1;
        fetchBookLibrary();
        refreshDisplay(drawBookLibraryScreen);
        touch_loop_interval = millis() + 300;
        return;
    }

    if (!showingClock && !showingCalculator && !showingSettings && !showingSettingsMenu && !showingContentSettings && !showingBookLibrary && !showingBookReader && !showingSdMenu && !showingSdFolder && touchHitsCalculatorTile(x, y)) {
        showingCalculator = true;
        refreshDisplay(drawCalculatorScreen);
        touch_loop_interval = millis() + 300;
        return;
    }

    if (!showingClock && !showingCalculator && !showingSettings && !showingSettingsMenu && !showingContentSettings && !showingBookLibrary && !showingBookReader && !showingSdMenu && !showingSdFolder && touchHitsClockTile(x, y)) {
        showingClock = true;
        fetchClockWeatherInfo();
        refreshDisplay(drawAnalogClockScreen);
        clock_refresh_interval = millis() + 60000;
        touch_loop_interval = millis() + 300;
        return;
    }

    if (!showingSettings && !showingSettingsMenu && !showingContentSettings && !showingSdMenu && !showingSdFolder && touchHitsWifiStatusIcon(x, y)) {
        showingClock = false;
        showingCalculator = false;
        showingSettings = true;
        showingContentSettings = false;
        showingBookLibrary = false;
        showingBookReader = false;
        showingSdMenu = false;
        showingSdFolder = false;
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
        // Use default content URL when preferences are unavailable
        snprintf(saved_content_url, sizeof(saved_content_url), "http://43.133.150.106:3001");
        snprintf(content_url_input, sizeof(content_url_input), "http://43.133.150.106:3001");
        return;
    }

    String url = appPrefs.getString("contentUrl", "");
    appPrefs.end();

    if (url.length() == 0) {
        // Use default content URL when none is saved
        snprintf(saved_content_url, sizeof(saved_content_url), "http://43.133.150.106:3001");
        snprintf(content_url_input, sizeof(content_url_input), "http://43.133.150.106:3001");
    } else {
        snprintf(saved_content_url, sizeof(saved_content_url), "%s", url.c_str());
        snprintf(content_url_input, sizeof(content_url_input), "%s", url.c_str());
    }
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

    char *query = strchr(normalized, '?');
    if (query) {
        *query = '\0';
        len = strlen(normalized);
    }

    char *booksEndpoint = strstr(normalized, "/api/books");
    if (booksEndpoint != NULL) {
        // If the saved content URL is already an /api/books endpoint, remove
        // any extra path/query before appending the current page parameters.
        // This matches reference/server.js: /api/books?page=<n>&perPage=<n>.
        booksEndpoint[strlen("/api/books")] = '\0';
        snprintf(out, outSize, "%s?page=%ld&perPage=%d", normalized, (long)book_current_page, MAX_BOOK_ITEMS);
    } else {
        // The reference web app is served from the same origin as the API. If
        // the user saved a page URL like http(s)://host/index.html, use only
        // the origin before appending /api/books.
        char *scheme = strstr(normalized, "://");
        if (scheme) {
            char *path = strchr(scheme + 3, '/');
            if (path) {
                *path = '\0';
            }
        }
        snprintf(out, outSize, "%s/api/books?page=%ld&perPage=%d", normalized, (long)book_current_page, MAX_BOOK_ITEMS);
    }
}

static void buildBookDetailApiUrl(char *out, size_t outSize, int32_t bookId)
{
    if (!out || outSize == 0) {
        return;
    }
    out[0] = '\0';

    const char *base = saved_content_url[0] != '\0' ? saved_content_url : content_url_input;
    if (!base || base[0] == '\0' || bookId <= 0) {
        return;
    }

    char normalized[256];
    snprintf(normalized, sizeof(normalized), "%s", base);
    size_t len = strlen(normalized);
    while (len > 0 && normalized[len - 1] == '/') {
        normalized[len - 1] = '\0';
        --len;
    }

    char *query = strchr(normalized, '?');
    if (query) {
        *query = '\0';
    }

    char *booksEndpoint = strstr(normalized, "/api/books");
    if (booksEndpoint != NULL) {
        booksEndpoint[strlen("/api/books")] = '\0';
        snprintf(out, outSize, "%s/%ld", normalized, (long)bookId);
    } else {
        char *scheme = strstr(normalized, "://");
        if (scheme) {
            char *path = strchr(scheme + 3, '/');
            if (path) {
                *path = '\0';
            }
        }
        snprintf(out, outSize, "%s/api/books/%ld", normalized, (long)bookId);
    }
}

static void buildContentApiUrl(char *out, size_t outSize, const char *endpoint, const char *query)
{
    if (!out || outSize == 0) {
        return;
    }
    out[0] = '\0';

    const char *base = saved_content_url[0] != '\0' ? saved_content_url : content_url_input;
    if (!base || base[0] == '\0' || !endpoint || endpoint[0] == '\0') {
        return;
    }

    char origin[256];
    snprintf(origin, sizeof(origin), "%s", base);

    char *queryStart = strchr(origin, '?');
    if (queryStart) {
        *queryStart = '\0';
    }
    size_t len = strlen(origin);
    while (len > 0 && origin[len - 1] == '/') {
        origin[len - 1] = '\0';
        --len;
    }

    char *scheme = strstr(origin, "://");
    if (scheme) {
        char *path = strchr(scheme + 3, '/');
        if (path) {
            *path = '\0';
        }
    }

    snprintf(out, outSize, "%s%s%s", origin, endpoint, query ? query : "");
}

static bool httpGetString(const char *url, String &payload, char *status, size_t statusSize, uint32_t timeoutMs)
{
    if (!url || url[0] == '\0') {
        if (status && statusSize > 0) snprintf(status, statusSize, "Set Content URL first");
        return false;
    }
    if (WiFi.status() != WL_CONNECTED) {
        if (status && statusSize > 0) snprintf(status, statusSize, "WiFi not connected");
        return false;
    }

    Serial.printf("HTTP GET: %s\n", url);
    HTTPClient http;
    http.setTimeout(timeoutMs);
    WiFiClient plainClient;
    WiFiClientSecure secureClient;
    bool began = false;
    if (strncmp(url, "https://", 8) == 0) {
        secureClient.setInsecure();
        began = http.begin(secureClient, url);
    } else {
        began = http.begin(plainClient, url);
    }
    if (!began) {
        if (status && statusSize > 0) snprintf(status, statusSize, "Bad URL");
        return false;
    }

    http.setReuse(false);
    http.addHeader("Connection", "close");
    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        if (status && statusSize > 0) snprintf(status, statusSize, "HTTP error: %d", httpCode);
        http.end();
        return false;
    }

    payload = http.getString();
    http.end();
    return true;
}

static void urlEncodeSpaces(char *text)
{
    if (!text || !strchr(text, ' ')) {
        return;
    }
    char encoded[96];
    size_t outLen = 0;
    for (size_t i = 0; text[i] != '\0' && outLen < sizeof(encoded) - 1; ++i) {
        if (text[i] == ' ') {
            if (outLen + 3 >= sizeof(encoded)) break;
            encoded[outLen++] = '%';
            encoded[outLen++] = '2';
            encoded[outLen++] = '0';
        } else {
            encoded[outLen++] = text[i];
        }
    }
    encoded[outLen] = '\0';
    snprintf(text, 64, "%s", encoded);
}

static bool fetchClockWeatherInfo()
{
    clock_weather.loaded = false;
    snprintf(clock_weather.status, sizeof(clock_weather.status), "Syncing clock/weather...");
    snprintf(clock_weather.city, sizeof(clock_weather.city), "Shenzhen");

    char url[320];
    String payload;

    buildContentApiUrl(url, sizeof(url), "/api/geoip", NULL);
    if (!httpGetString(url, payload, clock_weather.status, sizeof(clock_weather.status), 10000)) {
        return false;
    }

    JsonDocument geoDoc;
    DeserializationError geoErr = deserializeJson(geoDoc, payload);
    if (geoErr) {
        snprintf(clock_weather.status, sizeof(clock_weather.status), "Geo JSON failed");
        Serial.printf("Geo JSON parse failed: %s\n", geoErr.c_str());
        return false;
    }

    JsonObject geo = geoDoc.as<JsonObject>();
    copyJsonString(clock_weather.city, sizeof(clock_weather.city), geo, "city", "");
    copyJsonString(clock_weather.timezone, sizeof(clock_weather.timezone), geo, "timezone", "Asia/Shanghai");
    if (clock_weather.city[0] == '\0') {
        snprintf(clock_weather.city, sizeof(clock_weather.city), "Shenzhen");
    }

    char weatherCity[64];
    snprintf(weatherCity, sizeof(weatherCity), "%s", clock_weather.city[0] ? clock_weather.city : "Shenzhen");
    urlEncodeSpaces(weatherCity);

    char query[96];
    snprintf(query, sizeof(query), "?city=%s", weatherCity);
    buildContentApiUrl(url, sizeof(url), "/api/weather", query);

    bool weatherOk = false;
    for (int attempt = 1; attempt <= 3 && !weatherOk; ++attempt) {
        payload = "";
        snprintf(clock_weather.status, sizeof(clock_weather.status), "Weather try %d/3...", attempt);
        weatherOk = httpGetString(url, payload, clock_weather.status, sizeof(clock_weather.status), 45000);
        if (!weatherOk && attempt < 3) {
            delay(2500);
        }
    }

    if (!weatherOk) {
        // Retry without a city query. The server can auto-detect location and this
        // avoids failures caused by spaces/non-ASCII city names in the query.
        buildContentApiUrl(url, sizeof(url), "/api/weather", NULL);
        for (int attempt = 1; attempt <= 2 && !weatherOk; ++attempt) {
            payload = "";
            snprintf(clock_weather.status, sizeof(clock_weather.status), "Weather fallback %d/2...", attempt);
            weatherOk = httpGetString(url, payload, clock_weather.status, sizeof(clock_weather.status), 45000);
            if (!weatherOk && attempt < 2) {
                delay(2500);
            }
        }
    }
    if (!weatherOk) {
        snprintf(clock_weather.status, sizeof(clock_weather.status), "天气获取失败");
        return false;
    }

    JsonDocument weatherDoc;
    DeserializationError weatherErr = deserializeJson(weatherDoc, payload);
    if (weatherErr) {
        snprintf(clock_weather.status, sizeof(clock_weather.status), "Weather JSON failed");
        Serial.printf("Weather JSON parse failed: %s\n", weatherErr.c_str());
        return false;
    }

    JsonObject weather = weatherDoc.as<JsonObject>();
    copyJsonString(clock_weather.temp, sizeof(clock_weather.temp), weather, "temp", "--");
    copyJsonString(clock_weather.desc, sizeof(clock_weather.desc), weather, "desc", "Weather");
    copyJsonString(clock_weather.humidity, sizeof(clock_weather.humidity), weather, "humidity", "--");
    copyJsonString(clock_weather.wind, sizeof(clock_weather.wind), weather, "wind", "--");

    char returnedCity[64];
    copyJsonString(returnedCity, sizeof(returnedCity), weather, "city", "");
    if (returnedCity[0] != '\0') {
        snprintf(clock_weather.city, sizeof(clock_weather.city), "%s", returnedCity);
    } else if (clock_weather.city[0] == '\0') {
        snprintf(clock_weather.city, sizeof(clock_weather.city), "Shenzhen");
    }

    snprintf(clock_weather.status, sizeof(clock_weather.status), "Weather synced");
    clock_weather.loaded = true;
    return true;
}

static void updateBookReaderPagination()
{
    book_reader_total_pages = countBookReaderPagesByPixelWrap(selected_book_content.c_str());
    if (book_reader_page < 0) {
        book_reader_page = 0;
    }
    if (book_reader_page >= book_reader_total_pages) {
        book_reader_page = book_reader_total_pages - 1;
    }
}

static bool fetchSelectedBook(int32_t bookId)
{
    if (WiFi.status() != WL_CONNECTED) {
        snprintf(book_reader_status, sizeof(book_reader_status), "WiFi not connected");
        return false;
    }

    char url[320];
    buildBookDetailApiUrl(url, sizeof(url), bookId);
    if (url[0] == '\0') {
        snprintf(book_reader_status, sizeof(book_reader_status), "Set Content URL first");
        return false;
    }

    String payload;
    bool loaded = false;
    for (int attempt = 1; attempt <= 3 && !loaded; ++attempt) {
        snprintf(book_reader_status, sizeof(book_reader_status), "Book try %d/3...", attempt);
        Serial.printf("Fetching selected book (try %d/3): %s\n", attempt, url);
        loaded = httpGetString(url, payload, book_reader_status, sizeof(book_reader_status), 20000);
        if (!loaded && attempt < 3) {
            WiFiClient().stop();
            delay(1200);
        }
    }
    if (!loaded) {
        return false;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        snprintf(book_reader_status, sizeof(book_reader_status), "Book JSON failed");
        Serial.printf("Book detail JSON parse failed: %s\n", err.c_str());
        return false;
    }

    JsonObject item = doc.as<JsonObject>();
    selected_book_id = item["id"] | bookId;
    copyBookTitle(selected_book_title, sizeof(selected_book_title), item);
    copyJsonString(selected_book_author, sizeof(selected_book_author), item, "author", "");
    copyJsonString(selected_book_category, sizeof(selected_book_category), item, "category", "");
    const char *content = item["content"] | "";
    selected_book_content = content;

    book_reader_page = 0;
    updateBookReaderPagination();
    snprintf(book_reader_status, sizeof(book_reader_status), "Loaded book");
    return true;
}

static bool fetchBookLibrary()
{
    BookListItem fetched_items[MAX_BOOK_ITEMS];
    int fetched_count = 0;
    int fetched_total = book_total;

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

    String payload;
    bool loaded = false;
    for (int attempt = 1; attempt <= 3 && !loaded; ++attempt) {
        snprintf(book_library_status, sizeof(book_library_status), "Books try %d/3...", attempt);
        Serial.printf("Fetching book library (try %d/3): %s\n", attempt, url);
        loaded = httpGetString(url, payload, book_library_status, sizeof(book_library_status), 20000);
        if (!loaded && attempt < 3) {
            WiFiClient().stop();
            delay(1200);
        }
    }
    if (!loaded) {
        return false;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        snprintf(book_library_status, sizeof(book_library_status), "JSON parse failed");
        Serial.printf("Book JSON parse failed: %s\n", err.c_str());
        return false;
    }

    JsonArray items = doc["items"].as<JsonArray>();
    fetched_total = doc["total"] | 0;
    if (items.isNull()) {
        snprintf(book_library_status, sizeof(book_library_status), "No items in JSON");
        return false;
    }

    for (JsonObject item : items) {
        if (fetched_count >= MAX_BOOK_ITEMS) {
            break;
        }
        BookListItem &book = fetched_items[fetched_count];
        book.id = item["id"] | 0;
        copyBookTitle(book.title, sizeof(book.title), item);
        copyJsonString(book.author, sizeof(book.author), item, "author", "");
        copyJsonString(book.category, sizeof(book.category), item, "category", "");
        ++fetched_count;
    }

    if (fetched_count <= 0) {
        snprintf(book_library_status, sizeof(book_library_status), "No books found");
        return false;
    }

    memcpy(book_items, fetched_items, sizeof(BookListItem) * fetched_count);
    book_count = fetched_count;
    book_total = fetched_total;
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
        } else if (showingSettingsMenu) {
            refreshDisplayExtended(drawSettingsMenuScreen, true);
        } else if (showingContentSettings) {
            refreshDisplayExtended(drawContentSettingsScreen, true);
        } else if (showingBookLibrary) {
            refreshDisplayExtended(drawBookLibraryScreen, true);
        } else if (showingBookReader) {
            refreshDisplayExtended(drawBookReaderScreen, true);
        } else if (showingSdMenu) {
            refreshDisplayExtended(drawSdMenuScreen, true);
        } else if (showingSdFolder) {
            refreshDisplayExtended(drawSdFolderScreen, true);
        } else {
            refreshDisplayExtended(drawPortraitHome, true);
        }
    }

    if (showingClock && !showingCalculator && millis() > clock_refresh_interval) {
        // Use partial refresh for regular 1-minute updates (only refreshes clock face + time area)
        refreshClockTimeArea();
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
