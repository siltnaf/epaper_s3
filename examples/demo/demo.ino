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
#include <AudioFileSourceSD.h>
#include <AudioGeneratorMP3.h>
#include <AudioGeneratorWAV.h>
#include <AudioOutputI2S.h>
#include <esp_sntp.h>
#include <esp_heap_caps.h>
#if USE_LOCAL_ESP_TTS
extern "C" {
#include "esp_tts.h"
#include "esp_tts_voice_xiaole.h"
}
extern const esp_tts_voice_t esp_tts_voice_template;
#else
// Arduino's .ino preprocessor can emit prototypes for functions that are
// inside #if USE_LOCAL_ESP_TTS blocks before it evaluates the preprocessor
// guard. Keep the type known when local TTS is disabled; the guarded ESP-TTS
// code is still excluded from the final build.
typedef void *esp_tts_handle_t;
#endif
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
uint8_t pressedHomeIcon = 0; // 1 = settings, 2 = book, 3 = voice, 4 = calculator, 5 = clock
bool showingClock = false;
bool showingCalculator = false;
bool showingSettings = false;
bool showingSettingsMenu = false;
bool showingContentSettings = false;
bool showingBookLibrary = false;
bool showingBookReader = false;
bool showingVoiceStoryLibrary = false;
bool showingVoiceStoryReader = false;
bool showingMusicLibrary = false;
bool showingMusicPlayer = false;
bool showingSdMenu = false;
bool showingSdFolder = false;
char sd_status_message[96] = "";
bool touchWasPressed = false;
bool lastWifiConnected = false;
bool wifiEnabled = true;
volatile bool wifiStatusRefreshPending = false;
volatile bool contentServerWarmupPending = false;
bool contentServerWarmed = false;
uint32_t lastContentServerWarmup = 0;
bool touchLatchActive = false;
int16_t latchedTouchX = 0;
int16_t latchedTouchY = 0;
int16_t releasedTouchX = 0;
int16_t releasedTouchY = 0;
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
    bool saved;
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
bool pending_book_auto_save = false;
int32_t pending_book_auto_save_id = 0;
uint32_t pending_book_auto_save_after = 0;

// Voice Story variables and structures
#define MAX_STORY_ITEMS 10
struct StoryListItem {
    int32_t id;
    char title[80];
    char author[40];
    char category[40];
    bool saved;
};
StoryListItem story_items[MAX_STORY_ITEMS];
int story_count = 0;
int story_total = 0;
int32_t story_current_page = 1;
char story_library_status[96] = "Tap story icon to load library";
int32_t selected_story_id = 0;
char selected_story_title[80] = "";
char selected_story_author[40] = "";
char selected_story_category[40] = "";
char story_reader_status[96] = "Select a story";
String selected_story_content;
int32_t story_reader_page = 0;
int32_t story_reader_total_pages = 1;
bool pending_story_auto_save = false;
int32_t pending_story_auto_save_id = 0;
uint32_t pending_story_auto_save_after = 0;
bool story_playing = false;

// I2S audio wiring confirmed by user:
// ESP32-S3 GPIO39 -> MAX98357A BCLK + INMP441 SCK
// ESP32-S3 GPIO48 -> MAX98357A LRC  + INMP441 WS
// ESP32-S3 GPIO45 -> MAX98357A DIN
// ESP32-S3 GPIO10 -> INMP441 SD (reserved for future microphone input)
static constexpr int AUDIO_I2S_BCLK_PIN = 39;
static constexpr int AUDIO_I2S_LRCK_PIN = 48;
static constexpr int AUDIO_I2S_DOUT_PIN = 45;
static constexpr int AUDIO_I2S_MIC_DIN_PIN = 10;
static constexpr size_t LOCAL_TTS_TASK_STACK_BYTES = 32768;
static constexpr size_t LOCAL_TTS_MIN_INTERNAL_FREE_BYTES = 96 * 1024;
static constexpr size_t LOCAL_TTS_MIN_INTERNAL_LARGEST_BLOCK_BYTES = 48 * 1024;
AudioGeneratorMP3 *audioMp3 = nullptr;
AudioGeneratorWAV *audioWav = nullptr;
AudioFileSourceSD *audioFile = nullptr;
AudioOutputI2S *audioOut = nullptr;
char current_audio_path[128] = "";

#define MAX_MUSIC_ITEMS 10
struct MusicListItem {
    int32_t id;
    char title[80];
    char filename[80];
    char audio_url[180];
    char source[40];
    bool saved;
};
MusicListItem music_items[MAX_MUSIC_ITEMS];
int music_count = 0;
int music_total = 0;
int32_t music_current_page = 1;
char music_library_status[96] = "Tap music icon to load songs";
int32_t selected_music_id = 0;
char selected_music_title[80] = "";
char selected_music_filename[80] = "";
char selected_music_url[180] = "";
char selected_music_source[40] = "";
char music_player_status[96] = "Select a song";
bool music_playing = false;

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
bool clock_hands_initialized = false;
int32_t last_clock_hour_x = 0;
int32_t last_clock_hour_y = 0;
int32_t last_clock_minute_x = 0;
int32_t last_clock_minute_y = 0;
char last_clock_time_line[16] = "";
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
    {"C", calc_key_icon_c, 34, 280, 105, 76}, {"<-", calc_key_icon_delete, 153, 280, 105, 76}, {"/", calc_key_icon_solidus, 272, 280, 105, 76}, {"*", calc_key_icon_asterisk, 391, 280, 105, 76},
    {"7", calc_key_icon_7, 34, 372, 105, 76}, {"8", calc_key_icon_8, 153, 372, 105, 76}, {"9", calc_key_icon_9, 272, 372, 105, 76}, {"-", calc_key_icon_minus, 391, 372, 105, 76},
    {"4", calc_key_icon_4, 34, 464, 105, 76}, {"5", calc_key_icon_5, 153, 464, 105, 76}, {"6", calc_key_icon_6, 272, 464, 105, 76}, {"+", calc_key_icon_plus, 391, 464, 105, 76},
    {"1", calc_key_icon_1, 34, 556, 105, 76}, {"2", calc_key_icon_2, 153, 556, 105, 76}, {"3", calc_key_icon_3, 272, 556, 105, 76}, {"=", calc_key_icon_equal, 391, 556, 105, 168},
    {"0", calc_key_icon_0, 34, 648, 224, 76}, {".", calc_key_icon_dot, 272, 638, 105, 76},
};

static void drawPortraitHome();
static void drawAnalogClockScreen();
static void refreshClockTimeArea();
static void refreshClockHandsArea();
static void refreshClockInfoArea();
static void drawCalculatorScreen();
static void drawSettingsMenuScreen();
static void toggleWifi();
static void drawSettingsScreen();
static void drawContentSettingsScreen();
static void drawBookLibraryScreen();
static void drawBookReaderScreen();
static void drawBookLibraryLoadingScreen();
static void drawVoiceStoryLibraryScreen();
static void drawVoiceStoryReaderScreen();
static void drawVoiceStoryPlayerHeader();
static void drawMusicLibraryScreen();
static void drawMusicPlayerScreen();
static void drawMusicPlayerHeader();
static void drawVoiceStoryPlayPauseIcon(int32_t x, int32_t y, int32_t w, int32_t h, bool pauseIcon, uint8_t color);
static void drawSdMenuScreen();
static void drawSdFolderScreen();
static void formatSdCard();
static void drawSdStatusArea();
static void refreshDisplayWhiteOnly(void (*drawFn)());
static void refreshSdStatusArea();
static void drawWifiScanningScreen();
static void drawBookIcon(int32_t x, int32_t y, int32_t w, int32_t h);
static void drawVoiceStoryIcon(int32_t x, int32_t y, int32_t w, int32_t h);
static void drawMusicIcon(int32_t x, int32_t y, int32_t w, int32_t h);
static void drawBookNavArrowIcon(int32_t x, int32_t y, bool up);
static void refreshDisplay(void (*drawFn)());
static void refreshDisplayExtended(void (*drawFn)(), bool use_black_refresh, int32_t refreshTime);
static void epdDrawFastWithGhostControl(Rect_t area, uint8_t *areaBuffer, uint8_t frameCount = 8, bool forceClean = false);
static void wipeHomeIconArea(uint8_t iconId);
static void drawWifiPasswordInputBox();
static void drawContentUrlInputBox();
static void refreshWifiPasswordArea();
static void refreshContentUrlArea();
static void refreshWifiKeyboardArea();
static void refreshContentKeyboardArea();
static void refreshWifiStatusIconArea();
static void refreshBookLibraryListArea();
static void refreshBookReaderContentArea();
static void refreshVoiceStoryLibraryListArea();
static void refreshVoiceStoryReaderContentArea();
static void refreshVoiceStoryPlayerHeaderArea();
static void refreshMusicLibraryListArea();
static void refreshMusicPlayerHeaderArea();
static void refreshVoiceStorySaveIconArea(int storyIndex);
static uint8_t *copyPhysicalAreaFromFramebuffer(Rect_t area);
static bool findChangedArea(Rect_t baseArea, const uint8_t *oldBuffer, const uint8_t *newBuffer, Rect_t *changedArea);
static bool findWhiteRecoveryArea(Rect_t baseArea, const uint8_t *oldBuffer, const uint8_t *newBuffer, Rect_t *wipeArea);
static bool findBlackDrawArea(Rect_t baseArea, const uint8_t *oldBuffer, const uint8_t *newBuffer, Rect_t *drawArea);
static bool fetchSelectedBook(int32_t bookId);
static bool fetchSelectedVoiceStory(int32_t storyId);
static bool prepareAndStartStoryAudio(int32_t storyId);
static bool fetchMusicLibrary();
static bool fetchSelectedMusic(int32_t songId);
static bool saveMusicToSd(int32_t songId, const char *title, const char *filename, const char *audioUrl, const char *source);
static bool isMusicSavedOnSd(int32_t songId);
static bool loadMusicFromSd(int32_t songId, char *title, char *filename, char *audioPath, char *source);
static bool loadSavedMusicFromSd();
static bool fetchAndSaveMusicItem(MusicListItem &song);
static void refreshMusicSaveIconArea(int musicIndex);
static void initAudioOutput();
static void stopAudioPlayback();
static bool startAudioPlayback(const char *path);
static void serviceAudioPlayback();
static void buildMusicApiUrl(char *out, size_t outSize);
static void buildMusicDetailApiUrl(char *out, size_t outSize, int32_t songId);
static bool buildMusicAudioDownloadUrl(const char *audioUrl, char *out, size_t outSize);
static bool handleMusicPlayerTouch(int16_t tx, int16_t ty);
static bool synthesizeStoryTtsToSd(int32_t storyId, const char *text, char *audioPath, size_t audioPathSize);
static bool writeWavHeader(File &file, uint32_t dataBytes, uint32_t sampleRate, uint16_t channels, uint16_t bitsPerSample);
static void buildBookDetailApiUrl(char *out, size_t outSize, int32_t bookId);
static void buildVoiceStoryDetailApiUrl(char *out, size_t outSize, int32_t storyId);
static void buildVoiceStoryTtsApiUrl(char *out, size_t outSize, int32_t storyId);
static void buildContentApiUrl(char *out, size_t outSize, const char *endpoint, const char *query = NULL);
static bool httpGetString(const char *url, String &payload, char *status, size_t statusSize, uint32_t timeoutMs = 10000);
static bool httpDownloadToSdFile(const char *url, const char *path, char *status, size_t statusSize, uint32_t timeoutMs = 60000);
static bool waitForWifiReady(uint32_t timeoutMs);
static void warmupContentServer(bool force = false);
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
static bool handleVoiceStoryReaderTouch(int16_t tx, int16_t ty);
static bool touchHitsVoiceStoryTile(int16_t tx, int16_t ty);
static bool touchHitsMusicTile(int16_t tx, int16_t ty);
static void processTouchRelease(int16_t startX, int16_t startY, int16_t endX, int16_t endY);
static bool handleBookSwipe(int16_t startX, int16_t startY, int16_t endX, int16_t endY);
static bool getPortraitSwipeDelta(int16_t startX, int16_t startY, int16_t endX, int16_t endY, int32_t *dx, int32_t *dy);
static bool touchHitsSettingsTile(int16_t tx, int16_t ty);
static bool touchHitsBookTile(int16_t tx, int16_t ty);
static bool touchHitsWifiStatusIcon(int16_t tx, int16_t ty);
static bool loadSavedWifiCredentials();
static void saveWifiCredentials(const char *ssid, const char *password);
static void loadContentUrl();
static void saveContentUrl();
static bool fetchBookLibrary();
static void buildBooksApiUrl(char *out, size_t outSize);
static bool saveBookToSd(int32_t bookId, const char *title, const char *author, const char *category, const char *content);
static bool loadBookFromSd(int32_t bookId, char *title, char *author, char *category, String *content);
static bool loadSavedBooksFromSd();
static bool isBookSavedOnSd(int32_t bookId);
static void drawBookSaveIcon(int32_t x, int32_t y, bool saved);
static void refreshBookSaveIconArea(int bookIndex);
static bool fetchAndSaveBookItem(BookListItem &book);
static void queueSelectedBookAutoSave();
static void processPendingBookAutoSave();

static bool fetchVoiceStoryLibrary();
static void buildVoiceStoriesApiUrl(char *out, size_t outSize);
static bool saveStoryToSd(int32_t storyId, const char *title, const char *author, const char *category, const char *content);
static bool loadStoryFromSd(int32_t storyId, char *title, char *author, char *category, String *content);
static bool loadSavedStoriesFromSd();
static bool isStorySavedOnSd(int32_t storyId);
static bool fetchAndSaveStoryItem(StoryListItem &story);
static void queueSelectedStoryAutoSave();
static void processPendingStoryAutoSave();
static const char *voiceStoryPayloadFromJson(JsonObject item);

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
static const int32_t BOOK_SAVE_ICON_SIZE = 28;
static const int32_t BOOK_SAVE_ICON_MARGIN = 4;
static const char BOOK_SD_FOLDER[] = "/books";
static const char STORY_SD_FOLDER[] = "/voice";
static const char MUSIC_SD_FOLDER[] = "/music";
static const int32_t BOOK_READER_CONTENT_X = 24;
static const int32_t BOOK_READER_CONTENT_Y = 187;
static const int32_t BOOK_READER_CONTENT_W = PORTRAIT_WIDTH - 48;
static const int32_t BOOK_READER_LINE_H = 50;
static const int32_t BOOK_READER_CHARS_PER_LINE = 14;
static const float BOOK_LIST_FONT_SCALE = 1.21f;
static const float BOOK_READER_FONT_SCALE = 1.61f;
static const float BOOK_READER_FONT_X_SCALE = 0.88f; // squared, e-reader-like width
static const int32_t BOOK_READER_FONT_BOLD_PIXELS = 1; // about +20% visual stroke weight for reader content
static const int32_t SETTINGS_MENU_ITEM_X = 54;
static const int32_t SETTINGS_MENU_ITEM_W = PORTRAIT_WIDTH - 108;
static const int32_t SETTINGS_MENU_ITEM_H = 86;
static const int32_t SETTINGS_MENU_ITEM_GAP = 28;
static const int32_t SETTINGS_MENU_FIRST_Y = 190;
static const uint8_t EPD_FAST_PARTIAL_FRAMES = 4;
static const uint8_t EPD_BALANCED_PARTIAL_FRAMES = 8;
static const uint8_t EPD_CLEANUP_PARTIAL_FRAMES = 12;
static const uint8_t EPD_PARTIAL_CLEAN_INTERVAL = 6;
static uint8_t epd_partial_refresh_count = 0;
static const int32_t STORY_PLAYER_CARD_X = 18;
static const int32_t STORY_PLAYER_CARD_Y = 72;
static const int32_t STORY_PLAYER_CARD_W = PORTRAIT_WIDTH - 36;
static const int32_t STORY_PLAYER_CARD_H = 96;
static const int32_t STORY_PLAYER_BUTTON_X = PORTRAIT_WIDTH - 152;
static const int32_t STORY_PLAYER_BUTTON_Y = 96;
static const int32_t STORY_PLAYER_BUTTON_W = 116;
static const int32_t STORY_PLAYER_BUTTON_H = 50;

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

    // Connect the 32 points with gear outline
    int32_t thickness = (pressedHomeIcon == 1) ? 4 : 1;
    for (int i = 0; i < 32; ++i) {
        int next = (i + 1) % 32;
        if (thickness > 1) {
            drawThickPortraitLine(px[i], py[i], px[next], py[next], thickness, 0x00);
        } else {
            portraitDrawLine(px[i], py[i], px[next], py[next], 0x00);
        }
    }

    // Draw the dotted circle outline (between r_inner and R1)
    const int32_t num_dots = 24;
    const float r_dotted = 24.5f;
    for (int i = 0; i < num_dots; ++i) {
        float angle = (i * 360.0f / (float)num_dots) * DEG_TO_RAD;
        int32_t dx = cx + (int32_t)roundf(cosf(angle) * r_dotted);
        int32_t dy = cy + (int32_t)roundf(sinf(angle) * r_dotted);
        if (thickness > 1) {
            portraitFillCircle(dx, dy, 2, 0x00);
        } else {
            portraitPixel(dx, dy, 0x00);
        }
    }

    // Draw the inner circle outline
    if (thickness > 1) {
        for (int32_t t = 0; t < thickness; ++t) {
            portraitDrawCircle(cx, cy, (int32_t)r_inner - t + thickness / 2, 0x00);
        }
    } else {
        portraitDrawCircle(cx, cy, (int32_t)r_inner, 0x00);
    }
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
    const int32_t thickness = (pressedHomeIcon == 2) ? 4 : 1;

    if (thickness > 1) {
        // Left cover/page outline
        drawThickPortraitLine(center, top + curve, left, top, thickness, 0x00);
        drawThickPortraitLine(left, top, left, bottom - curve, thickness, 0x00);
        drawThickPortraitLine(left, bottom - curve, center, bottom, thickness, 0x00);

        // Right cover/page outline
        drawThickPortraitLine(center, top + curve, right, top, thickness, 0x00);
        drawThickPortraitLine(right, top, right, bottom - curve, thickness, 0x00);
        drawThickPortraitLine(right, bottom - curve, center, bottom, thickness, 0x00);

        // Center spine
        drawThickPortraitLine(center, top + curve, center, bottom, thickness + 1, 0x00);

        // Page lines
        for (int32_t i = 0; i < 3; ++i) {
            int32_t yy = top + 10 + i * 9;
            drawThickPortraitLine(left + 9, yy, center - 9, yy + 4, thickness, 0x00);
            drawThickPortraitLine(center + 9, yy + 4, right - 9, yy, thickness, 0x00);
        }
    } else {
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
}

static void drawRoundedRectOutline(int32_t rx, int32_t ry, int32_t rw, int32_t rh, int32_t r, uint8_t color)
{
    // Draw 4 straight lines
    portraitDrawLine(rx + r, ry, rx + rw - r, ry, color);
    portraitDrawLine(rx + r, ry + rh - 1, rx + rw - r, ry + rh - 1, color);
    portraitDrawLine(rx, ry + r, rx, ry + rh - r, color);
    portraitDrawLine(rx + rw - 1, ry + r, rx + rw - 1, ry + rh - r, color);

    // Draw 4 corner arcs
    for (int deg = 180; deg <= 270; deg += 3) {
        float a = deg * DEG_TO_RAD;
        portraitPixel(rx + r + (int32_t)roundf(cosf(a) * r), ry + r + (int32_t)roundf(sinf(a) * r), color);
    }
    for (int deg = 270; deg <= 360; deg += 3) {
        float a = deg * DEG_TO_RAD;
        portraitPixel(rx + rw - r + (int32_t)roundf(cosf(a) * r), ry + r + (int32_t)roundf(sinf(a) * r), color);
    }
    for (int deg = 0; deg <= 90; deg += 3) {
        float a = deg * DEG_TO_RAD;
        portraitPixel(rx + rw - r + (int32_t)roundf(cosf(a) * r), ry + rh - r + (int32_t)roundf(sinf(a) * r), color);
    }
    for (int deg = 90; deg <= 180; deg += 3) {
        float a = deg * DEG_TO_RAD;
        portraitPixel(rx + r + (int32_t)roundf(cosf(a) * r), ry + rh - r + (int32_t)roundf(sinf(a) * r), color);
    }
}

static void drawVoiceStoryIcon(int32_t x, int32_t y, int32_t w, int32_t h)
{
    // Voice-story icon: exact vector match of the uploaded headset (headphones) graphic.
    const int32_t cx = x + w / 2;
    const int32_t cy = y + h / 2;
    const int32_t arcCenterY = cy - 2;
    const int32_t R = 41;
    // THINNEST line (1px) for the voice story icon by default, and thick line (4px) when pressed
    const int32_t thickness = (pressedHomeIcon == 3) ? 4 : 1;

    // Draw the headband arc (thick top semicircle from 180 to 360 degrees)
    for (int32_t t = 0; t < thickness; ++t) {
        int32_t r_t = R - t;
        for (int deg = 180; deg <= 360; deg += 1) {
            float a = deg * DEG_TO_RAD;
            int32_t px = cx + (int32_t)roundf(cosf(a) * r_t);
            int32_t py = arcCenterY + (int32_t)roundf(sinf(a) * r_t);
            portraitPixel(px, py, 0x00);
        }
    }

    // Ear pads parameters
    const int32_t earW = 24;
    const int32_t earH = 44;
    const int32_t earY = arcCenterY;
    const int32_t leftEarX = cx - R;
    const int32_t rightEarX = cx + R - earW;

    // Draw thick/thin rounded rectangular ear pads
    for (int32_t t = 0; t < thickness; ++t) {
        drawRoundedRectOutline(leftEarX + t, earY + t, earW - 2 * t, earH - 2 * t, max((int32_t)1, 8 - t), 0x00);
    }
    for (int32_t t = 0; t < thickness; ++t) {
        drawRoundedRectOutline(rightEarX + t, earY + t, earW - 2 * t, earH - 2 * t, max((int32_t)1, 8 - t), 0x00);
    }
}

static void drawMusicIcon(int32_t x, int32_t y, int32_t w, int32_t h)
{
    // Music icon: exact vector match of the uploaded music note graphic.
    const int32_t cx = x + w / 2;
    const int32_t cy = y + h / 2;
    const int32_t thickness = (pressedHomeIcon == 6) ? 4 : 1;

    // The note head is a tilted ellipse/circle drawn near the bottom-left of the icon box.
    const int32_t headCx = cx - 14;
    const int32_t headCy = cy + 18;
    const int32_t headR = 19;

    // Draw the ellipse/note head outline
    if (thickness > 1) {
        for (int32_t t = 0; t < thickness; ++t) {
            portraitDrawCircle(headCx, headCy, headR - t + thickness / 2, 0x00);
        }
    } else {
        portraitDrawCircle(headCx, headCy, headR, 0x00);
    }

    // Stem: vertical line going up from the right of the note head.
    const int32_t stemX = headCx + headR - thickness / 2;
    const int32_t stemStartY = headCy;
    const int32_t stemEndY = cy - 28;

    if (thickness > 1) {
        drawThickPortraitLine(stemX, stemStartY, stemX, stemEndY, thickness, 0x00);
    } else {
        portraitDrawLine(stemX, stemStartY, stemX, stemEndY, 0x00);
    }

    // Flag: curved flag going down and right from the top of the stem.
    const int32_t flagStartX = stemX;
    const int32_t flagStartY = stemEndY;
    const int32_t flagEndX = stemX + 26;
    const int32_t flagEndY = stemEndY + 12;

    // The curved flag is made of a bezier-like curve using a set of coordinates, or arcs.
    // Let's implement it smoothly using thin lines.
    if (thickness > 1) {
        for (int32_t t = 0; t < thickness; ++t) {
            // Upper curve of flag
            portraitDrawLine(flagStartX, flagStartY + t, flagStartX + 12, flagStartY + 2 + t, 0x00);
            portraitDrawLine(flagStartX + 12, flagStartY + 2 + t, flagStartX + 20, flagStartY + 8 + t, 0x00);
            portraitDrawLine(flagStartX + 20, flagStartY + 8 + t, flagEndX, flagEndY + t, 0x00);
            // Stem connection / flag thickness
            portraitDrawLine(flagEndX, flagEndY + t, flagEndX - 4, flagEndY + 12 + t, 0x00);
            portraitDrawLine(flagEndX - 4, flagEndY + 12 + t, flagStartX + 10, flagStartY + 16 + t, 0x00);
            portraitDrawLine(flagStartX + 10, flagStartY + 16 + t, flagStartX, flagStartY + 14 + t, 0x00);
        }
    } else {
        // Draw the precise thinnest continuous line outline of the flag:
        // Upper edge
        portraitDrawLine(flagStartX, flagStartY, flagStartX + 12, flagStartY + 2, 0x00);
        portraitDrawLine(flagStartX + 12, flagStartY + 2, flagStartX + 20, flagStartY + 8, 0x00);
        portraitDrawLine(flagStartX + 20, flagStartY + 8, flagEndX, flagEndY, 0x00);
        // Outer tip/end drop
        portraitDrawLine(flagEndX, flagEndY, flagEndX - 4, flagEndY + 12, 0x00);
        // Under edge curve back to stem
        portraitDrawLine(flagEndX - 4, flagEndY + 12, flagStartX + 10, flagStartY + 16, 0x00);
        portraitDrawLine(flagStartX + 10, flagStartY + 16, flagStartX, flagStartY + 14, 0x00);
    }
}

static void drawBookNavArrowIcon(int32_t x, int32_t y, bool up)
{
    // Arrow-only navigation icon for the book list.  Keep the 64x64 touch
    // target/layout unchanged, but intentionally omit the old surrounding
    // circle from book_nav_icons.h.
    const int32_t cx = x + BOOK_NAV_ICON_SIZE / 2;
    if (up) {
        for (int32_t yy = 13; yy <= 34; ++yy) {
            int32_t half = yy - 13;
            portraitDrawLine(cx - half, y + yy, cx + half, y + yy, 0x00);
        }
        portraitFillRect(cx - 7, y + 34, 15, 18, 0x00);
    } else {
        portraitFillRect(cx - 7, y + 12, 15, 18, 0x00);
        for (int32_t yy = 30; yy <= 51; ++yy) {
            int32_t half = 51 - yy;
            portraitDrawLine(cx - half, y + yy, cx + half, y + yy, 0x00);
        }
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
    const int32_t charY = y - (int32_t)ceilf(18.0f * BOOK_READER_FONT_SCALE);
    const int32_t charW = (int32_t)ceilf(12.0f * BOOK_READER_FONT_SCALE);
    const int32_t charH = (int32_t)ceilf(36.0f * BOOK_READER_FONT_SCALE);
    drawPortraitTextInRectCenteredScaled(label, x, charY, charW, charH, (GFXfont *)&FiraSans, BOOK_READER_FONT_SCALE);
    // Reader content only: redraw one pixel to the right to increase stroke weight
    // without changing layout, line wrapping, or the book-list/title typography.
    drawPortraitTextInRectCenteredScaled(label, x + BOOK_READER_FONT_BOLD_PIXELS, charY, charW, charH, (GFXfont *)&FiraSans, BOOK_READER_FONT_SCALE);
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
                for (int32_t bold = 1; bold <= BOOK_READER_FONT_BOLD_PIXELS; ++bold) {
                    if (x + dx + bold < clipRight) {
                        portraitPixel(x + dx + bold, y + dy, 0x00);
                    }
                }
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

    // Menu Item 3: WiFi Toggle
    int item3Y = 418;
    portraitFillRect(34, item3Y, 472, 90, 0xFF);
    portraitDrawRect(34, item3Y, 472, 90, 0x00);
    portraitDrawRect(36, item3Y + 2, 468, 86, 0x00);
    drawPortraitTextInRectCentered("WiFi", 34, item3Y, 280, 90, (GFXfont *)&FiraSans);
    const char *wifiStatus = wifiEnabled ? "ON" : "OFF";
    drawPortraitTextInRectCentered(wifiStatus, 310, item3Y, 100, 90, (GFXfont *)&FiraSans);
}

static void toggleWifi()
{
    wifiEnabled = !wifiEnabled;
    if (wifiEnabled) {
        // Turn WiFi on
        WiFi.mode(WIFI_STA);
        if (saved_wifi_ssid[0] != '\0') {
            WiFi.begin(saved_wifi_ssid, saved_wifi_password);
        } else {
            WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        }
    } else {
        // Turn WiFi off
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
    }
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

    // Menu Item 3: WiFi Toggle (Y=418)
    if (pointInRect(px, py, 34, 418, 472, 90)) {
        toggleWifi();
        refreshDisplay(drawSettingsMenuScreen);
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
    int32_t cx = startX + icon + gap;
    int32_t cy = startY;
    drawSettingsIcon(sx, sy, icon);

    // Book icon placed directly below the settings icon.
    drawBookIcon(sx, sy + icon + gap, icon, icon);

    // Voice story icon placed to the right of the book icon on the second row.
    drawVoiceStoryIcon(cx, sy + icon + gap, icon, icon);

    int32_t kx = startX + (icon + gap) * 2;
    // Music icon placed directly below the clock icon on the second row.
    drawMusicIcon(kx, sy + icon + gap, icon, icon);

    // Calculator Icon (Outline, no fill)
    int32_t calcThickness = (pressedHomeIcon == 4) ? 4 : 1;
    if (calcThickness > 1) {
        drawThickPortraitLine(cx + 28, cy + 24, cx + 28 + 62, cy + 24, calcThickness, 0x00);
        drawThickPortraitLine(cx + 28, cy + 24 + 18, cx + 28 + 62, cy + 24 + 18, calcThickness, 0x00);
        drawThickPortraitLine(cx + 28, cy + 24, cx + 28, cy + 24 + 18, calcThickness, 0x00);
        drawThickPortraitLine(cx + 28 + 62, cy + 24, cx + 28 + 62, cy + 24 + 18, calcThickness, 0x00);

        for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 3; ++c) {
                int32_t bx = cx + 28 + c * 24;
                int32_t by = cy + 54 + r * 18;
                drawThickPortraitLine(bx, by, bx + 14, by, calcThickness, 0x00);
                drawThickPortraitLine(bx, by + 10, bx + 14, by + 10, calcThickness, 0x00);
                drawThickPortraitLine(bx, by, bx, by + 10, calcThickness, 0x00);
                drawThickPortraitLine(bx + 14, by, bx + 14, by + 10, calcThickness, 0x00);
            }
        }
    } else {
        portraitDrawRect(cx + 28, cy + 24, 62, 18, 0x00);
        for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 3; ++c) {
                portraitDrawRect(cx + 28 + c * 24, cy + 54 + r * 18, 14, 10, 0x00);
            }
        }
    }

    int32_t ky = startY;
    // Clock Icon (Outline, thinnest line as possible, no fill)
    int32_t clock_cx = kx + 59;
    int32_t clock_cy = ky + 59;
    int32_t clockThickness = (pressedHomeIcon == 5) ? 4 : 1;
    if (clockThickness > 1) {
        for (int32_t t = 0; t < clockThickness; ++t) {
            portraitDrawCircle(clock_cx, clock_cy, 45 - t + clockThickness/2, 0x00);
        }
        portraitFillCircle(clock_cx, clock_cy, 4, 0x00);
        drawThickPortraitLine(clock_cx, clock_cy, clock_cx - 15, clock_cy - 12, clockThickness, 0x00); // Hour hand
        drawThickPortraitLine(clock_cx, clock_cy, clock_cx + 25, clock_cy - 15, clockThickness, 0x00); // Minute hand
    } else {
        portraitDrawCircle(clock_cx, clock_cy, 45, 0x00);
        portraitDrawCircle(clock_cx, clock_cy, 2, 0x00);
        portraitDrawLine(clock_cx, clock_cy, clock_cx - 15, clock_cy - 12, 0x00); // Hour hand
        portraitDrawLine(clock_cx, clock_cy, clock_cx + 25, clock_cy - 15, 0x00); // Minute hand
    }
}

static void drawBookLibraryLoadingScreen()
{
    memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
    drawTopStatusBar();
    drawPortraitTextCentered("Loading Books...", 360, (GFXfont *)&FiraSans);
}

static void drawMusicLibraryLoadingScreen()
{
    memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
    drawTopStatusBar();
    drawUtf8ChineseTextInRectSingleWidth("加载儿歌", 0, 340, PORTRAIT_WIDTH, 60);
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
        portraitFillRect(54, 126, PORTRAIT_WIDTH - 108, 38, 0xFF);
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

        // Category names are intentionally hidden in the list UI; keep the
        // right side clean except for the SD save icon.
    }

    // Draw save icons inside the row boxes, near the right edge
    for (int i = 0; i < book_count; ++i) {
        const int32_t y = BOOK_LIST_Y + i * BOOK_LIST_ROW_H;
        int32_t saveIconX = BOOK_LIST_X + BOOK_LIST_W - BOOK_SAVE_ICON_SIZE - 8;
        int32_t saveIconY = y + (BOOK_LIST_ROW_BOX_H - BOOK_SAVE_ICON_SIZE) / 2;
        drawBookSaveIcon(saveIconX, saveIconY, book_items[i].saved);
    }
}

static void drawBookSaveIcon(int32_t x, int32_t y, bool saved)
{
    // Unsaved: normal black icon on white. Saved: inverted white icon on black
    // so the user can see the saved state from the list without redrawing the page.
    portraitFillRect(x - 2, y - 2, BOOK_SAVE_ICON_SIZE + 4, BOOK_SAVE_ICON_SIZE + 4, saved ? 0x00 : 0xFF);
    int32_t stride = (BOOK_SAVE_ICON_W + 7) / 8;
    for (int32_t yy = 0; yy < BOOK_SAVE_ICON_H; ++yy) {
        for (int32_t xx = 0; xx < BOOK_SAVE_ICON_W; ++xx) {
            uint8_t packed = book_save_icon_24x24[yy * stride + xx / 8];
            if (packed & (0x80 >> (xx & 7))) {
                portraitPixel(x + xx, y + yy, saved ? 0xFF : 0x00);
            }
        }
    }
}

static void drawBookLibraryScreen()
{
    memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
    drawTopStatusBar();

    // draw "书库" in Chinese instead of "Book Library"
    drawUtf8ChineseTextInRectSingleWidth("书库", 0, 84, PORTRAIT_WIDTH, 40);

    // Book-list paging is gesture-only: swipe up/left for next page,
    // swipe down/right for previous page. No visible up/down icons.

    drawBookLibraryRowsArea();
}

static void drawMusicRowsArea()
{
    portraitFillRect(0,
                     BOOK_LIST_REFRESH_Y,
                     PORTRAIT_WIDTH,
                     PORTRAIT_HEIGHT - BOOK_LIST_REFRESH_Y - BOOK_LIST_REFRESH_BOTTOM_MARGIN,
                     0xFF);

    if (music_count > 0 && !showingMusicPlayer) {
        char summary[32];
        int displayedCount = (int)((music_current_page - 1) * MAX_MUSIC_ITEMS) + music_count;
        if (music_total > 0 && displayedCount > music_total) {
            displayedCount = music_total;
        }
        snprintf(summary, sizeof(summary), "%d/%d", displayedCount, music_total);
        portraitFillRect(54, 126, PORTRAIT_WIDTH - 108, 38, 0xFF);
        drawPortraitTextInRectCenteredScaled(summary, 54, 132, PORTRAIT_WIDTH - 108, 28, (GFXfont *)&FiraSans, 0.46f);
    }

    if (music_count <= 0) {
        drawPortraitTextInRectCenteredScaled(music_library_status, 34, 250, PORTRAIT_WIDTH - 68, 80, (GFXfont *)&FiraSans, 0.72f);
        return;
    }

    for (int i = 0; i < music_count; ++i) {
        const int32_t y = BOOK_LIST_Y + i * BOOK_LIST_ROW_H;
        if (y + BOOK_LIST_ROW_BOX_H > PORTRAIT_HEIGHT - 14) {
            break;
        }
        portraitDrawRect(BOOK_LIST_X, y, BOOK_LIST_W, BOOK_LIST_ROW_BOX_H, 0x00);
        drawUtf8ChineseTextLeftAlignedClipped(music_items[i].title,
                                              BOOK_LIST_X + 16,
                                              y,
                                              BOOK_LIST_W - BOOK_SAVE_ICON_SIZE - 34,
                                              BOOK_LIST_ROW_BOX_H);

        int32_t saveIconX = BOOK_LIST_X + BOOK_LIST_W - BOOK_SAVE_ICON_SIZE - 8;
        int32_t saveIconY = y + (BOOK_LIST_ROW_BOX_H - BOOK_SAVE_ICON_SIZE) / 2;
        drawBookSaveIcon(saveIconX, saveIconY, music_items[i].saved);
    }
}

static void drawMusicLibraryScreen()
{
    memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
    drawTopStatusBar();
    drawUtf8ChineseTextInRectSingleWidth("儿歌", 0, 64, PORTRAIT_WIDTH, 40);
    drawMusicRowsArea();
}

static void drawMusicPlayerHeader()
{
    portraitFillRect(0, TOP_STATUS_BAR_H, PORTRAIT_WIDTH, BOOK_READER_CONTENT_Y - TOP_STATUS_BAR_H, 0xFF);
    portraitDrawRect(STORY_PLAYER_CARD_X, STORY_PLAYER_CARD_Y, STORY_PLAYER_CARD_W, STORY_PLAYER_CARD_H, 0x00);
    portraitDrawRect(STORY_PLAYER_CARD_X + 2, STORY_PLAYER_CARD_Y + 2, STORY_PLAYER_CARD_W - 4, STORY_PLAYER_CARD_H - 4, 0x00);

    const char *title = selected_music_title[0] != '\0' ? selected_music_title : music_player_status;
    drawUtf8ChineseTextLeftAlignedClipped(title,
                                          STORY_PLAYER_CARD_X + 18,
                                          STORY_PLAYER_CARD_Y + 12,
                                          STORY_PLAYER_BUTTON_X - STORY_PLAYER_CARD_X - 34,
                                          40);
    drawUtf8ChineseTextLeftAlignedClipped(title,
                                          STORY_PLAYER_CARD_X + 18 + BOOK_READER_FONT_BOLD_PIXELS,
                                          STORY_PLAYER_CARD_Y + 12,
                                          STORY_PLAYER_BUTTON_X - STORY_PLAYER_CARD_X - 34,
                                          40);

    const char *status = music_playing ? "正在播放" : "状态: 已暂停";
    if (selected_music_url[0] == '\0') {
        status = music_player_status;
    }
    drawUtf8ChineseTextLeftAlignedClipped(status,
                                          STORY_PLAYER_CARD_X + 18,
                                          STORY_PLAYER_CARD_Y + 54,
                                          STORY_PLAYER_BUTTON_X - STORY_PLAYER_CARD_X - 34,
                                          32);

    portraitFillRect(STORY_PLAYER_BUTTON_X, STORY_PLAYER_BUTTON_Y, STORY_PLAYER_BUTTON_W, STORY_PLAYER_BUTTON_H, 0xFF);
    if (selected_music_url[0] == '\0') {
        drawUtf8ChineseTextInRectSingleWidth("加载", STORY_PLAYER_BUTTON_X, STORY_PLAYER_BUTTON_Y + 7, STORY_PLAYER_BUTTON_W, STORY_PLAYER_BUTTON_H - 14);
    } else {
        drawVoiceStoryPlayPauseIcon(STORY_PLAYER_BUTTON_X,
                                    STORY_PLAYER_BUTTON_Y,
                                    STORY_PLAYER_BUTTON_W,
                                    STORY_PLAYER_BUTTON_H,
                                    music_playing,
                                    0x00);
    }
}

static void drawMusicPlayerScreen()
{
    memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
    drawTopStatusBar();
    drawMusicPlayerHeader();
    drawMusicRowsArea();
}

static void drawVoiceStoryRowsArea()
{
    portraitFillRect(0,
                     BOOK_LIST_REFRESH_Y,
                     PORTRAIT_WIDTH,
                     PORTRAIT_HEIGHT - BOOK_LIST_REFRESH_Y - BOOK_LIST_REFRESH_BOTTOM_MARGIN,
                     0xFF);

    if (story_count > 0 && !showingVoiceStoryReader) {
        char summary[32];
        int displayedCount = (int)((story_current_page - 1) * MAX_STORY_ITEMS) + story_count;
        if (story_total > 0 && displayedCount > story_total) {
            displayedCount = story_total;
        }
        snprintf(summary, sizeof(summary), "%d/%d", displayedCount, story_total);
        portraitFillRect(54, 126, PORTRAIT_WIDTH - 108, 38, 0xFF);
        drawPortraitTextInRectCenteredScaled(summary,
                                             54,
                                             132,
                                             PORTRAIT_WIDTH - 108,
                                             28,
                                             (GFXfont *)&FiraSans,
                                             0.46f);
    }

    if (story_count <= 0) {
        drawPortraitTextInRectCenteredScaled(story_library_status,
                                             34,
                                             250,
                                             PORTRAIT_WIDTH - 68,
                                             80,
                                             (GFXfont *)&FiraSans,
                                             0.72f);
        return;
    }

    for (int i = 0; i < story_count; ++i) {
        const int32_t y = BOOK_LIST_Y + i * BOOK_LIST_ROW_H;
        if (y + BOOK_LIST_ROW_BOX_H > PORTRAIT_HEIGHT - 14) {
            break;
        }
        portraitDrawRect(BOOK_LIST_X, y, BOOK_LIST_W, BOOK_LIST_ROW_BOX_H, 0x00);

        // Display Chinese story title
        drawUtf8ChineseTextLeftAligned(story_items[i].title, BOOK_LIST_X + 16, y, BOOK_LIST_ROW_BOX_H);

        // Category names are intentionally hidden in the list UI; keep the
        // right side clean except for the SD save icon.
    }

    // Draw save icons inside the row boxes, near the right edge
    for (int i = 0; i < story_count; ++i) {
        const int32_t y = BOOK_LIST_Y + i * BOOK_LIST_ROW_H;
        int32_t saveIconX = BOOK_LIST_X + BOOK_LIST_W - BOOK_SAVE_ICON_SIZE - 8;
        int32_t saveIconY = y + (BOOK_LIST_ROW_BOX_H - BOOK_SAVE_ICON_SIZE) / 2;
        drawBookSaveIcon(saveIconX, saveIconY, story_items[i].saved);
    }
}

static void drawVoiceStoryLibraryScreen()
{
    memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
    drawTopStatusBar();

    // draw "讲故事" (Tell Stories) in Chinese
    drawUtf8ChineseTextInRectSingleWidth("讲故事", 0, 64, PORTRAIT_WIDTH, 40);

    drawVoiceStoryRowsArea();
}

static void drawVoiceStoryReaderContentPage()
{
    portraitFillRect(0,
                     BOOK_READER_CONTENT_Y,
                     PORTRAIT_WIDTH,
                     PORTRAIT_HEIGHT - BOOK_READER_CONTENT_Y - BOOK_LIST_REFRESH_BOTTOM_MARGIN,
                     0xFF);

    if (selected_story_content.length() == 0) {
        drawPortraitTextInRectCenteredScaled(story_reader_status,
                                             34,
                                             250,
                                             PORTRAIT_WIDTH - 68,
                                             80,
                                             (GFXfont *)&FiraSans,
                                             0.72f);
        return;
    }

    const int32_t linesPerPage = max((int32_t)1, (PORTRAIT_HEIGHT - BOOK_READER_CONTENT_Y - 30) / BOOK_READER_LINE_H);
    const int32_t targetPage = max((int32_t)0, story_reader_page);
    const int32_t maxLineWidth = BOOK_READER_CONTENT_W - 4;

    const char *p = selected_story_content.c_str();
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

static void drawVoiceStoryReaderScreen()
{
    memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
    drawTopStatusBar();

    drawVoiceStoryPlayerHeader();
    // Voice stories are audio-first: the full story text is kept in memory/SD
    // for the voice module to read, but it is intentionally not displayed.
    // Keep the story list visible below the player card, matching the web
    // voice-story layout and allowing the user to stay focused on playback.
    drawVoiceStoryRowsArea();
}

static void drawVoiceStoryPlayerHeader()
{
    portraitFillRect(0, TOP_STATUS_BAR_H, PORTRAIT_WIDTH, BOOK_READER_CONTENT_Y - TOP_STATUS_BAR_H, 0xFF);

    const bool contentLoading = selected_story_content.length() == 0 && strstr(story_reader_status, "Loading") != NULL;
    if (contentLoading) {
        // During the network/SD fetch, keep the player title bar hidden. The
        // previous list row title can otherwise ghost into this dense header
        // band on e-paper and create dark noise below the selected title.
        drawUtf8ChineseTextInRectSingleWidth("加载中", 0, 112, PORTRAIT_WIDTH, 44);
        return;
    }

    portraitDrawRect(STORY_PLAYER_CARD_X, STORY_PLAYER_CARD_Y, STORY_PLAYER_CARD_W, STORY_PLAYER_CARD_H, 0x00);
    portraitDrawRect(STORY_PLAYER_CARD_X + 2, STORY_PLAYER_CARD_Y + 2, STORY_PLAYER_CARD_W - 4, STORY_PLAYER_CARD_H - 4, 0x00);

    const char *title = selected_story_title[0] != '\0' ? selected_story_title : story_reader_status;
    drawUtf8ChineseTextLeftAlignedClipped(title,
                                          STORY_PLAYER_CARD_X + 18,
                                          STORY_PLAYER_CARD_Y + 12,
                                          STORY_PLAYER_BUTTON_X - STORY_PLAYER_CARD_X - 34,
                                          40);
    drawUtf8ChineseTextLeftAlignedClipped(title,
                                          STORY_PLAYER_CARD_X + 18 + BOOK_READER_FONT_BOLD_PIXELS,
                                          STORY_PLAYER_CARD_Y + 12,
                                          STORY_PLAYER_BUTTON_X - STORY_PLAYER_CARD_X - 34,
                                          40);

    const char *status = story_playing ? "正在播放" : "状态: 已暂停";
    if (selected_story_content.length() == 0) {
        status = story_reader_status;
    }
    drawUtf8ChineseTextLeftAlignedClipped(status,
                                          STORY_PLAYER_CARD_X + 18,
                                          STORY_PLAYER_CARD_Y + 54,
                                          STORY_PLAYER_BUTTON_X - STORY_PLAYER_CARD_X - 34,
                                          32);

    // Play/pause control: icon only. No filled button and no frame.
    portraitFillRect(STORY_PLAYER_BUTTON_X,
                     STORY_PLAYER_BUTTON_Y,
                     STORY_PLAYER_BUTTON_W,
                     STORY_PLAYER_BUTTON_H,
                     0xFF);

    if (selected_story_content.length() == 0) {
        drawUtf8ChineseTextInRectSingleWidth("加载", STORY_PLAYER_BUTTON_X, STORY_PLAYER_BUTTON_Y + 7, STORY_PLAYER_BUTTON_W, STORY_PLAYER_BUTTON_H - 14);
    } else {
        // Draw a thin black outline play/pause icon only, matching the
        // uploaded play.jpg / pause.jpg style with no surrounding frame.
        drawVoiceStoryPlayPauseIcon(STORY_PLAYER_BUTTON_X,
                                    STORY_PLAYER_BUTTON_Y,
                                    STORY_PLAYER_BUTTON_W,
                                    STORY_PLAYER_BUTTON_H,
                                    story_playing,
                                    0x00);
    }

    char summary[32];
    snprintf(summary, sizeof(summary), "%ld/%ld", (long)(story_reader_page + 1), (long)story_reader_total_pages);
    drawPortraitTextInRectCenteredScaled(summary,
                                         54,
                                         132,
                                         PORTRAIT_WIDTH - 108,
                                         28,
                                         (GFXfont *)&FiraSans,
                                         0.46f);
}

static void drawVoiceStoryPlayPauseIcon(int32_t x, int32_t y, int32_t w, int32_t h, bool pauseIcon, uint8_t color)
{
    const int32_t cx = x + w / 2;
    const int32_t cy = y + h / 2;

    if (pauseIcon) {
        const int32_t barW = 10;
        const int32_t barH = 28;
        const int32_t gap = 10;
        // Thin outline pause icon, matching the uploaded pause.jpg style.
        portraitDrawRect(cx - gap / 2 - barW, cy - barH / 2, barW, barH, color);
        portraitDrawRect(cx + gap / 2, cy - barH / 2, barW, barH, color);
        return;
    }

    const int32_t triW = 30;
    const int32_t triH = 32;
    const int32_t leftX = cx - triW / 2 + 3;
    const int32_t rightX = cx + triW / 2;
    const int32_t topY = cy - triH / 2;
    const int32_t bottomY = cy + triH / 2;

    // Thin outline play icon, matching the uploaded play.jpg style.
    portraitDrawLine(leftX, topY, leftX, bottomY, color);
    portraitDrawLine(leftX, topY, rightX, cy, color);
    portraitDrawLine(leftX, bottomY, rightX, cy, color);
}

static void refreshVoiceStoryPlayerHeaderArea()
{
    drawVoiceStoryPlayerHeader();

    Rect_t headerArea = portraitRectToPhysicalRect(0, TOP_STATUS_BAR_H, PORTRAIT_WIDTH, BOOK_READER_CONTENT_Y - TOP_STATUS_BAR_H);
    uint8_t *headerBuffer = copyPhysicalAreaFromFramebuffer(headerArea);
    if (!headerBuffer) {
        return;
    }

    epd_poweron();
    epd_push_pixels(headerArea, 55, 1);
    epd_push_pixels(headerArea, 55, 1);
    epd_draw_grayscale_image(headerArea, headerBuffer);
    epd_poweroff();
    free(headerBuffer);
}

static bool handleVoiceStoryReaderTouch(int16_t tx, int16_t ty)
{
    int32_t px = 0;
    int32_t py = 0;
    if (!portraitPointFromTouch(tx, ty, &px, &py, true)) {
        if (!portraitPointFromTouch(tx, ty, &px, &py, false)) {
            return false;
        }
    }

    if (!pointInRect(px, py, STORY_PLAYER_BUTTON_X, STORY_PLAYER_BUTTON_Y, STORY_PLAYER_BUTTON_W, STORY_PLAYER_BUTTON_H)) {
        return false;
    }

    if (selected_story_content.length() == 0) {
        snprintf(story_reader_status, sizeof(story_reader_status), "Loading story...");
        refreshVoiceStoryPlayerHeaderArea();
        if (selected_story_id > 0 && fetchSelectedVoiceStory(selected_story_id)) {
            story_playing = prepareAndStartStoryAudio(selected_story_id);
            // Loading from the player button changes only the player/title
            // header; keep the visible story list below untouched.
            refreshVoiceStoryPlayerHeaderArea();
            queueSelectedStoryAutoSave();
        } else {
            refreshVoiceStoryPlayerHeaderArea();
        }
        return true;
    }

    if (story_playing) {
        stopAudioPlayback();
        story_playing = false;
    } else {
        story_playing = prepareAndStartStoryAudio(selected_story_id);
    }
    snprintf(story_reader_status, sizeof(story_reader_status), "%s", story_playing ? "正在播放" : "已暂停");
    refreshVoiceStoryPlayerHeaderArea();
    return true;
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
    drawUtf8ChineseTextLeftAlignedClipped(title, 24, 84, PORTRAIT_WIDTH - 48, 40);
    // Reader title only: redraw one pixel to the right to match the
    // approximately +20% stroke weight used by book content text.
    drawUtf8ChineseTextLeftAlignedClipped(title, 24 + BOOK_READER_FONT_BOLD_PIXELS, 84, PORTRAIT_WIDTH - 48, 40);

    // Reader paging is gesture-only: swipe up/left for next page,
    // swipe down/right for previous page. No visible up/down icons.

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
static const int32_t CLOCK_DIGITAL_CELL_COUNT = 5;
static const int32_t CLOCK_DIGITAL_CELL_W[CLOCK_DIGITAL_CELL_COUNT] = {36, 36, 22, 36, 36};
static const int32_t CLOCK_DIGITAL_CELL_H = 50;
static const float CLOCK_DIGITAL_TIME_SCALE = 0.80f;

static uint32_t millisUntilNextMinute(const struct tm &timeinfo)
{
    int32_t seconds = timeinfo.tm_sec;
    if (seconds < 0 || seconds > 59) {
        seconds = 0;
    }
    return (uint32_t)((60 - seconds) * 1000 + 200);
}

static int32_t clockDigitalTimeStartX()
{
    int32_t totalW = 0;
    for (int32_t i = 0; i < CLOCK_DIGITAL_CELL_COUNT; ++i) {
        totalW += CLOCK_DIGITAL_CELL_W[i];
    }
    return 34 + ((PORTRAIT_WIDTH - 68) - totalW) / 2;
}

static int32_t clockDigitalCellX(int32_t index)
{
    int32_t x = clockDigitalTimeStartX();
    for (int32_t i = 0; i < index && i < CLOCK_DIGITAL_CELL_COUNT; ++i) {
        x += CLOCK_DIGITAL_CELL_W[i];
    }
    return x;
}

static void drawClockDigitalTimeCell(char ch, int32_t index)
{
    if (index < 0 || index >= CLOCK_DIGITAL_CELL_COUNT) {
        return;
    }
    char label[2] = {ch, '\0'};
    drawPortraitTextInRectCenteredScaled(label,
                                         clockDigitalCellX(index),
                                         CLOCK_TIME_Y,
                                         CLOCK_DIGITAL_CELL_W[index],
                                         CLOCK_DIGITAL_CELL_H,
                                         (GFXfont *)&FiraSans,
                                         CLOCK_DIGITAL_TIME_SCALE);
}

static void drawClockDigitalTimeLine(const char *timeLine)
{
    if (!timeLine || strlen(timeLine) < CLOCK_DIGITAL_CELL_COUNT) {
        return;
    }
    for (int32_t i = 0; i < CLOCK_DIGITAL_CELL_COUNT; ++i) {
        drawClockDigitalTimeCell(timeLine[i], i);
    }
    snprintf(last_clock_time_line, sizeof(last_clock_time_line), "%s", timeLine);
}

static void refreshChangedClockDigitalTimeCells(const char *timeLine)
{
    if (!timeLine || strlen(timeLine) < CLOCK_DIGITAL_CELL_COUNT) {
        return;
    }

    for (int32_t i = 0; i < CLOCK_DIGITAL_CELL_COUNT; ++i) {
        if (last_clock_time_line[i] == timeLine[i]) {
            continue;
        }

        const int32_t margin = 4;
        const int32_t x = clockDigitalCellX(i) - margin;
        const int32_t y = CLOCK_TIME_Y - margin;
        const int32_t w = CLOCK_DIGITAL_CELL_W[i] + margin * 2;
        const int32_t h = CLOCK_DIGITAL_CELL_H + margin * 2;

        portraitFillRect(x, y, w, h, 0xFF);
        drawClockDigitalTimeCell(timeLine[i], i);

        Rect_t area = portraitRectToPhysicalRect(x, y, w, h);
        uint8_t *areaBuffer = copyPhysicalAreaFromFramebuffer(area);
        if (!areaBuffer) {
            continue;
        }

        epd_poweron();
        epdDrawFastWithGhostControl(area, areaBuffer, EPD_BALANCED_PARTIAL_FRAMES);
        epd_poweroff();
        free(areaBuffer);
    }

    snprintf(last_clock_time_line, sizeof(last_clock_time_line), "%s", timeLine);
}

static void calculateClockHandEndpoints(const struct tm &timeinfo, int32_t *hourX, int32_t *hourY, int32_t *minuteX, int32_t *minuteY)
{
    const int32_t cx = PORTRAIT_WIDTH / 2;
    const int32_t cy = CLOCK_CENTER_Y;
    float minuteAngle = ((timeinfo.tm_min + timeinfo.tm_sec / 60.0f) * 6.0f - 90.0f) * DEG_TO_RAD;
    float hourAngle = (((timeinfo.tm_hour % 12) + timeinfo.tm_min / 60.0f) * 30.0f - 90.0f) * DEG_TO_RAD;

    if (hourX) *hourX = cx + (int32_t)(cosf(hourAngle) * 80);
    if (hourY) *hourY = cy + (int32_t)(sinf(hourAngle) * 80);
    if (minuteX) *minuteX = cx + (int32_t)(cosf(minuteAngle) * 125);
    if (minuteY) *minuteY = cy + (int32_t)(sinf(minuteAngle) * 125);
}

static void redrawClockFaceDetails()
{
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
}

static void drawClockHands(const struct tm &timeinfo)
{
    const int32_t cx = PORTRAIT_WIDTH / 2;
    const int32_t cy = CLOCK_CENTER_Y;
    int32_t hourX = 0;
    int32_t hourY = 0;
    int32_t minuteX = 0;
    int32_t minuteY = 0;

    calculateClockHandEndpoints(timeinfo, &hourX, &hourY, &minuteX, &minuteY);
    drawThickPortraitLine(cx, cy, hourX, hourY, 7, 0x00);
    drawThickPortraitLine(cx, cy, minuteX, minuteY, 5, 0x00);
    portraitFillCircle(cx, cy, 10, 0x00);

    last_clock_hour_x = hourX;
    last_clock_hour_y = hourY;
    last_clock_minute_x = minuteX;
    last_clock_minute_y = minuteY;
    clock_hands_initialized = true;
}

static void drawUtf8ChineseTextInRectSingleWidthScaled(const char *text, int32_t rx, int32_t ry, int32_t rw, int32_t rh, float scale)
{
    if (!text || text[0] == '\0' || scale <= 0.0f) {
        return;
    }

    int32_t textW = (int32_t)ceilf(singleWidthTextWidth(text) * scale);
    int32_t x = rx + (rw - textW) / 2;
    if (x < rx + 2) {
        x = rx + 2;
    }

    const float effectiveScale = BOOK_LIST_FONT_SCALE * scale;
    int32_t scaledFontHeight = (int32_t)ceilf(ChineseFontHeight * effectiveScale);
    int32_t y = ry + (rh - scaledFontHeight) / 2;
    if (y < ry + 1) {
        y = ry + 1;
    }

    const char *p = text;
    uint32_t cp = 0;
    while (utf8NextCodepoint(&p, &cp) && x < rx + rw - 2) {
        if (cp < 0x80) {
            char label[2] = {(char)cp, '\0'};
            const int32_t charW = (int32_t)ceilf(12.0f * effectiveScale);
            const int32_t charH = (int32_t)ceilf(36.0f * effectiveScale);
            drawPortraitTextInRectCenteredScaled(label,
                                                 x,
                                                 y + scaledFontHeight / 2 - charH / 2,
                                                 charW,
                                                 charH,
                                                 (GFXfont *)&FiraSans,
                                                 effectiveScale);
            x += (int32_t)ceilf(((cp == ' ') ? 8.0f : 12.0f) * effectiveScale);
            continue;
        }

        const ChineseGlyph *glyph = findChineseGlyph(cp);
        if (!glyph) {
            x += (int32_t)ceilf(14.0f * effectiveScale);
            continue;
        }

        const int32_t glyphW = (int32_t)ceilf(glyph->width * effectiveScale);
        const int32_t glyphH = (int32_t)ceilf(glyph->height * effectiveScale);
        for (int32_t dy = 0; dy < glyphH && y + dy < ry + rh - 1; ++dy) {
            int32_t srcY = (int32_t)floorf(dy / effectiveScale);
            if (srcY < 0) srcY = 0;
            if (srcY >= glyph->height) srcY = glyph->height - 1;
            for (int32_t dx = 0; dx < glyphW && x + dx < rx + rw - 2; ++dx) {
                int32_t srcX = (int32_t)floorf(dx / effectiveScale);
                if (srcX < 0) srcX = 0;
                if (srcX >= glyph->width) srcX = glyph->width - 1;
                uint8_t packed = ChineseFontBitmap[glyph->offset + srcY * glyph->rowBytes + srcX / 8];
                if (packed & (0x80 >> (srcX & 7))) {
                    portraitPixel(x + dx, y + dy, 0x00);
                }
            }
        }
        x += glyphW + (int32_t)ceilf(2.0f * effectiveScale);
    }
}

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

    drawPortraitTextInRectCenteredScaled(yearBuf, 58, y - 8, 132, 54, (GFXfont *)&FiraSans, 0.75f);
    drawUtf8ChineseTextInRectSingleWidthScaled("年", 178, y - 8, 44, 54, 1.0f);
    drawPortraitTextInRectCenteredScaled(monthBuf, 218, y - 8, 60, 54, (GFXfont *)&FiraSans, 0.75f);
    drawUtf8ChineseTextInRectSingleWidthScaled("月", 278, y - 8, 44, 54, 1.0f);
    drawPortraitTextInRectCenteredScaled(dayBuf, 318, y - 8, 60, 54, (GFXfont *)&FiraSans, 0.75f);
    drawUtf8ChineseTextInRectSingleWidthScaled("日", 378, y - 8, 44, 54, 1.0f);
    drawUtf8ChineseTextInRectSingleWidthScaled(weekdayZh[timeinfo.tm_wday], 418, y - 8, 118, 54, 1.0f);
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

    drawClockHands(timeinfo);

    // Digital time display - ASCII only, no Chinese mixing
    char timeLine[16];
    snprintf(timeLine, sizeof(timeLine), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
    drawClockDigitalTimeLine(timeLine);

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
        drawPortraitTextInRectCenteredScaled(tempBuf, 52, weatherBoxY + 14, 82, 54, (GFXfont *)&FiraSans, 0.75f);
        // Degree symbol and C
        drawUtf8ChineseTextInRectSingleWidthScaled("度", 128, weatherBoxY + 14, 50, 54, 1.0f);

        // Weather description
        drawUtf8ChineseTextInRectSingleWidth(descZh, 174, weatherBoxY + 20, 170, 40);

        // Humidity and wind use separate Chinese labels + ASCII values to keep one consistent size and avoid overlap.
        drawUtf8ChineseTextInRectSingleWidthScaled("湿度", 50, weatherBoxY + 78, 116, 46, 1.0f);
        drawPortraitTextInRectCenteredScaled(clock_weather.humidity[0] ? clock_weather.humidity : "--", 184, weatherBoxY + 74, 86, 50, (GFXfont *)&FiraSans, 0.75f);
        drawPortraitTextInRectCenteredScaled("%", 260, weatherBoxY + 74, 42, 50, (GFXfont *)&FiraSans, 0.75f);

        drawUtf8ChineseTextInRectSingleWidthScaled("风速", 50, weatherBoxY + 132, 116, 46, 1.0f);
        drawPortraitTextInRectCenteredScaled(clock_weather.wind[0] ? clock_weather.wind : "--", 184, weatherBoxY + 128, 86, 50, (GFXfont *)&FiraSans, 0.75f);
        drawUtf8ChineseTextInRectSingleWidthScaled("千米每时", 276, weatherBoxY + 132, 180, 46, 1.0f);
    } else {
        drawUtf8ChineseTextInRectSingleWidth("加载中", 48, weatherBoxY + 50, PORTRAIT_WIDTH - 96, 60);
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
    drawClockDigitalTimeLine(timeLine);

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
    epdDrawFastWithGhostControl(area, areaBuffer, EPD_BALANCED_PARTIAL_FRAMES);
    epd_poweroff();
    free(areaBuffer);
}

static void refreshClockHandsArea()
{
    // Minute clock update is framebuffer-diff based, but intentionally limited
    // to the clock-arm pixels.  We compare the old clock-face framebuffer with
    // the newly rendered one, then:
    //   - white-wipe only pixels where the old hands disappeared
    //   - draw only pixels where the new hands appeared
    // This avoids refreshing the whole analog face every minute.
    const int32_t faceMargin = 20;
    const int32_t faceX = max((int32_t)0, PORTRAIT_WIDTH / 2 - CLOCK_RADIUS - faceMargin);
    const int32_t faceY = max((int32_t)0, CLOCK_CENTER_Y - CLOCK_RADIUS - faceMargin);
    const int32_t faceW = min(PORTRAIT_WIDTH - faceX, CLOCK_RADIUS * 2 + faceMargin * 2);
    const int32_t faceH = min(PORTRAIT_HEIGHT - faceY, CLOCK_RADIUS * 2 + faceMargin * 2);
    Rect_t faceArea = portraitRectToPhysicalRect(faceX, faceY, faceW, faceH);

    uint8_t *oldFaceBuffer = copyPhysicalAreaFromFramebuffer(faceArea);
    if (!oldFaceBuffer) {
        if (oldFaceBuffer) free(oldFaceBuffer);
        return;
    }

    time_t now = time(NULL);
    struct tm timeinfo;
    if (now < 100000 || !localtime_r(&now, &timeinfo)) {
        memset(&timeinfo, 0, sizeof(timeinfo));
        timeinfo.tm_hour = 10;
        timeinfo.tm_min = 10;
        timeinfo.tm_sec = 0;
    }

    // Rebuild only the analog face in the framebuffer.  This creates the new
    // reference image for comparison while preserving all non-clock-page state.
    portraitFillRect(faceX, faceY, faceW, faceH, 0xFF);
    redrawClockFaceDetails();
    drawClockHands(timeinfo);

    uint8_t *newFaceBuffer = copyPhysicalAreaFromFramebuffer(faceArea);
    if (!newFaceBuffer) {
        free(oldFaceBuffer);
        if (newFaceBuffer) free(newFaceBuffer);
        return;
    }

    Rect_t wipeOldArmArea;
    Rect_t drawNewArmArea;
    bool wipeOldArm = findWhiteRecoveryArea(faceArea, oldFaceBuffer, newFaceBuffer, &wipeOldArmArea);
    bool drawNewArm = findBlackDrawArea(faceArea, oldFaceBuffer, newFaceBuffer, &drawNewArmArea);

    char timeLine[16];
    snprintf(timeLine, sizeof(timeLine), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);

    epd_poweron();
    if (wipeOldArm) {
        // Strong white pulses erase only the old arm pixels that are no longer
        // black in the framebuffer; the static dial is left untouched.
        epd_push_pixels(wipeOldArmArea, 50, 1);
        epd_push_pixels(wipeOldArmArea, 50, 1);
    }
    if (drawNewArm) {
        uint8_t *newArmBuffer = copyPhysicalAreaFromFramebuffer(drawNewArmArea);
        if (newArmBuffer) {
            epdDrawFastWithGhostControl(drawNewArmArea, newArmBuffer, EPD_FAST_PARTIAL_FRAMES);
            free(newArmBuffer);
        }
    }

    // Keep the digital time synchronized, but update only changed digit cells.
    for (int32_t i = 0; i < CLOCK_DIGITAL_CELL_COUNT; ++i) {
        if (last_clock_time_line[i] == timeLine[i]) {
            continue;
        }

        const int32_t margin = 4;
        const int32_t x = clockDigitalCellX(i) - margin;
        const int32_t y = CLOCK_TIME_Y - margin;
        const int32_t w = CLOCK_DIGITAL_CELL_W[i] + margin * 2;
        const int32_t h = CLOCK_DIGITAL_CELL_H + margin * 2;

        portraitFillRect(x, y, w, h, 0xFF);
        drawClockDigitalTimeCell(timeLine[i], i);

        Rect_t cellArea = portraitRectToPhysicalRect(x, y, w, h);
        uint8_t *cellBuffer = copyPhysicalAreaFromFramebuffer(cellArea);
        if (cellBuffer) {
            epd_push_pixels(cellArea, 45, 1);
            epdDrawFastWithGhostControl(cellArea, cellBuffer, EPD_FAST_PARTIAL_FRAMES);
            free(cellBuffer);
        }
    }
    epd_poweroff();

    snprintf(last_clock_time_line, sizeof(last_clock_time_line), "%s", timeLine);

    free(oldFaceBuffer);
    free(newFaceBuffer);
}

static void refreshClockInfoArea()
{
    // After the clock page is already visible, update only the lower
    // location/weather section with the API result.  This keeps the Home ->
    // Clock transition fast because the slow HTTP calls no longer block the
    // initial analog-clock render.
    const int32_t infoY = CLOCK_LOC_TITLE_Y - 18;
    const int32_t infoH = PORTRAIT_HEIGHT - infoY - 10;

    portraitFillRect(0, infoY, PORTRAIT_WIDTH, infoH, 0xFF);

    portraitDrawLine(34, CLOCK_LOC_TITLE_Y - 8, PORTRAIT_WIDTH - 34, CLOCK_LOC_TITLE_Y - 8, 0x00);
    const char *cityZh = translateCityZh(clock_weather.city[0] != '\0' ? clock_weather.city : "Shenzhen");
    drawUtf8ChineseTextInRectSingleWidth(cityZh, 34, CLOCK_LOC_TITLE_Y + 14, PORTRAIT_WIDTH - 68, 46);

    portraitDrawLine(34, CLOCK_WEATHER_TITLE_Y - 8, PORTRAIT_WIDTH - 34, CLOCK_WEATHER_TITLE_Y - 8, 0x00);

    const int32_t weatherBoxY = CLOCK_WEATHER_TITLE_Y + 16;
    const int32_t weatherBoxH = 220;
    portraitDrawRect(34, weatherBoxY, PORTRAIT_WIDTH - 68, weatherBoxH, 0x00);
    portraitDrawRect(38, weatherBoxY + 4, PORTRAIT_WIDTH - 76, weatherBoxH - 8, 0x00);

    if (clock_weather.loaded) {
        const char *descZh = translateWeatherDescZh(clock_weather.desc);

        char tempBuf[16];
        snprintf(tempBuf, sizeof(tempBuf), "%s", clock_weather.temp[0] ? clock_weather.temp : "--");
        drawPortraitTextInRectCenteredScaled(tempBuf, 52, weatherBoxY + 14, 82, 54, (GFXfont *)&FiraSans, 0.75f);
        drawUtf8ChineseTextInRectSingleWidthScaled("度", 128, weatherBoxY + 14, 50, 54, 1.0f);
        drawUtf8ChineseTextInRectSingleWidth(descZh, 174, weatherBoxY + 20, 170, 40);

        drawUtf8ChineseTextInRectSingleWidthScaled("湿度", 50, weatherBoxY + 78, 116, 46, 1.0f);
        drawPortraitTextInRectCenteredScaled(clock_weather.humidity[0] ? clock_weather.humidity : "--", 184, weatherBoxY + 74, 86, 50, (GFXfont *)&FiraSans, 0.75f);
        drawPortraitTextInRectCenteredScaled("%", 260, weatherBoxY + 74, 42, 50, (GFXfont *)&FiraSans, 0.75f);

        drawUtf8ChineseTextInRectSingleWidthScaled("风速", 50, weatherBoxY + 132, 116, 46, 1.0f);
        drawPortraitTextInRectCenteredScaled(clock_weather.wind[0] ? clock_weather.wind : "--", 184, weatherBoxY + 128, 86, 50, (GFXfont *)&FiraSans, 0.75f);
        drawUtf8ChineseTextInRectSingleWidthScaled("千米每时", 276, weatherBoxY + 132, 180, 46, 1.0f);
    } else {
        drawUtf8ChineseTextInRectSingleWidth("加载中", 48, weatherBoxY + 50, PORTRAIT_WIDTH - 96, 60);
    }

    Rect_t infoArea = portraitRectToPhysicalRect(0, infoY, PORTRAIT_WIDTH, infoH);
    uint8_t *infoBuffer = copyPhysicalAreaFromFramebuffer(infoArea);
    if (!infoBuffer) {
        return;
    }

    epd_poweron();
    // The weather/location band can contain dense old Chinese text and box
    // borders.  Use three white pulses before drawing the API result so stale
    // city/weather pixels are fully cleared on the e-paper panel.
    for (int i = 0; i < 3; ++i) {
        epd_push_pixels(infoArea, 50, 1);
    }
    epdDrawFastWithGhostControl(infoArea, infoBuffer, EPD_BALANCED_PARTIAL_FRAMES, true);
    epd_poweroff();
    free(infoBuffer);
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

static uint8_t packedBufferPixel(const uint8_t *buffer, int32_t width, int32_t x, int32_t y)
{
    int32_t stride = width / 2 + width % 2;
    uint8_t packed = buffer[y * stride + x / 2];
    return (x & 1) ? (packed >> 4) : (packed & 0x0F);
}

static bool findWhiteRecoveryArea(Rect_t baseArea, const uint8_t *oldBuffer, const uint8_t *newBuffer, Rect_t *wipeArea)
{
    if (!oldBuffer || !newBuffer || !wipeArea || baseArea.width <= 0 || baseArea.height <= 0) {
        return false;
    }

    int32_t minX = baseArea.width;
    int32_t minY = baseArea.height;
    int32_t maxX = -1;
    int32_t maxY = -1;

    for (int32_t y = 0; y < baseArea.height; ++y) {
        for (int32_t x = 0; x < baseArea.width; ++x) {
            const uint8_t oldPx = packedBufferPixel(oldBuffer, baseArea.width, x, y);
            const uint8_t newPx = packedBufferPixel(newBuffer, baseArea.width, x, y);

            // Fast partial drawing is weak at turning old black pixels fully white.
            // Detect pixels that belonged to the previous screen but are blank in
            // the new screen, then wipe just their bounding rectangle before draw.
            if (oldPx <= 2 && newPx >= 14) {
                if (x < minX) minX = x;
                if (y < minY) minY = y;
                if (x > maxX) maxX = x;
                if (y > maxY) maxY = y;
            }
        }
    }

    if (maxX < minX || maxY < minY) {
        return false;
    }

    const int32_t margin = 8;
    minX = max((int32_t)0, minX - margin);
    minY = max((int32_t)0, minY - margin);
    maxX = min(baseArea.width - 1, maxX + margin);
    maxY = min(baseArea.height - 1, maxY + margin);

    wipeArea->x = baseArea.x + minX;
    wipeArea->y = baseArea.y + minY;
    wipeArea->width = maxX - minX + 1;
    wipeArea->height = maxY - minY + 1;
    return true;
}

static bool findBlackDrawArea(Rect_t baseArea, const uint8_t *oldBuffer, const uint8_t *newBuffer, Rect_t *drawArea)
{
    if (!oldBuffer || !newBuffer || !drawArea || baseArea.width <= 0 || baseArea.height <= 0) {
        return false;
    }

    int32_t minX = baseArea.width;
    int32_t minY = baseArea.height;
    int32_t maxX = -1;
    int32_t maxY = -1;

    for (int32_t y = 0; y < baseArea.height; ++y) {
        for (int32_t x = 0; x < baseArea.width; ++x) {
            const uint8_t oldPx = packedBufferPixel(oldBuffer, baseArea.width, x, y);
            const uint8_t newPx = packedBufferPixel(newBuffer, baseArea.width, x, y);

            // Pixels that were white/empty before but are black in the new
            // framebuffer are the newly moved clock-arm pixels to draw.
            if (oldPx >= 14 && newPx <= 2) {
                if (x < minX) minX = x;
                if (y < minY) minY = y;
                if (x > maxX) maxX = x;
                if (y > maxY) maxY = y;
            }
        }
    }

    if (maxX < minX || maxY < minY) {
        return false;
    }

    const int32_t margin = 8;
    minX = max((int32_t)0, minX - margin);
    minY = max((int32_t)0, minY - margin);
    maxX = min(baseArea.width - 1, maxX + margin);
    maxY = min(baseArea.height - 1, maxY + margin);

    drawArea->x = baseArea.x + minX;
    drawArea->y = baseArea.y + minY;
    drawArea->width = maxX - minX + 1;
    drawArea->height = maxY - minY + 1;
    return true;
}

static bool findChangedArea(Rect_t baseArea, const uint8_t *oldBuffer, const uint8_t *newBuffer, Rect_t *changedArea)
{
    if (!oldBuffer || !newBuffer || !changedArea || baseArea.width <= 0 || baseArea.height <= 0) {
        return false;
    }

    int32_t minX = baseArea.width;
    int32_t minY = baseArea.height;
    int32_t maxX = -1;
    int32_t maxY = -1;

    for (int32_t y = 0; y < baseArea.height; ++y) {
        for (int32_t x = 0; x < baseArea.width; ++x) {
            const uint8_t oldPx = packedBufferPixel(oldBuffer, baseArea.width, x, y);
            const uint8_t newPx = packedBufferPixel(newBuffer, baseArea.width, x, y);
            if (oldPx != newPx) {
                if (x < minX) minX = x;
                if (y < minY) minY = y;
                if (x > maxX) maxX = x;
                if (y > maxY) maxY = y;
            }
        }
    }

    if (maxX < minX || maxY < minY) {
        return false;
    }

    const int32_t margin = 6;
    minX = max((int32_t)0, minX - margin);
    minY = max((int32_t)0, minY - margin);
    maxX = min(baseArea.width - 1, maxX + margin);
    maxY = min(baseArea.height - 1, maxY + margin);

    changedArea->x = baseArea.x + minX;
    changedArea->y = baseArea.y + minY;
    changedArea->width = maxX - minX + 1;
    changedArea->height = maxY - minY + 1;
    return true;
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
    // every changed value/operator/backspace result is visibly refreshed.
    // Calculator typing changes this dense area repeatedly, so first drive the
    // old result pixels white to remove ghosting, then use the normal grayscale
    // waveform instead of the fast partial waveform to make new black digits
    // and operators darker/crisper.
    const int32_t margin = CALC_DIGITS_REFRESH_MARGIN;
    int32_t refreshX = CALC_DIGITS_X - margin;
    int32_t refreshY = CALC_DIGITS_Y - margin;
    int32_t refreshW = CALC_DIGITS_W + margin * 2;
    int32_t refreshH = CALC_DIGITS_H + margin * 2;

    Rect_t area = portraitRectToPhysicalRect(refreshX, refreshY, refreshW, refreshH);

    epd_poweron();
    drawCalculatorResultArea();
    uint8_t *areaBuffer = copyPhysicalAreaFromFramebuffer(area);
    if (!areaBuffer) {
        epd_poweroff();
        return;
    }

    epd_push_pixels(area, 60, 1);
    epd_push_pixels(area, 60, 1);
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

static void refreshDisplayExtended(void (*drawFn)(), bool use_black_refresh, int32_t refreshTime = 60)
{
    epd_poweron();

    // Keep the top status bar untouched during refresh. Normal navigation uses
    // a fast partial-style update: draw the new image directly without the old
    // black/white compensation flashes. The periodic anti-ghost refresh still
    // uses a short black/white wipe by passing use_black_refresh=true.
    Rect_t contentArea = portraitRectToPhysicalRect(0, TOP_STATUS_BAR_H, PORTRAIT_WIDTH, PORTRAIT_HEIGHT - TOP_STATUS_BAR_H);
    uint8_t *oldContentBuffer = use_black_refresh ? NULL : copyPhysicalAreaFromFramebuffer(contentArea);
    if (use_black_refresh) {
        epd_push_pixels(contentArea, refreshTime, 0); // Black wipe below top bar
        epd_push_pixels(contentArea, refreshTime, 1); // White wipe below top bar
        epd_push_pixels(contentArea, refreshTime, 1); // Extra white pulse
    }

    // Redraw the framebuffer, then update only the content area below the top bar.
    drawFn();

    uint8_t *contentBuffer = copyPhysicalAreaFromFramebuffer(contentArea);
    if (contentBuffer) {
        if (use_black_refresh) {
            epd_draw_grayscale_image(contentArea, contentBuffer);
        } else {
            Rect_t whiteRecoveryArea;
            if (findWhiteRecoveryArea(contentArea, oldContentBuffer, contentBuffer, &whiteRecoveryArea)) {
                epd_push_pixels(whiteRecoveryArea, 50, 1);
                epd_push_pixels(whiteRecoveryArea, 50, 1);
            }
            // Full-screen navigation changes need stronger black than the fast
            // partial waveform can provide.  Keep the compare-based white wipe
            // above to remove unrelated previous-screen pixels, then draw the
            // new content with the normal grayscale waveform so Clock/Home
            // outlines and icons render dark and crisp.
            epd_draw_grayscale_image(contentArea, contentBuffer);
        }
        free(contentBuffer);
    }

    if (oldContentBuffer) {
        free(oldContentBuffer);
    }

    epd_poweroff();
}

static void refreshDisplay(void (*drawFn)())
{
    refreshDisplayExtended(drawFn, false);
}

static void epdDrawFastWithGhostControl(Rect_t area, uint8_t *areaBuffer, uint8_t frameCount, bool forceClean)
{
    if (!areaBuffer) {
        return;
    }

    epd_partial_refresh_count++;
    const bool doClean = forceClean || epd_partial_refresh_count >= EPD_PARTIAL_CLEAN_INTERVAL;

    if (doClean) {
        // Periodic white pre-drive removes residual charge/ghosting without a
        // heavy full black/white flash. Then use a stronger partial waveform
        // for better black density on the refreshed area.
        epd_push_pixels(area, 55, 1);
        epd_push_pixels(area, 55, 1);
        epd_draw_grayscale_image_fast(area, areaBuffer, EPD_CLEANUP_PARTIAL_FRAMES);
        epd_partial_refresh_count = 0;
    } else {
        epd_draw_grayscale_image_fast(area, areaBuffer, frameCount);
    }
}

static void wipeHomeIconArea(uint8_t iconId)
{
    if (iconId < 1 || iconId > 6) {
        return;
    }

    const int32_t icon = HOME_ICON_SIZE;
    const int32_t gap = HOME_ICON_GAP;
    const int32_t startX = homeIconStartX();
    const int32_t startY = HOME_ICON_START_Y;
    const int32_t cx = startX + icon + gap;
    const int32_t kx = startX + (icon + gap) * 2;

    int32_t rx = 0;
    int32_t ry = 0;

    switch (iconId) {
    case 1: // Settings
        rx = startX;
        ry = startY;
        break;
    case 2: // Book
        rx = startX;
        ry = startY + icon + gap;
        break;
    case 3: // Voice story
        rx = cx;
        ry = startY + icon + gap;
        break;
    case 4: // Calculator
        rx = cx;
        ry = startY;
        break;
    case 5: // Clock
        rx = kx;
        ry = startY;
        break;
    case 6: // Music
        rx = kx;
        ry = startY + icon + gap;
        break;
    }

    // Expand area slightly to make sure all thick bold line pixels are covered
    const int32_t margin = 10;
    Rect_t area = portraitRectToPhysicalRect(rx - margin, ry - margin, icon + margin * 2, icon + margin * 2);

    epd_poweron();
    // Partial wipe to white: push multiple white pulses to clean the EPD cells for this icon region
    epd_push_pixels(area, 50, 1);
    epd_push_pixels(area, 50, 1);
    epd_push_pixels(area, 50, 1);
    epd_poweroff();
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
    // home/status bar, "书库", and up/down icons are not refreshed.
    // Refresh the page counter separately plus the rows below the icon/header band.
    drawBookLibraryRowsArea();

    Rect_t counterArea = portraitRectToPhysicalRect(54, 126, PORTRAIT_WIDTH - 108, 38);
    uint8_t *counterBuffer = copyPhysicalAreaFromFramebuffer(counterArea);

    const int32_t listRefreshH = PORTRAIT_HEIGHT - BOOK_LIST_REFRESH_Y - BOOK_LIST_REFRESH_BOTTOM_MARGIN;
    Rect_t listArea = portraitRectToPhysicalRect(0, BOOK_LIST_REFRESH_Y, PORTRAIT_WIDTH, listRefreshH);
    uint8_t *listBuffer = copyPhysicalAreaFromFramebuffer(listArea);
    if (!counterBuffer || !listBuffer) {
        if (counterBuffer) free(counterBuffer);
        if (listBuffer) free(listBuffer);
        return;
    }

    epd_poweron();
    // Page swipes replace dense row text/boxes in the same lower content
    // band.  White-wipe the counter and content areas before drawing the next
    // page so old book-list rows/counter digits do not ghost into the new page.
    epd_push_pixels(counterArea, 55, 1);
    epd_push_pixels(counterArea, 55, 1);
    epd_push_pixels(counterArea, 55, 1);
    epd_push_pixels(listArea, 55, 1);
    epd_push_pixels(listArea, 55, 1);
    epd_push_pixels(listArea, 55, 1);
    epd_draw_grayscale_image(counterArea, counterBuffer);
    epdDrawFastWithGhostControl(listArea, listBuffer, EPD_BALANCED_PARTIAL_FRAMES, true);
    epd_poweroff();

    free(counterBuffer);
    free(listBuffer);
}

static void refreshBookSaveIconArea(int bookIndex)
{
    if (bookIndex < 0 || bookIndex >= book_count) {
        return;
    }

    const int32_t rowY = BOOK_LIST_Y + bookIndex * BOOK_LIST_ROW_H;
    const int32_t saveIconX = BOOK_LIST_X + BOOK_LIST_W - BOOK_SAVE_ICON_SIZE - 8;
    const int32_t saveIconY = rowY + (BOOK_LIST_ROW_BOX_H - BOOK_SAVE_ICON_SIZE) / 2;
    const int32_t margin = 4;

    drawBookSaveIcon(saveIconX, saveIconY, book_items[bookIndex].saved);

    Rect_t iconArea = portraitRectToPhysicalRect(saveIconX - margin,
                                                 saveIconY - margin,
                                                 BOOK_SAVE_ICON_SIZE + margin * 2,
                                                 BOOK_SAVE_ICON_SIZE + margin * 2);
    uint8_t *iconBuffer = copyPhysicalAreaFromFramebuffer(iconArea);
    if (!iconBuffer) {
        return;
    }

    epd_poweron();
    epdDrawFastWithGhostControl(iconArea, iconBuffer, EPD_FAST_PARTIAL_FRAMES);
    epd_poweroff();

    free(iconBuffer);
}

static void refreshBookReaderContentArea()
{
    updateBookReaderPagination();
    drawBookReaderScreen();

    Rect_t counterArea = portraitRectToPhysicalRect(54, 126, PORTRAIT_WIDTH - 108, 38);
    uint8_t *counterBuffer = copyPhysicalAreaFromFramebuffer(counterArea);

    const int32_t readerRefreshH = PORTRAIT_HEIGHT - BOOK_READER_CONTENT_Y - BOOK_LIST_REFRESH_BOTTOM_MARGIN;
    Rect_t readerArea = portraitRectToPhysicalRect(0, BOOK_READER_CONTENT_Y, PORTRAIT_WIDTH, readerRefreshH);
    uint8_t *readerBuffer = copyPhysicalAreaFromFramebuffer(readerArea);
    if (!counterBuffer || !readerBuffer) {
        if (counterBuffer) free(counterBuffer);
        if (readerBuffer) free(readerBuffer);
        return;
    }

    epd_poweron();
    // Reader page swipes replace dense Chinese text in the same content band.
    // White-wipe the page counter and reader content area before drawing the
    // next page, keeping the top status/title area stable while clearing old
    // counter/text ghosts.
    epd_push_pixels(counterArea, 55, 1);
    epd_push_pixels(counterArea, 55, 1);
    epd_push_pixels(counterArea, 55, 1);
    epd_push_pixels(readerArea, 55, 1);
    epd_push_pixels(readerArea, 55, 1);
    epd_push_pixels(readerArea, 55, 1);
    epd_draw_grayscale_image(counterArea, counterBuffer);
    // After a white wipe, the fast partial waveform can leave newly drawn
    // reader text faint. Use the normal grayscale waveform for this partial
    // content band so the new page's black Chinese glyphs are darker/crisper.
    epd_draw_grayscale_image(readerArea, readerBuffer);
    epd_poweroff();

    free(counterBuffer);
    free(readerBuffer);
}

static void refreshMusicLibraryListArea()
{
    drawMusicRowsArea();

    Rect_t counterArea = portraitRectToPhysicalRect(54, 126, PORTRAIT_WIDTH - 108, 38);
    uint8_t *counterBuffer = copyPhysicalAreaFromFramebuffer(counterArea);

    const int32_t listRefreshH = PORTRAIT_HEIGHT - BOOK_LIST_REFRESH_Y - BOOK_LIST_REFRESH_BOTTOM_MARGIN;
    Rect_t listArea = portraitRectToPhysicalRect(0, BOOK_LIST_REFRESH_Y, PORTRAIT_WIDTH, listRefreshH);
    uint8_t *listBuffer = copyPhysicalAreaFromFramebuffer(listArea);
    if (!counterBuffer || !listBuffer) {
        if (counterBuffer) free(counterBuffer);
        if (listBuffer) free(listBuffer);
        return;
    }

    epd_poweron();
    epd_push_pixels(counterArea, 55, 1);
    epd_push_pixels(counterArea, 55, 1);
    epd_push_pixels(counterArea, 55, 1);
    epd_push_pixels(listArea, 55, 1);
    epd_push_pixels(listArea, 55, 1);
    epd_push_pixels(listArea, 55, 1);
    epd_draw_grayscale_image(counterArea, counterBuffer);
    epdDrawFastWithGhostControl(listArea, listBuffer, EPD_BALANCED_PARTIAL_FRAMES, true);
    epd_poweroff();

    free(counterBuffer);
    free(listBuffer);
}

static void refreshMusicSaveIconArea(int musicIndex)
{
    if (musicIndex < 0 || musicIndex >= music_count) {
        return;
    }
    const int32_t rowY = BOOK_LIST_Y + musicIndex * BOOK_LIST_ROW_H;
    const int32_t saveIconX = BOOK_LIST_X + BOOK_LIST_W - BOOK_SAVE_ICON_SIZE - 8;
    const int32_t saveIconY = rowY + (BOOK_LIST_ROW_BOX_H - BOOK_SAVE_ICON_SIZE) / 2;
    const int32_t margin = 4;

    drawBookSaveIcon(saveIconX, saveIconY, music_items[musicIndex].saved);
    Rect_t iconArea = portraitRectToPhysicalRect(saveIconX - margin,
                                                 saveIconY - margin,
                                                 BOOK_SAVE_ICON_SIZE + margin * 2,
                                                 BOOK_SAVE_ICON_SIZE + margin * 2);
    uint8_t *iconBuffer = copyPhysicalAreaFromFramebuffer(iconArea);
    if (!iconBuffer) {
        return;
    }

    epd_poweron();
    epdDrawFastWithGhostControl(iconArea, iconBuffer, EPD_FAST_PARTIAL_FRAMES);
    epd_poweroff();

    free(iconBuffer);
}

static void refreshMusicPlayerHeaderArea()
{
    drawMusicPlayerHeader();

    Rect_t headerArea = portraitRectToPhysicalRect(0, TOP_STATUS_BAR_H, PORTRAIT_WIDTH, BOOK_READER_CONTENT_Y - TOP_STATUS_BAR_H);
    uint8_t *headerBuffer = copyPhysicalAreaFromFramebuffer(headerArea);
    if (!headerBuffer) {
        return;
    }

    epd_poweron();
    epd_push_pixels(headerArea, 55, 1);
    epd_push_pixels(headerArea, 55, 1);
    epd_draw_grayscale_image(headerArea, headerBuffer);
    epd_poweroff();
    free(headerBuffer);
}

static void refreshVoiceStoryLibraryListArea()
{
    drawVoiceStoryRowsArea();

    Rect_t counterArea = portraitRectToPhysicalRect(54, 126, PORTRAIT_WIDTH - 108, 38);
    uint8_t *counterBuffer = copyPhysicalAreaFromFramebuffer(counterArea);

    const int32_t listRefreshH = PORTRAIT_HEIGHT - BOOK_LIST_REFRESH_Y - BOOK_LIST_REFRESH_BOTTOM_MARGIN;
    Rect_t listArea = portraitRectToPhysicalRect(0, BOOK_LIST_REFRESH_Y, PORTRAIT_WIDTH, listRefreshH);
    uint8_t *listBuffer = copyPhysicalAreaFromFramebuffer(listArea);
    if (!counterBuffer || !listBuffer) {
        if (counterBuffer) free(counterBuffer);
        if (listBuffer) free(listBuffer);
        return;
    }

    epd_poweron();
    epd_push_pixels(counterArea, 55, 1);
    epd_push_pixels(counterArea, 55, 1);
    epd_push_pixels(counterArea, 55, 1);
    epd_push_pixels(listArea, 55, 1);
    epd_push_pixels(listArea, 55, 1);
    epd_push_pixels(listArea, 55, 1);
    epd_draw_grayscale_image(counterArea, counterBuffer);
    epdDrawFastWithGhostControl(listArea, listBuffer, EPD_BALANCED_PARTIAL_FRAMES, true);
    epd_poweroff();

    free(counterBuffer);
    free(listBuffer);
}

static void refreshVoiceStorySaveIconArea(int storyIndex)
{
    if (storyIndex < 0 || storyIndex >= story_count) {
        return;
    }

    const int32_t rowY = BOOK_LIST_Y + storyIndex * BOOK_LIST_ROW_H;
    const int32_t saveIconX = BOOK_LIST_X + BOOK_LIST_W - BOOK_SAVE_ICON_SIZE - 8;
    const int32_t saveIconY = rowY + (BOOK_LIST_ROW_BOX_H - BOOK_SAVE_ICON_SIZE) / 2;
    const int32_t margin = 4;

    drawBookSaveIcon(saveIconX, saveIconY, story_items[storyIndex].saved);

    Rect_t iconArea = portraitRectToPhysicalRect(saveIconX - margin,
                                                 saveIconY - margin,
                                                 BOOK_SAVE_ICON_SIZE + margin * 2,
                                                 BOOK_SAVE_ICON_SIZE + margin * 2);
    uint8_t *iconBuffer = copyPhysicalAreaFromFramebuffer(iconArea);
    if (!iconBuffer) {
        return;
    }

    epd_poweron();
    epdDrawFastWithGhostControl(iconArea, iconBuffer, EPD_FAST_PARTIAL_FRAMES);
    epd_poweroff();

    free(iconBuffer);
}

static void refreshVoiceStoryReaderContentArea()
{
    story_reader_total_pages = countBookReaderPagesByPixelWrap(selected_story_content.c_str());
    if (story_reader_page < 0) story_reader_page = 0;
    if (story_reader_page >= story_reader_total_pages) story_reader_page = story_reader_total_pages - 1;

    drawVoiceStoryReaderScreen();

    const int32_t listRefreshH = PORTRAIT_HEIGHT - BOOK_LIST_REFRESH_Y - BOOK_LIST_REFRESH_BOTTOM_MARGIN;
    Rect_t listArea = portraitRectToPhysicalRect(0, BOOK_LIST_REFRESH_Y, PORTRAIT_WIDTH, listRefreshH);
    uint8_t *listBuffer = copyPhysicalAreaFromFramebuffer(listArea);
    if (!listBuffer) {
        return;
    }

    epd_poweron();
    epd_push_pixels(listArea, 55, 1);
    epd_push_pixels(listArea, 55, 1);
    epd_push_pixels(listArea, 55, 1);
    epd_draw_grayscale_image(listArea, listBuffer);
    epd_poweroff();

    free(listBuffer);
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
    epdDrawFastWithGhostControl(passwordArea, passwordBuffer, EPD_BALANCED_PARTIAL_FRAMES);
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
    epdDrawFastWithGhostControl(urlArea, urlBuffer, EPD_BALANCED_PARTIAL_FRAMES);
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
    epdDrawFastWithGhostControl(keyboardArea, keyboardBuffer, EPD_BALANCED_PARTIAL_FRAMES, true);
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
    epdDrawFastWithGhostControl(keyboardArea, keyboardBuffer, EPD_BALANCED_PARTIAL_FRAMES, true);
    epd_poweroff();

    free(keyboardBuffer);
}

static void refreshWifiStatusIconArea()
{
    // Redraw the status bar in the framebuffer so the WiFi icon reflects the
    // current connection state, then update only the icon/tap region on the EPD.
    // The disconnected icon contains an extra "X" stroke, so connection
    // transitions need a strong white pre-drive before drawing the connected
    // icon; otherwise that old X can remain visible on the e-paper panel.
    drawTopStatusBar();

    Rect_t wifiIconArea = portraitRectToPhysicalRect(PORTRAIT_WIDTH - 158, 0, 86, TOP_STATUS_BAR_H + 4);
    uint8_t *wifiIconBuffer = copyPhysicalAreaFromFramebuffer(wifiIconArea);
    if (!wifiIconBuffer) {
        return;
    }

    epd_poweron();
    epd_push_pixels(wifiIconArea, 55, 1);
    epd_push_pixels(wifiIconArea, 55, 1);
    epd_push_pixels(wifiIconArea, 55, 1);
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

static bool touchHitsVoiceStoryTile(int16_t tx, int16_t ty)
{
    const int32_t voiceX = homeIconStartX() + HOME_ICON_SIZE + HOME_ICON_GAP;
    const int32_t voiceY = HOME_ICON_START_Y + HOME_ICON_SIZE + HOME_ICON_GAP;
    return touchHitsPortraitRect(tx, ty, voiceX, voiceY, HOME_ICON_SIZE, HOME_ICON_SIZE);
}

static bool touchHitsMusicTile(int16_t tx, int16_t ty)
{
    const int32_t musicX = homeIconStartX() + (HOME_ICON_SIZE + HOME_ICON_GAP) * 2;
    const int32_t musicY = HOME_ICON_START_Y + HOME_ICON_SIZE + HOME_ICON_GAP;
    return touchHitsPortraitRect(tx, ty, musicX, musicY, HOME_ICON_SIZE, HOME_ICON_SIZE);
}

static bool touchHitsWifiStatusIcon(int16_t tx, int16_t ty)
{
    // Tap area centered on the top status bar's WiFi icon (PORTRAIT_WIDTH - 136, y = 10)
    return touchHitsPortraitRect(tx, ty, PORTRAIT_WIDTH - 150, 0, 70, 56);
}

static bool getPortraitSwipeDelta(int16_t startX, int16_t startY, int16_t endX, int16_t endY, int32_t *dx, int32_t *dy)
{
    if (!dx || !dy) {
        return false;
    }

    int32_t sx = 0, sy = 0, ex = 0, ey = 0;
    if (portraitPointFromTouch(startX, startY, &sx, &sy, true) && portraitPointFromTouch(endX, endY, &ex, &ey, true)) {
        *dx = ex - sx;
        *dy = ey - sy;
        return true;
    }
    if (portraitPointFromTouch(startX, startY, &sx, &sy, false) && portraitPointFromTouch(endX, endY, &ex, &ey, false)) {
        *dx = ex - sx;
        *dy = ey - sy;
        return true;
    }
    return false;
}

static bool handleBookSwipe(int16_t startX, int16_t startY, int16_t endX, int16_t endY)
{
    int32_t dx = 0;
    int32_t dy = 0;
    if (!getPortraitSwipeDelta(startX, startY, endX, endY, &dx, &dy)) {
        return false;
    }

    const int32_t SWIPE_THRESHOLD = 70;
    if (abs(dx) < SWIPE_THRESHOLD && abs(dy) < SWIPE_THRESHOLD) {
        return false;
    }

    const bool nextPage = (abs(dx) >= abs(dy)) ? (dx < 0) : (dy < 0);  // left/up

    if (showingBookLibrary) {
        if (nextPage) {
            if (book_current_page * MAX_BOOK_ITEMS < book_total) {
                int32_t previousPage = book_current_page;
                book_current_page++;
                if (!fetchBookLibrary()) {
                    book_current_page = previousPage;
                } else {
                    refreshBookLibraryListArea();
                }
            }
        } else {
            if (book_current_page > 1) {
                int32_t previousPage = book_current_page;
                book_current_page--;
                if (!fetchBookLibrary()) {
                    book_current_page = previousPage;
                } else {
                    refreshBookLibraryListArea();
                }
            }
        }
        return true;
    }

    if (showingBookReader) {
        if (nextPage) {
            if (book_reader_page + 1 < book_reader_total_pages) {
                book_reader_page++;
                refreshBookReaderContentArea();
            }
        } else {
            if (book_reader_page > 0) {
                book_reader_page--;
                refreshBookReaderContentArea();
            } else {
                showingBookReader = false;
                showingBookLibrary = true;
                // Switching from book content back to the library changes a
                // dense text page into row boxes. Use a compensated refresh so
                // old reader text is wiped before the book list is drawn.
                refreshDisplayExtended(drawBookLibraryScreen, true, 90);
            }
        }
        return true;
    }

    if (showingVoiceStoryLibrary) {
        if (nextPage) {
            if (story_current_page * MAX_STORY_ITEMS < story_total) {
                int32_t previousPage = story_current_page;
                story_current_page++;
                if (!fetchVoiceStoryLibrary()) {
                    story_current_page = previousPage;
                } else {
                    refreshVoiceStoryLibraryListArea();
                }
            }
        } else {
            if (story_current_page > 1) {
                int32_t previousPage = story_current_page;
                story_current_page--;
                if (!fetchVoiceStoryLibrary()) {
                    story_current_page = previousPage;
                } else {
                    refreshVoiceStoryLibraryListArea();
                }
            }
        }
        return true;
    }

    if (showingMusicLibrary || showingMusicPlayer) {
        if (nextPage) {
            if (music_current_page * MAX_MUSIC_ITEMS < music_total) {
                int32_t previousPage = music_current_page;
                music_current_page++;
                if (!fetchMusicLibrary()) {
                    music_current_page = previousPage;
                } else {
                    refreshMusicLibraryListArea();
                }
            }
        } else {
            if (music_current_page > 1) {
                int32_t previousPage = music_current_page;
                music_current_page--;
                if (!fetchMusicLibrary()) {
                    music_current_page = previousPage;
                } else {
                    refreshMusicLibraryListArea();
                }
            }
        }
        return true;
    }

    if (showingVoiceStoryReader) {
        if (nextPage) {
            if (story_reader_page + 1 < story_reader_total_pages) {
                story_reader_page++;
                refreshVoiceStoryReaderContentArea();
            }
        } else {
            if (story_reader_page > 0) {
                story_reader_page--;
                refreshVoiceStoryReaderContentArea();
            } else {
                showingVoiceStoryReader = false;
                showingVoiceStoryLibrary = true;
                refreshDisplayExtended(drawVoiceStoryLibraryScreen, true, 90);
            }
        }
        return true;
    }

    return false;
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

static void processTouchRelease(int16_t startX, int16_t startY, int16_t endX, int16_t endY)
{
    const int16_t x = startX;
    const int16_t y = startY;
    if ((showingClock || showingCalculator || showingSettings || showingSettingsMenu || showingContentSettings || showingBookLibrary || showingBookReader || showingVoiceStoryLibrary || showingVoiceStoryReader || showingMusicLibrary || showingMusicPlayer || showingSdMenu || showingSdFolder) && touchHitsHomeStatusIcon(x, y)) {
        showingClock = false;
        showingCalculator = false;
        showingSettings = false;
        showingSettingsMenu = false;
        showingContentSettings = false;
        showingBookLibrary = false;
        showingBookReader = false;
        showingVoiceStoryLibrary = false;
        showingVoiceStoryReader = false;
        showingMusicLibrary = false;
        showingMusicPlayer = false;
        story_playing = false;
        music_playing = false;
        stopAudioPlayback();
        showingSdMenu = false;
        showingSdFolder = false;
        show_password_prompt = false;
        // Returning to Home from another full-screen app can leave visible
        // ghosting, especially after the dense Clock screen.  Use one
        // compensated refresh with double-length black/white pulses instead
        // of two separate refreshes, so it clears strongly but returns faster.
        refreshDisplayExtended(drawPortraitHome, true, 120);
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
        if (handleBookSwipe(startX, startY, endX, endY)) {
            touch_loop_interval = millis() + 300;
            return;
        }

        for (int i = 0; i < book_count; ++i) {
            const int32_t rowY = BOOK_LIST_Y + i * BOOK_LIST_ROW_H;
            
            // Check if save icon was tapped (icon is inside the row, near right edge)
            int32_t saveIconX = BOOK_LIST_X + BOOK_LIST_W - BOOK_SAVE_ICON_SIZE - 8;
            int32_t saveIconY = rowY + (BOOK_LIST_ROW_BOX_H - BOOK_SAVE_ICON_SIZE) / 2;
            if (touchHitsPortraitRect(x, y, saveIconX, saveIconY, BOOK_SAVE_ICON_SIZE, BOOK_SAVE_ICON_SIZE)) {
                // Manual save: fetch and write in the background, then update only
                // the save icon. Do not refresh the book-list screen.
                if (book_items[i].id > 0 && !book_items[i].saved && WiFi.status() == WL_CONNECTED) {
                    if (fetchAndSaveBookItem(book_items[i])) {
                        book_items[i].saved = true;
                        refreshBookSaveIconArea(i);
                    }
                }
                touch_loop_interval = millis() + 300;
                return;
            }
            
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

                    // First switch from the dense book-list rows to the reader
                    // loading page with a real refresh, otherwise row/title
                    // remnants can stay visible while the book is fetched.
                    refreshDisplayExtended(drawBookReaderScreen, true, 90);
                    if (!fetchSelectedBook(selected_book_id)) {
                        selected_book_content = "";
                    }
                }
                showingBookLibrary = false;
                showingBookReader = true;
                // Final switch to the loaded reader content also uses a
                // compensated refresh so the loading/list pixels are cleared
                // and the reader text is written with stronger black.
                refreshDisplayExtended(drawBookReaderScreen, true, 90);
                queueSelectedBookAutoSave();
                touch_loop_interval = millis() + 300;
                return;
            }
        }
    }

    if (showingBookReader) {
        if (handleBookSwipe(startX, startY, endX, endY)) {
            touch_loop_interval = millis() + 300;
            return;
        }
    }

    if (showingVoiceStoryLibrary) {
        if (handleBookSwipe(startX, startY, endX, endY)) {
            touch_loop_interval = millis() + 300;
            return;
        }

        for (int i = 0; i < story_count; ++i) {
            const int32_t rowY = BOOK_LIST_Y + i * BOOK_LIST_ROW_H;

            int32_t saveIconX = BOOK_LIST_X + BOOK_LIST_W - BOOK_SAVE_ICON_SIZE - 8;
            int32_t saveIconY = rowY + (BOOK_LIST_ROW_BOX_H - BOOK_SAVE_ICON_SIZE) / 2;
            if (touchHitsPortraitRect(x, y, saveIconX, saveIconY, BOOK_SAVE_ICON_SIZE, BOOK_SAVE_ICON_SIZE)) {
                if (story_items[i].id > 0 && !story_items[i].saved && WiFi.status() == WL_CONNECTED) {
                    if (fetchAndSaveStoryItem(story_items[i])) {
                        story_items[i].saved = true;
                        refreshVoiceStorySaveIconArea(i);
                    }
                }
                touch_loop_interval = millis() + 300;
                return;
            }

            if (touchHitsPortraitRect(x, y, BOOK_LIST_X, rowY, BOOK_LIST_W, BOOK_LIST_ROW_BOX_H)) {
                const int32_t tappedStoryId = story_items[i].id;
                const bool canUseCachedStory = (selected_story_id == tappedStoryId && selected_story_content.length() > 0);
                selected_story_id = tappedStoryId;
                snprintf(selected_story_title, sizeof(selected_story_title), "%s", story_items[i].title);
                snprintf(selected_story_author, sizeof(selected_story_author), "%s", story_items[i].author);
                snprintf(selected_story_category, sizeof(selected_story_category), "%s", story_items[i].category);
                story_reader_page = 0;
                if (canUseCachedStory) {
                    story_reader_total_pages = countBookReaderPagesByPixelWrap(selected_story_content.c_str());
                    snprintf(story_reader_status, sizeof(story_reader_status), "Loaded story");
                } else {
                    selected_story_content = "";
                    story_reader_total_pages = 1;
                    snprintf(story_reader_status, sizeof(story_reader_status), "Loading story...");
                }
                showingVoiceStoryLibrary = false;
                showingVoiceStoryReader = true;

                // The story list already stays visible in the same screen area.
                // On selection, update only the player/title header band, not
                // the whole page/list below it.
                story_playing = false;
                refreshVoiceStoryPlayerHeaderArea();

                if (!canUseCachedStory) {
                    if (!fetchSelectedVoiceStory(selected_story_id)) {
                        selected_story_content = "";
                    }
                }

                story_playing = selected_story_content.length() > 0 && prepareAndStartStoryAudio(selected_story_id);
                refreshVoiceStoryPlayerHeaderArea();
                if (story_playing) {
                    queueSelectedStoryAutoSave();
                }
                touch_loop_interval = millis() + 300;
                return;
            }
        }
    }

    if (showingVoiceStoryReader) {
        // While the voice-story player is open, the story list remains visible
        // below the player card. Tapping a different story should immediately
        // stop the current playback state, switch the header to the new title,
        // fetch/load that story, then auto-start playback for the new story.
        for (int i = 0; i < story_count; ++i) {
            const int32_t rowY = BOOK_LIST_Y + i * BOOK_LIST_ROW_H;
            if (!touchHitsPortraitRect(x, y, BOOK_LIST_X, rowY, BOOK_LIST_W, BOOK_LIST_ROW_BOX_H)) {
                continue;
            }

            const int32_t tappedStoryId = story_items[i].id;
            if (tappedStoryId <= 0 || tappedStoryId == selected_story_id) {
                break;
            }

            story_playing = false;
            selected_story_id = tappedStoryId;
            snprintf(selected_story_title, sizeof(selected_story_title), "%s", story_items[i].title);
            snprintf(selected_story_author, sizeof(selected_story_author), "%s", story_items[i].author);
            snprintf(selected_story_category, sizeof(selected_story_category), "%s", story_items[i].category);
            selected_story_content = "";
            story_reader_page = 0;
            story_reader_total_pages = 1;
            snprintf(story_reader_status, sizeof(story_reader_status), "Loading story...");

            refreshVoiceStoryPlayerHeaderArea();
            if (fetchSelectedVoiceStory(selected_story_id)) {
                story_playing = prepareAndStartStoryAudio(selected_story_id);
                refreshVoiceStoryPlayerHeaderArea();
                queueSelectedStoryAutoSave();
            } else {
                story_playing = false;
                refreshVoiceStoryPlayerHeaderArea();
            }
            touch_loop_interval = millis() + 300;
            return;
        }

        if (handleVoiceStoryReaderTouch(x, y)) {
            touch_loop_interval = millis() + 300;
            return;
        }
        if (handleBookSwipe(startX, startY, endX, endY)) {
            touch_loop_interval = millis() + 300;
            return;
        }
    }

    if (showingMusicLibrary) {
        if (handleBookSwipe(startX, startY, endX, endY)) {
            touch_loop_interval = millis() + 300;
            return;
        }

        for (int i = 0; i < music_count; ++i) {
            const int32_t rowY = BOOK_LIST_Y + i * BOOK_LIST_ROW_H;
            const int32_t saveIconX = BOOK_LIST_X + BOOK_LIST_W - BOOK_SAVE_ICON_SIZE - 8;
            const int32_t saveIconY = rowY + (BOOK_LIST_ROW_BOX_H - BOOK_SAVE_ICON_SIZE) / 2;
            if (touchHitsPortraitRect(x, y, saveIconX, saveIconY, BOOK_SAVE_ICON_SIZE, BOOK_SAVE_ICON_SIZE)) {
                if (music_items[i].id > 0 && !music_items[i].saved && WiFi.status() == WL_CONNECTED) {
                    snprintf(music_library_status, sizeof(music_library_status), "Saving music...");
                    if (fetchAndSaveMusicItem(music_items[i])) {
                        music_items[i].saved = true;
                        refreshMusicSaveIconArea(i);
                    } else {
                        refreshMusicLibraryListArea();
                    }
                }
                touch_loop_interval = millis() + 300;
                return;
            }
            if (touchHitsPortraitRect(x, y, BOOK_LIST_X, rowY, BOOK_LIST_W, BOOK_LIST_ROW_BOX_H)) {
                selected_music_id = music_items[i].id;
                snprintf(selected_music_title, sizeof(selected_music_title), "%s", music_items[i].title);
                snprintf(selected_music_filename, sizeof(selected_music_filename), "%s", music_items[i].filename);
                snprintf(selected_music_source, sizeof(selected_music_source), "%s", music_items[i].source);
                selected_music_url[0] = '\0';
                music_player_status[0] = '\0';
                music_playing = false;
                showingMusicLibrary = false;
                showingMusicPlayer = true;
                refreshMusicPlayerHeaderArea();
                fetchSelectedMusic(selected_music_id);
                music_playing = startAudioPlayback(selected_music_url);
                snprintf(music_player_status, sizeof(music_player_status), "%s", music_playing ? "正在播放" : "Load failed");
                refreshMusicPlayerHeaderArea();
                touch_loop_interval = millis() + 300;
                return;
            }
        }
    }

    if (showingMusicPlayer) {
        for (int i = 0; i < music_count; ++i) {
            const int32_t rowY = BOOK_LIST_Y + i * BOOK_LIST_ROW_H;
            if (!touchHitsPortraitRect(x, y, BOOK_LIST_X, rowY, BOOK_LIST_W, BOOK_LIST_ROW_BOX_H)) {
                continue;
            }
            if (music_items[i].id <= 0 || music_items[i].id == selected_music_id) {
                break;
            }
            music_playing = false;
            stopAudioPlayback();
            selected_music_id = music_items[i].id;
            snprintf(selected_music_title, sizeof(selected_music_title), "%s", music_items[i].title);
            snprintf(selected_music_filename, sizeof(selected_music_filename), "%s", music_items[i].filename);
            snprintf(selected_music_source, sizeof(selected_music_source), "%s", music_items[i].source);
            selected_music_url[0] = '\0';
            music_player_status[0] = '\0';
            refreshMusicPlayerHeaderArea();
            fetchSelectedMusic(selected_music_id);
            music_playing = startAudioPlayback(selected_music_url);
            snprintf(music_player_status, sizeof(music_player_status), "%s", music_playing ? "正在播放" : "Load failed");
            refreshMusicPlayerHeaderArea();
            touch_loop_interval = millis() + 300;
            return;
        }

        if (handleMusicPlayerTouch(x, y)) {
            touch_loop_interval = millis() + 300;
            return;
        }
        if (handleBookSwipe(startX, startY, endX, endY)) {
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

    if (!showingClock && !showingCalculator && !showingSettings && !showingSettingsMenu && !showingContentSettings && !showingBookLibrary && !showingBookReader && !showingVoiceStoryLibrary && !showingVoiceStoryReader && !showingMusicLibrary && !showingMusicPlayer && !showingSdMenu && !showingSdFolder) {
        if (touchHitsSettingsTile(x, y)) {
            pressedHomeIcon = 1;
            refreshDisplay(drawPortraitHome);
            delay(350); // delay so user can see visual feedback
            // Partial wipe only the bolded settings icon area before jumping
            wipeHomeIconArea(1);
            pressedHomeIcon = 0;
            showingSettingsMenu = true;
            refreshDisplay(drawSettingsMenuScreen);
            touch_loop_interval = millis() + 300;
            return;
        }

        if (touchHitsBookTile(x, y)) {
            pressedHomeIcon = 2;
            refreshDisplay(drawPortraitHome);
            delay(350); // delay so user can see visual feedback
            // Partial wipe only the bolded book icon area before jumping
            wipeHomeIconArea(2);
            pressedHomeIcon = 0;
            showingBookLibrary = true;
            book_count = 0;
            book_total = 0;
            book_current_page = 1;
            if (WiFi.status() != WL_CONNECTED) {
                memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
                drawTopStatusBar();
                drawPortraitTextCentered("wifi is not connected", 400, (GFXfont *)&FiraSans);
                epd_poweron();
                epd_clear();
                epd_draw_grayscale_image(epd_full_screen(), framebuffer);
                epd_poweroff();
                delay(2000);
            }
            fetchBookLibrary();
            refreshDisplay(drawBookLibraryScreen);
            touch_loop_interval = millis() + 300;
            return;
        }

        if (touchHitsVoiceStoryTile(x, y)) {
            pressedHomeIcon = 3;
            refreshDisplay(drawPortraitHome);
            delay(350); // delay so user can see visual feedback
            wipeHomeIconArea(3);
            pressedHomeIcon = 0;
            showingVoiceStoryLibrary = true;
            story_playing = false;
            story_count = 0;
            story_total = 0;
            story_current_page = 1;
            if (WiFi.status() != WL_CONNECTED) {
                memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
                drawTopStatusBar();
                drawPortraitTextCentered("wifi is not connected", 400, (GFXfont *)&FiraSans);
                epd_poweron();
                epd_clear();
                epd_draw_grayscale_image(epd_full_screen(), framebuffer);
                epd_poweroff();
                delay(2000);
            }
            fetchVoiceStoryLibrary();
            refreshDisplay(drawVoiceStoryLibraryScreen);
            touch_loop_interval = millis() + 300;
            return;
        }

        if (touchHitsMusicTile(x, y)) {
            pressedHomeIcon = 6;
            refreshDisplay(drawPortraitHome);
            delay(350); // delay so user can see visual feedback
            wipeHomeIconArea(6);
            pressedHomeIcon = 0;
            showingMusicLibrary = true;
            showingMusicPlayer = false;
            music_playing = false;
            music_count = 0;
            music_total = 0;
            music_current_page = 1;
            selected_music_id = 0;
            selected_music_title[0] = '\0';
            selected_music_url[0] = '\0';
            if (WiFi.status() != WL_CONNECTED) {
                memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
                drawTopStatusBar();
                drawPortraitTextCentered("wifi is not connected", 400, (GFXfont *)&FiraSans);
                epd_poweron();
                epd_clear();
                epd_draw_grayscale_image(epd_full_screen(), framebuffer);
                epd_poweroff();
                delay(2000);
            }
            refreshDisplayExtended(drawMusicLibraryLoadingScreen, true, 80);
            fetchMusicLibrary();
            refreshDisplayExtended(drawMusicLibraryScreen, true, 80);
            touch_loop_interval = millis() + 300;
            return;
        }

        if (touchHitsCalculatorTile(x, y)) {
            pressedHomeIcon = 4;
            refreshDisplay(drawPortraitHome);
            delay(350); // delay so user can see visual feedback
            // Partial wipe only the bolded calculator icon area before jumping
            wipeHomeIconArea(4);
            pressedHomeIcon = 0;
            showingCalculator = true;
            refreshDisplay(drawCalculatorScreen);
            touch_loop_interval = millis() + 300;
            return;
        }

        if (touchHitsCalculatorTile(x, y)) {
            pressedHomeIcon = 4;
            refreshDisplay(drawPortraitHome);
            delay(350); // delay so user can see visual feedback
            // Partial wipe only the bolded calculator icon area before jumping
            wipeHomeIconArea(4);
            pressedHomeIcon = 0;
            showingCalculator = true;
            refreshDisplay(drawCalculatorScreen);
            touch_loop_interval = millis() + 300;
            return;
        }

        if (touchHitsClockTile(x, y)) {
            pressedHomeIcon = 5;
            refreshDisplay(drawPortraitHome);
            delay(350); // delay so user can see visual feedback
            // Partial wipe only the bolded clock icon area before jumping
            wipeHomeIconArea(5);
            pressedHomeIcon = 0;
            showingClock = true;
            clock_weather.loaded = false;
            snprintf(clock_weather.city, sizeof(clock_weather.city), "Shenzhen");
            snprintf(clock_weather.status, sizeof(clock_weather.status), "Syncing clock/weather...");
            refreshDisplay(drawAnalogClockScreen);
            time_t now = time(NULL);
            struct tm timeinfo;
            if (now >= 100000 && localtime_r(&now, &timeinfo)) {
                clock_refresh_interval = millis() + millisUntilNextMinute(timeinfo);
            } else {
                clock_refresh_interval = millis() + 60000;
            }
            if (fetchClockWeatherInfo() && showingClock) {
                refreshClockInfoArea();
            } else if (showingClock) {
                refreshClockInfoArea();
            }
            touch_loop_interval = millis() + 300;
            return;
        }
    }

    if (!showingSettings && !showingSettingsMenu && !showingContentSettings && !showingSdMenu && !showingSdFolder && touchHitsWifiStatusIcon(x, y)) {
        showingClock = false;
        showingCalculator = false;
        showingSettings = true;
        showingContentSettings = false;
        showingBookLibrary = false;
        showingBookReader = false;
        showingVoiceStoryLibrary = false;
        showingVoiceStoryReader = false;
        showingMusicLibrary = false;
        showingMusicPlayer = false;
        music_playing = false;
        stopAudioPlayback();
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
    contentServerWarmupPending = true;
    contentServerWarmed = false;
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
    contentServerWarmed = false;
    contentServerWarmupPending = (WiFi.status() == WL_CONNECTED);
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
    if (!waitForWifiReady(5000)) {
        if (status && statusSize > 0) snprintf(status, statusSize, "WiFi not connected");
        return false;
    }

    Serial.printf("HTTP GET: %s\n", url);
    HTTPClient http;
    http.setTimeout(timeoutMs);
    http.setConnectTimeout(8000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    WiFiClient plainClient;
    WiFiClientSecure secureClient;
    plainClient.setTimeout((timeoutMs + 999) / 1000);
    secureClient.setTimeout((timeoutMs + 999) / 1000);
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
    http.addHeader("User-Agent", "T5-ePaper-S3/1.0");
    delay(30);
    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        String err = http.errorToString(httpCode);
        if (status && statusSize > 0) snprintf(status, statusSize, "HTTP %d %s", httpCode, err.c_str());
        Serial.printf("HTTP GET failed (%d): %s\n", httpCode, err.c_str());
        http.end();
        delay(100);
        return false;
    }

    payload = http.getString();
    http.end();
    delay(80);
    return true;
}

static bool httpDownloadToSdFile(const char *url, const char *path, char *status, size_t statusSize, uint32_t timeoutMs)
{
    if (!url || url[0] == '\0' || !path || path[0] == '\0') {
        if (status && statusSize > 0) snprintf(status, statusSize, "Bad download path");
        return false;
    }
    if (!waitForWifiReady(5000)) {
        if (status && statusSize > 0) snprintf(status, statusSize, "WiFi not connected");
        return false;
    }

    HTTPClient http;
    http.setTimeout(timeoutMs);
    http.setConnectTimeout(10000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    WiFiClient plainClient;
    WiFiClientSecure secureClient;
    plainClient.setTimeout((timeoutMs + 999) / 1000);
    secureClient.setTimeout((timeoutMs + 999) / 1000);
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
    http.addHeader("User-Agent", "T5-ePaper-S3/1.0");
    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
        if (status && statusSize > 0) snprintf(status, statusSize, "HTTP %d", httpCode);
        http.end();
        return false;
    }

    File out = SD.open(path, FILE_WRITE);
    if (!out) {
        if (status && statusSize > 0) snprintf(status, statusSize, "SD open failed");
        http.end();
        return false;
    }

    uint8_t buffer[1024];
    WiFiClient *stream = http.getStreamPtr();
    int remaining = http.getSize();
    uint32_t lastData = millis();
    while (http.connected() && (remaining > 0 || remaining == -1)) {
        size_t avail = stream->available();
        if (avail) {
            int readLen = stream->readBytes(buffer, min((size_t)sizeof(buffer), avail));
            if (readLen > 0) {
                out.write(buffer, readLen);
                lastData = millis();
                if (remaining > 0) remaining -= readLen;
            }
        } else {
            if (millis() - lastData > timeoutMs) break;
            delay(1);
        }
    }
    out.close();
    http.end();
    if (remaining > 0) {
        if (status && statusSize > 0) snprintf(status, statusSize, "Download incomplete");
        return false;
    }
    if (status && statusSize > 0) snprintf(status, statusSize, "Downloaded");
    return true;
}

static bool waitForWifiReady(uint32_t timeoutMs)
{
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
        delay(100);
    }
    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }

    // Give DNS/TCP/IP stack a short settle time after the GOT_IP event. HTTP -1
    // on ESP32 often occurs when the first client starts immediately after WiFi.
    if (WiFi.localIP() == IPAddress((uint32_t)0)) {
        delay(250);
    }
    return true;
}

static void warmupContentServer(bool force)
{
    if (!force && contentServerWarmed) {
        return;
    }
    if (!waitForWifiReady(3000)) {
        return;
    }
    if (!force && millis() - lastContentServerWarmup < 30000) {
        return;
    }

    char url[320];
    buildContentApiUrl(url, sizeof(url), "/api/geoip", NULL);
    if (url[0] == '\0') {
        return;
    }

    Serial.printf("Warming content server: %s\n", url);
    String payload;
    char status[96];
    bool ok = httpGetString(url, payload, status, sizeof(status), 8000);
    contentServerWarmed = ok;
    lastContentServerWarmup = millis();
    if (!ok) {
        Serial.printf("Content server warmup failed: %s\n", status);
    }
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
    warmupContentServer();
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
    if (loadBookFromSd(bookId, selected_book_title, selected_book_author, selected_book_category, &selected_book_content)) {
        selected_book_id = bookId;
        book_reader_page = 0;
        updateBookReaderPagination();
        snprintf(book_reader_status, sizeof(book_reader_status), "Loaded from SD");
        Serial.printf("Loaded selected book %ld from SD cache\n", (long)bookId);
        return true;
    }

    if (WiFi.status() != WL_CONNECTED) {
        snprintf(book_reader_status, sizeof(book_reader_status), "WiFi not connected");
        return false;
    }

    warmupContentServer();

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

static void queueSelectedBookAutoSave()
{
    if (WiFi.status() != WL_CONNECTED || selected_book_id <= 0 || selected_book_content.length() == 0) {
        return;
    }
    if (isBookSavedOnSd(selected_book_id)) {
        for (int i = 0; i < book_count; ++i) {
            if (book_items[i].id == selected_book_id) {
                book_items[i].saved = true;
                break;
            }
        }
        return;
    }

    pending_book_auto_save = true;
    pending_book_auto_save_id = selected_book_id;
    pending_book_auto_save_after = millis() + 50;
    Serial.printf("Queued book %ld for SD auto-save after display\n", (long)selected_book_id);
}

static void processPendingBookAutoSave()
{
    if (!pending_book_auto_save || millis() < pending_book_auto_save_after) {
        return;
    }

    const int32_t bookId = pending_book_auto_save_id;
    pending_book_auto_save = false;
    pending_book_auto_save_id = 0;

    if (bookId <= 0 || selected_book_id != bookId || selected_book_content.length() == 0) {
        return;
    }

    Serial.printf("Background SD auto-save for displayed book %ld\n", (long)bookId);
    if (saveBookToSd(bookId, selected_book_title, selected_book_author, selected_book_category, selected_book_content.c_str())) {
        for (int i = 0; i < book_count; ++i) {
            if (book_items[i].id == bookId) {
                book_items[i].saved = true;
                if (showingBookLibrary) {
                    refreshBookSaveIconArea(i);
                }
                break;
            }
        }
    }
}


static bool isBookSavedOnSd(int32_t bookId)
{
    if (!ensureSdReady()) {
        return false;
    }
    char path[64];
    snprintf(path, sizeof(path), "%s/%ld/meta.txt", BOOK_SD_FOLDER, (long)bookId);
    return SD.exists(path);
}

static bool saveBookToSd(int32_t bookId, const char *title, const char *author, const char *category, const char *content)
{
    if (!ensureSdReady()) {
        return false;
    }
    
    if (!SD.exists(BOOK_SD_FOLDER)) {
        SD.mkdir(BOOK_SD_FOLDER);
    }

    // Create books directory if it doesn't exist
    char dirPath[32];
    snprintf(dirPath, sizeof(dirPath), "%s/%ld", BOOK_SD_FOLDER, (long)bookId);
    if (!SD.exists(dirPath)) {
        SD.mkdir(dirPath);
    }
    
    // Save metadata
    char metaPath[64];
    snprintf(metaPath, sizeof(metaPath), "%s/meta.txt", dirPath);
    if (SD.exists(metaPath)) {
        SD.remove(metaPath);
    }
    File metaFile = SD.open(metaPath, FILE_WRITE);
    if (!metaFile) {
        Serial.println("Failed to open meta file for writing");
        return false;
    }
    metaFile.printf("%ld\n%s\n%s\n%s\n", (long)bookId, title, author, category);
    metaFile.close();
    
    // Save content
    char contentPath[64];
    snprintf(contentPath, sizeof(contentPath), "%s/content.txt", dirPath);
    if (SD.exists(contentPath)) {
        SD.remove(contentPath);
    }
    File contentFile = SD.open(contentPath, FILE_WRITE);
    if (!contentFile) {
        Serial.println("Failed to open content file for writing");
        return false;
    }
    contentFile.print(content);
    contentFile.close();
    
    Serial.printf("Book %ld saved to SD card\n", (long)bookId);
    return true;
}

static bool fetchAndSaveBookItem(BookListItem &book)
{
    if (WiFi.status() != WL_CONNECTED || book.id <= 0) {
        return false;
    }

    warmupContentServer();

    char url[320];
    buildBookDetailApiUrl(url, sizeof(url), book.id);
    if (url[0] == '\0') {
        return false;
    }

    String payload;
    char status[96];
    bool loaded = false;
    for (int attempt = 1; attempt <= 3 && !loaded; ++attempt) {
        Serial.printf("Saving book %ld in background (try %d/3): %s\n", (long)book.id, attempt, url);
        loaded = httpGetString(url, payload, status, sizeof(status), 20000);
        if (!loaded && attempt < 3) {
            WiFiClient().stop();
            delay(1200);
        }
    }
    if (!loaded) {
        Serial.printf("Failed to fetch book %ld for SD save: %s\n", (long)book.id, status);
        return false;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        Serial.printf("Save book JSON parse failed: %s\n", err.c_str());
        return false;
    }

    JsonObject item = doc.as<JsonObject>();
    char title[80];
    char author[40];
    char category[40];
    copyBookTitle(title, sizeof(title), item);
    copyJsonString(author, sizeof(author), item, "author", book.author);
    copyJsonString(category, sizeof(category), item, "category", book.category);
    const char *content = item["content"] | "";

    if (!saveBookToSd(book.id, title, author, category, content)) {
        return false;
    }

    snprintf(book.title, sizeof(book.title), "%s", title);
    snprintf(book.author, sizeof(book.author), "%s", author);
    snprintf(book.category, sizeof(book.category), "%s", category);
    return true;
}

static bool loadBookFromSd(int32_t bookId, char *title, char *author, char *category, String *content)
{
    if (!ensureSdReady()) {
        return false;
    }
    
    char dirPath[32];
    snprintf(dirPath, sizeof(dirPath), "%s/%ld", BOOK_SD_FOLDER, (long)bookId);
    
    // Load metadata
    char metaPath[64];
    snprintf(metaPath, sizeof(metaPath), "%s/meta.txt", dirPath);
    File metaFile = SD.open(metaPath, FILE_READ);
    if (!metaFile) {
        return false;
    }
    
    // Skip ID line
    metaFile.readStringUntil('\n');
    // Read title
    String t = metaFile.readStringUntil('\n');
    t.trim();
    snprintf(title, 80, "%s", t.c_str());
    // Read author
    String a = metaFile.readStringUntil('\n');
    a.trim();
    snprintf(author, 40, "%s", a.c_str());
    // Read category
    String c = metaFile.readStringUntil('\n');
    c.trim();
    snprintf(category, 40, "%s", c.c_str());
    metaFile.close();
    
    // Load content
    char contentPath[64];
    snprintf(contentPath, sizeof(contentPath), "%s/content.txt", dirPath);
    File contentFile = SD.open(contentPath, FILE_READ);
    if (!contentFile) {
        return false;
    }
    *content = contentFile.readString();
    contentFile.close();
    
    Serial.printf("Book %ld loaded from SD card\n", (long)bookId);
    return true;
}

static bool loadSavedBooksFromSd()
{
    if (!ensureSdReady()) {
        return false;
    }
    
    File booksDir = SD.open(BOOK_SD_FOLDER);
    if (!booksDir || !booksDir.isDirectory()) {
        return false;
    }
    
    book_count = 0;
    File entry = booksDir.openNextFile();
    while (entry && book_count < MAX_BOOK_ITEMS) {
        if (entry.isDirectory()) {
            const char *entryName = entry.name();
            const char *lastSlash = strrchr(entryName, '/');
            int32_t bookId = atoi(lastSlash ? lastSlash + 1 : entryName);
            if (bookId > 0) {
                char title[80], author[40], category[40];
                String content;
                if (loadBookFromSd(bookId, title, author, category, &content)) {
                    book_items[book_count].id = bookId;
                    snprintf(book_items[book_count].title, sizeof(book_items[book_count].title), "%s", title);
                    snprintf(book_items[book_count].author, sizeof(book_items[book_count].author), "%s", author);
                    snprintf(book_items[book_count].category, sizeof(book_items[book_count].category), "%s", category);
                    book_items[book_count].saved = true;
                    book_count++;
                }
            }
        }
        entry.close();
        entry = booksDir.openNextFile();
    }
    booksDir.close();
    
    if (book_count > 0) {
        snprintf(book_library_status, sizeof(book_library_status), "Loaded %d books from SD", book_count);
        return true;
    }
    return false;
}

static bool fetchBookLibrary()
{
    // Try loading from SD card first when WiFi is not available
    if (WiFi.status() != WL_CONNECTED) {
        if (loadSavedBooksFromSd()) {
            book_total = book_count;
            return true;
        }
        snprintf(book_library_status, sizeof(book_library_status), "WiFi not connected");
        return false;
    }

    warmupContentServer();

    BookListItem fetched_items[MAX_BOOK_ITEMS];
    int fetched_count = 0;
    int fetched_total = book_total;

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
        book.saved = isBookSavedOnSd(book.id);
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

// Voice Stories Core functions
static void buildVoiceStoriesApiUrl(char *out, size_t outSize)
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
    }

    snprintf(out, outSize, "%s/api/voice-stories?page=%ld&perPage=%d", normalized, (long)story_current_page, MAX_STORY_ITEMS);
}

static void buildVoiceStoryDetailApiUrl(char *out, size_t outSize, int32_t storyId)
{
    if (!out || outSize == 0) {
        return;
    }
    out[0] = '\0';

    const char *base = saved_content_url[0] != '\0' ? saved_content_url : content_url_input;
    if (!base || base[0] == '\0' || storyId <= 0) {
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

    snprintf(out, outSize, "%s/api/voice-stories/%ld", normalized, (long)storyId);
}

static void buildVoiceStoryTtsApiUrl(char *out, size_t outSize, int32_t storyId)
{
    if (!out || outSize == 0) return;
    out[0] = '\0';
    if (storyId <= 0) return;
    char endpoint[64];
    snprintf(endpoint, sizeof(endpoint), "/api/voice-stories/%ld/tts.mp3", (long)storyId);
    buildContentApiUrl(out, outSize, endpoint, NULL);
}

static bool isValidAudioFile(const char *path)
{
    if (!path || path[0] == '\0' || !ensureSdReady() || !SD.exists(path)) return false;
    File file = SD.open(path, FILE_READ);
    if (!file) return false;
    const size_t size = file.size();
    uint8_t header[12] = {0};
    const size_t n = file.read(header, sizeof(header));
    file.close();

    if (size < 1024 || n < 4) return false;

    // WAV: RIFF....WAVE
    if (n >= 12 && memcmp(header, "RIFF", 4) == 0 && memcmp(header + 8, "WAVE", 4) == 0) return true;
    // MP3: ID3 tag or MPEG frame sync 0xFFE/0xFFF
    if (memcmp(header, "ID3", 3) == 0) return true;
    if (header[0] == 0xFF && (header[1] & 0xE0) == 0xE0) return true;

    Serial.printf("Invalid audio cache file: %s, size=%u, header=%02X %02X %02X %02X\n",
                  path, (unsigned)size, header[0], header[1], header[2], header[3]);
    return false;
}

static bool loadStoryAudioFromSd(int32_t storyId, char *audioPath, size_t audioPathSize)
{
    if (!audioPath || audioPathSize == 0 || !ensureSdReady() || storyId <= 0) return false;
    char mp3Path[80];
    snprintf(mp3Path, sizeof(mp3Path), "%s/%ld/tts.mp3", STORY_SD_FOLDER, (long)storyId);
    if (SD.exists(mp3Path)) {
        if (isValidAudioFile(mp3Path)) {
            snprintf(audioPath, audioPathSize, "%s", mp3Path);
            return true;
        }
        SD.remove(mp3Path);
    }

    // Backward compatibility: older firmware cached local/server WAV files.
    char wavPath[80];
    snprintf(wavPath, sizeof(wavPath), "%s/%ld/tts.wav", STORY_SD_FOLDER, (long)storyId);
    if (SD.exists(wavPath)) {
        if (isValidAudioFile(wavPath)) {
            snprintf(audioPath, audioPathSize, "%s", wavPath);
            return true;
        }
        SD.remove(wavPath);
    }
    return false;
}

static void writeLe16(File &file, uint16_t value)
{
    uint8_t b[2] = {(uint8_t)(value & 0xFF), (uint8_t)((value >> 8) & 0xFF)};
    file.write(b, sizeof(b));
}

static void writeLe32(File &file, uint32_t value)
{
    uint8_t b[4] = {
        (uint8_t)(value & 0xFF),
        (uint8_t)((value >> 8) & 0xFF),
        (uint8_t)((value >> 16) & 0xFF),
        (uint8_t)((value >> 24) & 0xFF)};
    file.write(b, sizeof(b));
}

static bool writeWavHeader(File &file, uint32_t dataBytes, uint32_t sampleRate, uint16_t channels, uint16_t bitsPerSample)
{
    if (!file) return false;
    const uint32_t byteRate = sampleRate * channels * bitsPerSample / 8;
    const uint16_t blockAlign = channels * bitsPerSample / 8;

    file.write((const uint8_t *)"RIFF", 4);
    writeLe32(file, 36 + dataBytes);
    file.write((const uint8_t *)"WAVE", 4);
    file.write((const uint8_t *)"fmt ", 4);
    writeLe32(file, 16);
    writeLe16(file, 1); // PCM
    writeLe16(file, channels);
    writeLe32(file, sampleRate);
    writeLe32(file, byteRate);
    writeLe16(file, blockAlign);
    writeLe16(file, bitsPerSample);
    file.write((const uint8_t *)"data", 4);
    writeLe32(file, dataBytes);
    return true;
}

#if USE_LOCAL_ESP_TTS
static bool localTtsHeapLooksSafe()
{
    const size_t freeInternal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    const size_t largestInternal = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    Serial.printf("Local TTS heap check: free_internal=%u largest_internal=%u\n",
                  (unsigned)freeInternal,
                  (unsigned)largestInternal);

    if (freeInternal < LOCAL_TTS_MIN_INTERNAL_FREE_BYTES ||
        largestInternal < LOCAL_TTS_MIN_INTERNAL_LARGEST_BLOCK_BYTES) {
        snprintf(story_reader_status, sizeof(story_reader_status), "TTS heap low");
        Serial.println("Skipping local ESP-TTS: not enough contiguous internal heap");
        return false;
    }
    return true;
}

static bool appendTtsChunkToWav(esp_tts_handle_t ttsHandle, const char *chunk, File &wavFile, uint32_t *dataBytes)
{
    if (!ttsHandle || !chunk || chunk[0] == '\0' || !dataBytes) {
        return false;
    }

    if (!esp_tts_parse_chinese(ttsHandle, chunk)) {
        Serial.printf("esp_tts_parse_chinese failed for chunk: %.48s\n", chunk);
        esp_tts_stream_reset(ttsHandle);
        return false;
    }

    while (true) {
        int len = 0;
        short *pcm = esp_tts_stream_play(ttsHandle, &len, 4);
        if (!pcm || len <= 0) {
            break;
        }
        const size_t bytes = (size_t)len * sizeof(short);
        wavFile.write((const uint8_t *)pcm, bytes);
        *dataBytes += bytes;
        delay(1);
    }
    esp_tts_stream_reset(ttsHandle);
    return true;
}

static bool synthesizeStoryTtsToSd_internal(int32_t storyId, const char *text, char *audioPath, size_t audioPathSize)
{
    if (!audioPath || audioPathSize == 0 || storyId <= 0 || !text || text[0] == '\0' || !ensureSdReady()) {
        return false;
    }

    if (loadStoryAudioFromSd(storyId, audioPath, audioPathSize)) {
        return true;
    }

    if (!SD.exists(STORY_SD_FOLDER)) SD.mkdir(STORY_SD_FOLDER);
    char dirPath[32];
    snprintf(dirPath, sizeof(dirPath), "%s/%ld", STORY_SD_FOLDER, (long)storyId);
    if (!SD.exists(dirPath)) SD.mkdir(dirPath);

    char path[80];
    snprintf(path, sizeof(path), "%s/tts.wav", dirPath);
    if (SD.exists(path)) SD.remove(path);

    snprintf(story_reader_status, sizeof(story_reader_status), "本机语音生成...");
    Serial.printf("Generating local esp-tts WAV for story %ld -> %s\n", (long)storyId, path);

    // Espressif's esp-tts reference initializes the voice from a template plus
    // a voice-data pointer, usually mapped from a model partition:
    //   esp_tts_voice_set_init(&esp_tts_voice_template, voicedata)
    // Our bundled libvoice_set_xiaole.a embeds the xiaole voice-data pointer in
    // esp_tts_voice_xiaole.data, so pass that data explicitly instead of NULL.
    // Passing NULL can make esp_tts_voice_set_init() create an invalid voice set
    // and crash later when parse/play dereferences voice->data.
    void *xiaoleVoiceData = (void *)esp_tts_voice_xiaole.data;
    if (!xiaoleVoiceData) {
        snprintf(story_reader_status, sizeof(story_reader_status), "TTS data missing");
        Serial.println("Local ESP-TTS xiaole voice data pointer is NULL");
        return false;
    }

    esp_tts_voice_t *ttsVoice = esp_tts_voice_set_init(&esp_tts_voice_template, xiaoleVoiceData);
    if (!ttsVoice) {
        snprintf(story_reader_status, sizeof(story_reader_status), "TTS voice failed");
        return false;
    }

    esp_tts_handle_t ttsHandle = esp_tts_create(ttsVoice);
    if (!ttsHandle) {
        esp_tts_voice_set_free(ttsVoice);
        snprintf(story_reader_status, sizeof(story_reader_status), "TTS init failed");
        return false;
    }

    File wavFile = SD.open(path, FILE_WRITE);
    if (!wavFile) {
        esp_tts_destroy(ttsHandle);
        esp_tts_voice_set_free(ttsVoice);
        snprintf(story_reader_status, sizeof(story_reader_status), "SD open failed");
        return false;
    }

    const uint32_t sampleRate = esp_tts_voice_xiaole.sample_rate > 0 ? (uint32_t)esp_tts_voice_xiaole.sample_rate : 16000;
    writeWavHeader(wavFile, 0, sampleRate, 1, 16);

    uint32_t dataBytes = 0;
    const char *p = text;
    char chunk[520];
    size_t chunkLen = 0;
    bool generatedAny = false;

    while (*p) {
        const char *cpStart = p;
        uint32_t cp = 0;
        if (!utf8NextCodepoint(&p, &cp)) break;
        size_t cpBytes = (size_t)(p - cpStart);

        bool boundary = (cp == '\n' || cp == '\r' || cp == 0x3002 || cp == 0xFF01 || cp == 0xFF1F || cp == 0xFF1B || cp == 0xFF0C || cp == ',' || cp == '.' || cp == '!' || cp == '?' || cp == ';');
        if (cp == '\r') continue;

        if (chunkLen + cpBytes >= sizeof(chunk) - 1) {
            chunk[chunkLen] = '\0';
            generatedAny = appendTtsChunkToWav(ttsHandle, chunk, wavFile, &dataBytes) || generatedAny;
            chunkLen = 0;
        }

        if (cp != '\n') {
            memcpy(chunk + chunkLen, cpStart, cpBytes);
            chunkLen += cpBytes;
            chunk[chunkLen] = '\0';
        }

        if (boundary && chunkLen > 0) {
            chunk[chunkLen] = '\0';
            generatedAny = appendTtsChunkToWav(ttsHandle, chunk, wavFile, &dataBytes) || generatedAny;
            chunkLen = 0;
        }
    }

    if (chunkLen > 0) {
        chunk[chunkLen] = '\0';
        generatedAny = appendTtsChunkToWav(ttsHandle, chunk, wavFile, &dataBytes) || generatedAny;
    }

    wavFile.seek(0);
    writeWavHeader(wavFile, dataBytes, sampleRate, 1, 16);
    wavFile.close();
    esp_tts_destroy(ttsHandle);
    esp_tts_voice_set_free(ttsVoice);

    if (!generatedAny || dataBytes == 0) {
        SD.remove(path);
        snprintf(story_reader_status, sizeof(story_reader_status), "TTS failed");
        return false;
    }

    Serial.printf("Local esp-tts WAV generated: %s, %lu bytes PCM, %lu Hz\n", path, (unsigned long)dataBytes, (unsigned long)sampleRate);
    snprintf(audioPath, audioPathSize, "%s", path);
    return true;
}

struct TtsTaskParams {
    int32_t storyId;
    const char *text;
    char *audioPath;
    size_t audioPathSize;
    bool success;
    SemaphoreHandle_t sem;
};

static void synthesizeStoryTtsTask(void *pvParameters)
{
    TtsTaskParams *params = (TtsTaskParams *)pvParameters;
    params->success = synthesizeStoryTtsToSd_internal(params->storyId, params->text, params->audioPath, params->audioPathSize);
    xSemaphoreGive(params->sem);
    vTaskDelete(NULL);
    // FreeRTOS tasks must not return; vTaskDelete handles termination.
    for (;;) { vTaskDelay(portMAX_DELAY); }
}
#endif

static bool synthesizeStoryTtsToSd(int32_t storyId, const char *text, char *audioPath, size_t audioPathSize)
{
#if USE_LOCAL_ESP_TTS
    if (!localTtsHeapLooksSafe()) {
        return false;
    }

    // Run synthesis synchronously.  The earlier helper task consumed a large
    // 32 KB internal stack and added a pointer-lifetime boundary around text
    // and audioPath.  The official esp-tts sample runs parse/play directly;
    // appendTtsChunkToWav() yields with delay(1), so watchdog/idle tasks still
    // get time while long stories are synthesized.
    return synthesizeStoryTtsToSd_internal(storyId, text, audioPath, audioPathSize);
#else
    (void)storyId;
    (void)text;
    (void)audioPath;
    (void)audioPathSize;
    return false;
#endif
}

static bool downloadStoryAudioToSd(int32_t storyId, char *audioPath, size_t audioPathSize)
{
    if (!audioPath || audioPathSize == 0 || storyId <= 0 || !ensureSdReady()) return false;
    if (loadStoryAudioFromSd(storyId, audioPath, audioPathSize)) return true;
    if (WiFi.status() != WL_CONNECTED) {
        snprintf(story_reader_status, sizeof(story_reader_status), "WiFi not connected");
        return false;
    }

    if (!SD.exists(STORY_SD_FOLDER)) SD.mkdir(STORY_SD_FOLDER);
    char dirPath[32];
    snprintf(dirPath, sizeof(dirPath), "%s/%ld", STORY_SD_FOLDER, (long)storyId);
    if (!SD.exists(dirPath)) SD.mkdir(dirPath);

    char url[320];
    buildVoiceStoryTtsApiUrl(url, sizeof(url), storyId);
    if (url[0] == '\0') {
        snprintf(story_reader_status, sizeof(story_reader_status), "Set Content URL first");
        return false;
    }

    char path[80];
    snprintf(path, sizeof(path), "%s/tts.mp3", dirPath);
    if (SD.exists(path)) SD.remove(path);

    char status[96];
    snprintf(story_reader_status, sizeof(story_reader_status), "下载在线语音...");
    Serial.printf("Downloading story TTS MP3 %ld: %s -> %s\n", (long)storyId, url, path);
    if (!httpDownloadToSdFile(url, path, status, sizeof(status), 180000)) {
        Serial.printf("Story TTS download failed: %s\n", status);
        snprintf(story_reader_status, sizeof(story_reader_status), "%s", status);
        if (SD.exists(path)) SD.remove(path);
        return false;
    }
    if (!isValidAudioFile(path)) {
        snprintf(story_reader_status, sizeof(story_reader_status), "Invalid audio");
        if (SD.exists(path)) SD.remove(path);
        return false;
    }

    snprintf(audioPath, audioPathSize, "%s", path);
    return true;
}

static bool prepareAndStartStoryAudio(int32_t storyId)
{
    char audioPath[128];
    audioPath[0] = '\0';

    // Stop the old decoder before fetching/generating the next story audio.
    // Keeping ESP8266Audio decoder objects alive while HTTP/TTS/SD work runs
    // increases internal-heap pressure and was a likely reset trigger when
    // selecting or replaying voice stories.
    stopAudioPlayback();

    // Prefer already-cached audio, then server-generated MP3.  Local ESP-TTS
    // is retained only as an offline fallback because it needs a large task
    // stack and contiguous internal heap on the ESP32-S3.
    if (!loadStoryAudioFromSd(storyId, audioPath, sizeof(audioPath)) &&
        !downloadStoryAudioToSd(storyId, audioPath, sizeof(audioPath)) &&
        !synthesizeStoryTtsToSd(storyId, selected_story_content.c_str(), audioPath, sizeof(audioPath))) {
        return false;
    }
    bool ok = startAudioPlayback(audioPath);
    snprintf(story_reader_status, sizeof(story_reader_status), "%s", ok ? "正在播放" : "Play failed");
    return ok;
}

static bool isStorySavedOnSd(int32_t storyId)
{
    if (!ensureSdReady()) {
        return false;
    }
    char path[64];
    snprintf(path, sizeof(path), "%s/%ld/meta.txt", STORY_SD_FOLDER, (long)storyId);
    return SD.exists(path);
}

static bool saveStoryToSd(int32_t storyId, const char *title, const char *author, const char *category, const char *content)
{
    if (!ensureSdReady()) {
        return false;
    }
    
    if (!SD.exists(STORY_SD_FOLDER)) {
        SD.mkdir(STORY_SD_FOLDER);
    }

    char dirPath[32];
    snprintf(dirPath, sizeof(dirPath), "%s/%ld", STORY_SD_FOLDER, (long)storyId);
    if (!SD.exists(dirPath)) {
        SD.mkdir(dirPath);
    }
    
    // Save metadata
    char metaPath[64];
    snprintf(metaPath, sizeof(metaPath), "%s/meta.txt", dirPath);
    if (SD.exists(metaPath)) {
        SD.remove(metaPath);
    }
    File metaFile = SD.open(metaPath, FILE_WRITE);
    if (!metaFile) {
        Serial.println("Failed to open story meta file for writing");
        return false;
    }
    metaFile.printf("%ld\n%s\n%s\n%s\n", (long)storyId, title, author, category);
    metaFile.close();
    
    // Save content
    char contentPath[64];
    snprintf(contentPath, sizeof(contentPath), "%s/content.txt", dirPath);
    if (SD.exists(contentPath)) {
        SD.remove(contentPath);
    }
    File contentFile = SD.open(contentPath, FILE_WRITE);
    if (!contentFile) {
        Serial.println("Failed to open story content file for writing");
        return false;
    }
    contentFile.print(content);
    contentFile.close();
    
    Serial.printf("Story %ld saved to SD card\n", (long)storyId);
    return true;
}

static bool loadStoryFromSd(int32_t storyId, char *title, char *author, char *category, String *content)
{
    if (!ensureSdReady()) {
        return false;
    }
    
    char dirPath[32];
    snprintf(dirPath, sizeof(dirPath), "%s/%ld", STORY_SD_FOLDER, (long)storyId);
    
    // Load metadata
    char metaPath[64];
    snprintf(metaPath, sizeof(metaPath), "%s/meta.txt", dirPath);
    File metaFile = SD.open(metaPath, FILE_READ);
    if (!metaFile) {
        return false;
    }
    
    // Skip ID line
    metaFile.readStringUntil('\n');
    // Read title
    String t = metaFile.readStringUntil('\n');
    t.trim();
    snprintf(title, 80, "%s", t.c_str());
    // Read author
    String a = metaFile.readStringUntil('\n');
    a.trim();
    snprintf(author, 40, "%s", a.c_str());
    // Read category
    String c = metaFile.readStringUntil('\n');
    c.trim();
    snprintf(category, 40, "%s", c.c_str());
    metaFile.close();
    
    // Load content
    char contentPath[64];
    snprintf(contentPath, sizeof(contentPath), "%s/content.txt", dirPath);
    File contentFile = SD.open(contentPath, FILE_READ);
    if (!contentFile) {
        return false;
    }
    *content = contentFile.readString();
    contentFile.close();
    
    Serial.printf("Story %ld loaded from SD card\n", (long)storyId);
    return true;
}

static bool loadSavedStoriesFromSd()
{
    if (!ensureSdReady()) {
        return false;
    }
    
    File storiesDir = SD.open(STORY_SD_FOLDER);
    if (!storiesDir || !storiesDir.isDirectory()) {
        return false;
    }
    
    story_count = 0;
    File entry = storiesDir.openNextFile();
    while (entry && story_count < MAX_STORY_ITEMS) {
        if (entry.isDirectory()) {
            const char *entryName = entry.name();
            const char *lastSlash = strrchr(entryName, '/');
            int32_t storyId = atoi(lastSlash ? lastSlash + 1 : entryName);
            if (storyId > 0) {
                char title[80], author[40], category[40];
                String content;
                if (loadStoryFromSd(storyId, title, author, category, &content)) {
                    story_items[story_count].id = storyId;
                    snprintf(story_items[story_count].title, sizeof(story_items[story_count].title), "%s", title);
                    snprintf(story_items[story_count].author, sizeof(story_items[story_count].author), "%s", author);
                    snprintf(story_items[story_count].category, sizeof(story_items[story_count].category), "%s", category);
                    story_items[story_count].saved = true;
                    story_count++;
                }
            }
        }
        entry.close();
        entry = storiesDir.openNextFile();
    }
    storiesDir.close();
    
    if (story_count > 0) {
        snprintf(story_library_status, sizeof(story_library_status), "Loaded %d stories from SD", story_count);
        return true;
    }
    return false;
}

static bool fetchVoiceStoryLibrary()
{
    if (WiFi.status() != WL_CONNECTED) {
        if (loadSavedStoriesFromSd()) {
            story_total = story_count;
            return true;
        }
        snprintf(story_library_status, sizeof(story_library_status), "WiFi not connected");
        return false;
    }

    warmupContentServer();

    StoryListItem fetched_items[MAX_STORY_ITEMS];
    int fetched_count = 0;
    int fetched_total = story_total;

    char url[320];
    buildVoiceStoriesApiUrl(url, sizeof(url));
    if (url[0] == '\0') {
        snprintf(story_library_status, sizeof(story_library_status), "Set Content URL first");
        return false;
    }

    String payload;
    bool loaded = false;
    for (int attempt = 1; attempt <= 3 && !loaded; ++attempt) {
        snprintf(story_library_status, sizeof(story_library_status), "Stories try %d/3...", attempt);
        Serial.printf("Fetching voice story library (try %d/3): %s\n", attempt, url);
        loaded = httpGetString(url, payload, story_library_status, sizeof(story_library_status), 20000);
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
        snprintf(story_library_status, sizeof(story_library_status), "JSON parse failed");
        Serial.printf("Story JSON parse failed: %s\n", err.c_str());
        return false;
    }

    JsonArray items = doc["items"].as<JsonArray>();
    fetched_total = doc["total"] | 0;
    if (items.isNull()) {
        snprintf(story_library_status, sizeof(story_library_status), "No items in JSON");
        return false;
    }

    for (JsonObject item : items) {
        if (fetched_count >= MAX_STORY_ITEMS) {
            break;
        }
        StoryListItem &story = fetched_items[fetched_count];
        story.id = item["id"] | 0;
        copyBookTitle(story.title, sizeof(story.title), item);
        copyJsonString(story.author, sizeof(story.author), item, "author", "");
        copyJsonString(story.category, sizeof(story.category), item, "category", "");
        story.saved = isStorySavedOnSd(story.id);
        ++fetched_count;
    }

    if (fetched_count <= 0) {
        snprintf(story_library_status, sizeof(story_library_status), "No stories found");
        return false;
    }

    memcpy(story_items, fetched_items, sizeof(StoryListItem) * fetched_count);
    story_count = fetched_count;
    story_total = fetched_total;
    snprintf(story_library_status, sizeof(story_library_status), "Loaded %d stories", story_count);
    return true;
}

static bool fetchSelectedVoiceStory(int32_t storyId)
{
    if (loadStoryFromSd(storyId, selected_story_title, selected_story_author, selected_story_category, &selected_story_content)) {
        selected_story_id = storyId;
        story_reader_page = 0;
        story_reader_total_pages = countBookReaderPagesByPixelWrap(selected_story_content.c_str());
        snprintf(story_reader_status, sizeof(story_reader_status), "Loaded from SD");
        Serial.printf("Loaded selected story %ld from SD cache\n", (long)storyId);
        return true;
    }

    if (WiFi.status() != WL_CONNECTED) {
        snprintf(story_reader_status, sizeof(story_reader_status), "WiFi not connected");
        return false;
    }

    warmupContentServer();

    char url[320];
    buildVoiceStoryDetailApiUrl(url, sizeof(url), storyId);
    if (url[0] == '\0') {
        snprintf(story_reader_status, sizeof(story_reader_status), "Set Content URL first");
        return false;
    }

    String payload;
    bool loaded = false;
    for (int attempt = 1; attempt <= 3 && !loaded; ++attempt) {
        snprintf(story_reader_status, sizeof(story_reader_status), "Story try %d/3...", attempt);
        Serial.printf("Fetching selected voice story (try %d/3): %s\n", attempt, url);
        loaded = httpGetString(url, payload, story_reader_status, sizeof(story_reader_status), 20000);
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
        snprintf(story_reader_status, sizeof(story_reader_status), "Story JSON failed");
        Serial.printf("Story detail JSON parse failed: %s\n", err.c_str());
        return false;
    }

    JsonObject item = doc.as<JsonObject>();
    selected_story_id = item["id"] | storyId;
    copyBookTitle(selected_story_title, sizeof(selected_story_title), item);
    copyJsonString(selected_story_author, sizeof(selected_story_author), item, "author", "");
    copyJsonString(selected_story_category, sizeof(selected_story_category), item, "category", "");
    const char *content = item["content"] | "";
    selected_story_content = content;

    story_reader_page = 0;
    story_reader_total_pages = countBookReaderPagesByPixelWrap(selected_story_content.c_str());
    snprintf(story_reader_status, sizeof(story_reader_status), "Loaded story");
    return true;
}

static bool fetchAndSaveStoryItem(StoryListItem &story)
{
    if (WiFi.status() != WL_CONNECTED || story.id <= 0) {
        return false;
    }

    warmupContentServer();

    char url[320];
    buildVoiceStoryDetailApiUrl(url, sizeof(url), story.id);
    if (url[0] == '\0') {
        return false;
    }

    String payload;
    char status[96];
    bool loaded = false;
    for (int attempt = 1; attempt <= 3 && !loaded; ++attempt) {
        Serial.printf("Saving story %ld in background (try %d/3): %s\n", (long)story.id, attempt, url);
        loaded = httpGetString(url, payload, status, sizeof(status), 20000);
        if (!loaded && attempt < 3) {
            WiFiClient().stop();
            delay(1200);
        }
    }
    if (!loaded) {
        Serial.printf("Failed to fetch story %ld for SD save: %s\n", (long)story.id, status);
        return false;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        Serial.printf("Save story JSON parse failed: %s\n", err.c_str());
        return false;
    }

    JsonObject item = doc.as<JsonObject>();
    char title[80];
    char author[40];
    char category[40];
    copyBookTitle(title, sizeof(title), item);
    copyJsonString(author, sizeof(author), item, "author", story.author);
    copyJsonString(category, sizeof(category), item, "category", story.category);
    const char *content = item["content"] | "";

    if (!saveStoryToSd(story.id, title, author, category, content)) {
        return false;
    }

    snprintf(story.title, sizeof(story.title), "%s", title);
    snprintf(story.author, sizeof(story.author), "%s", author);
    snprintf(story.category, sizeof(story.category), "%s", category);
    return true;
}

static void queueSelectedStoryAutoSave()
{
    if (WiFi.status() != WL_CONNECTED || selected_story_id <= 0 || selected_story_content.length() == 0) {
        return;
    }
    if (isStorySavedOnSd(selected_story_id)) {
        for (int i = 0; i < story_count; ++i) {
            if (story_items[i].id == selected_story_id) {
                story_items[i].saved = true;
                break;
            }
        }
        return;
    }

    pending_story_auto_save = true;
    pending_story_auto_save_id = selected_story_id;
    pending_story_auto_save_after = millis() + 50;
    Serial.printf("Queued story %ld for SD auto-save after display\n", (long)selected_story_id);
}

static void processPendingStoryAutoSave()
{
    if (!pending_story_auto_save || millis() < pending_story_auto_save_after) {
        return;
    }

    const int32_t storyId = pending_story_auto_save_id;
    pending_story_auto_save = false;
    pending_story_auto_save_id = 0;

    if (storyId <= 0 || selected_story_id != storyId || selected_story_content.length() == 0) {
        return;
    }

    Serial.printf("Background SD auto-save for displayed story %ld\n", (long)storyId);
    if (saveStoryToSd(storyId, selected_story_title, selected_story_author, selected_story_category, selected_story_content.c_str())) {
        for (int i = 0; i < story_count; ++i) {
            if (story_items[i].id == storyId) {
                story_items[i].saved = true;
                if (showingVoiceStoryLibrary) {
                    refreshVoiceStorySaveIconArea(i);
                }
                break;
            }
        }
    }
}


// Kid Songs / Music Core functions
static void buildMusicApiUrl(char *out, size_t outSize)
{
    if (!out || outSize == 0) return;
    out[0] = '\0';
    char query[64];
    snprintf(query, sizeof(query), "?page=%ld&perPage=%d", (long)music_current_page, MAX_MUSIC_ITEMS);
    buildContentApiUrl(out, outSize, "/api/kid-songs", query);
}

static void buildMusicDetailApiUrl(char *out, size_t outSize, int32_t songId)
{
    if (!out || outSize == 0) return;
    out[0] = '\0';
    if (songId <= 0) return;
    char endpoint[48];
    snprintf(endpoint, sizeof(endpoint), "/api/kid-songs/%ld", (long)songId);
    buildContentApiUrl(out, outSize, endpoint, NULL);
}

static bool fetchMusicLibrary()
{
    if (WiFi.status() != WL_CONNECTED) {
        if (loadSavedMusicFromSd()) {
            music_total = music_count;
            return true;
        }
        snprintf(music_library_status, sizeof(music_library_status), "WiFi not connected");
        return false;
    }
    warmupContentServer();
    int fetched_count = 0;
    char url[320];
    buildMusicApiUrl(url, sizeof(url));
    if (url[0] == '\0') {
        snprintf(music_library_status, sizeof(music_library_status), "Set Content URL first");
        return false;
    }
    String payload;
    bool loaded = false;
    for (int attempt = 1; attempt <= 3 && !loaded; ++attempt) {
        snprintf(music_library_status, sizeof(music_library_status), "Songs try %d/3...", attempt);
        Serial.printf("Fetching music library (try %d/3): %s\n", attempt, url);
        loaded = httpGetString(url, payload, music_library_status, sizeof(music_library_status), 8000);
        if (!loaded && attempt < 3) { WiFiClient().stop(); delay(1200); }
    }
    if (!loaded) return false;
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        snprintf(music_library_status, sizeof(music_library_status), "JSON parse failed");
        Serial.printf("Music JSON parse failed: %s\n", err.c_str());
        return false;
    }
    JsonArray items = doc["items"].as<JsonArray>();
    music_total = doc["total"] | 0;
    if (items.isNull()) {
        snprintf(music_library_status, sizeof(music_library_status), "No items in JSON");
        return false;
    }

    // Fill the global list directly instead of using a large temporary array
    // on the loop/touch stack.  The music item has long URL fields, and the
    // extra stack pressure could reset the ESP32-S3 as soon as the music icon
    // started fetching the list.
    music_count = 0;
    for (JsonObject item : items) {
        if (fetched_count >= MAX_MUSIC_ITEMS) break;
        MusicListItem &song = music_items[fetched_count];
        song.id = item["id"] | 0;
        copyJsonString(song.title, sizeof(song.title), item, "title", "Untitled");
        copyJsonString(song.filename, sizeof(song.filename), item, "filename", "");
        // Keep the library fetch lightweight: do not retain the MP3 URL here.
        // The detail endpoint is fetched only when a row is played or saved.
        song.audio_url[0] = '\0';
        copyJsonString(song.source, sizeof(song.source), item, "source", "");
        song.saved = isMusicSavedOnSd(song.id);
        ++fetched_count;
    }
    if (fetched_count <= 0) {
        snprintf(music_library_status, sizeof(music_library_status), "No songs found");
        return false;
    }
    music_count = fetched_count;
    snprintf(music_library_status, sizeof(music_library_status), "Loaded %d songs", music_count);
    return true;
}

static bool buildMusicAudioDownloadUrl(const char *audioUrl, char *out, size_t outSize)
{
    if (!out || outSize == 0) return false;
    out[0] = '\0';
    if (!audioUrl || audioUrl[0] == '\0') return false;
    if (strncmp(audioUrl, "http://", 7) == 0 || strncmp(audioUrl, "https://", 8) == 0) {
        snprintf(out, outSize, "%s", audioUrl);
        return true;
    }
    if (audioUrl[0] == '/') {
        buildContentApiUrl(out, outSize, audioUrl, NULL);
        return out[0] != '\0';
    }
    char endpoint[220];
    snprintf(endpoint, sizeof(endpoint), "/%s", audioUrl);
    buildContentApiUrl(out, outSize, endpoint, NULL);
    return out[0] != '\0';
}

static bool isMusicSavedOnSd(int32_t songId)
{
    if (!ensureSdReady()) {
        return false;
    }
    char path[64];
    snprintf(path, sizeof(path), "%s/%ld/meta.txt", MUSIC_SD_FOLDER, (long)songId);
    return SD.exists(path);
}

static bool loadMusicFromSd(int32_t songId, char *title, char *filename, char *audioPath, char *source)
{
    if (!ensureSdReady() || songId <= 0) {
        return false;
    }

    char dirPath[32];
    snprintf(dirPath, sizeof(dirPath), "%s/%ld", MUSIC_SD_FOLDER, (long)songId);

    char metaPath[64];
    snprintf(metaPath, sizeof(metaPath), "%s/meta.txt", dirPath);
    File metaFile = SD.open(metaPath, FILE_READ);
    if (!metaFile) {
        return false;
    }

    metaFile.readStringUntil('\n'); // id
    String t = metaFile.readStringUntil('\n');
    String f = metaFile.readStringUntil('\n');
    String s = metaFile.readStringUntil('\n');
    t.trim();
    f.trim();
    s.trim();
    metaFile.close();

    if (f.length() == 0) {
        return false;
    }

    char localPath[128];
    snprintf(localPath, sizeof(localPath), "%s/%s", dirPath, f.c_str());
    if (!SD.exists(localPath)) {
        return false;
    }

    if (title) snprintf(title, 80, "%s", t.c_str());
    if (filename) snprintf(filename, 80, "%s", f.c_str());
    if (source) snprintf(source, 40, "%s", s.c_str());
    if (audioPath) snprintf(audioPath, 180, "%s", localPath);
    Serial.printf("Loaded selected music %ld from SD cache: %s\n", (long)songId, localPath);
    return true;
}

static bool loadSavedMusicFromSd()
{
    if (!ensureSdReady()) {
        return false;
    }

    File musicDir = SD.open(MUSIC_SD_FOLDER);
    if (!musicDir || !musicDir.isDirectory()) {
        return false;
    }

    music_count = 0;
    File entry = musicDir.openNextFile();
    while (entry && music_count < MAX_MUSIC_ITEMS) {
        if (entry.isDirectory()) {
            const char *entryName = entry.name();
            const char *lastSlash = strrchr(entryName, '/');
            int32_t songId = atoi(lastSlash ? lastSlash + 1 : entryName);
            if (songId > 0) {
                char title[80] = "";
                char filename[80] = "";
                char audioPath[180] = "";
                char source[40] = "";
                if (loadMusicFromSd(songId, title, filename, audioPath, source)) {
                    MusicListItem &song = music_items[music_count];
                    song.id = songId;
                    snprintf(song.title, sizeof(song.title), "%s", title[0] ? title : filename);
                    snprintf(song.filename, sizeof(song.filename), "%s", filename);
                    snprintf(song.audio_url, sizeof(song.audio_url), "%s", audioPath);
                    snprintf(song.source, sizeof(song.source), "%s", source);
                    song.saved = true;
                    music_count++;
                }
            }
        }
        entry.close();
        entry = musicDir.openNextFile();
    }
    musicDir.close();

    if (music_count > 0) {
        snprintf(music_library_status, sizeof(music_library_status), "Loaded %d songs from SD", music_count);
        return true;
    }
    return false;
}

static void initAudioOutput()
{
    if (audioOut) {
        return;
    }

    audioOut = new AudioOutputI2S();
    if (!audioOut) {
        Serial.println("AudioOutputI2S allocation failed");
        return;
    }

    audioOut->SetPinout(AUDIO_I2S_BCLK_PIN, AUDIO_I2S_LRCK_PIN, AUDIO_I2S_DOUT_PIN);
    audioOut->SetGain(0.65f);
    Serial.printf("I2S audio initialized: BCLK=%d LRCK=%d DOUT=%d MIC_DIN=%d\n",
                  AUDIO_I2S_BCLK_PIN,
                  AUDIO_I2S_LRCK_PIN,
                  AUDIO_I2S_DOUT_PIN,
                  AUDIO_I2S_MIC_DIN_PIN);
}

static void stopAudioPlayback()
{
    if (audioWav) {
        if (audioWav->isRunning()) {
            audioWav->stop();
        }
        delete audioWav;
        audioWav = nullptr;
    }
    if (audioMp3) {
        if (audioMp3->isRunning()) {
            audioMp3->stop();
        }
        delete audioMp3;
        audioMp3 = nullptr;
    }
    if (audioFile) {
        delete audioFile;
        audioFile = nullptr;
    }
    current_audio_path[0] = '\0';
}

static bool startAudioPlayback(const char *path)
{
    if (!path || path[0] == '\0') {
        return false;
    }
    if (!ensureSdReady() || !SD.exists(path)) {
        Serial.printf("Audio file not found: %s\n", path);
        return false;
    }

    initAudioOutput();
    if (!audioOut) {
        return false;
    }

    stopAudioPlayback();
    audioFile = new AudioFileSourceSD(path);
    const char *dot = strrchr(path, '.');
    const bool isWav = dot && strcasecmp(dot, ".wav") == 0;
    if (isWav) {
        audioWav = new AudioGeneratorWAV();
    } else {
        audioMp3 = new AudioGeneratorMP3();
    }
    if (!audioFile || (!audioMp3 && !audioWav)) {
        Serial.println("Audio decoder allocation failed");
        stopAudioPlayback();
        return false;
    }

    bool began = isWav ? audioWav->begin(audioFile, audioOut) : audioMp3->begin(audioFile, audioOut);
    if (!began) {
        Serial.printf("Audio begin failed: %s\n", path);
        stopAudioPlayback();
        return false;
    }

    snprintf(current_audio_path, sizeof(current_audio_path), "%s", path);
    Serial.printf("Audio playback started: %s\n", current_audio_path);
    return true;
}

static void serviceAudioPlayback()
{
    bool running = (audioMp3 && audioMp3->isRunning()) || (audioWav && audioWav->isRunning());
    if (!running) {
        return;
    }

    bool ok = audioMp3 ? audioMp3->loop() : audioWav->loop();
    if (!ok) {
        Serial.println("Audio playback finished");
        stopAudioPlayback();
        if (story_playing) {
            story_playing = false;
            snprintf(story_reader_status, sizeof(story_reader_status), "播放结束");
            if (showingVoiceStoryReader) {
                refreshVoiceStoryPlayerHeaderArea();
            }
        }
        if (music_playing) {
            music_playing = false;
            snprintf(music_player_status, sizeof(music_player_status), "播放结束");
            if (showingMusicPlayer) {
                refreshMusicPlayerHeaderArea();
            }
        }
    }
}

static bool saveMusicToSd(int32_t songId, const char *title, const char *filename, const char *audioUrl, const char *source)
{
    if (songId <= 0 || !ensureSdReady()) return false;
    if (!audioUrl || audioUrl[0] == '\0') return false;

    if (!SD.exists(MUSIC_SD_FOLDER)) SD.mkdir(MUSIC_SD_FOLDER);
    char dirPath[32];
    snprintf(dirPath, sizeof(dirPath), "%s/%ld", MUSIC_SD_FOLDER, (long)songId);
    if (!SD.exists(dirPath)) SD.mkdir(dirPath);

    char downloadUrl[320];
    if (!buildMusicAudioDownloadUrl(audioUrl, downloadUrl, sizeof(downloadUrl))) return false;

    char safeName[96];
    snprintf(safeName, sizeof(safeName), "%s", (filename && filename[0]) ? filename : "song.mp3");
    for (size_t i = 0; safeName[i]; ++i) {
        if (safeName[i] == '/' || safeName[i] == '\\' || safeName[i] == ':' || safeName[i] == '*' ||
            safeName[i] == '?' || safeName[i] == '"' || safeName[i] == '<' || safeName[i] == '>' || safeName[i] == '|') {
            safeName[i] = '_';
        }
    }
    if (!strstr(safeName, ".mp3") && !strstr(safeName, ".MP3")) {
        strncat(safeName, ".mp3", sizeof(safeName) - strlen(safeName) - 1);
    }

    char audioPath[128];
    snprintf(audioPath, sizeof(audioPath), "%s/%s", dirPath, safeName);
    if (SD.exists(audioPath)) SD.remove(audioPath);

    char status[96];
    snprintf(music_library_status, sizeof(music_library_status), "Saving music...");
    Serial.printf("Downloading music %ld to SD: %s -> %s\n", (long)songId, downloadUrl, audioPath);
    if (!httpDownloadToSdFile(downloadUrl, audioPath, status, sizeof(status), 90000)) {
        Serial.printf("Music download failed: %s\n", status);
        snprintf(music_library_status, sizeof(music_library_status), "%s", status);
        if (SD.exists(audioPath)) SD.remove(audioPath);
        return false;
    }

    char metaPath[64];
    snprintf(metaPath, sizeof(metaPath), "%s/meta.txt", dirPath);
    if (SD.exists(metaPath)) SD.remove(metaPath);
    File metaFile = SD.open(metaPath, FILE_WRITE);
    if (!metaFile) return false;
    metaFile.printf("%ld\n%s\n%s\n%s\n%s\n", (long)songId, title ? title : "", safeName, source ? source : "", audioUrl);
    metaFile.close();
    snprintf(music_library_status, sizeof(music_library_status), "Saved music");
    Serial.printf("Music %ld saved to SD card\n", (long)songId);
    return true;
}

static bool fetchAndSaveMusicItem(MusicListItem &song)
{
    if (song.id <= 0) return false;
    if (song.saved || loadMusicFromSd(song.id, song.title, song.filename, song.audio_url, song.source)) {
        song.saved = true;
        return true;
    }
    if (WiFi.status() != WL_CONNECTED) return false;

    int32_t oldSelectedId = selected_music_id;
    char oldTitle[sizeof(selected_music_title)];
    char oldFilename[sizeof(selected_music_filename)];
    char oldUrl[sizeof(selected_music_url)];
    char oldSource[sizeof(selected_music_source)];
    snprintf(oldTitle, sizeof(oldTitle), "%s", selected_music_title);
    snprintf(oldFilename, sizeof(oldFilename), "%s", selected_music_filename);
    snprintf(oldUrl, sizeof(oldUrl), "%s", selected_music_url);
    snprintf(oldSource, sizeof(oldSource), "%s", selected_music_source);

    bool ok = false;
    selected_music_id = song.id;
    snprintf(selected_music_title, sizeof(selected_music_title), "%s", song.title);
    snprintf(selected_music_filename, sizeof(selected_music_filename), "%s", song.filename);
    snprintf(selected_music_source, sizeof(selected_music_source), "%s", song.source);
    selected_music_url[0] = '\0';
    if (fetchSelectedMusic(song.id)) {
        // fetchSelectedMusic() already performs the online detail fetch,
        // downloads the MP3 to /music/<id>/, and reloads selected_music_url as
        // the local SD path. Do not call saveMusicToSd() a second time with
        // that local path.
        ok = true;
        snprintf(song.title, sizeof(song.title), "%s", selected_music_title);
        snprintf(song.filename, sizeof(song.filename), "%s", selected_music_filename);
        snprintf(song.audio_url, sizeof(song.audio_url), "%s", selected_music_url);
        snprintf(song.source, sizeof(song.source), "%s", selected_music_source);
        song.saved = true;
    }

    selected_music_id = oldSelectedId;
    snprintf(selected_music_title, sizeof(selected_music_title), "%s", oldTitle);
    snprintf(selected_music_filename, sizeof(selected_music_filename), "%s", oldFilename);
    snprintf(selected_music_url, sizeof(selected_music_url), "%s", oldUrl);
    snprintf(selected_music_source, sizeof(selected_music_source), "%s", oldSource);
    return ok;
}

static bool fetchSelectedMusic(int32_t songId)
{
    if (loadMusicFromSd(songId, selected_music_title, selected_music_filename, selected_music_url, selected_music_source)) {
        selected_music_id = songId;
        snprintf(music_player_status, sizeof(music_player_status), "Loaded from SD");
        return true;
    }

    if (WiFi.status() != WL_CONNECTED) {
        snprintf(music_player_status, sizeof(music_player_status), "WiFi not connected");
        return false;
    }
    warmupContentServer();
    char url[320];
    buildMusicDetailApiUrl(url, sizeof(url), songId);
    if (url[0] == '\0') {
        snprintf(music_player_status, sizeof(music_player_status), "Set Content URL first");
        return false;
    }
    String payload;
    bool loaded = false;
    for (int attempt = 1; attempt <= 3 && !loaded; ++attempt) {
        snprintf(music_player_status, sizeof(music_player_status), "Song try %d/3...", attempt);
        Serial.printf("Fetching selected song (try %d/3): %s\n", attempt, url);
        loaded = httpGetString(url, payload, music_player_status, sizeof(music_player_status), 20000);
        if (!loaded && attempt < 3) { WiFiClient().stop(); delay(1200); }
    }
    if (!loaded) return false;
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        snprintf(music_player_status, sizeof(music_player_status), "Song JSON failed");
        Serial.printf("Song detail JSON parse failed: %s\n", err.c_str());
        return false;
    }
    JsonObject item = doc.as<JsonObject>();
    selected_music_id = item["id"] | songId;
    copyJsonString(selected_music_title, sizeof(selected_music_title), item, "title", "Untitled");
    copyJsonString(selected_music_filename, sizeof(selected_music_filename), item, "filename", "");
    copyJsonString(selected_music_url, sizeof(selected_music_url), item, "audio_url", "");
    copyJsonString(selected_music_source, sizeof(selected_music_source), item, "source", "");
    if (selected_music_url[0] == '\0') {
        snprintf(music_player_status, sizeof(music_player_status), "No audio URL");
        return false;
    }

    // Match books/voice stories: once a song is selected online, immediately
    // persist the MP3 to SD and play from the local cached path. This lets the
    // same song work later when WiFi is unavailable.
    if (!saveMusicToSd(selected_music_id, selected_music_title, selected_music_filename, selected_music_url, selected_music_source)) {
        snprintf(music_player_status, sizeof(music_player_status), "Save failed");
        return false;
    }
    if (!loadMusicFromSd(selected_music_id, selected_music_title, selected_music_filename, selected_music_url, selected_music_source)) {
        snprintf(music_player_status, sizeof(music_player_status), "SD load failed");
        return false;
    }
    for (int i = 0; i < music_count; ++i) {
        if (music_items[i].id == selected_music_id) {
            music_items[i].saved = true;
            snprintf(music_items[i].title, sizeof(music_items[i].title), "%s", selected_music_title);
            snprintf(music_items[i].filename, sizeof(music_items[i].filename), "%s", selected_music_filename);
            snprintf(music_items[i].audio_url, sizeof(music_items[i].audio_url), "%s", selected_music_url);
            snprintf(music_items[i].source, sizeof(music_items[i].source), "%s", selected_music_source);
            break;
        }
    }
    snprintf(music_player_status, sizeof(music_player_status), "Loaded from SD");
    return selected_music_url[0] != '\0';
}

static bool handleMusicPlayerTouch(int16_t tx, int16_t ty)
{
    int32_t px = 0, py = 0;
    if (!portraitPointFromTouch(tx, ty, &px, &py, true)) {
        if (!portraitPointFromTouch(tx, ty, &px, &py, false)) return false;
    }
    if (!pointInRect(px, py, STORY_PLAYER_BUTTON_X, STORY_PLAYER_BUTTON_Y, STORY_PLAYER_BUTTON_W, STORY_PLAYER_BUTTON_H)) return false;
    if (selected_music_url[0] == '\0') {
        music_player_status[0] = '\0';
        refreshMusicPlayerHeaderArea();
        if (selected_music_id > 0 && fetchSelectedMusic(selected_music_id)) {
            music_playing = startAudioPlayback(selected_music_url);
            snprintf(music_player_status, sizeof(music_player_status), "%s", music_playing ? "正在播放" : "Play failed");
        }
        refreshMusicPlayerHeaderArea();
        return true;
    }
    if (music_playing) {
        stopAudioPlayback();
        music_playing = false;
    } else {
        music_playing = startAudioPlayback(selected_music_url);
    }
    snprintf(music_player_status, sizeof(music_player_status), "%s", music_playing ? "正在播放" : "已暂停");
    refreshMusicPlayerHeaderArea();
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
    serviceAudioPlayback();

    processPendingBookAutoSave();
    processPendingStoryAutoSave();

    if (contentServerWarmupPending && WiFi.status() == WL_CONNECTED) {
        contentServerWarmupPending = false;
        delay(750);
        warmupContentServer(true);
    }

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

    if (!wifiConnected && wifiEnabled && saved_wifi_ssid[0] != '\0' && millis() > wifi_reconnect_interval) {
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
            time_t now = time(NULL);
            struct tm timeinfo;
            if (now >= 100000 && localtime_r(&now, &timeinfo)) {
                clock_refresh_interval = millis() + millisUntilNextMinute(timeinfo);
            } else {
                clock_refresh_interval = millis() + 60000;
            }
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
        } else if (showingVoiceStoryLibrary) {
            refreshDisplayExtended(drawVoiceStoryLibraryScreen, true);
        } else if (showingVoiceStoryReader) {
            refreshDisplayExtended(drawVoiceStoryReaderScreen, true);
        } else if (showingMusicLibrary) {
            refreshDisplayExtended(drawMusicLibraryScreen, true);
        } else if (showingMusicPlayer) {
            refreshDisplayExtended(drawMusicPlayerScreen, true);
        } else if (showingSdMenu) {
            refreshDisplayExtended(drawSdMenuScreen, true);
        } else if (showingSdFolder) {
            refreshDisplayExtended(drawSdFolderScreen, true);
        } else {
            refreshDisplayExtended(drawPortraitHome, true);
        }
    }

    if (showingClock && !showingCalculator && millis() > clock_refresh_interval) {
        // Tight partial refresh: only update the union rectangle around the
        // previous and current hour/minute hands.
        refreshClockHandsArea();
        time_t now = time(NULL);
        struct tm timeinfo;
        if (now >= 100000 && localtime_r(&now, &timeinfo)) {
            clock_refresh_interval = millis() + millisUntilNextMinute(timeinfo);
        } else {
            clock_refresh_interval = millis() + 60000;
        }
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
            releasedTouchX = x;
            releasedTouchY = y;
            if (!touchLatchActive) {
                touchLatchActive = true;
                latchedTouchX = x;
                latchedTouchY = y;
                releasedTouchX = x;
                releasedTouchY = y;
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
                    processTouchRelease(latchedTouchX, latchedTouchY, releasedTouchX, releasedTouchY);
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
