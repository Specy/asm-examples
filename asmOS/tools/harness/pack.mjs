// Scratch: packs .cache/asmos-ide-source (asmOS with the Assembly Editor) into an archive, as
// packages/asm-editor/scripts/pack-example-projects.mjs packs the examples (version 3).
import { existsSync, readFileSync, readdirSync, statSync, writeFileSync } from 'node:fs'
import { registerHooks } from 'node:module'
import { join, relative } from 'node:path'
import { fileURLToPath } from 'node:url'
import { zipSync } from 'fflate'

registerHooks({
    resolve(specifier, context, next) {
        if (/^[.#/]/.test(specifier) && !/\.[a-z]+$/i.test(specifier)) {
            for (const suffix of ['.ts', '/index.ts']) {
                try {
                    const resolved = next(specifier + suffix, context)
                    if (existsSync(fileURLToPath(resolved.url))) return resolved
                } catch {}
            }
        }
        return next(specifier, context)
    }
})
const EDITOR = (process.env.ASM_EDITOR ?? new URL('../../../../asm-editor', import.meta.url).pathname) + '/packages/asm-editor'
const { compilationInputs } = await import(EDITOR + '/src/lib/sourceCompilation/compilationInputs.ts')
const { fileFingerprint } = await import(EDITOR + '/src/lib/sourceCompilation/fingerprints.ts')

const [folder, output] = process.argv.slice(2)
const original = JSON.parse(readFileSync(process.argv[4], 'utf8')).project
const BINARY = /\.(bmp|png|jpe?g|gif|bin|wav|mid|wad|vid)$/i
const PACKED_AT = Date.UTC(2026, 10, 3)
//PACK_NAME, PACK_DESCRIPTION, PACK_TABS (comma-separated, the first-listed active unless PACK_ACTIVE) and
//PACK_EXAMPLE=1 (the example's fixed times) override what the owner's export had
const env = process.env
const walk = (directory, root = directory) =>
    readdirSync(directory).flatMap((name) => {
        const path = join(directory, name)
        return statSync(path).isDirectory() ? walk(path, root) : [relative(root, path).split('\\').join('/')]
    })
const names = walk(folder).sort()
const entries = {}, manifestFiles = {}, projectFiles = {}
for (const name of names) {
    const bytes = readFileSync(join(folder, name))
    const encoding = BINARY.test(name) ? 'base64' : 'plain'
    manifestFiles[name] = { encoding }
    projectFiles[name] = { encoding, content: encoding === 'base64' ? bytes.toString('base64') : bytes.toString('utf8') }
    entries[`files/${name}`] = [new Uint8Array(bytes), { mtime: PACKED_AT }]
}
const unit = (sourcePath, optimization) => ({
    runtimeAbi: 'v1', sourcePath, outputPath: sourcePath + '.asm', target: 'X86', language: 'c',
    compilerId: 'cg142', optimization
})
const compilations = [
    unit('os/main.c', '1'), unit('os/apps/apps.c', '1'), unit('os/ide/ide.c', '2'),
    unit('os/doom/doom_game.c', '2'), unit('os/doom/doom_play.c', '2'), unit('os/doom/doom_render.c', '2'),
    unit('os/doom/doom_world.c', '2')
].map((record) => ({
    ...record,
    inputs: compilationInputs(record.sourcePath, projectFiles),
    outputFingerprint: fileFingerprint(projectFiles[record.outputPath])
}))
const manifest = {
    version: 3,
    project: {
        id: '',
        name: env.PACK_NAME ?? original.name,
        description: env.PACK_DESCRIPTION ?? original.description,
        language: 'X86',
        createdAt: original.createdAt,
        updatedAt: env.PACK_EXAMPLE === '1' ? original.createdAt : PACKED_AT,
        programs: {
            'os/main.c.asm': { settings: original.settings, testcases: original.testcases, display: original.display }
        },
        selectedProgram: 'os/main.c.asm',
        compilations,
        openTabs: (env.PACK_TABS ? env.PACK_TABS.split(',') : original.openTabs).filter((path) => path in projectFiles),
        activeTab: env.PACK_ACTIVE ?? original.activeTab,
        fileViews: original.fileViews
    },
    files: manifestFiles
}
entries['project.json'] = [new TextEncoder().encode(JSON.stringify(manifest, null, 2)), { mtime: PACKED_AT }]
const archive = zipSync(entries, { level: 6 })
writeFileSync(output, archive)
console.log(`${output}: ${names.length} Files, ${archive.length} bytes`)
for (const record of compilations) console.log(record.sourcePath, '-O' + record.optimization, Object.keys(record.inputs).length, 'inputs')
