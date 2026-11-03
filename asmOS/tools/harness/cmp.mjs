// cmp.mjs <dirA> <dirB> [names...] : compares same-named PNGs pixel by pixel; the tray (clock, CPU meter) is counted apart
import { readFileSync, readdirSync, existsSync } from 'node:fs'
import { inflateSync } from 'node:zlib'
const read = (f) => { const b = readFileSync(f); let p = 8, w, h, d = []; while (p < b.length) { const l = b.readUInt32BE(p), t = b.toString('ascii', p + 4, p + 8); if (t === 'IHDR') { w = b.readUInt32BE(p + 8); h = b.readUInt32BE(p + 12) } if (t === 'IDAT') d.push(b.subarray(p + 8, p + 8 + l)); p += 12 + l } return { w, h, raw: inflateSync(Buffer.concat(d)) } }
const [a, b, ...only] = process.argv.slice(2)
const names = (only.length ? only : readdirSync(a).filter((n) => n.endsWith('.png'))).sort()
const tray = (x, y) => y >= 454 && x >= 536
for (const n of names) {
  if (!existsSync(`${b}/${n}`)) { console.log(`${n}: missing in B`); continue }
  const A = read(`${a}/${n}`), B = read(`${b}/${n}`)
  let diff = 0, trayDiff = 0, x0 = 1e9, y0 = 1e9, x1 = -1, y1 = -1
  for (let y = 0; y < A.h; y++) for (let x = 0; x < A.w; x++) {
    const i = y * (A.w * 3 + 1) + 1 + x * 3
    if (A.raw[i] !== B.raw[i] || A.raw[i + 1] !== B.raw[i + 1] || A.raw[i + 2] !== B.raw[i + 2]) {
      if (tray(x, y)) { trayDiff++; continue }
      diff++; x0 = Math.min(x0, x); y0 = Math.min(y0, y); x1 = Math.max(x1, x); y1 = Math.max(y1, y)
    }
  }
  console.log(`${n.padEnd(26)} ${diff ? `DIFF ${diff} px in ${x0},${y0}-${x1},${y1}` : 'same'}${trayDiff ? ` (tray ${trayDiff} px)` : ''}`)
}
