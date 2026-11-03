// grid.mjs <out.png> <columns> <in.png>... : tiles same-size PNGs, a 4-pixel gap
import { readFileSync, writeFileSync } from 'node:fs'
import { inflateSync, deflateSync } from 'node:zlib'
const [out, cols, ...ins] = process.argv.slice(2)
const read = (f) => { const b = readFileSync(f); let p = 8, w, h, d = []; while (p < b.length) { const l = b.readUInt32BE(p), t = b.toString('ascii', p + 4, p + 8); if (t === 'IHDR') { w = b.readUInt32BE(p + 8); h = b.readUInt32BE(p + 12) } if (t === 'IDAT') d.push(b.subarray(p + 8, p + 8 + l)); p += 12 + l } return { w, h, raw: inflateSync(Buffer.concat(d)) } }
const imgs = ins.map(read), C = +cols, w = imgs[0].w, h = imgs[0].h, R = Math.ceil(imgs.length / C)
const W = C * w + (C - 1) * 4, H = R * h + (R - 1) * 4, o = Buffer.alloc((W * 3 + 1) * H, 255)
for (let y = 0; y < H; y++) o[y * (W * 3 + 1)] = 0
imgs.forEach((im, k) => { const ox = (k % C) * (w + 4), oy = Math.floor(k / C) * (h + 4)
  for (let y = 0; y < h; y++) im.raw.copy(o, (oy + y) * (W * 3 + 1) + 1 + ox * 3, y * (w * 3 + 1) + 1, (y + 1) * (w * 3 + 1)) })
const crc = (b) => { let c = ~0; for (const v of b) { c ^= v; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1 } return ~c >>> 0 }
const chunk = (t, d) => { const b = Buffer.alloc(12 + d.length); b.writeUInt32BE(d.length); b.write(t, 4); d.copy(b, 8); b.writeUInt32BE(crc(b.subarray(4, 8 + d.length)), 8 + d.length); return b }
const hd = Buffer.alloc(13); hd.writeUInt32BE(W); hd.writeUInt32BE(H, 4); hd[8] = 8; hd[9] = 2
writeFileSync(out, Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk('IHDR', hd), chunk('IDAT', deflateSync(o)), chunk('IEND', Buffer.alloc(0))]))
