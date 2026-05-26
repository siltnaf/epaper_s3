const { Pool } = require("pg");
const pool = new Pool({ host: "127.0.0.1", port: 5432, user: "ebook", password: "ebook123", database: "ebook" });
(async () => {
  const tables = await pool.query("SELECT tablename FROM pg_catalog.pg_tables WHERE schemaname='public'");
  console.log("Tables:", JSON.stringify(tables.rows.map(r => r.tablename)));

  const count = await pool.query("SELECT COUNT(*)::int AS cnt FROM image_learn_items");
  console.log("image_learn_items count:", count.rows[0].cnt);

  const items = await pool.query("SELECT word, source, sort_order FROM image_learn_items ORDER BY sort_order");
  console.log("Items:", JSON.stringify(items.rows));
  await pool.end();
})().catch(e => { console.error(e.stack || e.message); process.exit(1); });
