const { Pool } = require('pg');
const pool = new Pool({ host: '127.0.0.1', port: 5432, user: 'ebook', password: 'ebook123', database: 'ebook' });

// The 100 story titles from 3.pdf, in order
const pdfStories = [
  '彼得兔的故事', '三只小猪', '小红帽', '灰姑娘', '白雪公主', '睡美人', '青蛙王子',
  '杰克与豆茎', '绿野仙踪', '爱丽丝梦游仙境', '拇指姑娘', '丑小鸭', '海的女儿',
  '卖火柴的小女孩', '坚定的锡兵', '夜莺', '皇帝的新衣', '狐狸和乌鸦', '龟兔赛跑',
  '农夫与蛇', '狼来了', '三只山羊嘎啦嘎啦', '金发姑娘和三只熊', '木偶奇遇记',
  '小王子', '秘密花园', '小熊维尼', '彼得·潘', '金银岛', '罗宾汉', '好饿的毛毛虫',
  '小黑羊', '小兔子乖乖', '小猫咪上学', '小熊刷牙', '我会自己吃饭', '我会自己穿衣',
  '晚安小星星', '下雨啦', '雪孩子', '小蜗牛的慢旅行', '勇敢的小刺猬',
  '好朋友手拉手', '分享最快乐', '道歉不可怕', '耐心等一等', '爱护小花小草',
  '节约用水', '不乱扔垃圾', '交通小卫士', '我的情绪小怪兽', '我喜欢我自己',
  '最棒的妈妈', '爸爸的拥抱', '爷爷奶奶的爱', '幼儿园的一天', '我爱上幼儿园',
  '第一次独自睡觉', '我不怕黑', '生病了不怕', '小松鼠的大尾巴', '小蜜蜂爱劳动',
  '小蚂蚁搬大米', '小蝴蝶的蜕变', '小蜻蜓捉蚊子', '小青蛙保庄稼', '小鸭子学游泳',
  '小鸡找妈妈', '小鸟学飞', '小狐狸的智慧', '大灰狼的坏主意', '狐假虎威',
  '狮子和老鼠', '大象和小老鼠', '骆驼和小羊', '猴子捞月亮', '乌鸦喝水',
  '孔雀的尾巴', '啄木鸟医生', '猫头鹰的夜晚', '司马光砸缸', '孔融让梨',
  '曹冲称象', '井底之蛙', '亡羊补牢', '守株待兔', '画蛇添足',
  '对牛弹琴', '刻舟求剑', '闻鸡起舞', '悬梁刺股', '女娲补天', '夸父追日',
  '后羿射日', '嫦娥奔月', '牛郎织女', '十二生肖的故事', '年兽的传说', '田螺姑娘'
];

// 200-word summaries for each story
const summaries = {
  '小王子': '在遥远的B612星球上，住着一位孤独的小王子。他深爱着一朵骄傲的玫瑰花，每天为她浇水、挡风，却因不懂如何爱而选择离开。小王子游历了六颗星球，遇见了专制的国王、虚荣的人、颓废的酒鬼、贪婪的商人、忙碌的点灯人和纸上谈兵的地理学家，这些大人让他感到困惑。最后他来到地球，在沙漠中遇见了一只聪明的狐狸。狐狸教他"驯养"的意义：建立联系，成为彼此独一无二的存在。狐狸告诉他一个秘密："真正重要的东西，用眼睛是看不见的，要用心去感受。"小王子终于明白，自己星球上那朵骄傲的玫瑰，正是他用心浇灌、独一无二的爱。他回到了玫瑰身边，学会了珍惜、责任和陪伴。这个故事告诉我们：爱就是责任和珍惜，最珍贵的东西需要用心去看、用心守护。'
};

async function main() {
  const c = await pool.connect();
  try {
    // 1. Check which voice_stories already exist
    const vsTitles = (await c.query(`SELECT title FROM voice_stories`)).rows.map(r => r.title);
    const vsSet = new Set(vsTitles);

    const missingFromVS = pdfStories.filter(t => !vsSet.has(t));
    console.log(`\n=== Voice Stories Check ===`);
    console.log(`PDF stories: ${pdfStories.length}, in voice_stories: ${pdfStories.length - missingFromVS.length}, missing: ${missingFromVS.length}`);
    if (missingFromVS.length > 0) {
      console.log('Missing from voice_stories:', missingFromVS);
    } else {
      console.log('All 100 stories already in voice_stories ✓');
    }

    // 2. Check which books already exist
    const bkTitles = (await c.query(`SELECT id, title FROM books ORDER BY id`)).rows;
    const bkSet = new Set(bkTitles.map(r => r.title));
    
    const missingFromBooks = pdfStories.filter(t => !bkSet.has(t));
    console.log(`\n=== Books Check ===`);
    console.log(`PDF stories: ${pdfStories.length}, in books: ${pdfStories.length - missingFromBooks.length}, missing: ${missingFromBooks.length}`);
    
    if (missingFromBooks.length > 0) {
      console.log('Missing from books:', missingFromBooks);
    }

    // 3. Insert missing voice_stories
    for (const title of missingFromVS) {
      // We don't have content for these in the current setup, skip for now
      console.log(`  Would insert voice_story: ${title} (need content)`);
    }

    // 4. For the books, we need to generate descriptions for missing ones
    // We already have summary for 小王子
    for (const title of missingFromBooks) {
      const desc = summaries[title];
      if (desc) {
        // Check if already exists by title
        const exists = await c.query(`SELECT id FROM books WHERE title = $1`, [title]);
        if (exists.rowCount === 0) {
          await c.query(
            `INSERT INTO books (title, author, description, category, cover_url) VALUES ($1, $2, $3, $4, $5)`,
            [title, '未知', desc, '儿童绘本', '/img/covers/' + title + '.jpg']
          );
          console.log(`INSERTED book: ${title}`);
        } else {
          console.log(`SKIP (exists): ${title}`);
        }
      } else {
        console.log(`  NEED_SUMMARY: ${title}`);
      }
    }

    // Show final counts
    const finalBk = await c.query(`SELECT COUNT(*) as cnt FROM books`);
    const finalVS = await c.query(`SELECT COUNT(*) as cnt FROM voice_stories`);
    console.log(`\n=== Final Counts ===`);
    console.log(`Books: ${finalBk.rows[0].cnt}`);
    console.log(`Voice Stories: ${finalVS.rows[0].cnt}`);

  } finally { c.release(); pool.end(); }
}

main().catch(e => { console.error(e); process.exit(1); });