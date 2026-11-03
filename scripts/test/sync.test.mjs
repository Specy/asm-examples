import assert from 'node:assert/strict'
import { writeFileSync } from 'node:fs'
import { join } from 'node:path'
import { describe, it } from 'node:test'
import { zipSync } from 'fflate'
import { fileFingerprint } from '../lib/fingerprint.mjs'
import { fixture } from './fixture.mjs'

/** A Project as the editor exports it: `files` maps paths to text or bytes. */
function exportArchive(repo, name, { version = 3, project, files }) {
    const entries = {}
    const manifestFiles = {}
    for (const [path, content] of Object.entries(files)) {
        const binary = content instanceof Uint8Array
        manifestFiles[path] = { encoding: binary ? 'base64' : 'plain' }
        entries[`files/${path}`] = binary ? content : new TextEncoder().encode(content)
    }
    const manifest = {
        version,
        project: { id: 'abc', description: '', createdAt: 1, updatedAt: 2, ...project },
        files: manifestFiles
    }
    entries['project.json'] = new TextEncoder().encode(JSON.stringify(manifest))
    const path = join(repo.root, name)
    writeFileSync(path, zipSync(entries))
    return path
}

const plain = (content) => fileFingerprint({ encoding: 'plain', content })

describe('sync', () => {
    it('brings back the Files and the Compilation records of a Compile, and says what example.json gives otherwise', () => {
        const repo = fixture({
            'risc-v/game/example.json': {
                name: 'Game',
                description: 'A game.',
                project: { programs: { 'game.c.riscv': {} }, openTabs: ['game.c', 'game.c.riscv'] }
            },
            'risc-v/game/game.c': 'int main(void) { return 1; }\n',
            'risc-v/game/game.c.riscv': 'old\n'
        })
        assert.equal(repo.run('build.mjs').status, 0)
        const source = 'int main(void) { return 2; }\n'
        const generated = 'main: li a0, 2\nret\n'
        const record = {
            assemblerProfile: 'gnu-compiler-v1',
            runtimeAbi: 'v1',
            sourcePath: 'game.c',
            outputPath: 'game.c.riscv',
            target: 'RISC-V',
            language: 'c',
            compilerId: 'rv32-cclang2110',
            optimization: '2',
            inputs: { 'game.c': plain(source) },
            outputFingerprint: plain(generated)
        }
        //one Program, so the editor writes the oldest version, the Program's parts in the project
        const archive = exportArchive(repo, 'export.asmproj', {
            version: 1,
            project: {
                name: 'Game',
                language: 'RISC-V',
                entry: 'game.c.riscv',
                settings: {},
                testcases: [],
                compilations: [record],
                builds: [{ entry: 'game.c.riscv', outputPath: 'game' }],
                openTabs: ['game.c', 'game.c.riscv', 'notes.txt'],
                activeTab: 'game.c'
            },
            files: {
                'game.c': source,
                'game.c.riscv': generated,
                'notes.txt': 'todo\n',
                game: new Uint8Array([0x7f, 0x45, 0x4c, 0x46])
            }
        })
        const { status, output } = repo.run('sync.mjs', [archive, 'game'])
        assert.equal(status, 0, output)
        assert.equal(repo.read('risc-v/game/game.c'), source)
        assert.equal(repo.read('risc-v/game/game.c.riscv'), generated)
        assert.equal(repo.read('risc-v/game/notes.txt'), 'todo\n')
        assert.equal(repo.exists('risc-v/game/game'), false)
        assert.deepEqual(JSON.parse(repo.read('risc-v/game/compilations.json')), {
            'risc-v': [record]
        })
        assert.match(output, /left out game: a Build output/)
        assert.match(output, /changed:\n {2}game\.c\n {2}game\.c\.riscv/)
        assert.match(output, /added:\n {2}risc-v\/game\/notes\.txt/)
        assert.match(
            output,
            /openTabs: the archive has \["game\.c","game\.c\.riscv","notes\.txt"\], example\.json gives \["game\.c","game\.c\.riscv"\]/
        )
        assert.match(
            output,
            /activeTab: the archive has "game\.c", example\.json gives "game\.c\.riscv"/
        )

        const built = repo.run('build.mjs')
        assert.equal(built.status, 0, built.output)
        assert.deepEqual(repo.archive('risc-v/game.asmproj').manifest.project.compilations, [
            record
        ])
        repo.write({ 'risc-v/game/game.c': 'int main(void) { return 3; }\n' })
        assert.match(
            repo.run('build.mjs').output,
            /game\.c\.riscv is stale: game\.c changed since it was compiled/
        )
        assert.equal(repo.run('sync.mjs', [archive, 'risc-v/game']).status, 0)
        assert.equal(repo.run('build.mjs').status, 0)

        //the same records with their fields in another order, as the editor may write them
        const reordered = exportArchive(repo, 'reordered.asmproj', {
            version: 1,
            project: {
                name: 'Game',
                language: 'RISC-V',
                entry: 'game.c.riscv',
                settings: {},
                testcases: [],
                compilations: [Object.fromEntries(Object.entries(record).reverse())],
                openTabs: ['game.c', 'game.c.riscv', 'notes.txt'],
                activeTab: 'game.c'
            },
            files: {
                'game.c': source,
                'game.c.riscv': generated,
                'notes.txt': 'todo\n'
            }
        })
        const before = repo.read('risc-v/game/compilations.json')
        const again = repo.run('sync.mjs', [reordered, 'game'])
        assert.match(again.output, /nothing to change/)
        assert.equal(repo.read('risc-v/game/compilations.json'), before)
    })

    it('gives a shared example a new port, writes its shared Files back to the example and keeps them on --delete', () => {
        const repo = fixture({
            'shared/ball/example.json': { name: 'Ball', description: 'A ball.' },
            'shared/ball/README.md': '# Ball\n',
            'shared/ball/mips/ball.asm': 'nop\n',
            'shared/ball/mips/notes.txt': 'notes\n'
        })
        const z80 = exportArchive(repo, 'z80.asmproj', {
            project: {
                name: 'Ball',
                language: 'Z80',
                programs: { 'ball.z80': { settings: {}, testcases: [] } }
            },
            files: { 'README.md': '# Ball, on two machines\n', 'ball.z80': 'halt\n' }
        })
        let result = repo.run('sync.mjs', [z80, 'shared/ball'])
        assert.equal(result.status, 0, result.output)
        assert.match(result.output, /a new Z80 port: shared\/ball\/z80\//)
        assert.equal(repo.read('shared/ball/z80/ball.z80'), 'halt\n')
        assert.equal(repo.read('shared/ball/README.md'), '# Ball, on two machines\n')
        assert.equal(repo.exists('shared/ball/z80/README.md'), false)

        const mips = exportArchive(repo, 'mips.asmproj', {
            project: {
                name: 'Ball',
                language: 'MIPS',
                programs: { 'ball.asm': { settings: {}, testcases: [] } }
            },
            files: { 'ball.asm': 'nop\n' }
        })
        result = repo.run('sync.mjs', [mips, 'ball', '--delete'])
        assert.equal(result.status, 0, result.output)
        assert.equal(repo.exists('shared/ball/mips/notes.txt'), false)
        assert.equal(repo.exists('shared/ball/README.md'), true)
        assert.match(
            result.output,
            /kept shared\/ball\/README\.md: the example's other ports have it too/
        )
        assert.equal(repo.run('build.mjs').status, 0)
        assert.deepEqual(
            repo.index().examples.ball.ports.map((port) => port.target),
            ['MIPS', 'Z80']
        )
    })

    it('refuses an archive of another Target than a one-Target example’s', () => {
        const repo = fixture({
            'mips/ball/example.json': { name: 'Ball', description: 'A ball.' },
            'mips/ball/ball.asm': 'nop\n'
        })
        const archive = exportArchive(repo, 'z80.asmproj', {
            project: { name: 'Ball', language: 'Z80', programs: { 'ball.z80': {} } },
            files: { 'ball.z80': 'halt\n' }
        })
        const { status, output } = repo.run('sync.mjs', [archive, 'ball'])
        assert.equal(status, 1)
        assert.match(
            output,
            /mips\/ball is for MIPS, and the archive is Z80: move the example to shared\/ first/
        )
    })
})
