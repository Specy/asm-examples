// Original pixel assets for Relay Maze. Run from any directory with Node.
import { writeFileSync, mkdirSync } from 'node:fs'
import { fileURLToPath } from 'node:url'

const root = fileURLToPath(new URL('../risc-v/relay-maze/assets/', import.meta.url))
mkdirSync(root, { recursive: true })
function picture(width, height) {
    const pixels = new Uint32Array(width * height)
    const dot = (x, y, color) => {
        if (x >= 0 && x < width && y >= 0 && y < height) pixels[y * width + x] = color
    }
    const rect = (x, y, w, h, color) => {
        for (let dy = 0; dy < h; dy++) for (let dx = 0; dx < w; dx++) dot(x + dx, y + dy, color)
    }
    return { width, height, pixels, dot, rect }
}
function save(name, { width, height, pixels }) {
    const stride = (width * 3 + 3) & ~3
    const bmp = Buffer.alloc(54 + stride * height)
    bmp.write('BM')
    bmp.writeUInt32LE(bmp.length, 2)
    bmp.writeUInt32LE(54, 10)
    bmp.writeUInt32LE(40, 14)
    bmp.writeInt32LE(width, 18)
    bmp.writeInt32LE(height, 22)
    bmp.writeUInt16LE(1, 26)
    bmp.writeUInt16LE(24, 28)
    for (let y = 0; y < height; y++)
        for (let x = 0; x < width; x++) {
            const color = pixels[y * width + x],
                offset = 54 + (height - y - 1) * stride + x * 3
            bmp[offset] = color & 255
            bmp[offset + 1] = (color >> 8) & 255
            bmp[offset + 2] = color >> 16
        }
    writeFileSync(root + name, bmp)
}

// A symmetric arcade layout: a central spine, T-shaped walls, side alcoves,
// a ring around the pen, and winding lower lanes. The pen and spawn coordinates
// remain compatible with the engine's fixed paths.
const leftHalf = [
    '##############',
    '#............#',
    '#.#####.####.#',
    '#o#####.####.#',
    '#.#####.####.#',
    '#.............',
    '#.#####.##.###',
    '#.#####.##.###',
    '#.......##...#',
    '#######.#### #',
    '#######.#### #',
    '#######.#     ',
    '#######.# ###-',
    '#######.# #   ',
    '       .  #   ',
    '#######.# #   ',
    '#######.# ####',
    '#######.#     ',
    '#######.# ####',
    '#######.# ####',
    '#............#',
    '#.#####.####.#',
    '#.#####.####.#',
    '#o...##...... ',
    '####.##.##.###',
    '####.##.##.###',
    '#.......##...#',
    '#.##########.#',
    '#.##########.#',
    '#.............',
    '##############'
]
if (leftHalf.length !== 31 || leftHalf.some((row) => row.length !== 14))
    throw new Error('The maze needs 31 half-rows of 14 tiles')
const maze = leftHalf.map((row) => [...row, ...[...row].reverse()])
writeFileSync(root + 'maze.txt', maze.map((row) => row.join('')).join('\n') + '\n')

// Thin neon outlines around the walls. The floor and wall interiors stay black.
const board = picture(224, 248)
const wall = (x, y) =>
    x < 0 || y < 0 || x >= 224 || y >= 248
        ? false
        : maze[Math.floor(y / 8)][Math.floor(x / 8)] === '#'
for (let y = 0; y < 248; y++)
    for (let x = 0; x < 224; x++) {
        if (!wall(x, y)) continue
        let nearest = 100
        for (let dy = -3; dy <= 3; dy++)
            for (let dx = -3; dx <= 3; dx++) {
                const distance = dx * dx + dy * dy
                if (distance < nearest && !wall(x + dx, y + dy)) nearest = distance
            }
        if (nearest >= 4 && nearest <= 8) board.dot(x, y, 0x31b9b2)
        else if (nearest <= 2) board.dot(x, y, 0x0a484b)
    }
for (const x of [13, 14]) board.rect(x * 8, 12 * 8 + 3, 8, 2, 0xa68adf)
save('maze.bmp', board)

const art = picture(256, 144)
function cell(column, row, draw) {
    const ox = column * 16,
        oy = row * 16
    draw(
        (x, y, c) => art.dot(ox + x, oy + y, c),
        (x, y, w, h, c) => art.rect(ox + x, oy + y, w, h, c)
    )
}
const directions = [
    [1, 0],
    [-1, 0],
    [0, -1],
    [0, 1]
]
// Each direction has its own idle pose and two steps. The body keeps the same
// silhouette: only the feet and mouth move, and the pupils keep looking ahead.
function player(column, row, dir, phase) {
    cell(column, row, (p, r) => {
        const [dx, dy] = directions[dir]
        for (let y = 1; y < 13; y++)
            for (let x = 1; x < 15; x++) {
                const xx = (x - 7.5) / 6.3,
                    yy = (y - 7) / 6
                if (xx * xx + yy * yy <= 1) p(x, y, y < 6 ? 0xa1ffe0 : 0x57dfb4)
            }
        r(4, 5, 3, 4, 0xf5fff9)
        r(9, 5, 3, 4, 0xf5fff9)
        p(5 + dx, 6 + dy, 0x153d49)
        p(10 + dx, 6 + dy, 0x153d49)
        r(6, 10, 3, phase ? 2 : 1, 0x153d49)
        r(4, phase === 1 ? 12 : 13, 3, 1, 0xa1ffe0)
        r(9, phase === 2 ? 12 : 13, 3, 1, 0xa1ffe0)
    })
}
for (let direction = 0; direction < 4; direction++)
    for (let phase = 0; phase < 3; phase++) player(direction * 3 + phase, 0, direction, phase)
// Death: an expanding ring of sparks.
for (let phase = 0; phase <= 10; phase++)
    cell(phase, 1, (p) => {
        const radius = 2 + phase / 2
        for (let y = 0; y < 16; y++)
            for (let x = 0; x < 16; x++) {
                const distance = Math.hypot(x - 7.5, y - 7.5)
                if (Math.abs(distance - radius) < 0.7 && (x + y + phase) % 3)
                    p(x, y, phase < 6 ? 0x8fffd4 : 0xd5fff0)
            }
    })
const colors = [0xff7585, 0xc599ff, 0x66bdff, 0xffc267]
function enemy(column, row, color, kind, dir, phase) {
    cell(column, row, (p, r) => {
        for (let y = 1; y < 15; y++)
            for (let x = 1; x < 15; x++) {
                const ax = Math.abs(x - 7.5),
                    ay = Math.abs(y - 7.5)
                const inside =
                    kind === 0
                        ? ax + ay < 8.5
                        : kind === 1
                          ? Math.max(ax, ay) < 5.5 || (ax < 2 && ay < 7) || (ay < 2 && ax < 7)
                          : kind === 2
                            ? ax < 6.5 && ay < 6.5 && ax + ay < 9
                            : ax + ay < 9 && Math.min(ax, ay) < 5
                if (inside) p(x, y, color)
            }
        const [dx, dy] = directions[dir]
        r(4, 5, 3, 4, 0xffffff)
        r(9, 5, 3, 4, 0xffffff)
        p(5 + dx, 6 + dy, 0x18263d)
        p(10 + dx, 6 + dy, 0x18263d)
        if (phase) {
            r(3, 11, 3, 1, 0x18263d)
            r(10, 11, 3, 1, 0x18263d)
        }
    })
}
for (let kind = 0; kind < 4; kind++)
    for (let d = 0; d < 4; d++)
        for (let phase = 0; phase < 2; phase++)
            enemy(d * 2 + phase, kind + 2, colors[kind], kind, d, phase)
for (let phase = 0; phase < 4; phase++)
    enemy(phase, 6, phase < 2 ? 0x455282 : 0xdfddff, 2, 1, phase & 1)
// Defeated enemies return to the pen as a small ring.
for (let d = 0; d < 4; d++)
    cell(d + 4, 6, (p) => {
        for (let y = 4; y < 12; y++)
            for (let x = 4; x < 12; x++) {
                const dist = Math.hypot(x - 7.5, y - 7.5)
                if (dist > 2 && dist < 3.5) p(x, y, 0x9bddff)
            }
    })
// Geometric bonus items, with a different color and detail at each stage.
for (let i = 0; i < 8; i++)
    cell(i, 7, (p, r) => {
        const color = [
            0xf8bb77, 0xc6a1ff, 0x84d7ff, 0xf484ae, 0xc1ed7e, 0xffdd78, 0xa9b5ff, 0xf9a6e5
        ][i]
        for (let y = 2; y < 14; y++)
            for (let x = 2; x < 14; x++)
                if (Math.abs(x - 7.5) + Math.abs(y - 7.5) < 7) p(x, y, color)
        r(7, 4, 2, 8, 0xffffff)
        r(5, 7, 6, 2, 0xffffff)
    })

const glyphs = {
    '!': ['1', '1', '1', '1', '1', '0', '1'],
    A: ['01110', '10001', '10001', '11111', '10001', '10001', '10001'],
    B: ['11110', '10001', '10001', '11110', '10001', '10001', '11110'],
    C: ['01111', '10000', '10000', '10000', '10000', '10000', '01111'],
    D: ['11110', '10001', '10001', '10001', '10001', '10001', '11110'],
    E: ['11111', '10000', '10000', '11110', '10000', '10000', '11111'],
    F: ['11111', '10000', '10000', '11110', '10000', '10000', '10000'],
    G: ['01111', '10000', '10000', '10111', '10001', '10001', '01111'],
    H: ['10001', '10001', '10001', '11111', '10001', '10001', '10001'],
    I: ['111', '010', '010', '010', '010', '010', '111'],
    J: ['00111', '00010', '00010', '00010', '10010', '10010', '01100'],
    K: ['10001', '10010', '10100', '11000', '10100', '10010', '10001'],
    L: ['10000', '10000', '10000', '10000', '10000', '10000', '11111'],
    M: ['10001', '11011', '10101', '10101', '10001', '10001', '10001'],
    N: ['10001', '11001', '10101', '10011', '10001', '10001', '10001'],
    O: ['01110', '10001', '10001', '10001', '10001', '10001', '01110'],
    P: ['11110', '10001', '10001', '11110', '10000', '10000', '10000'],
    Q: ['01110', '10001', '10001', '10001', '10101', '10010', '01101'],
    R: ['11110', '10001', '10001', '11110', '10100', '10010', '10001'],
    S: ['01111', '10000', '10000', '01110', '00001', '00001', '11110'],
    T: ['11111', '00100', '00100', '00100', '00100', '00100', '00100'],
    U: ['10001', '10001', '10001', '10001', '10001', '10001', '01110'],
    V: ['10001', '10001', '10001', '10001', '10001', '01010', '00100'],
    W: ['10001', '10001', '10001', '10101', '10101', '11011', '10001'],
    X: ['10001', '10001', '01010', '00100', '01010', '10001', '10001'],
    Y: ['10001', '10001', '01010', '00100', '00100', '00100', '00100'],
    Z: ['11111', '00001', '00010', '00100', '01000', '10000', '11111'],
    0: ['01110', '10001', '10011', '10101', '11001', '10001', '01110'],
    1: ['010', '110', '010', '010', '010', '010', '111'],
    2: ['01110', '10001', '00001', '00010', '00100', '01000', '11111'],
    3: ['11110', '00001', '00001', '01110', '00001', '00001', '11110'],
    4: ['10010', '10010', '10010', '11111', '00010', '00010', '00010'],
    5: ['11111', '10000', '10000', '11110', '00001', '00001', '11110'],
    6: ['01110', '10000', '10000', '11110', '10001', '10001', '01110'],
    7: ['11111', '00001', '00010', '00100', '01000', '01000', '01000'],
    8: ['01110', '10001', '10001', '01110', '10001', '10001', '01110'],
    9: ['01110', '10001', '10001', '01111', '00001', '00001', '01110'],
    ':': ['0', '1', '0', '0', '1'],
    '-': ['00000', '00000', '00000', '11111'],
    '/': ['00001', '00010', '00010', '00100', '01000', '01000', '10000']
}

const tiles = picture(128, 40)
for (const [char, rows] of Object.entries(glyphs)) {
    const id = char.charCodeAt(0) - 32,
        x = (id % 16) * 8 + 1,
        y = Math.floor(id / 16) * 8
    rows.forEach((row, dy) =>
        [...row].forEach((v, dx) => {
            if (v === '1') tiles.dot(x + dx, y + dy, 0xffffff)
        })
    )
}
// Dots and larger diamond-shaped power-ups occupy the two cells below the font.
tiles.rect(3, 35, 2, 2, 0xffd3a8)
for (let y = 0; y < 8; y++)
    for (let x = 0; x < 8; x++)
        if (Math.abs(x - 3.5) + Math.abs(y - 3.5) < 4) tiles.dot(8 + x, 32 + y, 0xb3a1ff)
save('tiles.bmp', tiles)
// Tiny numerals keep scores up to four digits inside a 16-pixel sprite cell.
const digits = {
    0: ['111', '101', '101', '101', '111'],
    1: ['010', '110', '010', '010', '111'],
    2: ['111', '001', '111', '100', '111'],
    3: ['111', '001', '111', '001', '111'],
    4: ['101', '101', '111', '001', '001'],
    5: ['111', '100', '111', '001', '111'],
    6: ['111', '100', '111', '101', '111'],
    7: ['111', '001', '010', '010', '010'],
    8: ['111', '101', '111', '101', '111'],
    9: ['111', '101', '111', '001', '111']
}
const points = [200, 400, 800, 1600, 150, 350, 550, 850, 1250, 1750, 1750, 1750]
for (let i = 0; i < points.length; i++)
    cell(i, 8, (p) => {
        const text = String(points[i]),
            left = Math.floor((16 - text.length * 4 + 1) / 2)
        for (let n = 0; n < text.length; n++)
            digits[text[n]].forEach((row, y) =>
                [...row].forEach((v, x) => {
                    if (v === '1') p(left + n * 4 + x, 5 + y, 0xc9ecff)
                })
            )
    })
save('sprites.bmp', art)
