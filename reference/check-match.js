const { Pool } = require('pg');
const pool = new Pool({
  host: '127.0.0.1',
  port: 5432,
  user: 'ebook',
  password: 'ebook123',
  database: 'ebook',
});

async function main() {
  const c = await pool.connect();
  try {
    // Match books to voice_stories by title
    const { rows } = await c.query(`
      SELECT b.id as bid, b.title, b.category, LENGTH(b.description) as bdlen, LENGTH(b.content) as bclen,
             vs.id as vsid, LENGTH(vs.content) as vsclen
      FROM books b 
      LEFT JOIN voice_stories vs ON b.title = vs.title
      WHERE b.id >= 4 
      ORDER BY b.id
    `);
    
    let matched = 0, unmatched = 0;
    const unmatchedList = [];
    for (const r of rows) {
      if (r.vsid) {
        matched++;
        console.log(`MATCH ID${r.bid}: ${r.title} [${r.category}] | book=${r.bclen}c | voice=${r.vsclen}c`);
      } else {
        unmatched++;
        unmatchedList.push(`ID${r.bid}: ${r.title} [${r.category}]`);
      }
    }
    console.log(`\n=== Matched: ${matched}, Unmatched: ${unmatched} ===`);
    if (unmatchedList.length > 0) {
      console.log('\nUnmatched titles:');
      for (const t of unmatchedList) console.log('  ' + t);
    }

    // Also check: are there extra voice_stories with Grimm category not in books?
    const { rows: vsGrimm } = await c.query(`
      SELECT vs.id, vs.title, LENGTH(vs.content) as clen
      FROM voice_stories vs
      WHERE vs.category = '格林童话'
      AND vs.title IN (SELECT title FROM books WHERE category = '格林童话')
      ORDER BY vs.id
    `);
    console.log(`\nVoice stories matching book titles (格林童话): ${vsGrimm.length}`);
    for (const r of vsGrimm) {
      console.log(`  VS${r.id}: ${r.title} (${r.clen}c)`);
    }

  } finally {
    c.release();
    await pool.end();
  }
}
main().catch(console.error);