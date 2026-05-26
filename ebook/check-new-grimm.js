const { Pool } = require('pg');
const pool = new Pool({ host: '127.0.0.1', port: 5432, user: 'ebook', password: 'ebook123', database: 'ebook' });

async function main() {
  const c = await pool.connect();
  // Get stories where id is between 102 and 162
  const { rows } = await c.query(
    'SELECT id, title, LENGTH(content) as len, category FROM voice_stories WHERE id >= 102 AND id <= 165 ORDER BY len ASC'
  );
  console.log('Total in range: ' + rows.length);
  let short = 0, good = 0;
  for (const r of rows) {
    const mark = r.len < 800 ? 'SHORT' : 'OK';
    if (r.len < 800) short++; else good++;
    console.log(r.id + ': ' + r.title + ' (' + r.len + ' chars) [' + r.category + '] ' + mark);
  }
  console.log('\nShort: ' + short + ', Good: ' + good);
  c.release();
  await pool.end();
}
main().catch(e => console.error(e));