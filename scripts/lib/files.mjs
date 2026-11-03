import { readdirSync, readFileSync } from 'node:fs'
import { join } from 'node:path'

/** What the editor accepts in one Project (`projectFiles.ts`, `projectArchive.ts`). */
export const FILE_BYTE_LIMIT = 16 * 1024 * 1024
export const FILE_COUNT_LIMIT = 4096
export const PATH_BYTE_LIMIT = 1024
export const MANIFEST_BYTE_LIMIT = 1024 * 1024

/** The Files of an example folder that describe it and are never packed. */
export const EXAMPLE_FILE = 'example.json'
export const COMPILATIONS_FILE = 'compilations.json'
export const BUILD_FILE = 'build.mjs'
const METADATA = new Set([EXAMPLE_FILE, COMPILATIONS_FILE, BUILD_FILE])
/** Left out wherever they are. A `.keep` is not: a folder asmOS makes lives through one. */
const IGNORED = new Set(['.gitignore', '.gitattributes', '.DS_Store', 'Thumbs.db'])

/**
 * Files kept as bytes. Every other File must be UTF-8 text, so that a source saved in another
 * encoding fails the build instead of opening in the editor's Hex editor.
 */
const BINARY =
    /\.(bmp|png|jpe?g|gif|webp|ico|bin|raw|dat|wav|mp3|ogg|mid|midi|wad|vid|elf|o|a|so|zip|pdf|ttf|otf)$/i

const decoder = new TextDecoder('utf-8', { fatal: true, ignoreBOM: true })
const encoder = new TextEncoder()

/** Problems found while building one example, reported together. */
export class ExampleError extends Error {
    constructor(problems) {
        super(problems.join('\n'))
        this.problems = problems
    }
}

/**
 * The Files under `directory`, as sorted paths from it with forward slashes, leaving out the
 * example's own metadata at its root. A symbolic link is a problem: it could pack a file from
 * outside the example.
 */
export function walk(directory, { root = directory, prefix = '' } = {}) {
    const paths = []
    for (const entry of readdirSync(directory, { withFileTypes: true })) {
        if (IGNORED.has(entry.name)) continue
        if (!prefix && METADATA.has(entry.name) && directory === root) continue
        const path = prefix + entry.name
        if (entry.isSymbolicLink()) throw new ExampleError([`${path} is a symbolic link`])
        if (entry.isDirectory())
            paths.push(...walk(join(directory, entry.name), { root, prefix: path + '/' }))
        else if (entry.isFile()) paths.push(path)
    }
    return paths.sort()
}

/** A File as the editor holds it, from the bytes of `sourcePath`. */
export function readProjectFile(sourcePath, archivePath) {
    const bytes = readFileSync(sourcePath)
    if (BINARY.test(archivePath))
        return { encoding: 'base64', content: bytes.toString('base64'), bytes }
    try {
        return { encoding: 'plain', content: decoder.decode(bytes), bytes }
    } catch {
        throw new ExampleError([
            `${archivePath} is not UTF-8 text; save it as UTF-8, or add its extension to BINARY in scripts/lib/files.mjs if it is not text`
        ])
    }
}

/** The editor's rule for a File path (`isValidFilePath`). */
export function isValidFilePath(path) {
    return (
        typeof path === 'string' &&
        path.length > 0 &&
        encoder.encode(path).length <= PATH_BYTE_LIMIT &&
        decoder.decode(encoder.encode(path)) === path &&
        !path.includes('\\') &&
        !Array.from(path).some((character) => {
            const code = character.charCodeAt(0)
            return code <= 0x1f || (code >= 0x7f && code <= 0x9f)
        }) &&
        path.split('/').every((part) => part !== '' && part !== '.' && part !== '..')
    )
}
