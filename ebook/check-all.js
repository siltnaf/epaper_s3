const { Pool } = require('pg');
const pool = new Pool({ host: '127.0.0.1', port: 5432, user: 'ebook', password: 'ebook123', database: 'ebook' });

async function main() {
  const c = await pool.connect();
  
  // Check for specific new Grimm titles
  const titles = ['钉子','白雪与红玫瑰','老苏丹','小精灵与鞋匠','费切尔的怪鸟','夏娃的各样孩子','池中女妖','纺锤、梭子和针','爱人罗兰','鼓手'];
  for (const t of titles) {
    const { rows } = await c.query('SELECT id, title, LENGTH(content) as len, category FROM voice_stories WHERE title = $1', [t]);
    if (rows.length > 0) {
      console.log(rows[0].id + ': ' + rows[0].title + ' (' + rows[0].len + ' chars) [' + rows[0].category + ']');
    } else {
      console.log('NOT FOUND: ' + t);
    }
  }
  
  // Check distinct categories
  const { rows: cats } = await c.query('SELECT DISTINCT category, COUNT(*) FROM voice_stories GROUP BY category ORDER BY count DESC');
  console.log('\nCategories:');
  for (const r of cats) console.log('  ' + r.category + ': ' + r.count);
  
  // Total stories
  const { rows: total } = await c.query('SELECT COUNT(*) as total FROM voice_stories');
  console.log('\nTotal stories: ' + total[0].total);
  
  // Check stories around ID 102
  const { rows: around } = await c.query('SELECT id, title, category FROM voice_stories WHERE id BETWEEN 95 AND 110 ORDER BY id');
  console.log('\nStories ID 95-110:');
  for (const r of around) console.log('  ' + r.id + ': ' + r.title + ' [' + r.category + ']');
  
  c.release();
  await pool.end();
}
main().catch(e => console.error(e));