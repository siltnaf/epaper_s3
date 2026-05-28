"""
Add missing pinyin to image_learn_items in PostgreSQL.

Reads all words from the image_learn_items table, generates pinyin
with pypinyin (default style with tone marks), and updates rows
where pinyin is currently empty or null.
"""

import psycopg2
from pypinyin import pinyin, Style

DB_CONFIG = {
    "host": "127.0.0.1",
    "port": 5432,
    "user": "ebook",
    "password": "ebook123",
    "dbname": "ebook",
}


def generate_pinyin_for_word(chinese_word: str) -> str:
    """Return pinyin with tone marks for a Chinese word/phrase."""
    if not chinese_word:
        return ""
    # Use default style (Style.TONE) for tone marks like rén
    syllables = pinyin(chinese_word, style=Style.TONE)
    return " ".join(s[0] for s in syllables)


def main():
    conn = psycopg2.connect(**DB_CONFIG)
    conn.autocommit = False
    cur = conn.cursor()

    # Fetch rows that have no pinyin data yet
    cur.execute(
        """
        SELECT id, word FROM image_learn_items
        WHERE COALESCE(pinyin, '') = ''
        ORDER BY sort_order, id
        """
    )
    rows = cur.fetchall()
    total = len(rows)
    print(f"Found {total} rows missing pinyin")

    updated = 0
    errors = 0

    for row_id, word in rows:
        try:
            py = generate_pinyin_for_word(word)
            if not py:
                print(f"  WARN: empty pinyin for id={row_id} word='{word}'")
                continue
            cur.execute(
                "UPDATE image_learn_items SET pinyin = %s WHERE id = %s",
                (py, row_id),
            )
            updated += 1
            if updated % 20 == 0:
                print(f"  Progress: {updated}/{total}")
        except Exception as e:
            errors += 1
            print(f"  ERROR id={row_id} word='{word}': {e}")

    conn.commit()
    cur.close()
    conn.close()

    print(f"\nDone: {updated} updated, {errors} errors, {total} total missing")


if __name__ == "__main__":
    main()