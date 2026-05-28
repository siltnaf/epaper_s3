const { Pool } = require('pg');
const fs = require('fs');
const path = require('path');
const { execFileSync } = require('child_process');

const pool = new Pool({
  host: '127.0.0.1',
  port: 5432,
  user: 'ebook',
  password: 'ebook123',
  database: 'ebook',
});

const PDF_PATH = path.join(__dirname, 'img', 'a.pdf');
const PAGE_DIR = path.join(__dirname, 'img', 'a-pages');
const PAGE_PREFIX = path.join(PAGE_DIR, 'page');
const OCR_CROP_DIR = path.join(PAGE_DIR, '.ocr-crops');
const REVIEW_CSV_PATH = path.join(__dirname, 'image-learn-review.csv');
const PAGE_COUNT = 144;

// a.pdf is a scanned 看圖學中文 picture book. OCR is intentionally used only
// as a helper for labels; the real learning image is the rendered PDF page.
const MANUAL_LABELS = {
  1: ['人', 'rén'],
  2: ['頭', 'tóu'],
  4: ['眉', 'méi'],
  6: ['耳', 'ěr'],
  9: ['舌', ''],
  10: ['心', ''],
  12: ['足', ''],
  14: ['一', ''],
  20: ['七', ''],
  21: ['八', ''],
  22: ['九', ''],
  36: ['雪', ''],
  38: ['電', ''],
  43: ['瓜', ''],
  50: ['花色', 'huā sè'],
  52: ['山', ''],
  54: ['竹', ''],
  64: ['弓', ''],
  65: ['刀', ''],
  83: ['弯', ''],
  87: ['吃', ''],
  88: ['喝', ''],
  91: ['坐', ''],
  94: ['看', ''],
  102: ['包', ''],
  106: ['画', ''],
  109: ['衣', ''],
  110: ['裙', ''],
  111: ['袜', ''],
  114: ['帽', ''],
  115: ['毛', ''],
  121: ['灰', ''],
  125: ['奶', ''],
  126: ['妈', ''],
  134: ['男', ''],
};

function ensurePdfPages() {
  fs.mkdirSync(PAGE_DIR, { recursive: true });
  const existing = fs.readdirSync(PAGE_DIR).filter(f => /^page-\d+\.png$/.test(f));
  if (existing.length >= PAGE_COUNT) return;

  console.log('Rendering a.pdf pages into img/a-pages ...');
  execFileSync('pdftoppm', ['-png', '-r', '120', PDF_PATH, PAGE_PREFIX], { stdio: 'inherit' });
}

function pageFileName(pageNo) {
  return `page-${String(pageNo).padStart(3, '0')}.png`;
}

function pageImageUrl(pageNo) {
  return `/img/a-pages/${pageFileName(pageNo)}`;
}

function parseCsvLine(line) {
  const fields = [];
  let current = '';
  let inQuotes = false;

  for (let i = 0; i < line.length; i++) {
    const char = line[i];
    const next = line[i + 1];

    if (char === '"') {
      if (inQuotes && next === '"') {
        current += '"';
        i++;
      } else {
        inQuotes = !inQuotes;
      }
    } else if (char === ',' && !inQuotes) {
      fields.push(current);
      current = '';
    } else {
      current += char;
    }
  }

  fields.push(current);
  return fields;
}

function readReviewCsvItems() {
  if (!fs.existsSync(REVIEW_CSV_PATH)) return [];

  const lines = fs.readFileSync(REVIEW_CSV_PATH, 'utf8')
    .split(/\r?\n/)
    .filter(line => line.trim());
  const [headerLine, ...dataLines] = lines;
  const headers = parseCsvLine(headerLine || '');

  return dataLines.map((line, index) => {
    const values = parseCsvLine(line);
    const row = Object.fromEntries(headers.map((header, i) => [header, values[i] || '']));
    const sortOrder = Number.parseInt(row.page, 10) || index + 1;

    return {
      word: row.word,
      pinyin: row.pinyin || '',
      emoji: '',
      image_url: row.image_url || pageImageUrl(sortOrder),
      category: row.category || '看圖學中文 a.pdf',
      source: row.source || 'a.pdf rendered page images',
      sort_order: sortOrder,
    };
  }).filter(item => item.word && item.image_url);
}

function ocrCropFileName(pageNo) {
  return `ocr-${String(pageNo).padStart(3, '0')}.png`;
}

function ensureOcrCrop(pageNo) {
  const sourcePath = path.join(PAGE_DIR, pageFileName(pageNo));
  const cropPath = path.join(OCR_CROP_DIR, ocrCropFileName(pageNo));
  if (!fs.existsSync(sourcePath)) return '';

  fs.mkdirSync(OCR_CROP_DIR, { recursive: true });

  // The label word in this scanned book is black and sits at the top-left of
  // the image card.  Full-page OCR often sees the drawing or footer first, so
  // preprocess that area only: keep neutral dark pixels, put them on a white
  // background, crop to their bounding box, and enlarge for Tesseract.
  const script = String.raw`
from PIL import Image, ImageEnhance
import sys

src, dest = sys.argv[1], sys.argv[2]
im = Image.open(src).convert('RGB')
w, h = im.size
region = im.crop((0, 0, int(w * 0.55), int(h * 0.35)))
out = Image.new('L', region.size, 255)
pix = region.load()
opix = out.load()
xs = []
ys = []

for y in range(region.height):
    for x in range(region.width):
        r, g, b = pix[x, y]
        # Keep black/gray text strokes and discard most colored illustration.
        if max(r, g, b) < 115 and (max(r, g, b) - min(r, g, b)) < 55:
            opix[x, y] = 0
            xs.append(x)
            ys.append(y)

if xs:
    pad = 20
    box = (
        max(0, min(xs) - pad),
        max(0, min(ys) - pad),
        min(out.width, max(xs) + pad + 1),
        min(out.height, max(ys) + pad + 1),
    )
    out = out.crop(box)

scale = 4
out = out.resize((max(1, out.width * scale), max(1, out.height * scale)), Image.Resampling.LANCZOS)
out = ImageEnhance.Contrast(out).enhance(3.0)
out.save(dest)
`;

  try {
    execFileSync('python3', ['-c', script, sourcePath, cropPath], {
      stdio: ['ignore', 'ignore', 'ignore'],
      timeout: 10000,
    });
    return cropPath;
  } catch (e) {
    return sourcePath;
  }
}

function ocrPage(pageNo) {
  const filePath = ensureOcrCrop(pageNo);
  if (!fs.existsSync(filePath)) return '';

  const outputs = [];
  // Try several page segmentation modes because labels are usually one large
  // character, but some are two-character words.
  for (const psm of ['13', '8', '10', '7', '6', '11']) {
    try {
      const output = execFileSync('tesseract', [filePath, 'stdout', '-l', 'chi_sim+chi_tra+eng', '--psm', psm], {
        encoding: 'utf8',
        stdio: ['ignore', 'pipe', 'ignore'],
        timeout: 15000,
      });
      outputs.push(`psm${psm}: ${output}`);
    } catch (e) {
      // Try the next OCR mode.
    }
  }
  return outputs.join('\n');
}

function cleanOcrLabel(raw) {
  const titleNoise = /看[圖图]?學?中?文?|看[圖图]|[學学]中文|中文|看圖學中文|看图学中文/;
  const candidates = [];

  for (const line of String(raw || '').split(/\r?\n/)) {
    const chineseParts = line.match(/[\u3400-\u9fff]{1,4}/g) || [];
    for (const part of chineseParts) {
      if (!part || titleNoise.test(part)) continue;
      // Prefer the label-like OCR result: one or two Chinese characters from
      // the black word crop.  Longer chunks are still allowed as a fallback.
      const score =
        (part.length <= 2 ? 20 : 0) +
        (part.length === 1 ? 5 : 0) -
        Math.abs(part.length - 1);
      candidates.push({ text: part, score });
    }
  }

  candidates.sort((a, b) => b.score - a.score);
  return candidates[0]?.text || '';
}

function buildItems() {
  const reviewedItems = readReviewCsvItems();
  if (reviewedItems.length) return reviewedItems;

  const items = [];
  const usedWords = new Map();
  for (let pageNo = 1; pageNo <= PAGE_COUNT; pageNo++) {
    const manual = MANUAL_LABELS[pageNo];
    const ocrLabel = manual ? manual[0] : cleanOcrLabel(ocrPage(pageNo));
    const baseWord = ocrLabel || `看圖學中文 ${pageNo}`;
    const seen = usedWords.get(baseWord) || 0;
    usedWords.set(baseWord, seen + 1);
    const word = seen ? `${baseWord} ${pageNo}` : baseWord;
    const pinyin = manual ? manual[1] : '';
    items.push({
      word,
      pinyin,
      emoji: '',
      image_url: pageImageUrl(pageNo),
      category: '看圖學中文 a.pdf',
      source: 'a.pdf rendered page images',
      sort_order: pageNo,
    });
  }
  return items;
}

async function main() {
  ensurePdfPages();
  const items = buildItems();

  await pool.query(`
    CREATE TABLE IF NOT EXISTS image_learn_items (
      id SERIAL PRIMARY KEY,
      word VARCHAR(64) NOT NULL UNIQUE,
      pinyin VARCHAR(128),
      emoji VARCHAR(32) NOT NULL DEFAULT '',
      image_url TEXT NOT NULL,
      category VARCHAR(64),
      source VARCHAR(128) DEFAULT 'a.pdf rendered page images',
      age_min INTEGER DEFAULT 3,
      age_max INTEGER DEFAULT 8,
      sort_order INTEGER NOT NULL DEFAULT 0,
      created_at TIMESTAMP DEFAULT NOW()
    )
  `);

  await pool.query('TRUNCATE TABLE image_learn_items RESTART IDENTITY');
  await pool.query('BEGIN');
  try {
    for (const item of items) {
      await pool.query(
        `INSERT INTO image_learn_items
          (word, pinyin, emoji, image_url, category, source, age_min, age_max, sort_order)
         VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9)`,
        [item.word, item.pinyin, item.emoji, item.image_url, item.category, item.source, 3, 8, item.sort_order]
      );
    }
    await pool.query('COMMIT');
  } catch (e) {
    await pool.query('ROLLBACK');
    throw e;
  }

  const count = await pool.query('SELECT COUNT(*)::int AS count FROM image_learn_items');
  console.log(`Seeded ${items.length} 看圖學中文 records from a.pdf. image_learn_items now has ${count.rows[0].count} records.`);
  console.log('Sample:', items.slice(0, 8));
}

main()
  .catch(err => {
    console.error(err.stack || err.message);
    process.exitCode = 1;
  })
  .finally(() => pool.end());
