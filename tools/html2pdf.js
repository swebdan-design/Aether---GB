// AETHER - by Synth Centric
// Renders the pandoc HTML manual to PDF with headless Chromium (Playwright).
// usage: node tools/html2pdf.js input.html output.pdf
const { chromium } = require('playwright');
(async () => {
  const [inp, out] = process.argv.slice(2);
  const browser = await chromium.launch();
  const page = await browser.newPage();
  await page.goto('file://' + require('path').resolve(inp), { waitUntil: 'load' });
  await page.pdf({
    path: out, format: 'A4', printBackground: true,
    margin: { top: '18mm', bottom: '18mm', left: '16mm', right: '16mm' },
    displayHeaderFooter: true,
    headerTemplate: '<div></div>',
    footerTemplate: '<div style="font-size:8px;width:100%;text-align:center;color:#666">AETHER — by Synth Centric · <span class="pageNumber"></span></div>'
  });
  await browser.close();
})();
