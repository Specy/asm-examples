/**
 * The harness for asmOS on MIPS and RISC-V, run as one of the editor's tests: takes the port
 * asmOS's build.mjs packs for OS_TARGET, compiles its units with a local Clang 21.1.0 as Compiler
 * Explorer's would be asked to (or takes the saved Generated assembly with OS_USE_ASM=1), builds the
 * Program on the editor's RARS or MARS with its Compilation records, so the units link as in the
 * editor, plays a script of pointer and key actions on the Screen's Keyboard and Mouse, which feed
 * the Input controller, and writes screenshots. Its scripts are scripts/sim-*.json, the x86 ones
 * moved to where the icons are on a desktop without DOOM and the Video Player. Besides the x86
 * harness's actions, `mark` and `untilPixel` time how long until the Screen shows a colour.
 *
 *   EXAMPLES_ROOT  the asm-examples checkout
 *   OS_TARGET   RISC-V or MIPS
 *   OS_SCRIPT   the script; OS_OUT where its shots go
 *   OS_USE_ASM  1: the Generated assembly in the tree, as packed, instead of compiling
 *   OS_ASM_OUT  a folder for the Generated assembly compiled here
 *   OS_KEEP     units (sourcePaths) to take from OS_ASM_OUT instead of compiling them again
 *   OS_UNDO     the Undo history's size, 0 (the Program's setting) by default
 *   OS_AUTOSTART  "app arg;app arg", an autostart.txt only this run sees
 *   OS_TRACE    1: print the Terminal as it goes
 */
import { existsSync, readFileSync, writeFileSync } from 'node:fs'
import { join } from 'node:path'
import { deflateSync } from 'node:zlib'
import { describe, it } from 'vite-plus/test'
import { MIPSEmulator } from '#lib/languages/MIPS/MIPSEmulator.svelte.ts'
import { RISCVEmulator } from '#lib/languages/RISC-V/RISC-VEmulator.svelte.ts'
import { FileSystem } from '#lib/languages/peripherals/FileSystem.ts'
import { projectBuildSources } from '#lib/buildSources.ts'
import type { ProjectFiles } from '#lib/projectFiles.ts'
import { compileSource } from '#lib/sourceCompilation/compilerExplorer.ts'
import type { CompilationRecord } from '#lib/sourceCompilation/records.ts'
import { localCompiler } from './localClang.ts'

const ROOT = process.env.EXAMPLES_ROOT
const TARGET = (process.env.OS_TARGET ?? 'RISC-V') as 'RISC-V' | 'MIPS'
const SCRIPT = process.env.OS_SCRIPT
const OUT = process.env.OS_OUT ?? '/tmp'
const UNDO = Number(process.env.OS_UNDO ?? '0')

const crcTable = Array.from({ length: 256 }, (_, n) => {
    let c = n
    for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1
    return c >>> 0
})
function crc32(bytes: Uint8Array) {
    let c = 0xffffffff
    for (const byte of bytes) c = crcTable[(c ^ byte) & 0xff] ^ (c >>> 8)
    return (c ^ 0xffffffff) >>> 0
}
function png(width: number, height: number, rgba: Uint8Array | Uint8ClampedArray) {
    const raw = new Uint8Array((width * 3 + 1) * height)
    for (let y = 0; y < height; y++)
        for (let x = 0; x < width; x++) {
            const from = (y * width + x) * 4
            const to = y * (width * 3 + 1) + 1 + x * 3
            raw[to] = rgba[from]
            raw[to + 1] = rgba[from + 1]
            raw[to + 2] = rgba[from + 2]
        }
    const chunk = (type: string, data: Uint8Array) => {
        const out = new Uint8Array(12 + data.length)
        const view = new DataView(out.buffer)
        view.setUint32(0, data.length)
        out.set(new TextEncoder().encode(type), 4)
        out.set(data, 8)
        view.setUint32(8 + data.length, crc32(out.subarray(4, 8 + data.length)))
        return out
    }
    const header = new Uint8Array(13)
    const view = new DataView(header.buffer)
    view.setUint32(0, width)
    view.setUint32(4, height)
    header.set([8, 2, 0, 0, 0], 8)
    return Buffer.concat([
        new Uint8Array([137, 80, 78, 71, 13, 10, 26, 10]),
        chunk('IHDR', header),
        chunk('IDAT', deflateSync(raw)),
        chunk('IEND', new Uint8Array())
    ])
}

type Action =
    | { wait: number }
    | { move: [number, number] }
    | { drag: [number, number, number, number]; steps?: number; button?: 'left' | 'right' }
    | { click: [number, number]; button?: 'left' | 'right' }
    | { dblclick: [number, number] }
    | { key: string; shift?: boolean; ctrl?: boolean; alt?: boolean }
    | { hold: string; ms: number }
    | { type: string }
    | { shot: string }
    | { stats: string }
    | { until: string; timeout?: number }
    | { mark: string }
    | { untilPixel: [number, number, string]; timeout?: number }

const KEYS: Record<string, string> = {
    ' ': 'Space', '\n': 'Enter', '.': 'Period', ',': 'Comma', '-': 'Minus', '=': 'Equal', '/': 'Slash',
    ';': 'Semicolon', "'": 'Quote', '[': 'BracketLeft', ']': 'BracketRight', '\\': 'Backslash', '`': 'Backquote'
}
const SHIFTED: Record<string, string> = {
    '!': '1', '@': '2', '#': '3', $: '4', '%': '5', '^': '6', '&': '7', '*': '8', '(': '9', ')': '0',
    _: '-', '+': '=', '?': '/', ':': ';', '"': "'", '<': ',', '>': '.', '{': '[', '}': ']', '|': '\\', '~': '`'
}
function codeOf(character: string): { code: string; shift: boolean } {
    if (SHIFTED[character]) return { code: codeOf(SHIFTED[character]).code, shift: true }
    if (/[a-z]/.test(character)) return { code: `Key${character.toUpperCase()}`, shift: false }
    if (/[A-Z]/.test(character)) return { code: `Key${character}`, shift: true }
    if (/[0-9]/.test(character)) return { code: `Digit${character}`, shift: false }
    if (KEYS[character]) return { code: KEYS[character], shift: false }
    throw new Error(`no key for ${JSON.stringify(character)}`)
}
/** The `key` a browser gives with a `code`, enough for the typed-character receiver. */
function keyOf(code: string, shift: boolean): string {
    const letter = /^Key([A-Z])$/.exec(code)
    if (letter) return shift ? letter[1] : letter[1].toLowerCase()
    const digit = /^Digit(\d)$/.exec(code)
    if (digit) return digit[1]
    const named: Record<string, string> = { Space: ' ', Enter: 'Enter', Escape: 'Escape', Backspace: 'Backspace', Tab: 'Tab' }
    return named[code] ?? code
}
const sleep = (ms: number) => new Promise((resolve) => setTimeout(resolve, ms))

describe.skipIf(!SCRIPT || !ROOT)('asmOS on MARS and RARS', () => {
    it('runs the script', { timeout: 3_600_000 }, async () => {
        const { buildExample, findExamples } = await import(join(ROOT!, 'scripts/lib/examples.mjs'))
        const { readProjectFile } = await import(join(ROOT!, 'scripts/lib/files.mjs'))
        const example = findExamples(ROOT!).examples.find((item: { id: string }) => item.id === 'asmos')
        const { captured, compilations } = await buildExample(example, { capture: true })
        const port = captured.find((item: { target: { name: string } }) => item.target.name === TARGET)
        const files: Record<string, ProjectFiles[string]> = {}
        for (const [path, source] of Object.entries<string>(port.files)) {
            const file = readProjectFile(source, path)
            files[path] = { encoding: file.encoding, content: file.content }
        }
        const folder = port.target.folder as string
        const listed = compilations[folder] as CompilationRecord[]
        const records: CompilationRecord[] = []
        let started = performance.now()
        const keep = (process.env.OS_KEEP ?? '').split(',').filter(Boolean)
        for (const record of listed) {
            if (process.env.OS_USE_ASM === '1') {
                records.push(record)
                continue
            }
            const saved = process.env.OS_ASM_OUT && join(process.env.OS_ASM_OUT, record.outputPath.replaceAll('/', '_'))
            if (saved && keep.includes(record.sourcePath) && existsSync(saved + '.json')) {
                files[record.outputPath] = { encoding: 'plain', content: readFileSync(saved, 'utf8') }
                records.push(JSON.parse(readFileSync(saved + '.json', 'utf8')))
                continue
            }
            const unitStarted = performance.now()
            const result = await compileSource(
                {
                    sourcePath: record.sourcePath,
                    outputPath: record.outputPath,
                    files: files as ProjectFiles,
                    target: TARGET,
                    optimization: record.optimization,
                    sourceAnnotations: true,
                    compiler: 'clang'
                },
                undefined,
                localCompiler as typeof fetch
            ).catch((error) => {
                console.log('COMPILE FAILED', record.sourcePath, error.message)
                for (const item of error.diagnostics ?? [])
                    console.log(`${item.file}:${item.lineIndex + 1}:${item.column}: ${item.message}`)
                throw error
            })
            for (const item of result.diagnostics)
                if (item.severity === 'error' || item.severity === 'warning')
                    console.log(`${item.severity} ${item.file}:${item.lineIndex + 1}: ${item.message}`)
            files[record.outputPath] = { encoding: 'plain', content: result.assembly }
            records.push(result.record)
            if (saved) {
                writeFileSync(saved, result.assembly)
                writeFileSync(saved + '.json', JSON.stringify(result.record))
            }
            console.log(
                `${record.sourcePath} -O${record.optimization}: ${result.assembly.split('\n').length} lines in ${Math.round(performance.now() - unitStarted)} ms`
            )
        }
        console.log(`compiled in ${Math.round(performance.now() - started)} ms`)
        const entry = Object.keys(port.project.programs)[0]
        const program = port.project.programs[entry]
        const sources = projectBuildSources({
            files: files as ProjectFiles,
            entry,
            language: TARGET,
            compilations: records,
            settings: program.settings
        })
        console.log('link units', sources.linkUnits, 'profile', sources.assemblerProfile)
        const runFiles = structuredClone(sources.files) as Record<string, ProjectFiles[string]>
        if (process.env.OS_AUTOSTART)
            runFiles['autostart.txt'] = { encoding: 'plain', content: process.env.OS_AUTOSTART.split(';').join('\n') }
        const fileSystem = new FileSystem(runFiles)
        const make = TARGET === 'MIPS' ? MIPSEmulator : RISCVEmulator
        const emulator = make(sources, {
            language: TARGET,
            display: program.display,
            peripherals: { fileSystem }
        })
        try {
            started = performance.now()
            Error.stackTraceLimit = 100000
            try {
                await emulator.compile(UNDO, sources)
            } catch (error) {
                const frames = String((error as Error).stack).split('\n').filter((line) => !/jur_|\$matches/.test(line))
                console.log('BUILD THREW', frames.slice(0, 60).join('\n'))
                throw error
            }
            console.log(`built in ${Math.round(performance.now() - started)} ms`)
            const errors = emulator.compilerDiagnostics.filter((d) => d.severity === 'error')
            if (errors.length) {
                console.log('BUILD ERRORS', JSON.stringify(errors.slice(0, 20), null, 1))
                return
            }
            const screen = emulator.peripherals.screen
            const keyboard = emulator.peripherals.keyboard
            const mouse = emulator.peripherals.mouse
            const terminal = emulator.peripherals.terminal
            const count = () => (emulator as unknown as { _getInstructionsExecuted(): bigint })._getInstructionsExecuted()
            const running = emulator.run(Number.MAX_SAFE_INTEGER)
            let lastCount = count()
            let lastTime = performance.now()
            const t0 = lastTime
            const script = JSON.parse(readFileSync(SCRIPT!, 'utf8')) as Action[]
            const down = (code: string, shift = false) => keyboard.keyDown({ code, key: keyOf(code, shift), repeat: false } as never)
            const up = (code: string, shift = false) => keyboard.keyUp({ code, key: keyOf(code, shift), repeat: false } as never)
            const press = async (code: string, shift = false, ctrl = false, alt = false) => {
                if (shift) down('ShiftLeft')
                if (ctrl) down('ControlLeft')
                if (alt) down('AltLeft')
                down(code, shift)
                await sleep(15)
                up(code, shift)
                if (alt) up('AltLeft')
                if (ctrl) up('ControlLeft')
                if (shift) up('ShiftLeft')
                await sleep(15)
            }
            let shown = 0
            const trace = () => {
                const text = terminal.output
                if (process.env.OS_TRACE === '1' && text.length > shown) {
                    process.stdout.write(text.slice(shown))
                    shown = text.length
                }
            }
            //`mark` starts a timer that `untilPixel` reads when the Screen shows a colour at a point
            let mark = { name: 'start', at: t0, instructions: count() }
            const pixel = (x: number, y: number) => {
                const at = (y * screen.width + x) * 4, rgba = screen.visiblePixels
                return ((rgba[at] << 16) | (rgba[at + 1] << 8) | rgba[at + 2]).toString(16).padStart(6, '0')
            }
            for (const action of script) {
                trace()
                if ('mark' in action) {
                    mark = { name: action.mark, at: performance.now(), instructions: count() }
                    continue
                }
                if ('untilPixel' in action) {
                    const [x, y, colour] = action.untilPixel
                    const deadline = performance.now() + (action.timeout ?? 60_000)
                    while (pixel(x, y) !== colour && performance.now() < deadline) await sleep(2)
                    const seen = pixel(x, y) === colour
                    console.log(
                        `[${mark.name}] ${seen ? 'shown' : 'NOT SHOWN'} after ${Math.round(performance.now() - mark.at)} ms, ` +
                            `${count() - mark.instructions} instructions (pixel ${x},${y} ${pixel(x, y)})`
                    )
                    continue
                }
                if (emulator.terminated) {
                    console.log('TERMINATED early', emulator.errors)
                    break
                }
                if ('wait' in action) await sleep(action.wait)
                else if ('until' in action) {
                    const deadline = performance.now() + (action.timeout ?? 120_000)
                    while (!terminal.output.includes(action.until) && performance.now() < deadline && !emulator.terminated)
                        await sleep(100)
                    console.log(`[until ${action.until}] ${terminal.output.includes(action.until) ? 'seen' : 'TIMED OUT'} at t=${Math.round(performance.now() - t0)}ms`)
                } else if ('move' in action) {
                    mouse.moveTo(...action.move)
                    await sleep(20)
                } else if ('drag' in action) {
                    const [x0, y0, x1, y1] = action.drag
                    const steps = action.steps ?? 8
                    const button = action.button ?? 'left'
                    mouse.moveTo(x0, y0)
                    await sleep(20)
                    mouse.buttonDown(button, x0, y0)
                    await sleep(20)
                    for (let i = 1; i <= steps; i++) {
                        mouse.moveTo(Math.round(x0 + ((x1 - x0) * i) / steps), Math.round(y0 + ((y1 - y0) * i) / steps))
                        await sleep(20)
                    }
                    mouse.buttonUp(button, x1, y1)
                    await sleep(30)
                } else if ('click' in action) {
                    mouse.moveTo(...action.click)
                    await sleep(20)
                    mouse.buttonDown(action.button ?? 'left', ...action.click)
                    await sleep(30)
                    mouse.buttonUp(action.button ?? 'left', ...action.click)
                    await sleep(30)
                } else if ('dblclick' in action) {
                    mouse.moveTo(...action.dblclick)
                    for (let i = 0; i < 2; i++) {
                        mouse.buttonDown('left', ...action.dblclick)
                        await sleep(10)
                        mouse.buttonUp('left', ...action.dblclick)
                        await sleep(10)
                    }
                } else if ('hold' in action) {
                    down(action.hold)
                    await sleep(action.ms)
                    up(action.hold)
                    await sleep(15)
                } else if ('key' in action) await press(action.key, action.shift, action.ctrl, action.alt)
                else if ('type' in action) {
                    for (const character of action.type) {
                        const { code, shift } = codeOf(character)
                        await press(code, shift)
                    }
                } else if ('shot' in action) {
                    await sleep(30)
                    writeFileSync(join(OUT, `${action.shot}.png`), png(screen.width, screen.height, screen.visiblePixels))
                } else if ('stats' in action) {
                    const now = performance.now()
                    const executed = count()
                    const core = (emulator as unknown as { core: { getHeapStart(): number; getHeapBreak(): number } }).core
                    const heapStart = core.getHeapStart() >>> 0, heapBreak = core.getHeapBreak() >>> 0
                    console.log(
                        `[${action.stats}] t=${Math.round(now - t0)}ms +${executed - lastCount} instr in ${Math.round(now - lastTime)}ms ` +
                            `(${Math.round(Number(executed - lastCount) / (now - lastTime))} per ms), ${executed} in all; ` +
                            `static ${Math.round((heapStart - 0x10010000) / 1024)} KB, heap ${Math.round((heapBreak - heapStart) / 1024)} KB, ` +
                            `left ${Math.round((0x10400000 - heapBreak) / 1024)} KB`
                    )
                    lastCount = executed
                    lastTime = now
                }
            }
            trace()
            for (const [path, file] of Object.entries(fileSystem.files))
                if (!sources.files[path] || sources.files[path].content !== file.content)
                    console.log(`FILE ${path}: ${file.encoding} ${file.content.length} chars`)
            const text = terminal.output
            if (text && process.env.OS_TRACE !== '1') console.log('TERMINAL:\n' + text.slice(-4000))
            console.log('errors', emulator.errors, 'terminated', emulator.terminated)
            emulator.pause()
            await Promise.race([running, sleep(2000)])
        } finally {
            emulator.dispose()
        }
    })
})
