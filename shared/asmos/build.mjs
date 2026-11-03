// asmOS packs one archive for each machine it runs on, from one tree: the shared os/ with the
// machine's own folders of os/platform/, and the disk (Desktop, games, music, pictures, videos,
// code), without tools/. A machine's archive leaves out the other machines' platform folders and
// their Generated assembly, and whatever it has no room for.
import { readdirSync } from 'node:fs'
import { join } from 'node:path'

/** The Generated assembly each Target's Compile writes beside a C source. */
const GENERATED = { X86: /\.c\.asm$/, 'RISC-V': /\.c\.riscv$/, MIPS: /\.c\.mips$/ }

/** The Project of a 640 x 480 asmOS whose one Program's Entry is `entry`. */
function project(entry, settings, display) {
    return {
        programs: { [entry]: { settings, display } },
        selectedProgram: entry,
        openTabs: [entry],
        activeTab: entry,
        fileViews: { 'README.md': 'preview' }
    }
}

/** What MIPS and RISC-V have no room for in their 4 MB of static data and heap. */
const TOO_LARGE = ['os/doom/', 'games/doom/', 'os/apps/video.c', 'os/apps/video/', 'videos/']

/**
 * The machines asmOS runs on: the Target, its folders in os/platform/, its Project, what else it
 * leaves out, the Build outputs it carries, and its own description where example.json's, which
 * is x86's, does not fit.
 */
const MACHINES = [
    {
        target: 'X86',
        platforms: ['x86'],
        //keeping the history of every instruction makes DOOM three times slower
        project: project('os/main.c.asm', { undoEnabled: false }, { width: 640, height: 480 }),
        //the linked program, which DOOM's profiler reads its symbols from
        buildOutputs: ['os/main.c.asm']
    },
    {
        target: 'RISC-V',
        platforms: ['riscv', 'sim'],
        description:
            'A small operating system with processes, a window manager and sixteen applications, Pac-Man and an Assembly Editor among them, in C and RISC-V assembly, on RARS.',
        //keeping the history of every instruction makes drawing half as fast
        project: project(
            'os/main.c.riscv',
            { undoEnabled: false },
            { unitWidth: 1, unitHeight: 1, width: 640, height: 480 }
        ),
        exclude: TOO_LARGE,
        //x86's NASM assembly, which a RISC-V Build would not take
        withoutX86Assembly: true
    },
    {
        target: 'MIPS',
        platforms: ['mips', 'sim'],
        description:
            'A small operating system with processes, a window manager and sixteen applications, Pac-Man and an Assembly Editor that runs RISC-V programs among them, in C and MIPS assembly, on MARS.',
        project: project(
            'os/main.c.mips',
            { undoEnabled: false },
            { unitWidth: 1, unitHeight: 1, width: 640, height: 480 }
        ),
        exclude: TOO_LARGE,
        withoutX86Assembly: true
    }
]

export default function build({ folder, files, pack }) {
    const platforms = readdirSync(join(folder, 'os/platform'), { withFileTypes: true })
        .filter((entry) => entry.isDirectory())
        .map((entry) => entry.name)
    for (const machine of MACHINES) {
        const others = platforms.filter((name) => !machine.platforms.includes(name))
        const all = files({
            exclude: [
                'tools/',
                ...others.map((name) => `os/platform/${name}/`),
                ...(machine.exclude ?? [])
            ]
        })
        const foreign = Object.entries(GENERATED)
            .filter(([target]) => target !== machine.target)
            .map(([, pattern]) => pattern)
        const kept = Object.fromEntries(
            Object.entries(all).filter(
                ([path]) =>
                    !foreign.some((pattern) => pattern.test(path)) &&
                    !(machine.withoutX86Assembly && /\.asm$/.test(path))
            )
        )
        pack({
            target: machine.target,
            ...(machine.description ? { description: machine.description } : {}),
            files: kept,
            project: machine.project,
            buildOutputs: machine.buildOutputs,
            draft: machine.draft ?? false
        })
    }
}
