const {Pool} = require('pg');
const p = new Pool({host:'127.0.0.1',port:5432,user:'ebook',password:'ebook123',database:'ebook'});
(async()=>{
  const r = await p.query("SELECT id, title FROM image_learn_items WHERE title = $1 OR title LIKE $2", ['目', '%看圖%']);
  console.log('Image learn items:', JSON.stringify(r.rows));
  const r2 = await p.query("SELECT id, title FROM image_learn_items WHERE title LIKE $1 OR title LIKE $2", ['%目%', '%中文%']);
  console.log('Items with 目 or 中文:', JSON.stringify(r2.rows));
  // Show first 5 items
  const r3 = await p.query("SELECT id, title, sort_order FROM image_learn_items ORDER BY sort_order LIMIT 10");
  console.log('First 10 items:', JSON.stringify(r3.rows));
  await p.end();
})();
