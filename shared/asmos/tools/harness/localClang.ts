/**
 * A `fetch` for `compileSource` that answers Compiler Explorer's compile request with a local
 * Clang 21.1.0 (ASMOS_CLANG), the release CE's rv32-cclang2110 and mipsel-cclang2110 run, so that
 * a local build goes through the editor's own request and GNU-profile preparation without loading
 * Compiler Explorer while iterating. With --target=riscv32-unknown-elf it writes the instructions
 * CE wrote for the RISC-V Pac-Man, CE expanding tabs and dropping trailing comments.
 */
import { spawnSync } from 'node:child_process'
import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from 'node:fs'
import { tmpdir } from 'node:os'
import { dirname, join } from 'node:path'

export const CLANG =
    process.env.ASMOS_CLANG ?? 'clang'

/** The triple each Compiler Explorer Clang compiles for by default. */
const TRIPLES: Record<string, string> = {
    'rv32-cclang2110': 'riscv32-unknown-elf',
    'rv32-clang2110': 'riscv32-unknown-elf',
    'rv64-cclang2110': 'riscv64-unknown-elf',
    'mipsel-cclang2110': 'mipsel-unknown-linux-gnu',
    'mipsel-clang2110': 'mipsel-unknown-linux-gnu'
}

/** Compiler Explorer's `.loc`-derived `source` for each line, as its parser reports it. */
function withSources(text: string) {
    const files = new Map<number, string>()
    let location: { file: string | null; line: number; mainsource: boolean } | null = null
    return text.split('\n').map((line) => {
        const file = /^\s*\.file\s+(\d+)\s+"([^"]*)"(?:\s+"([^"]*)")?/.exec(line)
        const loc = /^\s*\.loc\s+(\d+)\s+(\d+)/.exec(line)
        if (file) {
            const [, number, first, second] = file
            files.set(Number(number), second === undefined ? first : second.startsWith('/') ? second : `${first}/${second}`)
        } else if (loc) {
            const name = files.get(Number(loc[1]))
            location = name === undefined ? null : { file: name.replace(/^.*?\/(project\/)/, '$1').replace(/^project\//, ''), line: Number(loc[2]), mainsource: /example\.c$/.test(name) }
            if (location && location.line === 0) location = { ...location, line: 0 }
        } else if (/^\s*\.(cfi_endproc|section|text|data)\b/.test(line) || /^\S+:/.test(line)) {
            if (/^\s*\.(cfi_endproc|section|text|data)\b/.test(line)) location = null
        }
        const instruction = /^\s+[a-z]/.test(line) && !/^\s*\./.test(line)
        return { text: line, source: instruction && location ? { ...location } : null }
    })
}

export async function localCompiler(url: string | URL | Request, init?: RequestInit) {
    const compilerId = /compiler\/([^/]+)\/compile/.exec(String(url))?.[1] ?? ''
    const triple = TRIPLES[compilerId]
    if (!triple) throw new Error(`no local compiler for ${compilerId}`)
    const body = JSON.parse(String(init!.body)) as {
        source: string
        options: { userArguments: string }
        files: { filename: string; contents: string }[]
    }
    const directory = mkdtempSync(join(tmpdir(), 'asmos-clang-'))
    try {
        for (const file of body.files) {
            mkdirSync(dirname(join(directory, file.filename)), { recursive: true })
            writeFileSync(join(directory, file.filename), file.contents)
        }
        writeFileSync(join(directory, 'example.c'), body.source)
        const command = `${CLANG} --target=${triple} -S -o - ${body.options.userArguments} ${process.env.ASMOS_CFLAGS ?? ''} example.c`
        const run = spawnSync('sh', ['-c', command], {
            cwd: directory,
            encoding: 'utf8',
            maxBuffer: 512 * 1024 * 1024
        })
        const lines = (text: string) =>
            text
                .split('\n')
                .filter(Boolean)
                .map((line) => ({ text: line }))
        const answer = JSON.stringify({
            code: run.status,
            stdout: [],
            stderr: lines(run.stderr ?? ''),
            asm: run.status === 0 ? withSources(run.stdout) : []
        })
        //Compiler Explorer's own answer adds labels and columns to each line: about 96 to 100 bytes
        //a line where this one writes 86 to 90, and the editor refuses one over 32 MiB
        const count = run.status === 0 ? run.stdout.split('\n').length : 0
        console.log(
            `answer for ${compilerId}: ${count} lines, ${(answer.length / 2 ** 20).toFixed(1)} MiB here, ` +
                `about ${((count * 96) / 2 ** 20).toFixed(1)} to ${((count * 100) / 2 ** 20).toFixed(1)} MiB from Compiler Explorer`
        )
        return new Response(answer)
    } finally {
        rmSync(directory, { recursive: true, force: true })
    }
}
