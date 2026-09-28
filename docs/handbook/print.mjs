// node print.mjs <in.html> <out.pdf>
import puppeteer from 'file:///D:/Garage/Fill.ai/node_modules/puppeteer-core/lib/puppeteer/puppeteer-core.js';
import { pathToFileURL } from 'node:url';
import path from 'node:path';
const [,, inp, out] = process.argv;
const browser = await puppeteer.launch({
  executablePath: 'C:/Program Files/Google/Chrome/Application/chrome.exe',
  headless: true, args: ['--allow-file-access-from-files'],
});
const page = await browser.newPage();
await page.goto(pathToFileURL(path.resolve(inp)).href, { waitUntil: 'load' });
await page.evaluate(() => document.fonts.ready);
const fonts = await page.evaluate(() => [...document.fonts].map(f => `${f.family} ${f.weight} ${f.status}`));
console.log(fonts.join('\n'));
await page.pdf({ path: path.resolve(out), preferCSSPageSize: true, printBackground: true });
await browser.close();
console.log('printed', out);
