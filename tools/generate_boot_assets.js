'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const { execFileSync } = require('child_process');

const ROOT = path.resolve(__dirname, '..');
const W = 320;
const H = 240;
const FPS = 30;
const VIDEO = process.env.GODSPEED_VIDEO || path.join(os.homedir(), 'Downloads', 'Godspeed Edited.mp4');
const LOGO = process.env.GODSPEED_LOGO || path.join(ROOT, 'LOGO.png');
const OUT_CPP = path.join(ROOT, 'src', 'boot', 'boot_assets.cpp');
const OUT_PNG = path.join(ROOT, 'assets', 'godspeed_logo_320.png');

function run(tool, args) {
    execFileSync(tool, ['-hide_banner', '-loglevel', 'error', '-y', ...args], {
        stdio: ['ignore', 'pipe', 'pipe'],
    });
}

function probeSize(file) {
    const out = execFileSync('ffprobe', [
        '-v', 'error', '-select_streams', 'v:0',
        '-show_entries', 'stream=width,height',
        '-of', 'csv=s=x:p=0', file,
    ], { encoding: 'utf8' }).trim();
    const [w, h] = out.split('x').map(Number);
    if (!w || !h) throw new Error('cannot read video size: ' + out);
    return { w, h };
}

function toRgb565(r, g, b) {
    return ((r & 0xf8) << 8) | ((g & 0xfc) << 3) | (b >> 3);
}

function rgb24To565(buf) {
    const out = new Uint16Array(W * H);
    for (let i = 0, p = 0; i < out.length; i++, p += 3) {
        out[i] = toRgb565(buf[p], buf[p + 1], buf[p + 2]);
    }
    return out;
}

function push16(out, value) {
    out.push(value & 0xff, (value >> 8) & 0xff);
}

function encodeAgainst(pixels, prev, allowRle = true) {
    const out = [];
    const total = pixels.length;
    let i = 0;
    while (i < total) {
        if (pixels[i] === prev[i]) {
            let skip = 0;
            while (i < total && pixels[i] === prev[i]) { ++i; ++skip; }
            while (skip > 0) {
                const n = Math.min(skip, 0xffff);
                push16(out, n);
                push16(out, 0);
                skip -= n;
            }
            continue;
        }

        let runStart = i;
        while (i < total && pixels[i] !== prev[i]) ++i;
        let run = i - runStart;
        while (run > 0) {
            const n = Math.min(run, 0x7fff);
            let same = true;
            const first = pixels[runStart];
            for (let k = 1; k < n; ++k) {
                if (pixels[runStart + k] !== first) { same = false; break; }
            }
            push16(out, 0);
            if (same && allowRle) {
                push16(out, 0x8000 | n);
                push16(out, first);
            } else {
                push16(out, n);
                for (let k = 0; k < n; ++k) push16(out, pixels[runStart + k]);
            }
            runStart += n;
            run -= n;
        }
    }
    push16(out, 0);
    push16(out, 0);
    return Uint8Array.from(out);
}

function decodeInto(out, data) {
    let pos = 0;
    let index = 0;
    while (pos + 4 <= data.length) {
        const skip = data[pos] | (data[pos + 1] << 8);
        const run = data[pos + 2] | (data[pos + 3] << 8);
        pos += 4;
        if (skip === 0 && run === 0) break;
        index += skip;
        if (run === 0) continue;
        const rle = (run & 0x8000) !== 0;
        const count = run & 0x7fff;
        if (rle) {
            const value = data[pos] | (data[pos + 1] << 8);
            pos += 2;
            for (let i = 0; i < count; ++i) out[index++] = value;
        } else {
            for (let i = 0; i < count; ++i) {
                out[index++] = data[pos] | (data[pos + 1] << 8);
                pos += 2;
            }
        }
    }
    if (index > out.length) {
        throw new Error(`decoded past frame index=${index} len=${out.length} pos=${pos} data=${data.length}`);
    }
}

function arrayToWords(bytes) {
    const words = [];
    for (let i = 0; i < bytes.length; i += 4) {
        const b0 = bytes[i] || 0;
        const b1 = bytes[i + 1] || 0;
        const b2 = bytes[i + 2] || 0;
        const b3 = bytes[i + 3] || 0;
        words.push(((b3 << 24) | (b2 << 16) | (b1 << 8) | b0) >>> 0);
    }
    return words;
}

function emitWords(name, words) {
    const lines = [];
    lines.push(`alignas(4) const uint32_t ${name}[] = {`);
    for (let i = 0; i < words.length; i += 16) {
        lines.push('    ' + words.slice(i, i + 16).map(v => `0x${v.toString(16).padStart(8, '0')}u`).join(', ') + ',');
    }
    lines.push('};');
    return lines.join('\n');
}

const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'godspeed-boot-'));
const framesRaw = path.join(tmp, 'frames.rgb');
const logoRaw = path.join(tmp, 'logo.rgb');
const logoScaledRaw = path.join(tmp, 'logo320.rgb');

const videoSize = probeSize(VIDEO);
run('ffmpeg', ['-i', VIDEO, '-vf', `fps=${FPS},scale=${W}:${H}:flags=lanczos`, '-pix_fmt', 'rgb24', '-f', 'rawvideo', framesRaw]);
const raw = fs.readFileSync(framesRaw);
const frameBytes = W * H * 3;
const frameCount = Math.floor(raw.length / frameBytes);
if (frameCount < 1) throw new Error('no frames extracted');

const unique = [];
const ticks = [];
for (let f = 0; f < frameCount; ++f) {
    const start = f * frameBytes;
    const frame = raw.subarray(start, start + frameBytes);
    const pixels = rgb24To565(frame);
    const prev = unique.length > 0 ? unique[unique.length - 1].pixels : null;
    let same = prev !== null;
    if (same) {
        for (let p = 0; p < pixels.length; ++p) {
            if (pixels[p] !== prev[p]) { same = false; break; }
        }
    }
    if (same) {
        ticks[ticks.length - 1]++;
    } else {
        unique.push({ pixels });
        ticks.push(1);
    }
}

const black = new Uint16Array(W * H);
const encoded = [];
let rawBytes = 0;
let fullRleBytes = 0;
let deltaLiteralBytes = 0;
let changedPixels = 0;
let bboxPixels = 0;
let maxBboxPixels = 0;
let minChanged = W * H;
let maxChanged = 0;
for (let i = 0; i < unique.length; ++i) {
    const prev = i === 0 ? black : unique[i - 1].pixels;
    const data = encodeAgainst(unique[i].pixels, prev);
    const literal = encodeAgainst(unique[i].pixels, prev, false);
    encoded.push(data);
    const full = encodeAgainst(unique[i].pixels, black);
    let changed = 0;
    let bx0 = W, by0 = H, bx1 = -1, by1 = -1;
    for (let p = 0; p < W * H; ++p) {
        if (unique[i].pixels[p] !== prev[p]) {
            changed++;
            const x = p % W, y = (p / W) | 0;
            if (x < bx0) bx0 = x;
            if (x > bx1) bx1 = x;
            if (y < by0) by0 = y;
            if (y > by1) by1 = y;
        }
    }
    if (bx1 >= bx0) { const area = (bx1 - bx0 + 1) * (by1 - by0 + 1); bboxPixels += area; maxBboxPixels = Math.max(maxBboxPixels, area); }
    changedPixels += changed;
    minChanged = Math.min(minChanged, changed);
    maxChanged = Math.max(maxChanged, changed);
    rawBytes += W * H * 2;
    fullRleBytes += full.length;
    deltaLiteralBytes += literal.length;

    const check = Uint16Array.from(prev);
    try { decodeInto(check, data); } catch (e) { console.error('frame', i, 'bytes', data.length); throw e; }
    for (let p = 0; p < W * H; ++p) {
        if (check[p] !== unique[i].pixels[p]) throw new Error(`decode mismatch frame ${i} pixel ${p}`);
    }
}

const logoSize = probeSize(LOGO);
run('ffmpeg', ['-i', LOGO, '-pix_fmt', 'rgb24', '-f', 'rawvideo', logoRaw]);
const logoRawData = fs.readFileSync(logoRaw);
let bx0 = logoSize.w, by0 = logoSize.h, bx1 = -1, by1 = -1;
for (let y = 0; y < logoSize.h; ++y) {
    for (let x = 0; x < logoSize.w; ++x) {
        const p = (y * logoSize.w + x) * 3;
        if (logoRawData[p] > 8 || logoRawData[p + 1] > 8 || logoRawData[p + 2] > 8) {
            if (x < bx0) bx0 = x;
            if (y < by0) by0 = y;
            if (x > bx1) bx1 = x;
            if (y > by1) by1 = y;
        }
    }
}
if (bx1 < bx0 || by1 < by0) throw new Error('logo has no visible content');
const bw = bx1 - bx0 + 1;
const bh = by1 - by0 + 1;
const logoW = 300;
const logoH = Math.max(1, Math.round(bh * logoW / bw));
run('ffmpeg', ['-i', LOGO, '-vf', `crop=${bw}:${bh}:${bx0}:${by0},scale=${logoW}:${logoH}:flags=lanczos,pad=${W}:${H}:${Math.floor((W - logoW) / 2)}:${Math.floor((H - logoH) / 2)}:black`, '-pix_fmt', 'rgb24', '-f', 'rawvideo', logoScaledRaw]);
run('ffmpeg', ['-i', LOGO, '-vf', `crop=${bw}:${bh}:${bx0}:${by0},scale=${logoW}:${logoH}:flags=lanczos,pad=${W}:${H}:${Math.floor((W - logoW) / 2)}:${Math.floor((H - logoH) / 2)}:black`, '-frames:v', '1', OUT_PNG]);
const logoPixels = rgb24To565(fs.readFileSync(logoScaledRaw));
const logoData = encodeAgainst(logoPixels, black);

const animBytes = Buffer.concat(encoded);
const logoBytes = Buffer.from(logoData);
const words = arrayToWords(animBytes);
const logoWords = arrayToWords(logoBytes);
const totalTicks = ticks.reduce((a, b) => a + b, 0);

const table = [];
let offset = 0;
for (let i = 0; i < encoded.length; ++i) {
    table.push(`    {${offset}u, ${encoded[i].length}u, ${ticks[i]}u, 0u},`);
    offset += encoded[i].length;
}

const cpp = [
    '#include "boot/boot_assets.h"',
    '',
    'namespace moto {',
    'namespace boot {',
    '',
    emitWords('kAnimationData', words),
    '',
    `const uint32_t kAnimationDataSize = ${animBytes.length}u;`,
    '',
    'const Frame kAnimationFrames[] = {',
    ...table,
    '};',
    '',
    `const uint16_t kAnimationFrameCount = ${encoded.length}u;`,
    `const uint16_t kAnimationTickCount = ${totalTicks}u;`,
    '',
    emitWords('kLogoData', logoWords),
    '',
    `const uint32_t kLogoDataSize = ${logoBytes.length}u;`,
    '',
    '} // namespace boot',
    '} // namespace moto',
    '',
].join('\n');

fs.writeFileSync(OUT_CPP, cpp);

const stats = {
    sourceVideo: videoSize,
    frames: frameCount,
    uniqueFrames: unique.length,
    ticks: totalTicks,
    sourceRawBytes: frameCount * W * H * 2,
    rawBytes,
    fullRleBytes,
    deltaRleBytes: animBytes.length,
    deltaLiteralBytes,
    avgDeltaPixels: Math.round(changedPixels / unique.length),
    avgDirtyBboxPixels: Math.round(bboxPixels / unique.length),
    maxDirtyBboxPixels: maxBboxPixels,
    minDeltaPixels: minChanged,
    maxDeltaPixels: maxChanged,
    logoSource: logoSize,
    logoBBox: { x: bx0, y: by0, w: bw, h: bh },
    logo320: { w: logoW, h: logoH, pngBytes: fs.statSync(OUT_PNG).size, rleBytes: logoBytes.length },
    cppBytes: Buffer.byteLength(cpp),
    output: OUT_CPP,
    logoOutput: OUT_PNG,
};
fs.writeFileSync(path.join(ROOT, 'assets', 'boot_stats.json'), JSON.stringify(stats, null, 2) + '\n');
console.log(JSON.stringify(stats, null, 2));
