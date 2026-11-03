import { fileFingerprint } from './fingerprint.mjs'
import { isValidFilePath } from './files.mjs'

const COMPILATION_TARGETS = ['MIPS', 'RISC-V', 'RISC-V-64', 'X86']
const OPTIMIZATIONS = ['0', '1', '2', '3', 's']
const FINGERPRINT = /^[a-f0-9]{32}$/

/**
 * What is wrong with the Compilation records a port ships, as the editor wrote them: a record it
 * would refuse, or Generated assembly that is not what Compile made from these sources. The
 * editor would show such assembly as stale, or as edited by hand; here it fails the build.
 *
 * The editor also recomputes what the source includes, which needs its C scanner. Recomputing the
 * recorded inputs is enough here: an include can only appear by its includer changing.
 */
export function compilationProblems(records, files, target) {
    if (!Array.isArray(records)) return ['its Compilation records are not a list']
    const problems = []
    const outputs = new Set()
    for (const record of records) {
        const shape = recordShapeProblem(record, target)
        if (shape) {
            problems.push(`Compilation record ${JSON.stringify(record?.sourcePath)}: ${shape}`)
            continue
        }
        if (outputs.has(record.outputPath)) {
            problems.push(`two Compilation records write ${record.outputPath}`)
            continue
        }
        outputs.add(record.outputPath)
        const again = `Compile ${record.sourcePath} again`
        if (record.renamedInput) {
            problems.push(`${record.outputPath} is stale: an input was renamed. ${again}`)
            continue
        }
        const changed = Object.entries(record.inputs)
            .filter(([path, fingerprint]) => fileFingerprint(files[path]) !== fingerprint)
            .map(([path]) => (files[path] ? path : `${path} (missing)`))
        if (changed.length) {
            problems.push(
                `${record.outputPath} is stale: ${changed.join(', ')} changed since it was compiled. ${again}`
            )
        } else if (!files[record.outputPath]) {
            problems.push(`${record.outputPath} is missing. ${again}`)
        } else if (fileFingerprint(files[record.outputPath]) !== record.outputFingerprint) {
            problems.push(`${record.outputPath} was changed after Compile wrote it. ${again}`)
        }
    }
    return problems
}

/** Why the editor would refuse `record` (`cleanCompilationRecords`), or nothing. */
function recordShapeProblem(record, target) {
    if (!record || typeof record !== 'object' || Array.isArray(record)) return 'not a record'
    if (!isValidFilePath(record.sourcePath) || !isValidFilePath(record.outputPath))
        return 'invalid sourcePath or outputPath'
    if (record.sourcePath === record.outputPath) return 'its source is its output'
    if (!COMPILATION_TARGETS.includes(record.target))
        return `${record.target} has no Source compilation`
    if (record.target !== target.name) return `compiled for ${record.target}, not ${target.name}`
    if (record.target === 'X86' && 'assemblerProfile' in record)
        return 'x86 Generated assembly takes no assembler profile'
    if (!['c', 'cpp'].includes(record.language)) return `unknown language ${record.language}`
    if (typeof record.compilerId !== 'string' || !/^[\w.+-]{1,80}$/.test(record.compilerId))
        return 'invalid compilerId'
    if (!OPTIMIZATIONS.includes(record.optimization))
        return `unknown optimization ${record.optimization}`
    if (typeof record.outputFingerprint !== 'string' || !FINGERPRINT.test(record.outputFingerprint))
        return 'invalid outputFingerprint'
    if (!record.inputs || typeof record.inputs !== 'object' || Array.isArray(record.inputs))
        return 'no inputs'
    const inputs = Object.entries(record.inputs)
    if (!inputs.some(([path]) => path === record.sourcePath)) return 'its source is not an input'
    if (inputs.some(([path, hash]) => !isValidFilePath(path) || !FINGERPRINT.test(hash)))
        return 'invalid inputs'
}
