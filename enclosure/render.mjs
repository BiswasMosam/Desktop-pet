// node render.mjs [views...]  ->  renders/<view>.png
// Serves this folder, opens render.html in headless Chrome and screenshots it.
// puppeteer-core comes from the copy in D:/Garage/Fill.ai/node_modules.
import puppeteer from 'file:///D:/Garage/Fill.ai/node_modules/puppeteer-core/lib/puppeteer/puppeteer-core.js';
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';

const views = process.argv.slice(2).length ? process.argv.slice(2) : ['hero', 'back', 'cutaway', 'exploded', 'tray', 'tray-board', 'side', 'reader'];
const types = { '.html': 'text/html', '.glb': 'model/gltf-binary', '.png': 'image/png', '.js': 'text/javascript' };
const server = http.createServer((req, res) => {
  const file = path.join(process.cwd(), decodeURIComponent(req.url.split(/[?#]/)[0]));
  if (!fs.existsSync(file)) { res.statusCode = 404; return res.end(); }
  res.setHeader('content-type', types[path.extname(file)] || 'application/octet-stream');
  fs.createReadStream(file).pipe(res);
}).listen(0);
const port = server.address().port;

const browser = await puppeteer.launch({
  executablePath: 'C:/Program Files/Google/Chrome/Application/chrome.exe',
  headless: true, args: ['--use-angle=swiftshader', '--enable-unsafe-swiftshader'],
});
const page = await browser.newPage();
page.on('console', m => m.type() === 'error' && console.log('page:', m.text()));
page.on('pageerror', e => console.log('page error:', e.message));
await page.setViewport({ width: 1600, height: 1200 });
for (const v of views) {
  await page.goto(`http://127.0.0.1:${port}/render.html#${v}`, { waitUntil: 'load' });
  await page.reload({ waitUntil: 'load' });           // a hash change alone doesn't re-run the module
  await page.waitForFunction('window.done === true', { timeout: 120000 });
  await page.screenshot({ path: `renders/${v}.png` });
  console.log('rendered', v);
}
await browser.close();
server.close();
