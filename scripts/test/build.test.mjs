import assert from 'node:assert/strict'
import { describe, it } from 'node:test'
import { contentFingerprint, fileFingerprint } from '../lib/fingerprint.mjs'
import { fixture } from './fixture.mjs'
import { buildExample, findExamples } from '../lib/examples.mjs'

const example = (name, more = {}) => ({ name, description: `${name}, described.`, ...more })

describe('the fingerprint', () => {
    it('is the editor’s, as its fingerprints.ts computes them', () => {
        //computed with asm-editor's src/lib/sourceCompilation/fingerprints.ts
        assert.equal(contentFingerprint(''), '027ae52e1ca83f5f86fc3073982eea02')
        assert.equal(contentFingerprint('hello'), '64c6a4161994e703045c8cfd790ecfe8')
        assert.equal(
            contentFingerprint('plain\0int main(void) {}\n'),
            '8a94fb96360b619b8078adee3ce737e3'
        )
        assert.equal(contentFingerprint('é😀\u{feff}x'), 'acce1ded2790c9fd2c954201a7cb9611')
    })
})

describe('an example for one Target', () => {
    it('is one archive whose one assembly File is the Entry, with the README beside it', () => {
        const repo = fixture({
            'mips/bitmap-tour/example.json': example('Bitmap tour'),
            'mips/bitmap-tour/bitmap-tour.asm': 'li $v0, 10\nsyscall\n',
            'mips/bitmap-tour/README.md': '# Bitmap tour\n',
            'mips/bitmap-tour/palette.bmp': new Uint8Array([0x42, 0x4d, 0, 0xff])
        })
        const { status, output } = repo.run('build.mjs')
        assert.equal(status, 0, output)
        const { manifest, files, names } = repo.archive('mips/bitmap-tour.asmproj')
        assert.deepEqual(manifest.project.programs, {
            'bitmap-tour.asm': { settings: {}, testcases: [] }
        })
        assert.equal(manifest.version, 3)
        assert.equal(manifest.project.language, 'MIPS')
        assert.equal(manifest.project.name, 'Bitmap tour')
        assert.equal(manifest.project.selectedProgram, 'bitmap-tour.asm')
        assert.deepEqual(manifest.project.openTabs, ['README.md', 'bitmap-tour.asm'])
        assert.equal(manifest.project.activeTab, 'bitmap-tour.asm')
        assert.deepEqual(manifest.project.fileViews, { 'README.md': 'preview' })
        assert.deepEqual(manifest.files, {
            'README.md': { encoding: 'plain' },
            'bitmap-tour.asm': { encoding: 'plain' },
            'palette.bmp': { encoding: 'base64' }
        })
        //example.json describes the example and is no File of it
        assert.deepEqual(names.sort(), [
            'files/README.md',
            'files/bitmap-tour.asm',
            'files/palette.bmp',
            'project.json'
        ])
        assert.equal(files['palette.bmp'].toString('hex'), '424d00ff')
        assert.deepEqual(repo.index(), {
            version: 1,
            examples: {
                'bitmap-tour': {
                    name: 'Bitmap tour',
                    description: 'Bitmap tour, described.',
                    tags: [],
                    license: 'MIT',
                    path: 'mips/bitmap-tour',
                    ports: [
                        {
                            target: 'MIPS',
                            archive: 'mips/bitmap-tour.asmproj',
                            bytes: repo.archiveBytes('mips/bitmap-tour.asmproj').length
                        }
                    ]
                }
            },
            targets: { MIPS: ['bitmap-tour'] }
        })
    })

    it('packs to the same bytes again, in any time zone', () => {
        const repo = fixture({
            'z80/bounce/example.json': example('Bounce'),
            'z80/bounce/bounce.z80': 'halt\n'
        })
        assert.equal(repo.run('build.mjs', [], { TZ: 'Europe/Rome' }).status, 0)
        const first = repo.archiveBytes('z80/bounce.asmproj')
        assert.equal(repo.run('build.mjs', [], { TZ: 'Pacific/Kiritimati' }).status, 0)
        assert.deepEqual(repo.archiveBytes('z80/bounce.asmproj'), first)
    })

    it('takes its Programs, tabs and File views from example.json', () => {
        const repo = fixture({
            'risc-v/pacman/example.json': example('Pac-Man', {
                project: {
                    programs: {
                        'assembly/main.s': { display: { width: 224, height: 288 } },
                        'c/pacman.c.riscv': {}
                    },
                    selectedProgram: 'assembly/main.s',
                    openTabs: ['assembly/main.s', 'c/pacman.c'],
                    activeTab: 'c/pacman.c'
                }
            }),
            'risc-v/pacman/assembly/main.s': 'nop\n',
            'risc-v/pacman/assembly/video.s': 'nop\n',
            'risc-v/pacman/c/pacman.c': 'int main(void) { return 0; }\n',
            'risc-v/pacman/c/pacman.c.riscv': 'nop\n'
        })
        assert.equal(repo.run('build.mjs').status, 0)
        const { project } = repo.archive('risc-v/pacman.asmproj').manifest
        assert.deepEqual(project.programs, {
            'assembly/main.s': {
                settings: {},
                testcases: [],
                display: { width: 224, height: 288 }
            },
            'c/pacman.c.riscv': { settings: {}, testcases: [] }
        })
        assert.deepEqual(project.openTabs, ['assembly/main.s', 'c/pacman.c'])
        assert.equal(project.activeTab, 'c/pacman.c')
        assert.equal(project.fileViews, undefined)
    })

    it('fails without a single assembly File to be the Entry, or with Programs that are not there', () => {
        const repo = fixture({
            'm68k/two/example.json': example('Two'),
            'm68k/two/a.x68': 'nop\n',
            'm68k/two/b.x68': 'nop\n',
            'm68k/none/example.json': example('None'),
            'm68k/none/notes.txt': 'text\n',
            'm68k/wrong/example.json': example('Wrong', {
                project: { programs: { 'main.x68': {} }, openTabs: ['other.x68'] }
            }),
            'm68k/wrong/a.x68': 'nop\n'
        })
        const { status, output } = repo.run('build.mjs')
        assert.equal(status, 1)
        assert.match(
            output,
            /m68k\/two: several assembly Files could be the Entry \(a\.x68, b\.x68\)/
        )
        assert.match(output, /m68k\/none: no \.x68 \.m68k \.asm \.s File to be the Entry/)
        assert.match(output, /m68k\/wrong: the Entry main\.x68 is not a File/)
        assert.match(output, /m68k\/wrong: the tab other\.x68 is not a File/)
        assert.match(output, /nothing written/)
        assert.equal(repo.exists('dist'), false)
    })
})

describe('a shared example', () => {
    it('is one archive a Target folder, each with the example’s other Files', () => {
        const repo = fixture({
            'shared/bouncing-ball/example.json': example('Bouncing ball', {
                tags: ['screen'],
                ports: { z80: { name: 'Bouncing ball on the Z80' } }
            }),
            'shared/bouncing-ball/README.md': '# Bouncing ball\n',
            'shared/bouncing-ball/assets/ball.bmp': new Uint8Array([1, 2]),
            'shared/bouncing-ball/z80/ball.z80': 'halt\n',
            'shared/bouncing-ball/m68k/ball.x68': 'nop\n',
            'shared/bouncing-ball/risc-v/ball.s': 'nop\n'
        })
        const { status, output } = repo.run('build.mjs')
        assert.equal(status, 0, output)
        const z80 = repo.archive('z80/bouncing-ball.asmproj')
        assert.deepEqual(Object.keys(z80.manifest.files), [
            'README.md',
            'assets/ball.bmp',
            'ball.z80'
        ])
        assert.equal(z80.manifest.project.name, 'Bouncing ball on the Z80')
        assert.equal(
            repo.archive('m68k/bouncing-ball.asmproj').manifest.project.name,
            'Bouncing ball'
        )
        const index = repo.index()
        assert.deepEqual(
            index.examples['bouncing-ball'].ports.map((port) => port.target),
            ['M68K', 'RISC-V', 'Z80']
        )
        assert.deepEqual(index.examples['bouncing-ball'].tags, ['screen'])
        assert.deepEqual(index.targets, {
            M68K: ['bouncing-ball'],
            'RISC-V': ['bouncing-ball'],
            Z80: ['bouncing-ball']
        })
    })

    it('fails when a port’s File and a shared one have the same path', () => {
        const repo = fixture({
            'shared/ball/example.json': example('Ball'),
            'shared/ball/README.md': '# Ball\n',
            'shared/ball/mips/README.md': '# Ball on MIPS\n',
            'shared/ball/mips/ball.asm': 'nop\n',
            'shared/ball/z80/ball.z80': 'halt\n'
        })
        const { status, output } = repo.run('build.mjs')
        assert.equal(status, 1)
        assert.match(
            output,
            /shared\/ball: mips\/README\.md and the shared README\.md would be the same File/
        )
    })
})

describe('Generated assembly', () => {
    const source = 'int main(void) { return 0; }\n'
    const header = '#define N 3\n'
    const output = 'main: li a0, 0\nret\n'
    const record = (inputs = { 'game.c': source, 'game.h': header }, generated = output) => ({
        'risc-v': [
            {
                assemblerProfile: 'gnu-compiler-v1',
                runtimeAbi: 'v1',
                sourcePath: 'game.c',
                outputPath: 'game.c.riscv',
                target: 'RISC-V',
                language: 'c',
                compilerId: 'rv32-cclang2110',
                optimization: '2',
                inputs: Object.fromEntries(
                    Object.entries(inputs).map(([path, text]) => [
                        path,
                        fileFingerprint({ encoding: 'plain', content: text })
                    ])
                ),
                outputFingerprint: fileFingerprint({ encoding: 'plain', content: generated })
            }
        ]
    })
    const files = (compilations) => ({
        'risc-v/game/example.json': example('Game'),
        'risc-v/game/game.c': source,
        'risc-v/game/game.h': header,
        'risc-v/game/game.c.riscv': output,
        'risc-v/game/compilations.json': compilations
    })

    it('ships with its Compilation record while it is what Compile made', () => {
        const repo = fixture(files(record()))
        const { status, output: log } = repo.run('build.mjs')
        assert.equal(status, 0, log)
        const { manifest } = repo.archive('risc-v/game.asmproj')
        assert.deepEqual(manifest.project.compilations, record()['risc-v'])
        assert.equal(manifest.project.selectedProgram, 'game.c.riscv')
        assert.equal('compilations.json' in manifest.files, false)
    })

    it('fails once a source it was compiled from changes', () => {
        const repo = fixture(files(record({ 'game.c': source, 'game.h': '#define N 2\n' })))
        const { status, output: log } = repo.run('build.mjs')
        assert.equal(status, 1)
        assert.match(
            log,
            /game\.c\.riscv is stale: game\.h changed since it was compiled\. Compile game\.c again/
        )
    })

    it('fails once it is changed by hand', () => {
        const repo = fixture(files(record(undefined, 'main: ret\n')))
        const { status, output: log } = repo.run('build.mjs')
        assert.equal(status, 1)
        assert.match(log, /game\.c\.riscv was changed after Compile wrote it/)
    })

    it('fails with a record for a Target the example has no port for', () => {
        const repo = fixture(files({ ...record(), mips: [] }))
        const { status, output: log } = repo.run('build.mjs')
        assert.equal(status, 1)
        assert.match(log, /compilations\.json has records for mips, which it has no port for/)
    })
})

describe('build.mjs', () => {
    const tree = {
        'shared/os/example.json': example('OS', { license: 'GPL-2.0-or-later' }),
        'shared/os/LICENSE': 'GPL\n',
        'shared/os/os/main.c': 'int main(void) { return 0; }\n',
        'shared/os/os/main.c.asm': 'ret\n',
        'shared/os/os/platform/x86/switch.asm': 'ret\n',
        'shared/os/os/platform/riscv/switch.riscv': 'ret\n',
        'shared/os/tools/harness.mjs': '//not packed\n'
    }

    it('keeps draft ports available to compilation tools without publishing an unfinished Project', async () => {
        const repo = fixture({
            ...tree,
            'shared/os/compilations.json': { 'risc-v': [{ sourcePath: 'os/main.c', outputPath: 'os/main.c.riscv' }] },
            'shared/os/build.mjs': `export default function build({ files, pack }) {
                pack({ target: 'X86', files: files({ include: ['os/main.c.asm'] }) })
                pack({ target: 'RISC-V', files: files({ include: ['os/main.c'] }), draft: true,
                    project: { programs: { 'os/main.c.riscv': {} } } })
            }`
        })
        const result = repo.run('build.mjs')
        assert.equal(result.status, 0, result.output)
        assert.deepEqual(repo.index().examples.os.ports.map((port) => port.target), ['X86'])
        assert.equal(repo.exists('dist/risc-v/os.asmproj'), false)
        const example = findExamples(repo.root).examples[0]
        const { captured, compilations } = await buildExample(example, { capture: true })
        assert.equal(captured.find((port) => port.target.name === 'RISC-V').draft, true)
        assert.equal(compilations['risc-v'][0].sourcePath, 'os/main.c')
    })

    it('packs what it asks for, its variants and Build outputs going into the index', () => {
        const repo = fixture({
            ...tree,
            'shared/os/build.mjs': `export default async function build({ files, pack }) {
                const project = { programs: { 'os/main.c.asm': { settings: { undoEnabled: false } } } }
                for (const variant of [undefined, 'lite'])
                    pack({ target: 'X86', variant, files: files({ include: ['os/', 'LICENSE'], exclude: ['os/platform/riscv/'] }), project, buildOutputs: ['os/main.c.asm'] })
                pack({ target: 'risc-v', files: files({ include: ['os/main.c', 'os/platform/riscv/'] }), project: { programs: { 'os/platform/riscv/switch.riscv': {} } } })
            }`
        })
        const { status, output } = repo.run('build.mjs')
        assert.equal(status, 0, output)
        const x86 = repo.archive('x86/os.asmproj')
        assert.deepEqual(Object.keys(x86.manifest.files), [
            'LICENSE',
            'os/main.c',
            'os/main.c.asm',
            'os/platform/x86/switch.asm'
        ])
        assert.equal(x86.manifest.project.programs['os/main.c.asm'].settings.undoEnabled, false)
        assert.deepEqual(
            repo.archiveBytes('x86/os-lite.asmproj'),
            repo.archiveBytes('x86/os.asmproj')
        )
        const ports = repo.index().examples.os.ports
        assert.deepEqual(
            ports.map(({ target, archive, variant, buildOutputs }) => ({
                target,
                archive,
                variant,
                buildOutputs
            })),
            [
                {
                    target: 'RISC-V',
                    archive: 'risc-v/os.asmproj',
                    variant: undefined,
                    buildOutputs: undefined
                },
                {
                    target: 'X86',
                    archive: 'x86/os.asmproj',
                    variant: undefined,
                    buildOutputs: ['os/main.c.asm']
                },
                {
                    target: 'X86',
                    archive: 'x86/os-lite.asmproj',
                    variant: 'lite',
                    buildOutputs: ['os/main.c.asm']
                }
            ]
        )
        assert.equal(repo.index().examples.os.license, 'GPL-2.0-or-later')
    })

    it('fails on what it gets wrong', () => {
        const repo = fixture({
            ...tree,
            'shared/os/build.mjs': `export default async function build({ files, pack }) {
                pack({ target: 'risc-v', files: files({ include: ['os/platform/riscv/'] }), buildOutputs: ['os/platform/riscv/switch.riscv'] })
            }`,
            'shared/typo/example.json': example('Typo'),
            'shared/typo/build.mjs': `export default ({ files }) => { files({ include: ['src/'] }) }`,
            'shared/twice/example.json': example('Twice'),
            'shared/twice/a.asm': 'ret\n',
            'shared/twice/build.mjs': `export default ({ files, pack }) => {
                pack({ target: 'X86', files: files() })
                pack({ target: 'X86', files: files() })
            }`
        })
        const { status, output } = repo.run('build.mjs')
        assert.equal(status, 1)
        assert.match(output, /shared\/os: only x86 saves a Build output, not RISC-V/)
        assert.match(output, /shared\/typo: files\(\): src\/ matches no File of the example/)
        assert.match(
            output,
            /shared\/twice: two archives are x86\/twice\.asmproj: give one a variant/
        )
    })
})

describe('the repository', () => {
    it('fails on folders, files and example.json fields it does not know', () => {
        const repo = fixture({
            'riscv/pacman/example.json': example('Pac-Man'),
            'mips/loose.asm': 'nop\n',
            'mips/ball/example.json': example('Ball', { tag: ['x'] }),
            'mips/ball/ball.asm': 'nop\n',
            'z80/ball/example.json': example('Ball'),
            'z80/ball/ball.z80': 'halt\n',
            'm68k/apache/example.json': example('Apache', { license: 'Apache-2.0' }),
            'm68k/apache/apache.x68': 'nop\n',
            'm68k/latin/example.json': example('Latin'),
            'm68k/latin/latin.x68': new Uint8Array([0x3b, 0xe8, 0x0a])
        })
        const { status, output } = repo.run('build.mjs')
        assert.equal(status, 1)
        assert.match(output, /riscv\/ is neither a Target folder/)
        assert.match(output, /mips\/loose\.asm: only example folders belong in mips\//)
        assert.match(output, /mips\/ball: unknown field tag in example\.json/)
        assert.match(
            output,
            /z80\/ball and mips\/ball are both called ball|mips\/ball and z80\/ball are both called ball/
        )
        assert.match(
            output,
            /m68k\/apache: its license is Apache-2\.0, not the repository's MIT, so it needs a LICENSE File/
        )
        assert.match(output, /m68k\/latin: latin\.x68 is not UTF-8 text/)
    })

    it('keeps the README’s table of examples up to date, and --check fails while it is not', () => {
        const repo = fixture({
            'mips/ball/example.json': example('Ball | bounce'),
            'mips/ball/ball.asm': 'nop\n',
            'shared/tour/example.json': example('A tour'),
            'shared/tour/mips/tour.asm': 'nop\n',
            'shared/tour/risc-v/tour.s': 'nop\n'
        })
        const check = repo.run('build.mjs', ['--check'])
        assert.equal(check.status, 1)
        assert.match(check.output, /its table of examples is out of date; run npm run build/)
        assert.equal(repo.run('build.mjs').status, 0)
        assert.equal(
            repo.read('README.md'),
            [
                '# Examples',
                '',
                '<!-- examples:start -->',
                '| Example | Targets | What it is | License |',
                '| ------- | ------- | ---------- | ------- |',
                '| [A tour](shared/tour/) | MIPS, RISC-V | A tour, described. | MIT |',
                '| [Ball \\| bounce](mips/ball/) | MIPS | Ball \\| bounce, described. | MIT |',
                '<!-- examples:end -->',
                ''
            ].join('\n')
        )
        assert.equal(repo.run('build.mjs', ['--check']).status, 0)
    })
})
