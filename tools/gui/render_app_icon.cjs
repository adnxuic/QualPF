#!/usr/bin/env node
'use strict';

// Technical size/format exports of the generated master; no artwork is redrawn.
const fs = require('node:fs/promises');
const path = require('node:path');
const sharp = require('sharp');
const root = path.resolve(__dirname, '../..');
const branding = path.join(root, 'src/gui/resources/branding');
const args = process.argv.slice(2);
let check = false;
let preview;
for (let i = 0; i < args.length; ++i) {
    if (args[i] === '--check') check = true;
    else if (args[i] === '--preview' && args[i + 1]) preview = path.resolve(args[++i]);
    else throw new Error('Usage: render_app_icon.cjs [--check] [--preview OUTPUT_DIRECTORY]');
}

async function main() {
    const master = await fs.readFile(path.join(branding, 'qualpf-master.png'));
    const metadata = await sharp(master).metadata();
    if (metadata.width !== metadata.height || metadata.width < 512 || !metadata.hasAlpha) {
        throw new Error('Expected a square master of at least 512px with genuine transparency');
    }
    const { channels } = await sharp(master).stats();
    if (channels[3].min !== 0 || channels[3].max !== 255) throw new Error('Missing transparent exterior or opaque tile');
    const images = new Map();
    async function png(size) {
        if (!images.has(size)) images.set(size, await sharp(master).resize(size, size)
            .png({ compressionLevel: 9, adaptiveFiltering: false }).toBuffer());
        return images.get(size);
    }
    async function output(relative, bytes) {
        const destination = path.join(root, relative);
        if (check) {
            if (!(await fs.readFile(destination)).equals(bytes)) throw new Error(`Out-of-date icon: ${relative}`);
        } else {
            await fs.mkdir(path.dirname(destination), { recursive: true });
            await fs.writeFile(destination, bytes);
        }
    }
    await output('src/gui/resources/branding/qualpf.png', await png(512));
    for (const size of [16, 32, 48, 256]) {
        await output(`deploy/linux/icons/${size}x${size}/qualpf.png`, await png(size));
    }
    await output('docs/user/images/qualpf.png', await png(256));
    await output('docs/user/images/qualpf-favicon.png', await png(32));

    // ICO directory with PNG-compressed 32-bit RGBA frames, supported by modern Windows.
    const sizes = [16, 24, 32, 48, 64, 128, 256];
    const header = Buffer.alloc(6 + sizes.length * 16);
    header.writeUInt16LE(1, 2);
    header.writeUInt16LE(sizes.length, 4);
    const frames = [];
    let offset = header.length;
    for (const [i, size] of sizes.entries()) {
        const bytes = await png(size);
        const entry = 6 + 16 * i;
        header[entry] = header[entry + 1] = size === 256 ? 0 : size;
        header.writeUInt16LE(1, entry + 4);
        header.writeUInt16LE(32, entry + 6);
        header.writeUInt32LE(bytes.length, entry + 8);
        header.writeUInt32LE(offset, entry + 12);
        offset += bytes.length;
        frames.push(bytes);
    }
    await output('src/gui/resources/branding/qualpf.ico', Buffer.concat([header, ...frames]));
    if (preview) {
        await fs.mkdir(preview, { recursive: true });
        for (const dark of [false, true]) {
            const foreground = dark ? '#E4EBF2' : '#29445F';
            let sheet = `<svg xmlns="http://www.w3.org/2000/svg" width="1056" height="400">
<rect width="100%" height="100%" fill="${dark ? '#25313D' : '#F0F3F7'}"/>
<g font-family="Segoe UI,Arial,sans-serif" fill="${foreground}">
<text x="28" y="38" font-size="24">QualPF · Discrete line to continuous profile</text>
<text x="28" y="64" font-size="14">PFSM application icon · original generated artwork · native pixel sizes</text>`;
            let x = 24;
            for (const size of [16, 24, 32, 48, 64, 128, 256]) {
                const y = 100 + 256 - size;
                sheet += `<image x="${x}" y="${y}" width="${size}" height="${size}" href="data:image/png;base64,${(await png(size)).toString('base64')}"/>
<text x="${x + size / 2}" y="382" text-anchor="middle" font-size="13">${size}px</text>`;
                x += size + 55;
            }
            sheet += '</g></svg>';
            await sharp(Buffer.from(sheet)).png().toFile(path.join(preview, dark ? 'sizes-dark.png' : 'sizes.png'));
        }
    }
    console.log(`${check ? 'Verified' : 'Exported'} application PNGs, seven Windows ICO sizes, Linux desktop and documentation assets`);
}

main().catch(error => { console.error(error.message); process.exitCode = 1; });
