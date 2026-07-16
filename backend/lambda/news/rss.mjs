// Fetch and parse the AWS "What's New" RSS feed. No dependencies:
// the feed is simple enough that two regexes do the job honestly.
const FEED_URL = 'https://aws.amazon.com/about-aws/whats-new/recent/feed/';

export async function fetchNews(limit = 10) {
  const res = await fetch(FEED_URL);
  if (!res.ok) throw new Error(`RSS fetch failed: ${res.status}`);
  const xml = await res.text();

  const items = [];
  const itemRe = /<item>([\s\S]*?)<\/item>/g;
  let m;
  while ((m = itemRe.exec(xml)) !== null && items.length < limit) {
    const title = pick(m[1], 'title');
    const link = pick(m[1], 'link');
    if (title) items.push({ title, link });
  }
  return items;
}

function pick(block, tag) {
  const m = block.match(new RegExp(`<${tag}>(?:<!\\[CDATA\\[)?([\\s\\S]*?)(?:\\]\\]>)?</${tag}>`));
  return m ? m[1].trim() : null;
}
