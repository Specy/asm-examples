// The differential tests of the Assembly Editor's RISC-V engine (asmOS, os/ide/): every corpus
// program runs through rvcli (the engine built for this computer) and through the RARS Core
// (@specy/risc-v, packages/risc-v), and the two reports must agree.
//
//     node suite.mjs RVCLI [name-filter]
//
// A program may start with a line `# @test limit=N keys=TEXT` (keys: characters typed for the
// MMIO keyboard, \n for Enter) and have a NAME.in beside it: the lines and characters its read
// ecalls get. Both sides answer program time with a clock that starts at 2000-01-01 and moves only
// with sleeps, and seed an unseeded generator with 42.
//
// Compared: for a program that does not assemble, that both refuse it and that every line RARS
// names is one rvcli names too (RARS stops at its first pass's errors, so it names fewer); for one
// that does, the machine code, the data, the console output with the echo of the input, how it
// ended (exit code, off the end, a fault's address, or the limit), x1 to x31 and pc. rvcli's
// --stepback-check must also say ok: stepping back to the start gives the machine rv_reset made,
// and running forward again gives the same end.
import { spawnSync } from 'node:child_process'
import { existsSync, readFileSync, readdirSync } from 'node:fs'
import { basename, dirname, join } from 'node:path'
import { fileURLToPath } from 'node:url'
//the editor: ASM_EDITOR, else the one asm-examples is a submodule of, else ../asm-editor beside it
const repository = fileURLToPath(new URL('../../../..', import.meta.url))
const editor =
    process.env.ASM_EDITOR ??
    (existsSync(join(repository, '../../packages/asm-editor'))
        ? join(repository, '../..')
        : join(repository, '../asm-editor'))
const { isRuntimeError, makeRiscVFromFiles, StopReason } = await import(
    editor + '/packages/risc-v/rarsjs/ts/dist/index.mjs'
)

const here = dirname(fileURLToPath(import.meta.url))
const [cli, filter] = process.argv.slice(2)
const CLOCK = 946684800000
const hex = (value) => (value >>> 0).toString(16).padStart(8, '0')

function options(source, extra) {
    const match = /^#\s*@test\b(.*)$/m.exec(source)
    const result = { limit: 5000000, keys: '' }
    const text = (match ? match[1] : '') + ' ' + (extra ?? '')
    for (const [, key, value] of text.matchAll(/(\w+)=(\S+)/g)) result[key] = key === 'limit' ? Number(value) : value
    return result
}

/* The report rvcli prints, made from the RARS Core. */
async function rars(name, source, input, opts) {
    const lines = []
    const core = makeRiscVFromFiles({ [name]: source }, name)
    core.removeMemoryObservers()
    let out = ''
    let cursor = 0
    let clock = CLOCK
    const nextLine = () => {
        let end = input.indexOf('\n', cursor)
        if (end < 0) end = input.length
        const line = input.slice(cursor, end)
        cursor = Math.min(input.length, end + 1)
        out += line + '\n'
        return line
    }
    core.registerHandler('printString', (text) => void (out += text))
    core.registerHandler('readInt', nextLine)
    core.registerHandler('readString', nextLine)
    core.registerHandler('readFloat', nextLine)
    core.registerHandler('readDouble', nextLine)
    core.registerHandler('readChar', () => {
        const ch = cursor < input.length ? input[cursor++] : ''
        out += ch
        return ch
    })
    core.registerHandler('time', () => clock)
    core.registerHandler('sleep', (ms) => void (clock += Math.max(0, ms)))
    core.registerHandler('midiOut', () => undefined)
    core.registerHandler('randomSeed', () => 42)
    const decoder = new TextDecoder()
    core.registerHandler('stdOut', (bytes) => void (out += decoder.decode(Uint8Array.from(bytes))))
    core.registerHandler('stdErr', (bytes) => void (out += decoder.decode(Uint8Array.from(bytes))))
    const result = core.assemble()
    const errors = result.errors.filter((error) => !error.isWarning)
    lines.push(`ERRORS ${errors.length}`)
    for (const error of errors) lines.push(`ERROR ${error.sourceLine}`, `# ${error.sourceLine}: ${error.message}`)
    if (result.hasErrors) return lines
    const statements = core.getCompiledStatements().slice().sort((a, b) => (a.address >>> 0) - (b.address >>> 0))
    lines.push('TEXT' + statements.map((statement) => ' ' + hex(statement.binaryStatement)).join(''))
    core.initialize(true)
    const data = core.readMemoryBytes(0x10000000, 0x50000)
    for (let at = 0; at < data.length; at += 16) {
        const chunk = Array.from(data.slice(at, at + 16), (byte) => byte & 0xff)
        if (chunk.some(Boolean)) lines.push(`DATA ${hex(0x10000000 + at)} ${chunk.map((b) => b.toString(16).padStart(2, '0')).join('')}`)
    }
    // the Keyboard and Display MMIO, as MarsDevices.ts drives it
    const keys = Array.from(opts.keys.replaceAll('\\n', '\n'), (ch) => ch.charCodeAt(0))
    let armed = false
    const refresh = () => {
        if (!keys.length) {
            armed = false
            core.setPeripheralWord(0xffff0000 | 0, 0)
            return
        }
        core.setPeripheralWord(0xffff0004 | 0, keys[0] > 0xff ? 63 : keys[0])
        core.setPeripheralWord(0xffff0000 | 0, 1)
        armed = true
    }
    const control = (register, value) => {
        if (value & 2) throw new Error('Interrupt-driven I/O is not supported')
        core.setPeripheralWord(register | 0, register === 0xffff0008 || armed ? 1 : 0)
    }
    core.addMemoryAccessObserver(0xffff0000 | 0, null, (_a, value) => control(0xffff0000, value))
    core.addMemoryAccessObserver(0xffff0004 | 0, () => {
        if (armed) keys.shift()
        refresh()
    }, null)
    core.addMemoryAccessObserver(0xffff0008 | 0, null, (_a, value) => control(0xffff0008, value))
    core.addMemoryAccessObserver(0xffff000c | 0, null, (_a, value) => {
        out += String.fromCharCode(value & 0xff)
    })
    core.setPeripheralWord(0xffff0008 | 0, 1)
    core.setPeripheralWord(0xffff0004 | 0, 0)
    refresh()

    let end
    let pc
    let budget = opts.limit
    try {
        for (;;) {
            if (budget <= 0) {
                end = 'limit'
                break
            }
            const before = core.getInstructionsExecuted()
            const stop = await core.simulateWithLimit(budget)
            budget -= core.getInstructionsExecuted() - before
            if (stop === StopReason.NORMAL_TERMINATION) {
                end = `exit ${core.exitCode}`
                break
            }
            if (stop === StopReason.CLIFF_TERMINATION) {
                end = 'cliff'
                break
            }
            if (stop === StopReason.MAX_STEPS) {
                end = 'limit'
                break
            }
        }
        pc = core.programCounter
    } catch (error) {
        if (!isRuntimeError(error)) throw error
        end = `fault ${hex(error.address)}`
        pc = error.address
        lines.push(`# fault: ${error.message}`)
    }
    lines.push(`OUTPUT ${JSON.stringify(out)}`)
    lines.push(`END ${end}`)
    const registers = core.getRegistersValues()
    lines.push('REGS' + Array.from(registers).slice(1, 32).map((value) => ' ' + hex(value)).join(''))
    lines.push(`PC ${hex(pc)}`)
    core.removeMemoryObservers()
    return lines
}

function field(report, key) {
    return report.filter((line) => line.startsWith(key + ' ') || line === key)
}

function compare(ours, theirs) {
    const problems = []
    const ourErrors = Number(field(ours, 'ERRORS')[0]?.split(' ')[1] ?? -1)
    const theirErrors = Number(field(theirs, 'ERRORS')[0]?.split(' ')[1] ?? -1)
    if ((ourErrors > 0) !== (theirErrors > 0)) {
        problems.push(`errors: rvcli ${ourErrors}, RARS ${theirErrors}`)
        return problems
    }
    if (ourErrors > 0) {
        const ourLines = new Set(field(ours, 'ERROR').map((line) => line.split(' ')[1]))
        for (const line of field(theirs, 'ERROR').map((l) => l.split(' ')[1]))
            if (!ourLines.has(line)) problems.push(`RARS reports an error on line ${line}, rvcli does not`)
        return problems
    }
    for (const key of ['TEXT', 'DATA', 'OUTPUT', 'END', 'REGS', 'PC']) {
        const a = field(ours, key).join('\n')
        const b = field(theirs, key).join('\n')
        if (a !== b) problems.push(`${key} differs:\n    rvcli: ${a.slice(0, 600)}\n    RARS:  ${b.slice(0, 600)}`)
    }
    const stepback = field(ours, 'STEPBACK')[0]
    if (stepback !== 'STEPBACK ok') problems.push(`step back: ${stepback ?? 'missing'}`)
    return problems
}

// CORPUS: another folder of programs, such as asmOS's code/; INPUTS: where their NAME.in and
// NAME.opts (the options a @test line would give) are, when not beside them
const corpus = process.env.CORPUS ?? join(here, 'corpus')
const inputs = process.env.INPUTS ?? corpus
const names = readdirSync(corpus).filter((name) => /\.(s|riscv)$/.test(name) && (!filter || name.includes(filter))).sort()
let passed = 0
const failed = []
for (const name of names) {
    const path = join(corpus, name)
    const source = readFileSync(path, 'utf8')
    const inputPath = join(inputs, name.replace(/\.(s|riscv)$/, '.in'))
    const input = existsSync(inputPath) ? readFileSync(inputPath, 'utf8') : ''
    const optsPath = join(inputs, name.replace(/\.(s|riscv)$/, '.opts'))
    const opts = options(source, existsSync(optsPath) ? readFileSync(optsPath, 'utf8') : '')
    const args = ['--stepback-check', '--limit', String(opts.limit)]
    if (opts.keys) args.push('--keys', opts.keys)
    if (input) args.push('--input', inputPath)
    const run = spawnSync(cli, [...args, path], { encoding: 'utf8', maxBuffer: 64 << 20 })
    if (run.status !== 0 || run.stderr) {
        failed.push(name)
        console.log(`FAIL ${name}: rvcli exited ${run.status}\n${run.stderr.slice(0, 2000)}`)
        continue
    }
    const ours = run.stdout.split('\n').filter(Boolean)
    const theirs = await rars(basename(path), source, input, opts)
    const problems = compare(ours, theirs)
    if (problems.length) {
        failed.push(name)
        console.log(`FAIL ${name}\n  ${problems.join('\n  ')}`)
        if (process.env.VERBOSE) console.log('  rvcli:\n    ' + ours.join('\n    ') + '\n  RARS:\n    ' + theirs.join('\n    '))
    } else {
        passed++
        if (process.env.VERBOSE) console.log(`ok   ${name}`)
    }
}
console.log(`\n${passed} of ${names.length} programs agree with RARS${failed.length ? `; failing: ${failed.join(', ')}` : ''}`)
process.exit(failed.length ? 1 : 0)
