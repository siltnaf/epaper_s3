const { Pool } = require('pg');
const poems = require('./poems-data');

const pool = new Pool({
  host: '127.0.0.1',
  port: 5432,
  user: 'ebook',
  password: 'ebook123',
  database: 'ebook',
});

async function seed() {
  const client = await pool.connect();
  try {
    for (const poem of poems) {
      await client.query(
        `INSERT INTO poems (id, title, author, dynasty, content, description, created_at)
         VALUES ($1, $2, $3, $4, $5, $6, NOW())
         ON CONFLICT (id) DO NOTHING`,
        [poem.id, poem.title, poem.author, poem.dynasty, poem.content, poem.description]
      );
      console.log(`✅ Inserted: ${poem.id} - ${poem.title}`);
    }
    console.log('\n🎉 All poems seeded successfully!');
  } catch (err) {
    console.error('❌ Seeding failed:', err.message);
  } finally {
    client.release();
    await pool.end();
  }
}

seed();