#!/usr/bin/env python3
"""Patch demo.ino to separate save icon from book row."""

def main():
    path = 'examples/demo/demo.ino'
    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()

    # 1. Adjust drawBookLibraryRowsArea to put save icon outside the row
    old_draw = '''        // Show category on the right side of the row (right-aligned, vertically centered).
        if (book_items[i].category[0] != '\\0') {
            drawUtf8ChineseTextRightAligned(book_items[i].category, BOOK_LIST_X + BOOK_LIST_W - BOOK_SAVE_ICON_SIZE - BOOK_SAVE_ICON_MARGIN * 2 - 16, y, BOOK_LIST_ROW_BOX_H);
        }

        // Draw save icon at end of row
        int32_t saveIconX = BOOK_LIST_X + BOOK_LIST_W - BOOK_SAVE_ICON_SIZE - BOOK_SAVE_ICON_MARGIN;
        int32_t saveIconY = y + (BOOK_LIST_ROW_BOX_H - BOOK_SAVE_ICON_SIZE) / 2;
        drawBookSaveIcon(saveIconX, saveIconY, book_items[i].saved);
    }
}'''

    new_draw = '''        // Show category on the right side of the row (right-aligned, vertically centered).
        if (book_items[i].category[0] != '\\0') {
            drawUtf8ChineseTextRightAligned(book_items[i].category, BOOK_LIST_X + BOOK_LIST_W - 16, y, BOOK_LIST_ROW_BOX_H);
        }
    }

    // Draw save icons outside the row boxes, aligned to the right edge
    for (int i = 0; i < book_count; ++i) {
        const int32_t y = BOOK_LIST_Y + i * BOOK_LIST_ROW_H;
        int32_t saveIconX = BOOK_LIST_X + BOOK_LIST_W + BOOK_SAVE_ICON_MARGIN;
        int32_t saveIconY = y + (BOOK_LIST_ROW_BOX_H - BOOK_SAVE_ICON_SIZE) / 2;
        drawBookSaveIcon(saveIconX, saveIconY, book_items[i].saved);
    }
}'''

    content = content.replace(old_draw, new_draw)

    # 2. Update touch handler to use new icon position
    old_touch = '''            // Check if save icon was tapped
            int32_t saveIconX = BOOK_LIST_X + BOOK_LIST_W - BOOK_SAVE_ICON_SIZE - BOOK_SAVE_ICON_MARGIN;
            int32_t saveIconY = rowY + (BOOK_LIST_ROW_BOX_H - BOOK_SAVE_ICON_SIZE) / 2;'''

    new_touch = '''            // Check if save icon was tapped (icon is now outside the row, to the right)
            int32_t saveIconX = BOOK_LIST_X + BOOK_LIST_W + BOOK_SAVE_ICON_MARGIN;
            int32_t saveIconY = rowY + (BOOK_LIST_ROW_BOX_H - BOOK_SAVE_ICON_SIZE) / 2;'''

    content = content.replace(old_touch, new_touch)

    with open(path, 'w', encoding='utf-8') as f:
        f.write(content)
    
    print("Save icon position patch applied successfully!")

if __name__ == '__main__':
    main()
