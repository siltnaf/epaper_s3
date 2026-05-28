from pathlib import Path
import re
from PIL import Image, ImageDraw, ImageFont

# Use DengXian Light to better match the thin, squared e-reader UI style
# shown in f.jpg.  The previous SimHei source was much heavier/bolder,
# which made long book content look dense on the e-paper screen.
FONT_PATH = r"C:/Windows/Fonts/Dengl.ttf"
FONT_SIZE = 24
OUT = Path("examples/demo/chinese_font.h")

seed = "书库本共册格林童话天方夜谭儿童绘本古诗图片学习作者分类未知寓言故事安徒生小王子。，、：；！？“”‘’（）《》——…·"
chars = set(seed)
# Scan the reference folder (JS and HTML files) and the examples folder for Chinese characters
# plus CJK punctuation.  The reader renders book content with this bitmap font;
# if punctuation is omitted, full-width marks fall back to small ASCII symbols or
# blank advance space, which makes Chinese text look broken on the e-paper.
for folder in ["reference", "examples"]:
    for p in Path(folder).rglob("*.*"):
        if "node_modules" in p.parts:
            continue
        if p.is_file() and p.suffix in [".js", ".html", ".ino", ".h"]:
            try:
                s = p.read_text(encoding="utf-8", errors="ignore")
                chars.update(re.findall(r"[\u3400-\u9fff\u3000-\u303f\uff00-\uffef\u2018\u2019\u201c\u201d\u2014\u2026]", s))
            except Exception as e:
                print(f"Skipping {p}: {e}")

chars = "".join(sorted(chars))
font = ImageFont.truetype(FONT_PATH, FONT_SIZE)
ascent, descent = font.getmetrics()
height = ascent + descent

glyphs = []
bitmap_bytes = []
offset = 0

for ch in chars:
    bbox = font.getbbox(ch)
    width = max(1, int(font.getlength(ch)) + 1)
    img = Image.new("1", (width, height), 0)
    draw = ImageDraw.Draw(img)
    draw.text((-bbox[0], 0), ch, font=font, fill=1)
    pixels = img.load()
    row_bytes = (width + 7) // 8
    data = []
    for y in range(height):
        for xb in range(row_bytes):
            b = 0
            for bit in range(8):
                x = xb * 8 + bit
                if x < width and pixels[x, y]:
                    b |= 0x80 >> bit
            data.append(b)
    glyphs.append((ord(ch), width, height, row_bytes, offset, len(data)))
    bitmap_bytes.extend(data)
    offset += len(data)

def chunks(seq, n):
    for i in range(0, len(seq), n):
        yield seq[i:i+n]

with OUT.open("w", encoding="utf-8") as f:
    f.write("#pragma once\n#include <stdint.h>\n#include <stddef.h>\n\n")
    f.write(f"static const uint8_t ChineseFontHeight = {height};\n")
    f.write(f"static const uint16_t ChineseFontGlyphCount = {len(glyphs)};\n\n")
    f.write("struct ChineseGlyph { uint32_t codepoint; uint8_t width; uint8_t height; uint8_t rowBytes; uint32_t offset; uint16_t length; };\n\n")
    f.write(f"static const uint8_t ChineseFontBitmap[{len(bitmap_bytes)}] = {{\n")
    for c in chunks(bitmap_bytes, 16):
        f.write("    " + ", ".join(f"0x{x:02X}" for x in c) + ",\n")
    f.write("};\n\n")
    f.write(f"static const ChineseGlyph ChineseFontGlyphs[{len(glyphs)}] = {{\n")
    for cp, w, h, rb, off, ln in glyphs:
        f.write(f"    {{0x{cp:X}, {w}, {h}, {rb}, {off}, {ln}}},\n")
    f.write("};\n")

print(f"wrote {OUT} with {len(glyphs)} glyphs, {len(bitmap_bytes)} bytes")