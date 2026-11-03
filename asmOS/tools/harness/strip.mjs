// strip.mjs out.png in1.png in2.png ... : puts PNGs (8-bit RGB, same height) side by side
import { readFileSync, writeFileSync } from 'node:fs'
import { inflateSync, deflateSync } from 'node:zlib'
const [out, ...ins] = process.argv.slice(2)
const read = (f) => { const b = readFileSync(f); let p = 8, w, h, d = []; while (p < b.length) { const l = b.readUInt32BE(p), t = b.toString('ascii', p + 4, p + 8); if (t === 'IHDR') { w = b.readUInt32BE(p + 8); h = b.readUInt32BE(p + 12) } if (t === 'IDAT') d.push(b.subarray(p + 8, p + 8 + l)); p += 12 + l } return { w, h, raw: inflateSync(Buffer.concat(d)) } }
const imgs = ins.map(read), H = Math.max(...imgs.map((i) => i.h)), W = imgs.reduce((a, i) => a + i.w + 4, 0)
const o = Buffer.alloc((W * 3 + 1) * H, 255)
for (let y = 0; y < H; y++) o[y * (W * 3 + 1)] = 0
let x0 = 0
for (const im of imgs) { for (let y = 0; y < im.h; y++) im.raw.copy(o, y * (W * 3 + 1) + 1 + x0 * 3, y * (im.w * 3 + 1) + 1, (y + 1) * (im.w * 3 + 1)); x0 += im.w + 4 }
const crc = (b) => { let c = ~0; for (const v of b) { c ^= v; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1 } return ~c >>> 0 }
const chunk = (t, d) => { const b = Buffer.alloc(12 + d.length); b.writeUInt32BE(d.length); b.write(t, 4); d.copy(b, 8); b.writeUInt32BE(crc(b.subarray(4, 8 + d.length)), 8 + d.length); return b }
const hd = Buffer.alloc(13); hd.writeUInt32BE(W); hd.writeUInt32BE(H, 4); hd[8] = 8; hd[9] = 2
writeFileSync(out, Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk('IHDR', hd), chunk('IDAT', deflateSync(o)), chunk('IEND', Buffer.alloc(0))]))
