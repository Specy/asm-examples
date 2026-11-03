/**
 * The Targets an example can be written for: the repository's folder for each, the name the
 * editor gives it (an archive's `language`, the index's keys) and the extensions of the assembly
 * Files a Build can begin from, which pick a port's Entry when its `example.json` names none.
 */
export const TARGETS = [
    { folder: 'm68k', name: 'M68K', assembly: ['x68', 'm68k', 'asm', 's'] },
    { folder: 'mips', name: 'MIPS', assembly: ['asm', 's', 'mips'] },
    { folder: 'risc-v', name: 'RISC-V', assembly: ['s', 'asm', 'riscv'] },
    { folder: 'risc-v-64', name: 'RISC-V-64', assembly: ['s', 'asm', 'riscv'] },
    { folder: 'x86', name: 'X86', assembly: ['asm', 's', 'nasm'] },
    { folder: 'z80', name: 'Z80', assembly: ['z80', 'asm', 's'] }
]

/** The folder of the examples written for more than one Target. */
export const SHARED_FOLDER = 'shared'

export const targetByFolder = new Map(TARGETS.map((target) => [target.folder, target]))
export const targetByName = new Map(TARGETS.map((target) => [target.name, target]))

/** A Target from its folder or its editor name, as `pack` takes either. */
export function findTarget(value) {
    return targetByFolder.get(value) ?? targetByName.get(value)
}

/** Whether `path` names an assembly File of `target` by its extension. */
export function isAssemblyPath(path, target) {
    const name = path.slice(path.lastIndexOf('/') + 1)
    const dot = name.lastIndexOf('.')
    return dot > 0 && target.assembly.includes(name.slice(dot + 1).toLowerCase())
}
