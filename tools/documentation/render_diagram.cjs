// Original manual illustration: reproducible raster export for HTML and PDF.
// Requires sharp; run from any directory. No external media is loaded.
const path = require('node:path');
const fs = require('node:fs/promises');
const sharp = require('sharp');
const root = path.resolve(__dirname, '../..');
const svg = path.join(root, 'docs/user/images/analysis-route.svg');
const png = path.join(root, 'docs/user/images/analysis-route.png');
(async () => {
  const bytes = await sharp(svg, { density: 144 }).png().toBuffer();
  if (process.argv.includes('--check')) {
    if (!bytes.equals(await fs.readFile(png))) throw new Error('Manual diagram export is stale');
    console.log('PASS: manual diagram matches its SVG master');
  } else {
    await fs.writeFile(png, bytes);
    console.log('Exported manual diagram');
  }
})().catch(error => { console.error(error.message); process.exitCode = 1; });
