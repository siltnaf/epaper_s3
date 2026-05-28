#!/usr/bin/env python3
"""Patch demo.ino to fix save icon position and size."""

def main():
    path = 'examples/demo/demo.ino'
    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()

    # 1. Change icon size to 28px (20% bigger than 24px)
    content = content.replace(
        'static const int32_t BOOK_SAVE_ICON_SIZE = 24;',
        'static const int32_t BOOK_SAVE_ICON_SIZE = 28;'
    )

    # 2. Move save icon to inside the row box, near right edge
    old_draw = '''    // Draw save icons outside the row boxes, aligned to the right edge
    for (int i = 0; i < book_count; ++i) {
        const int32_t y = BOOK_LIST_Y + i * BOOK_LIST_ROW_H;
        int32_t saveIconX = BOOK_LIST_X + BOOK_LIST_W + BOOK_SAVE_ICON_MARGIN;
        int32_t saveIconY = y + (BOOK_LIST_ROW_BOX_H - BOOK_SAVE_ICON_SIZE) / 2;
        drawBookSaveIcon(saveIconX, saveIconY, book_items[i].saved);
    }
}'''

    new_draw = '''    // Draw save icons inside the row boxes, near the right edge
    for (int i = 0; i < book_count; ++i) {
        const int32_t y = BOOK_LIST_Y + i * BOOK_LIST_ROW_H;
        int32_t saveIconX = BOOK_LIST_X + BOOK_LIST_W - BOOK_SAVE_ICON_SIZE - 8;
        int32_t saveIconY = y + (BOOK_LIST_ROW_BOX_H - BOOK_SAVE_ICON_SIZE) / 2;
        drawBookSaveIcon(saveIconX, saveIconY, book_items[i].saved);
    }
}'''

    content = content.replace(old_draw, new_draw)

    # 3. Update touch handler to match new icon position
    old_touch = '''            // Check if save icon was tapped (icon is now outside the row, to the right)
            int32_t saveIconX = BOOK_LIST_X + BOOK_LIST_W + BOOK_SAVE_ICON_MARGIN;
            int32_t saveIconY = rowY + (BOOK_LIST_ROW_BOX_H - BOOK_SAVE_ICON_SIZE) / 2;'''

    new_touch = '''            // Check if save icon was tapped (icon is inside the row, near right edge)
            int32_t saveIconX = BOOK_LIST_X + BOOK_LIST_W - BOOK_SAVE_ICON_SIZE - 8;
            int32_t saveIconY = rowY + (BOOK_LIST_ROW_BOX_H - BOOK_SAVE_ICON_SIZE) / 2;'''

    content = content.replace(old_touch, new_touch)

    # 4. Adjust category position to not overlap with save icon
    old_cat = '''        // Show category on the right side of the row (right-aligned, vertically centered).
        if (book_items[i].category[0] != '\\0') {
            drawUtf8ChineseTextRightAligned(book_items[i].category, BOOK_LIST_X + BOOK_LIST_W - 16, y, BOOK_LIST_ROW_BOX_H);
        }'''

    new_cat = '''        // Show category on the right side of the row (right-aligned, vertically centered).
        // Leave space for save icon on the right
        if (book_items[i].category[0] != '\\0') {
            drawUtf8ChineseTextRightAligned(book_items[i].category, BOOK_LIST_X + BOOK_LIST_W - BOOK_SAVE_ICON_SIZE - 20, y, BOOK_LIST_ROW_BOX_H);
        }'''

    content = content.replace(old_cat, new_cat)

    with open(path, 'w', encoding='utf-8') as f:
        f.write(content)
    
    print("Save icon position and size fix applied successfully!")

if __name__ == '__main__':
    main()
