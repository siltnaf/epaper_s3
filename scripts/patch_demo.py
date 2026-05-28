#!/usr/bin/env python3
"""Patch demo.ino to add book save/load functionality."""

import re

def main():
    path = 'examples/demo/demo.ino'
    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()

    # 1. Add saved flag to BookListItem struct
    content = content.replace(
        '    char category[40];\n};',
        '    char category[40];\n    bool saved;\n};'
    )

    # 2. Add book SD card constants after BOOK_LIST_REFRESH_BOTTOM_MARGIN
    content = content.replace(
        'static const int32_t BOOK_LIST_REFRESH_BOTTOM_MARGIN = 8;',
        '''static const int32_t BOOK_LIST_REFRESH_BOTTOM_MARGIN = 8;
static const int32_t BOOK_SAVE_ICON_SIZE = 24;
static const int32_t BOOK_SAVE_ICON_MARGIN = 4;
static const char BOOK_SD_FOLDER[] = "/books";'''
    )

    # 3. Add book SD card function declarations after buildBooksApiUrl
    content = content.replace(
        'static void buildBooksApiUrl(char *out, size_t outSize);',
        '''static void buildBooksApiUrl(char *out, size_t outSize);
static bool saveBookToSd(int32_t bookId, const char *title, const char *author, const char *category, const char *content);
static bool loadBookFromSd(int32_t bookId, char *title, char *author, char *category, String *content);
static bool loadSavedBooksFromSd();
static bool isBookSavedOnSd(int32_t bookId);
static void drawBookSaveIcon(int32_t x, int32_t y, bool saved);'''
    )

    # 4. Modify drawBookLibraryRowsArea to draw save icon
    old_rows_area = '''        // Show category on the right side of the row (right-aligned, vertically centered).
        if (book_items[i].category[0] != '\\0') {
            drawUtf8ChineseTextRightAligned(book_items[i].category, BOOK_LIST_X + BOOK_LIST_W - 16, y, BOOK_LIST_ROW_BOX_H);
        }
    }
}

static void drawBookLibraryScreen()'''
    
    new_rows_area = '''        // Show category on the right side of the row (right-aligned, vertically centered).
        if (book_items[i].category[0] != '\\0') {
            drawUtf8ChineseTextRightAligned(book_items[i].category, BOOK_LIST_X + BOOK_LIST_W - BOOK_SAVE_ICON_SIZE - BOOK_SAVE_ICON_MARGIN * 2 - 16, y, BOOK_LIST_ROW_BOX_H);
        }

        // Draw save icon at end of row
        int32_t saveIconX = BOOK_LIST_X + BOOK_LIST_W - BOOK_SAVE_ICON_SIZE - BOOK_SAVE_ICON_MARGIN;
        int32_t saveIconY = y + (BOOK_LIST_ROW_BOX_H - BOOK_SAVE_ICON_SIZE) / 2;
        drawBookSaveIcon(saveIconX, saveIconY, book_items[i].saved);
    }
}

static void drawBookSaveIcon(int32_t x, int32_t y, bool saved)
{
    int32_t stride = (BOOK_SAVE_ICON_W + 7) / 8;
    for (int32_t yy = 0; yy < BOOK_SAVE_ICON_H; ++yy) {
        for (int32_t xx = 0; xx < BOOK_SAVE_ICON_W; ++xx) {
            uint8_t packed = book_save_icon_24x24[yy * stride + xx / 8];
            if (packed & (0x80 >> (xx & 7))) {
                portraitPixel(x + xx, y + yy, saved ? 0x00 : 0x00);
            }
        }
    }
    // If saved, draw a filled checkmark overlay
    if (saved) {
        // Draw a small filled rectangle in the center to indicate saved
        int32_t cx = x + BOOK_SAVE_ICON_W / 2;
        int32_t cy = y + BOOK_SAVE_ICON_H / 2;
        portraitFillRect(cx - 4, cy - 4, 8, 8, 0x00);
        portraitFillRect(cx - 2, cy - 2, 4, 4, 0xFF);
    }
}

static void drawBookLibraryScreen()'''
    
    content = content.replace(old_rows_area, new_rows_area)

    # 5. Add SD card save/load functions before fetchBookLibrary
    sd_functions = '''
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
    
    // Create books directory if it doesn't exist
    char dirPath[32];
    snprintf(dirPath, sizeof(dirPath), "%s/%ld", BOOK_SD_FOLDER, (long)bookId);
    if (!SD.exists(dirPath)) {
        SD.mkdir(dirPath);
    }
    
    // Save metadata
    char metaPath[64];
    snprintf(metaPath, sizeof(metaPath), "%s/meta.txt", dirPath);
    File metaFile = SD.open(metaPath, FILE_WRITE);
    if (!metaFile) {
        Serial.println("Failed to open meta file for writing");
        return false;
    }
    metaFile.printf("%ld\\n%s\\n%s\\n%s\\n", (long)bookId, title, author, category);
    metaFile.close();
    
    // Save content
    char contentPath[64];
    snprintf(contentPath, sizeof(contentPath), "%s/content.txt", dirPath);
    File contentFile = SD.open(contentPath, FILE_WRITE);
    if (!contentFile) {
        Serial.println("Failed to open content file for writing");
        return false;
    }
    contentFile.print(content);
    contentFile.close();
    
    Serial.printf("Book %ld saved to SD card\\n", (long)bookId);
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
    metaFile.readStringUntil('\\n');
    // Read title
    String t = metaFile.readStringUntil('\\n');
    t.trim();
    snprintf(title, 80, "%s", t.c_str());
    // Read author
    String a = metaFile.readStringUntil('\\n');
    a.trim();
    snprintf(author, 40, "%s", a.c_str());
    // Read category
    String c = metaFile.readStringUntil('\\n');
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
    
    Serial.printf("Book %ld loaded from SD card\\n", (long)bookId);
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
            int32_t bookId = atoi(entry.name());
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

'''
    
    content = content.replace(
        'static bool fetchBookLibrary()\n{',
        sd_functions + 'static bool fetchBookLibrary()\n{'
    )

    # 6. Modify processTouchRelease to handle save icon tap
    # Find the book library touch handling section
    old_touch = '''        for (int i = 0; i < book_count; ++i) {
            const int32_t rowY = BOOK_LIST_Y + i * BOOK_LIST_ROW_H;
            if (touchHitsPortraitRect(x, y, BOOK_LIST_X, rowY, BOOK_LIST_W, BOOK_LIST_ROW_BOX_H)) {
                const int32_t tappedBookId = book_items[i].id;'''
    
    new_touch = '''        for (int i = 0; i < book_count; ++i) {
            const int32_t rowY = BOOK_LIST_Y + i * BOOK_LIST_ROW_H;
            
            // Check if save icon was tapped
            int32_t saveIconX = BOOK_LIST_X + BOOK_LIST_W - BOOK_SAVE_ICON_SIZE - BOOK_SAVE_ICON_MARGIN;
            int32_t saveIconY = rowY + (BOOK_LIST_ROW_BOX_H - BOOK_SAVE_ICON_SIZE) / 2;
            if (touchHitsPortraitRect(x, y, saveIconX, saveIconY, BOOK_SAVE_ICON_SIZE, BOOK_SAVE_ICON_SIZE)) {
                // Toggle save state
                if (WiFi.status() == WL_CONNECTED && book_items[i].id > 0) {
                    snprintf(book_library_status, sizeof(book_library_status), "Saving book %d...", book_items[i].id);
                    refreshDisplay(drawBookLibraryScreen);
                    
                    // Fetch full book data if not already loaded
                    if (selected_book_content.length() == 0 || selected_book_id != book_items[i].id) {
                        if (fetchSelectedBook(book_items[i].id)) {
                            saveBookToSd(book_items[i].id, book_items[i].title, book_items[i].author, book_items[i].category, selected_book_content.c_str());
                            book_items[i].saved = true;
                            snprintf(book_library_status, sizeof(book_library_status), "Book %d saved", book_items[i].id);
                        } else {
                            snprintf(book_library_status, sizeof(book_library_status), "Failed to save book %d", book_items[i].id);
                        }
                    } else {
                        saveBookToSd(book_items[i].id, book_items[i].title, book_items[i].author, book_items[i].category, selected_book_content.c_str());
                        book_items[i].saved = true;
                        snprintf(book_library_status, sizeof(book_library_status), "Book %d saved", book_items[i].id);
                    }
                    refreshDisplay(drawBookLibraryScreen);
                }
                touch_loop_interval = millis() + 300;
                return;
            }
            
            if (touchHitsPortraitRect(x, y, BOOK_LIST_X, rowY, BOOK_LIST_W, BOOK_LIST_ROW_BOX_H)) {
                const int32_t tappedBookId = book_items[i].id;'''
    
    content = content.replace(old_touch, new_touch)

    # 7. Modify fetchBookLibrary to try SD card first
    old_fetch = '''static bool fetchBookLibrary()
{
    BookListItem fetched_items[MAX_BOOK_ITEMS];
    int fetched_count = 0;
    int fetched_total = book_total;

    if (WiFi.status() != WL_CONNECTED) {
        snprintf(book_library_status, sizeof(book_library_status), "WiFi not connected");
        return false;
    }'''
    
    new_fetch = '''static bool fetchBookLibrary()
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

    BookListItem fetched_items[MAX_BOOK_ITEMS];
    int fetched_count = 0;
    int fetched_total = book_total;'''
    
    content = content.replace(old_fetch, new_fetch)

    # 8. Mark books as saved when loading from online source
    old_mark = '''        copyJsonString(book.category, sizeof(book.category), item, "category", "");
        ++fetched_count;'''
    
    new_mark = '''        copyJsonString(book.category, sizeof(book.category), item, "category", "");
        book.saved = isBookSavedOnSd(book.id);
        ++fetched_count;'''
    
    content = content.replace(old_mark, new_mark)

    with open(path, 'w', encoding='utf-8') as f:
        f.write(content)
    
    print("Patch applied successfully!")

if __name__ == '__main__':
    main()
