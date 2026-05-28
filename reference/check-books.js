const { Pool } = require('pg');
const pool = new Pool({ host: '127.0.0.1', port: 5432, user: 'ebook', password: 'ebook123', database: 'ebook' });

async function main() {
  const c = await pool.connect();
  
  const { rows: tables } = await c.query("SELECT table_name FROM information_schema.tables WHERE table_schema = 'public'");
  console.log('Tables:', tables.map(r => r.table_name));
  
  // Check books table
  const { rows } = await c.query("SELECT id, title, LENGTH(content) as len, category FROM books WHERE category = '格林童话' ORDER BY id");
  console.log('\nBooks table - Grimm stories: ' + rows.length);
  let short = 0, good = 0;
  for (const r of rows) {
    const mark = r.len < 800 ? 'SHORT' : 'OK';
    if (r.len < 800) short++; else good++;
    console.log(r.id + ': ' + r.title + ' (' + r.len + ' chars) ' + mark);
  }
  console.log('Short: ' + short + ', Good: ' + good);
  
  // Total in books
  const { rows: total } = await c.query('SELECT COUNT(*) as total FROM books');
  console.log('\nTotal books rows: ' + total[0].total);
  
  c.release();
  await pool.end();
}
main().catch(e => console.error(e));