from pathlib import Path
import re
from PIL import Image, ImageDraw, ImageFont

# Use DengXian Light to better match the thin, squared e-reader UI style
# shown in f.jpg.  The previous SimHei source was much heavier/bolder,
# which made long book content look dense on the e-paper screen.
FONT_PATH = r"C:/Windows/Fonts/Dengl.ttf"
FONT_SIZE = 24
OUT = Path("examples/demo/chinese_font.h")

seed = "\u4e66\u5e93\u672c\u5171\u518c\u683c\u6797\u7ae5\u8bdd\u5929\u65b9\u591c\u8c2d\u5152\u7ae5\u7ed8\u672c\u53e4\u8bd7\u56fe\u7247\u5b66\u4f5c\u8005\u5206\u7c7b\u672a\u77e5\u5bd3\u8a00\u7ae5\u4e8b\u5b89\u4e56\u751f\u5c0f\u738b\u5b50\u672c\u5730\u65f6\u949f\u5f53\u524d\u4f4d\u7f6e\u5f53\u5730\u5929\u6c14\u6674\u591a\u4e91\u9641\u5c0f\u96e8\u5927\u96e8\u96f4\u96f7\u6681\u96fe\u96f6\u8461\u8584\u96fe\u98ce\u70ed\u7ec1\u51b7\u5bd2\u6e29\u6696\u6e29\u5ea6\u5ea6\u6e29\u5ea6\u98ce\u98ce\u989c\u98ce\u516c\u91cc\u6444\u5c14\u516c\u6444\u6e05\u6837\u6670\u6e05\u516c\u6e05\u6e05\u516c\u6e05\u6837\u6e05\u6837\u516c\u6837\u516c\u6837\u6e05\u6837\u516c\u6837\u6e05\u6e05\u6837\u516c\u5e74\u6708\u65e5\u5468\u661f\u5468\u4e00\u4e8c\u4e09\u56db\u4e94\u516d\u5341\u5ea6\u7ea7\u5237\u65b0\u540c\u533b\u83b7\u53d6\u4e2d\u3002\uff0c\u3001\uff1a\uff1b\uff01\uff1f\u201c\u201d\u2018\u2019\uff08\uff09\u300a\u300b\u2014\u2026\u00b7"
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