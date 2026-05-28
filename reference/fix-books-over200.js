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
    // Step 1: Find all stories over 200 chars
    const { rows: over200 } = await c.query(
      "SELECT id, title, description, LENGTH(content) as clen, LENGTH(description) as dlen FROM books WHERE LENGTH(content) > 200 ORDER BY id"
    );
    
    console.log(`Found ${over200.length} stories over 200 chars\n`);
    
    // Separate into removable (novels) and fixable (can be summarized)
    const novels = [];
    const fixable = [];
    
    for (const r of over200) {
      if (r.id <= 3) {
        // IDs 1-3 are full novel excerpts (三体, 活着, 小王子) - cannot be shortened
        novels.push(r);
      } else {
        // IDs 102-182 are expanded stories - can replace content with description
        fixable.push(r);
      }
    }
    
    console.log("=== NOVELS TO REMOVE (cannot be shortened meaningfully) ===");
    for (const r of novels) {
      console.log(`  ID ${r.id}: ${r.title} (${r.clen} chars)`);
    }
    
    console.log("\n=== STORIES TO REPLACE WITH SUMMARY ===");
    for (const r of fixable) {
      console.log(`  ID ${r.id}: ${r.title} (${r.clen}c -> ${r.dlen}c)`);
    }
    
    // Step 2: Remove novels
    if (novels.length > 0) {
      const ids = novels.map(r => r.id);
      await c.query(`DELETE FROM books WHERE id IN (${ids.join(',')})`);
      console.log(`\nRemoved ${novels.length} novels: ${novels.map(r => r.title).join(', ')}`);
    }
    
    // Step 3: Replace content with description for fixable stories
    let updated = 0;
    for (const r of fixable) {
      if (r.description && r.description.length <= 200) {
        await c.query('UPDATE books SET content = $1 WHERE id = $2', [r.description, r.id]);
        console.log(`  Updated ID ${r.id}: ${r.title} (${r.clen}c -> ${r.description.length}c)`);
        updated++;
      } else {
        console.log(`  SKIP ID ${r.id}: ${r.title} - no valid description (${r.dlen}c)`);
      }
    }
    
    console.log(`\nTotal: ${updated} stories updated to summary, ${novels.length} novels removed`);
    
    // Step 4: Verify results
    const { rows: remaining } = await c.query(
      "SELECT COUNT(*) as total, SUM(CASE WHEN LENGTH(content) > 200 THEN 1 ELSE 0 END) as still_over FROM books"
    );
    console.log(`\nRemaining books: ${remaining[0].total}`);
    console.log(`Still over 200 chars: ${remaining[0].still_over}`);
    
    // Show all remaining stories with their lengths
    const { rows: all } = await c.query("SELECT id, title, LENGTH(content) as clen FROM books ORDER BY id");
    console.log("\n=== Final state ===");
    for (const r of all) {
      const mark = r.clen <= 200 ? 'OK' : 'OVER';
      console.log(`  ID ${r.id}: ${r.title} (${r.clen}c) ${mark}`);
    }
    
  } finally {
    c.release();
    await pool.end();
  }
}

main().catch(console.error);