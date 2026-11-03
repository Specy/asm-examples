// crop.mjs <in.png> <out.png> x y w h scale : crops and enlarges a PNG (no libraries: zlib only)
import { readFileSync, writeFileSync } from 'node:fs'
import { inflateSync, deflateSync } from 'node:zlib'
const [inp, out, x, y, w, h, scale] = process.argv.slice(2)
const buf = readFileSync(inp)
let pos = 8, width, height, idat = []
while (pos < buf.length) {
  const len = buf.readUInt32BE(pos), type = buf.toString('ascii', pos + 4, pos + 8)
  if (type === 'IHDR') { width = buf.readUInt32BE(pos + 8); height = buf.readUInt32BE(pos + 12) }
  if (type === 'IDAT') idat.push(buf.subarray(pos + 8, pos + 8 + len))
  pos += 12 + len
}
const raw = inflateSync(Buffer.concat(idat)), stride = width * 3 + 1
const X = +x, Y = +y, W = +w, H = +h, S = +scale
const o = Buffer.alloc((W * S * 3 + 1) * H * S)
for (let j = 0; j < H * S; j++) for (let i = 0; i < W * S; i++) {
  const sy = Y + Math.floor(j / S), sx = X + Math.floor(i / S)
  const f = sy * stride + 1 + sx * 3, t = j * (W * S * 3 + 1) + 1 + i * 3
  o[t] = raw[f]; o[t + 1] = raw[f + 1]; o[t + 2] = raw[f + 2]
}
const crc = (b) => { let c = ~0; for (const v of b) { c ^= v; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1 } return ~c >>> 0 }
const chunk = (t, d) => { const b = Buffer.alloc(12 + d.length); b.writeUInt32BE(d.length); b.write(t, 4); d.copy(b, 8); b.writeUInt32BE(crc(b.subarray(4, 8 + d.length)), 8 + d.length); return b }
const hd = Buffer.alloc(13); hd.writeUInt32BE(W * S); hd.writeUInt32BE(H * S, 4); hd[8] = 8; hd[9] = 2
writeFileSync(out, Buffer.concat([Buffer.from([137,80,78,71,13,10,26,10]), chunk('IHDR', hd), chunk('IDAT', deflateSync(o)), chunk('IEND', Buffer.alloc(0))]))
