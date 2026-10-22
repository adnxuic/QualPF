#!/usr/bin/env node
'use strict';

// Offline authoring helper only. The product builds from committed PNG resources.
const fs = require('node:fs/promises');
const path = require('node:path');
const sharp = require('sharp');

const root = path.resolve(__dirname, '../..');
const icons = path.join(root, 'src/gui/resources/icons');
const args = process.argv.slice(2);
let check = false;
let preview;
for (let i = 0; i < args.length; ++i) {
    if (args[i] === '--check') check = true;
    else if (args[i] === '--preview' && args[i + 1]) preview = path.resolve(args[++i]);
    else throw new Error('Usage: render_icons.cjs [--check] [--preview OUTPUT_DIRECTORY]');
}

async function main() {
    const files = (await fs.readdir(icons)).filter(file => file.endsWith('.svg')).sort();
    if (!files.length) throw new Error('No SVG masters found');
    const qrc = await fs.readFile(path.join(icons, '../resource.qrc'), 'utf8');
    const masters = [];
    for (const file of files) {
        const name = path.basename(file, '.svg');
        const svg = await fs.readFile(path.join(icons, file), 'utf8');
        if (!svg.includes('viewBox="0 0 24 24"') || /<(?:text|image|script)\b|\bhref\s*=/i.test(svg)) {
            throw new Error(`${file}: expected a self-contained 24-unit vector without fonts or external content`);
        }
        for (const scale of [1, 2]) {
            const png = `${name}${scale === 2 ? '@2x' : ''}.png`;
            const destination = path.join(icons, png);
            if (check) {
                const meta = await sharp(destination).metadata();
                if (meta.format !== 'png' || meta.width !== 24 * scale || meta.height !== 24 * scale || !meta.hasAlpha) {
                    throw new Error(`${png}: expected a transparent ${24 * scale}px PNG`);
                }
                const { data } = await sharp(destination).ensureAlpha().raw().toBuffer({ resolveWithObject: true });
                let visible = false;
                let transparent = false;
                for (let n = 3; n < data.length; n += 4) {
                    visible ||= data[n] > 0;
                    transparent ||= data[n] === 0;
                }
                if (!visible || !transparent) throw new Error(`${png}: empty artwork or missing transparent background`);
                if (!qrc.includes(`<file alias="${png}">icons/${png}</file>`)) {
                    throw new Error(`${png}: not registered in resource.qrc`);
                }
            } else {
                await sharp(Buffer.from(svg), { density: 96 * scale })
                    .resize(24 * scale, 24 * scale)
                    .png({ compressionLevel: 9, adaptiveFiltering: false }).toFile(destination);
            }
        }
        masters.push({ name, svg });
    }
    if (preview) {
        await fs.mkdir(preview, { recursive: true });
        for (const dark of [false, true]) {
            const width = 1280;
            const height = 76 + Math.ceil(masters.length / 4) * 100;
            const fg = dark ? '#E4EBF2' : '#29445F';
            let sheet = `<svg xmlns="http://www.w3.org/2000/svg" width="${width}" height="${height}">
<rect width="100%" height="100%" fill="${dark ? '#222B35' : '#EDF1F5'}"/>
<g font-family="Segoe UI,Arial,sans-serif" fill="${fg}">
<text x="24" y="30" font-size="22" font-weight="600">QualPF action icons</text>
<text x="24" y="54" font-size="13">Original SVG artwork · 24 / 36 / 48 px · ${masters.length} icons</text>`;
            masters.forEach(({ name, svg }, i) => {
                const x = 16 + (i % 4) * 316;
                const y = 76 + Math.floor(i / 4) * 100;
                const title = svg.match(/<title>(.*?)<\/title>/s)[1];
                const body = svg.replace(/<svg[^>]*>|<\/svg>|<title>.*?<\/title>/gs, '');
                sheet += `<rect x="${x}" y="${y}" width="304" height="88" rx="6" fill="${dark ? '#303C49' : '#FFFFFF'}"/>
<text x="${x + 12}" y="${y + 20}" font-size="12">${title}</text>`;
                for (const [dx, size] of [[12, 24], [66, 36], [132, 48]]) {
                    sheet += `<g transform="translate(${x + dx} ${y + 30}) scale(${size / 24})" fill="none" stroke-linecap="round" stroke-linejoin="round">${body}</g>`;
                }
                sheet += `<text x="${x + 194}" y="${y + 52}" font-size="10" fill="${dark ? '#B5C4D3' : '#687B8D'}">24 · 36 · 48</text>`;
            });
            sheet += '</g></svg>';
            await sharp(Buffer.from(sheet)).png().toFile(path.join(preview, dark ? 'overview-dark.png' : 'overview.png'));
        }
    }
    console.log(`${check ? 'Checked' : 'Exported'} ${files.length} icons, ${files.length * 2} PNG resources${preview ? '; previews saved' : ''}`);
}

main().catch(error => { console.error(error.message); process.exitCode = 1; });
