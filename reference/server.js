const express = require('express');
const { Pool } = require('pg');
const cors = require('cors');
const path = require('path');
const geoip = require('geoip-lite');
const fs = require('fs');
const http = require('http');
const https = require('https');
const crypto = require('crypto');
const { spawn } = require('child_process');

const app = express();
const PORT = process.env.PORT || 3001;
const USE_HTTPS = process.env.HTTPS === 'true';
const SSL_KEY = process.env.SSL_KEY || path.join(__dirname, 'certs', 'localhost-key.pem');
const SSL_CERT = process.env.SSL_CERT || path.join(__dirname, 'certs', 'localhost.pem');

// PostgreSQL connection
const pool = new Pool({
  host: '127.0.0.1',
  port: 5432,
  user: 'ebook',
  password: 'ebook123',
  database: 'ebook',
});

app.use(cors());
app.use(express.json());

const ttsCacheDir = path.join(__dirname, 'tts-cache');
fs.mkdirSync(ttsCacheDir, { recursive: true });

// Trust proxy headers - needed to get real client IP when behind a reverse proxy
app.set('trust proxy', true);

// Serve static files
app.use(express.static(path.join(__dirname)));

// API: Get books (paginated, metadata only, no content)
app.get('/api/books', async (req, res) => {
  try {
    const page = Math.max(1, parseInt(req.query.page) || 1);
    const perPage = Math.min(50, Math.max(1, parseInt(req.query.perPage) || 10));
    const offset = (page - 1) * perPage;
    const countRes = await pool.query('SELECT COUNT(*)::int FROM books');
    const count = countRes.rows[0].count;
    const { rows } = await pool.query(
      'SELECT id, title, author, category, created_at FROM books ORDER BY created_at DESC LIMIT $1 OFFSET $2',
      [perPage, offset]
    );
    res.json({ items: rows, total: count, page, perPage });
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

// API: Get single book
app.get('/api/books/:id', async (req, res) => {
  try {
    const { rows } = await pool.query('SELECT * FROM books WHERE id = $1', [req.params.id]);
    if (rows.length === 0) return res.status(404).json({ error: 'Book not found' });
    res.json(rows[0]);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

// API: Add a book
app.post('/api/books', async (req, res) => {
  const { title, author, description, content, cover_url, file_path, category } = req.body;
  try {
    const { rows } = await pool.query(
      'INSERT INTO books (title, author, description, content, cover_url, file_path, category) VALUES ($1,$2,$3,$4,$5,$6,$7) RETURNING *',
      [title, author, description, content, cover_url, file_path, category]
    );
    res.json(rows[0]);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

// API: Get reading progress
app.get('/api/progress/:bookId', async (req, res) => {
  try {
    const { rows } = await pool.query('SELECT * FROM reading_progress WHERE book_id = $1', [req.params.bookId]);
    res.json(rows[0] || {});
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

// API: Save reading progress
app.post('/api/progress', async (req, res) => {
  const { book_id, chapter, position } = req.body;
  try {
    const { rows } = await pool.query(
      `INSERT INTO reading_progress (book_id, chapter, position, updated_at)
       VALUES ($1, $2, $3, NOW())
       ON CONFLICT (book_id) DO UPDATE SET chapter = $2, position = $3, updated_at = NOW()
       RETURNING *`,
      [book_id, chapter, position]
    );
    res.json(rows[0]);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

// Helper: resolve client IP to geo info
function resolveClientGeo(req) {
  var clientIp = req.ip || req.connection.remoteAddress || req.socket.remoteAddress
  if (clientIp.startsWith('::ffff:')) {
    clientIp = clientIp.substring(7)
  }
  var isPrivate = /^(127\.|10\.|172\.(1[6-9]|2[0-9]|3[01])\.|192\.168\.|::1$|localhost)/.test(clientIp)
  if (isPrivate) {
    var fallback = geoip.lookup('218.17.0.1')
    if (fallback && fallback.city) {
      return { ip: clientIp, city: fallback.city, region: fallback.region, country: fallback.country, timezone: fallback.timezone, ll: fallback.ll }
    }
    return { ip: clientIp, city: null, region: null, country: null, timezone: null, ll: null }
  }
  var geo = geoip.lookup(clientIp)
  if (geo && geo.city) {
    return { ip: clientIp, city: geo.city, region: geo.region, country: geo.country, timezone: geo.timezone, ll: geo.ll }
  }
  return { ip: clientIp, city: null, region: null, country: null, timezone: null, ll: null }
}

// City name translation map
var cityZhMap = {
  'Shenzhen': '深圳', 'Shanghai': '上海', 'Beijing': '北京', 'Guangzhou': '广州',
  'Hangzhou': '杭州', 'Chengdu': '成都', 'Wuhan': '武汉', 'Nanjing': '南京',
  'Tianjin': '天津', 'Chongqing': '重庆', 'Suzhou': '苏州', "Xi'an": '西安',
  'Changsha': '长沙', 'Zhengzhou': '郑州', 'Dongguan': '东莞', 'Qingdao': '青岛',
  'Shenyang': '沈阳', 'Ningbo': '宁波', 'Kunming': '昆明', 'Dalian': '大连',
  'Xiamen': '厦门', 'Fuzhou': '福州', 'Hefei': '合肥', 'Wuxi': '无锡',
  'Foshan': '佛山', 'Changzhou': '常州', 'Jinan': '济南', 'Harbin': '哈尔滨',
  'Changchun': '长春', 'Lanzhou': '兰州', 'Guiyang': '贵阳', 'Nanning': '南宁',
  'Taiyuan': '太原', 'Shijiazhuang': '石家庄', 'Haikou': '海口', 'Sanya': '三亚',
  'Macau': '澳门', 'Hong Kong': '香港', 'Taipei': '台北'
}

function translateCityToZh(en) {
  return cityZhMap[en] || en
}

// API: Geo IP location (legacy endpoint, kept for compatibility)
app.get('/api/geoip', (req, res) => {
  var info = resolveClientGeo(req)
  res.json(info)
})

// API: Clock - returns current time and date based on IP-detected timezone
app.get('/api/clock', (req, res) => {
  var info = resolveClientGeo(req)
  var tz = info.timezone || Intl.DateTimeFormat().resolvedOptions().timeZone
  var now = new Date()
  var timeParts = new Intl.DateTimeFormat('zh-CN', {
    timeZone: tz,
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit',
    hour12: false
  }).formatToParts(now)
  var h = timeParts.find(function(p) { return p.type === 'hour' }).value
  var m = timeParts.find(function(p) { return p.type === 'minute' }).value
  var s = timeParts.find(function(p) { return p.type === 'second' }).value
  var time = h + ':' + m + ':' + s

  var date = new Intl.DateTimeFormat('zh-CN', {
    timeZone: tz,
    year: 'numeric',
    month: 'long',
    day: 'numeric',
    weekday: 'long'
  }).format(now)

  res.json({
    time: time,
    date: date,
    timezone: tz
  })
})

// API: Location - returns location message based on IP detection
app.get('/api/location', (req, res) => {
  var info = resolveClientGeo(req)
  var cityZh = info.city ? translateCityToZh(info.city) : null
  var regionZh = info.region ? translateCityToZh(info.region) : null
  // Build a location message
  var message = ''
  if (cityZh) {
    message = cityZh
    if (regionZh && regionZh !== cityZh) {
      message = regionZh + ' ' + cityZh
    }
    if (info.country && info.country !== 'CN') {
      message = message + ', ' + info.country
    }
  }
  res.json({
    city: cityZh,
    region: regionZh,
    country: info.country,
    message: message,
    ip: info.ip,
    timezone: info.timezone
  })
})

// API: Weather - auto-detects city from IP if no city parameter provided
app.get('/api/weather', async (req, res) => {
  var city = req.query.city
  if (!city) {
    // Auto-detect city from IP
    var info = resolveClientGeo(req)
    if (info.city) {
      city = translateCityToZh(info.city)
    }
  }
  if (!city) return res.status(400).json({ error: '无法检测城市，请提供城市参数' })
  try {
    var response = await fetch('https://wttr.in/' + encodeURIComponent(city) + '?format=j1')
    var data = await response.json()
    var current = data.current_condition[0]
    res.json({
      city: data.nearest_area[0].areaName[0].value,
      temp: current.temp_C,
      desc: current.weatherDesc[0].value,
      humidity: current.humidity,
      wind: current.windspeedKmph,
      icon: getWeatherEmoji(current.weatherDesc[0].value)
    })
  } catch (e) {
    res.status(500).json({ error: 'Weather fetch failed: ' + e.message })
  }
})

function getWeatherEmoji(desc) {
  var d = desc.toLowerCase()
  if (d.includes('sun') || d.includes('clear')) return '☀️'
  if (d.includes('cloud')) return '☁️'
  if (d.includes('rain') || d.includes('drizzle')) return '🌧️'
  if (d.includes('thunder') || d.includes('storm')) return '⛈️'
  if (d.includes('snow') || d.includes('ice')) return '❄️'
  if (d.includes('fog') || d.includes('mist') || d.includes('haze')) return '🌫️'
  if (d.includes('overcast')) return '☁️'
  return '🌡️'
}

// API: Get voice stories (paginated)
app.get('/api/voice-stories', async (req, res) => {
  try {
    const page = Math.max(1, parseInt(req.query.page) || 1);
    const perPage = Math.min(50, Math.max(1, parseInt(req.query.perPage) || 10));
    const offset = (page - 1) * perPage;
    const countRes = await pool.query('SELECT COUNT(*)::int FROM voice_stories');
    const count = countRes.rows[0].count;
    const { rows } = await pool.query(
      'SELECT id, title, author, category, created_at FROM voice_stories ORDER BY created_at DESC LIMIT $1 OFFSET $2',
      [perPage, offset]
    );
    res.json({ items: rows, total: count, page, perPage });
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

// API: Get single voice story with content
app.get('/api/voice-stories/:id', async (req, res) => {
  try {
    const { rows } = await pool.query('SELECT * FROM voice_stories WHERE id = $1', [req.params.id]);
    if (rows.length === 0) return res.status(404).json({ error: 'Voice story not found' });
    res.json(rows[0]);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

function normalizeTtsText(text) {
  return String(text || '')
    .replace(/\r/g, '\n')
    .replace(/\s+/g, ' ')
    .trim()
    .slice(0, 12000);
}

function synthesizeOfflineWav(text, outPath) {
  return new Promise((resolve, reject) => {
    // Offline Windows TTS through System.Speech.  This keeps the ESP32 simple:
    // it receives a normal WAV file and plays it through the MAX98357A I2S amp.
    const script = `
Add-Type -AssemblyName System.Speech
$text = [Console]::In.ReadToEnd()
$synth = New-Object System.Speech.Synthesis.SpeechSynthesizer
try { $synth.SelectVoiceByHints([System.Speech.Synthesis.VoiceGender]::Female, [System.Speech.Synthesis.VoiceAge]::Child, 0, [System.Globalization.CultureInfo]::GetCultureInfo('zh-CN')) } catch {}
$synth.Rate = -1
$synth.Volume = 100
$synth.SetOutputToWaveFile('${outPath.replace(/'/g, "''")}')
$synth.Speak($text)
$synth.Dispose()
`;
    const ps = spawn('powershell.exe', ['-NoProfile', '-ExecutionPolicy', 'Bypass', '-Command', script], {
      windowsHide: true,
      stdio: ['pipe', 'pipe', 'pipe']
    });
    let stderr = '';
    ps.stderr.on('data', d => { stderr += d.toString(); });
    ps.on('error', reject);
    ps.on('close', code => {
      if (code === 0 && fs.existsSync(outPath) && fs.statSync(outPath).size > 44) resolve();
      else reject(new Error(stderr || `PowerShell TTS exited with code ${code}`));
    });
    ps.stdin.end(text);
  });
}

// API: Offline TTS WAV for one voice story.  The file is generated once and
// cached locally, so repeated ESP32 requests do not re-synthesize the story.
app.get('/api/voice-stories/:id/tts', async (req, res) => {
  try {
    const { rows } = await pool.query('SELECT id, title, content FROM voice_stories WHERE id = $1', [req.params.id]);
    if (rows.length === 0) return res.status(404).json({ error: 'Voice story not found' });

    const story = rows[0];
    const text = normalizeTtsText(`${story.title || ''}. ${story.content || ''}`);
    if (!text) return res.status(400).json({ error: 'Story has no text to synthesize' });

    const hash = crypto.createHash('sha1').update(`${story.id}\n${text}`).digest('hex').slice(0, 16);
    const wavPath = path.join(ttsCacheDir, `voice-story-${story.id}-${hash}.wav`);
    if (!fs.existsSync(wavPath)) {
      console.log(`Generating offline TTS WAV for voice story ${story.id}: ${wavPath}`);
      await synthesizeOfflineWav(text, wavPath);
    }

    res.setHeader('Content-Type', 'audio/wav');
    res.setHeader('Cache-Control', 'public, max-age=31536000, immutable');
    res.sendFile(wavPath);
  } catch (err) {
    console.error('Voice story TTS failed:', err);
    res.status(500).json({ error: 'Offline TTS failed: ' + err.message });
  }
});

// API: Get kid songs (paginated MP3 list)
app.get('/api/kid-songs', async (req, res) => {
  try {
    const page = Math.max(1, parseInt(req.query.page) || 1);
    const perPage = Math.min(50, Math.max(1, parseInt(req.query.perPage) || 10));
    const offset = (page - 1) * perPage;
    const countRes = await pool.query('SELECT COUNT(*)::int FROM kid_songs');
    const count = countRes.rows[0].count;
    const { rows } = await pool.query(
      'SELECT id, title, filename, audio_url, source, sort_order, created_at FROM kid_songs ORDER BY sort_order ASC, id ASC LIMIT $1 OFFSET $2',
      [perPage, offset]
    );
    res.json({ items: rows, total: count, page, perPage });
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

// API: Get single kid song
app.get('/api/kid-songs/:id', async (req, res) => {
  try {
    const { rows } = await pool.query('SELECT * FROM kid_songs WHERE id = $1', [req.params.id]);
    if (rows.length === 0) return res.status(404).json({ error: 'Kid song not found' });
    res.json(rows[0]);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

// API: Get ancient poems (paginated)
app.get('/api/poems', async (req, res) => {
  try {
    const page = Math.max(1, parseInt(req.query.page) || 1);
    const perPage = Math.min(50, Math.max(1, parseInt(req.query.perPage) || 10));
    const offset = (page - 1) * perPage;
    const countRes = await pool.query('SELECT COUNT(*)::int FROM poems');
    const count = countRes.rows[0].count;
    const { rows } = await pool.query(
      'SELECT id, title, author, dynasty, created_at FROM poems ORDER BY id ASC LIMIT $1 OFFSET $2',
      [perPage, offset]
    );
    res.json({ items: rows, total: count, page, perPage });
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

// API: Get image learning items (paginated)
// ?page=1&perPage=12 for paginated grid; or ?all=1 for game (all items)
app.get('/api/image-learn', async (req, res) => {
  try {
    const all = req.query.all === '1';
    if (all) {
      const { rows } = await pool.query(
        `SELECT id, word, pinyin, emoji, image_url, category, age_min, age_max
         FROM image_learn_items
         ORDER BY sort_order ASC, id ASC
         LIMIT 500`
      );
      return res.json({ items: rows, total: rows.length, page: 1, perPage: rows.length });
    }
    const page = Math.max(1, parseInt(req.query.page) || 1);
    const perPage = Math.min(50, Math.max(1, parseInt(req.query.perPage) || 12));
    const offset = (page - 1) * perPage;
    const countRes = await pool.query('SELECT COUNT(*)::int FROM image_learn_items');
    const count = countRes.rows[0].count;
    const { rows } = await pool.query(
      `SELECT id, word, pinyin, emoji, image_url, category, age_min, age_max
       FROM image_learn_items
       ORDER BY sort_order ASC, id ASC
       LIMIT $1 OFFSET $2`,
      [perPage, offset]
    );
    res.json({ items: rows, total: count, page, perPage });
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

// API: Get single poem with content
app.get('/api/poems/:id', async (req, res) => {
  try {
    const { rows } = await pool.query('SELECT * FROM poems WHERE id = $1', [req.params.id]);
    if (rows.length === 0) return res.status(404).json({ error: 'Poem not found' });
    res.json(rows[0]);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

function startServer() {
  // Always start HTTP server
  http.createServer(app).listen(PORT, () => {
    console.log(`Ebook API running on http://localhost:${PORT}`);
  });

  // Optionally also start HTTPS server if certificates exist
  if (fs.existsSync(SSL_KEY) && fs.existsSync(SSL_CERT)) {
    const credentials = {
      key: fs.readFileSync(SSL_KEY),
      cert: fs.readFileSync(SSL_CERT),
    };
    const HTTPS_PORT = parseInt(PORT) + 1; // HTTPS on port 3002 by default
    https.createServer(credentials, app).listen(HTTPS_PORT, () => {
      console.log(`Ebook API running on https://localhost:${HTTPS_PORT}`);
    });
  }
}

startServer();
