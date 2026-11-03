import { spawnSync } from 'node:child_process'
import {
    cpSync,
    existsSync,
    mkdirSync,
    mkdtempSync,
    readFileSync,
    rmSync,
    symlinkSync,
    writeFileSync
} from 'node:fs'
import { tmpdir } from 'node:os'
import { dirname, join } from 'node:path'
import { after } from 'node:test'
import { fileURLToPath } from 'node:url'
import { unzipSync } from 'fflate'

const repository = fileURLToPath(new URL('../..', import.meta.url))
const made = []
after(() => {
    for (const folder of made) rmSync(folder, { recursive: true, force: true })
})

export const README = '# Examples\n\n<!-- examples:start -->\n<!-- examples:end -->\n'

/**
 * A repository of its own in a temporary folder, with this one's scripts, holding `files` (path →
 * text, bytes, or an object written as JSON).
 */
export function fixture(files = {}) {
    const root = mkdtempSync(join(tmpdir(), 'asm-examples-'))
    made.push(root)
    cpSync(join(repository, 'scripts'), join(root, 'scripts'), { recursive: true })
    symlinkSync(join(repository, 'node_modules'), join(root, 'node_modules'))
    write(root, { 'README.md': README, ...files })
    return {
        root,
        write: (more) => write(root, more),
        read: (path) => readFileSync(join(root, path), 'utf8'),
        exists: (path) => existsSync(join(root, path)),
        remove: (path) => rmSync(join(root, path), { recursive: true }),
        run: (script, args = [], env = {}) => {
            const result = spawnSync(process.execPath, [join(root, 'scripts', script), ...args], {
                cwd: root,
                encoding: 'utf8',
                env: { ...process.env, ...env }
            })
            return { status: result.status, output: result.stdout + result.stderr }
        },
        index: () => JSON.parse(readFileSync(join(root, 'dist/index.json'), 'utf8')),
        archive: (path) => readArchive(join(root, 'dist', path)),
        archiveBytes: (path) => readFileSync(join(root, 'dist', path))
    }
}

function write(root, files) {
    for (const [path, content] of Object.entries(files)) {
        mkdirSync(dirname(join(root, path)), { recursive: true })
        const data =
            typeof content === 'string' || content instanceof Uint8Array
                ? content
                : JSON.stringify(content, null, 4)
        writeFileSync(join(root, path), data)
    }
}

/** An archive's manifest and its Files' bytes as text. */
export function readArchive(path) {
    const zip = unzipSync(new Uint8Array(readFileSync(path)))
    const manifest = JSON.parse(new TextDecoder().decode(zip['project.json']))
    const files = {}
    for (const name of Object.keys(zip))
        if (name.startsWith('files/')) files[name.slice(6)] = Buffer.from(zip[name])
    return { manifest, files, names: Object.keys(zip) }
}
