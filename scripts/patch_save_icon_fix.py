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
