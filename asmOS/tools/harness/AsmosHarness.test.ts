/**
 * Scratch harness for asmOS (delete before committing): compiles a C Program with the host's GCC
 * as Compiler Explorer would be asked to, builds it on the x86 Core with the Project's Files as
 * its file system, replays a script of pointer and key actions and writes screenshots. Skipped
 * unless OS_DIR and OS_SCRIPT are set.
 */
import { spawnSync } from 'node:child_process'
import { mkdirSync, mkdtempSync, readFileSync, readdirSync, statSync, writeFileSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { dirname, join, relative } from 'node:path'
import { deflateSync } from 'node:zlib'
import { describe, it } from 'vite-plus/test'
import { FileSystem } from '#lib/languages/peripherals/FileSystem.ts'
import { X86Emulator } from '#lib/languages/X86/X86Emulator.svelte.ts'
import type { BuildSources, ProjectFiles } from '#lib/projectFiles.ts'
import { compileSource } from '#lib/sourceCompilation/compilerExplorer.ts'
import { generatedAssemblyPath } from '#lib/sourceCompilation/records.ts'

const DIR = process.env.OS_DIR
const SCRIPT = process.env.OS_SCRIPT
const OUT = process.env.OS_OUT ?? tmpdir()
const ENTRY = process.env.OS_ENTRY ?? 'main.c'
const OPT = (process.env.OS_OPT ?? '1') as '0' | '1' | '2'
//OS_OPTS: a source's own optimization, "os/doom_game.c=2,os/doom_play.c=2"
const OPTS = Object.fromEntries(
    (process.env.OS_OPTS ?? '').split(',').filter(Boolean).map((item) => item.split('='))
) as Record<string, '0' | '1' | '2'>
const UNDO = Number(process.env.OS_UNDO ?? '0')
const WIDTH = Number(process.env.OS_WIDTH ?? '640')
const HEIGHT = Number(process.env.OS_HEIGHT ?? '480')

function unbase64(line: string) {
    const match = /^(\s*)\.base64\s+"([^"]*)"\s*$/.exec(line)
    if (!match) return line
    const bytes = Buffer.from(match[2], 'base64')
    return `${match[1]}.ascii "${Array.from(bytes, (byte) => '\\' + byte.toString(8).padStart(3, '0')).join('')}"`
}

async function hostCompiler(_url: unknown, init?: RequestInit) {
    const body = JSON.parse(String(init!.body)) as {
        source: string
        lang: string
        options: { userArguments: string }
        files: { filename: string; contents: string }[]
    }
    const directory = mkdtempSync(join(tmpdir(), 'asmos-'))
    for (const file of body.files) {
        mkdirSync(dirname(join(directory, file.filename)), { recursive: true })
        writeFileSync(join(directory, file.filename), file.contents)
    }
    writeFileSync(join(directory, 'example.c'), body.source)
    const run = spawnSync('sh', ['-c', `gcc -S -o - ${body.options.userArguments} ${process.env.OS_CFLAGS ?? ''} example.c`], {
        cwd: directory,
        encoding: 'utf8',
        maxBuffer: 256 * 1024 * 1024
    })
    const lines = (text: string) =>
        text
            .split('\n')
            .filter(Boolean)
            .map((line) => ({ text: line }))
    return new Response(
        JSON.stringify({
            code: run.status,
            stdout: [],
            stderr: lines(run.stderr),
            asm: run.stdout.split('\n').map((text) => ({ text: unbase64(text), source: null }))
        })
    )
}

function walk(directory: string, root = directory): string[] {
    return readdirSync(directory).flatMap((name) => {
        const path = join(directory, name)
        return statSync(path).isDirectory() ? walk(path, root) : [relative(root, path).split('\\').join('/')]
    })
}

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
const sleep = (ms: number) => new Promise((resolve) => setTimeout(resolve, ms))

describe.skipIf(!DIR || !SCRIPT)('asmOS harness', () => {
    it('runs the script', { timeout: 3_600_000 }, async () => {
        const files: Record<string, ProjectFiles[string]> = {}
        for (const name of walk(DIR!)) {
            if (name.endsWith('.asmproj') || name.endsWith('.c.asm') || name === 'os/main') continue
            const bytes = readFileSync(join(DIR!, name))
            files[name] = /\.(bmp|png|bin|wav|mid|wad|vid)$/i.test(name)
                ? { encoding: 'base64', content: bytes.toString('base64') }
                : { encoding: 'plain', content: bytes.toString('utf8') }
        }
        //the first source is the Entry's; the others are compiled the same way and linked by name
        const sourcePaths = (process.env.OS_SOURCES ?? ENTRY).split(',')
        const outputPath = generatedAssemblyPath(sourcePaths[0], 'X86')
        const assemblies: Record<string, ProjectFiles[string]> = {}
        let started = performance.now()
        for (const sourcePath of sourcePaths) {
            const output = generatedAssemblyPath(sourcePath, 'X86')
            const compiled =
                process.env.OS_USE_ASM === '1' || (process.env.OS_KEEP ?? '').split(',').includes(sourcePath)
                    ? { assembly: readFileSync(join(DIR!, output), 'utf8'), diagnostics: [], record: undefined }
                    : await compileSource(
                          { sourcePath, outputPath: output, files, target: 'X86', optimization: OPTS[sourcePath] ?? OPT },
                          undefined,
                          process.env.OS_CE === '1'
                              ? async (url: string | URL | Request, init?: RequestInit) => {
                                    const response = await fetch(url, init)
                                    const text = await response.text()
                                    console.log(`Compiler Explorer answered ${text.length} bytes for ${sourcePath}, HTTP ${response.status}`)
                                    return new Response(text, { status: response.status })
                                }
                              : (hostCompiler as typeof fetch)
                      ).catch((error) => {
                          console.log('COMPILE FAILED', sourcePath, error.message)
                          for (const item of error.diagnostics ?? [])
                              console.log(`${item.file}:${item.lineIndex + 1}:${item.column}: ${item.message}`)
                          throw error
                      })
            for (const item of compiled.diagnostics)
                console.log(`${item.severity} ${item.file}:${item.lineIndex + 1}: ${item.message}`)
            console.log(`${sourcePath}: ${compiled.assembly.split('\n').length} lines of NASM`)
            if (process.env.OS_CE === '1') {
                writeFileSync(join(DIR!, output), compiled.assembly)
                console.log('RECORD', JSON.stringify(compiled.record))
            }
            if (process.env.OS_ASM_OUT)
                writeFileSync(join(process.env.OS_ASM_OUT, output.split('/').pop()!), compiled.assembly)
            assemblies[output] = { encoding: 'plain', content: compiled.assembly }
        }
        console.log(`compiled in ${Math.round(performance.now() - started)} ms`)
        const sources: BuildSources = {
            entry: outputPath,
            entrySource: ENTRY,
            entrySymbol: '_start',
            runtimeAbi: 'v1',
            files: { ...files, ...assemblies }
        }
        //OS_AUTOSTART: the applications asmOS starts, as an autostart.txt only this run sees
        const runFiles = structuredClone(sources.files) as Record<string, ProjectFiles[string]>
        if (process.env.OS_AUTOSTART)
            runFiles['autostart.txt'] = { encoding: 'plain', content: process.env.OS_AUTOSTART.split(';').join('\n') }
        const fileSystem = new FileSystem(runFiles)
        const emulator = await X86Emulator(sources, {
            automaticChecking: false,
            display: { width: WIDTH, height: HEIGHT },
            peripherals: { fileSystem }
        })
        const native = () => (emulator as unknown as { core: { getInstructionsExecuted(): bigint } }).core
        //the Build saves its program as a File, as the editor's does: os/main for os/main.c.asm
        emulator.setBuildOutputHost({
            claim: async () => 'save',
            save: (_sources, path, bytes) => {
                fileSystem.writeBytes(path, bytes)
                if (process.env.OS_SAVE_ELF) writeFileSync(process.env.OS_SAVE_ELF, bytes)
            },
            isBuildOutput: () => false
        })
        try {
            started = performance.now()
            await emulator.compile(UNDO, sources)
            console.log(`built in ${Math.round(performance.now() - started)} ms`)
            if (emulator.errors.length) console.log('BUILD ERRORS', emulator.errors)
            if (process.env.OS_LINE_FILES) {
                //which File the Core maps each instruction to: the source map C debugging reads
                const counts: Record<string, number> = {}
                for (const item of (native() as unknown as { getCompiledInstructions(): { file: string }[] }).getCompiledInstructions())
                    counts[item.file] = (counts[item.file] ?? 0) + 1
                console.log('LINE FILES', JSON.stringify(counts))
            }

            //what /dev/dsp played: the samples each stats line reports since the last
            const audio = emulator.peripherals.audio.x86
            const pcmOf = audio.pcm.bind(audio)
            let pcmFrames = 0
            let pcmRate = 0
            audio.pcm = (event) => {
                pcmFrames += event.frames.length >> 1
                pcmRate = event.rate
                pcmOf(event)
            }
            const screen = emulator.peripherals.screen
            const input = emulator.peripherals.x86Input
            const running = emulator.run(Number.MAX_SAFE_INTEGER)
            let lastCount = 0n
            let lastTime = performance.now()
            const t0 = lastTime
            const script = JSON.parse(readFileSync(SCRIPT!, 'utf8')) as Action[]
            const press = async (code: string, shift = false, ctrl = false, alt = false) => {
                if (shift) input.keyDown({ code: 'ShiftLeft' })
                if (ctrl) input.keyDown({ code: 'ControlLeft' })
                if (alt) input.keyDown({ code: 'AltLeft' })
                input.keyDown({ code })
                await sleep(15)
                input.keyUp({ code })
                if (alt) input.keyUp({ code: 'AltLeft' })
                if (ctrl) input.keyUp({ code: 'ControlLeft' })
                if (shift) input.keyUp({ code: 'ShiftLeft' })
                await sleep(15)
            }
            for (const action of script) {
                if ('wait' in action) await sleep(action.wait)
                else if ('move' in action) {
                    input.moveTo(...action.move)
                    await sleep(20)
                } else if ('drag' in action) {
                    const [x0, y0, x1, y1] = action.drag
                    const steps = action.steps ?? 8
                    const button = action.button ?? 'left'
                    input.moveTo(x0, y0)
                    await sleep(20)
                    input.buttonDown(button, x0, y0)
                    await sleep(20)
                    for (let i = 1; i <= steps; i++) {
                        input.moveTo(Math.round(x0 + ((x1 - x0) * i) / steps), Math.round(y0 + ((y1 - y0) * i) / steps))
                        await sleep(20)
                    }
                    input.buttonUp(button, x1, y1)
                    await sleep(30)
                } else if ('click' in action) {
                    input.moveTo(...action.click)
                    await sleep(20)
                    input.buttonDown(action.button ?? 'left', ...action.click)
                    await sleep(30)
                    input.buttonUp(action.button ?? 'left', ...action.click)
                    await sleep(30)
                } else if ('dblclick' in action) {
                    input.moveTo(...action.dblclick)
                    for (let i = 0; i < 2; i++) {
                        input.buttonDown('left', ...action.dblclick)
                        await sleep(10)
                        input.buttonUp('left', ...action.dblclick)
                        await sleep(10)
                    }
                } else if ('hold' in action) {
                    input.keyDown({ code: action.hold })
                    await sleep(action.ms)
                    input.keyUp({ code: action.hold })
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
                    const count = native().getInstructionsExecuted()
                    const now = performance.now()
                    console.log(
                        `[${action.stats}] t=${Math.round(now - t0)}ms +${count - lastCount} instr in ${Math.round(now - lastTime)}ms` +
                            (pcmFrames ? `, sound ${pcmFrames} frames at ${pcmRate} Hz` : '')
                    )
                    pcmFrames = 0
                    lastCount = count
                    lastTime = now
                }
            }
            for (const [path, file] of Object.entries(fileSystem.files))
                if (!sources.files[path] || sources.files[path].content !== file.content)
                    console.log(`FILE ${path}: ${file.encoding} ${file.content.length} chars`)
            const text = emulator.peripherals.terminal.output
            if (text) console.log('TERMINAL:\n' + text.slice(-4000))
            console.log('errors', emulator.errors, 'terminated', emulator.terminated)
            emulator.pause()
            await Promise.race([running, sleep(2000)])
        } finally {
            emulator.dispose()
        }
    })
})
