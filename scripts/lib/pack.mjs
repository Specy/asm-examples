import { zipSync } from 'fflate'
import { compilationProblems } from './compilations.mjs'
import {
    ExampleError,
    FILE_BYTE_LIMIT,
    FILE_COUNT_LIMIT,
    MANIFEST_BYTE_LIMIT,
    isValidFilePath,
    readProjectFile
} from './files.mjs'
import { isAssemblyPath } from './targets.mjs'

/** A fixed time, so that packing an unchanged example gives the same archive. */
export const PACKED_AT = Date.UTC(2026, 10, 3)
//A ZIP stores local time, which fflate reads from the Date's local fields: the same archive on
//every machine needs the same zone
process.env.TZ = 'UTC'
/** The archive version that holds Programs (asm-editor ADR 0054). */
const ARCHIVE_VERSION = 3

const PROJECT_FIELDS = ['programs', 'selectedProgram', 'openTabs', 'activeTab', 'fileViews']
const PROGRAM_FIELDS = ['settings', 'testcases', 'display']
const FILE_VIEWS = ['text', 'hex', 'listing', 'pseudocode', 'preview', 'pixels']
const README = 'README.md'
export const VARIANT = /^[a-z0-9]+(-[a-z0-9]+)*$/

/**
 * Packs one port into a Project archive as the editor writes one: `project.json` and each File's
 * bytes under `files/`. `files` maps each archive path to the file it is read from. What
 * `project` leaves out is filled in: one assembly File is the Entry of the one Program, and the
 * README, shown as a Preview, opens beside the Entries, the Entry active, since the Workbench
 * has no Build button while a Preview is active.
 */
export function packPort({
    id,
    target,
    files,
    name,
    description,
    project = {},
    compilations = [],
    buildOutputs,
    variant
}) {
    const problems = []
    const check = (condition, problem) => {
        if (!condition) problems.push(problem)
        return condition
    }
    if (!isRecord(project)) throw new ExampleError(['its project is not an object'])
    for (const field of Object.keys(project))
        check(PROJECT_FIELDS.includes(field), `unknown project field ${field}`)
    check(typeof name === 'string' && name.trim() !== '', 'it has no name')
    check(typeof description === 'string', 'it has no description')
    if (variant !== undefined)
        check(typeof variant === 'string' && VARIANT.test(variant), `invalid variant ${variant}`)

    const projectFiles = Object.create(null)
    const sources = Object.create(null)
    let bytes = 0
    for (const path of Object.keys(files).sort()) {
        if (!check(isValidFilePath(path), `invalid File path ${JSON.stringify(path)}`)) continue
        try {
            const file = readProjectFile(files[path], path)
            projectFiles[path] = { encoding: file.encoding, content: file.content }
            sources[path] = file.bytes
            bytes += file.bytes.length
        } catch (error) {
            if (!(error instanceof ExampleError)) throw error
            problems.push(...error.problems)
        }
    }
    const paths = Object.keys(projectFiles)
    check(paths.length > 0, 'it has no Files')
    check(paths.length <= FILE_COUNT_LIMIT, `${paths.length} Files, over ${FILE_COUNT_LIMIT}`)
    check(bytes <= FILE_BYTE_LIMIT, `${bytes} bytes of Files, over 16 MiB`)
    const isFile = (path) => typeof path === 'string' && path in projectFiles

    let programs = project.programs
    if (programs === undefined) {
        const candidates = paths.filter((path) => isAssemblyPath(path, target))
        programs = candidates.length === 1 ? { [candidates[0]]: {} } : {}
        check(
            candidates.length === 1,
            candidates.length
                ? `several assembly Files could be the Entry (${candidates.join(', ')}): name the Programs in project.programs`
                : `no ${target.assembly.map((extension) => '.' + extension).join(' ')} File to be the Entry: name the Programs in project.programs`
        )
    } else if (check(isRecord(programs), 'project.programs is not an object')) {
        for (const [entry, program] of Object.entries(programs)) {
            check(isFile(entry), `the Entry ${entry} is not a File`)
            if (!check(isRecord(program), `Program ${entry} is not an object`)) continue
            for (const field of Object.keys(program))
                check(PROGRAM_FIELDS.includes(field), `unknown field ${field} in Program ${entry}`)
            check(
                program.settings === undefined || isRecord(program.settings),
                `Program ${entry}: settings is not an object`
            )
            check(
                program.testcases === undefined || Array.isArray(program.testcases),
                `Program ${entry}: testcases is not a list`
            )
            check(
                program.display === undefined || isRecord(program.display),
                `Program ${entry}: display is not an object`
            )
        }
    } else programs = {}
    const entries = Object.keys(programs)

    const selectedProgram = project.selectedProgram ?? entries[0]
    if (project.selectedProgram !== undefined)
        check(entries.includes(selectedProgram), `selectedProgram ${selectedProgram} is no Program`)

    const openTabs = project.openTabs ?? [
        ...(isFile(README) ? [README] : []),
        ...entries.filter((entry) => entry !== README)
    ]
    if (check(Array.isArray(openTabs), 'project.openTabs is not a list'))
        for (const tab of openTabs) check(isFile(tab), `the tab ${tab} is not a File`)
    const activeTab =
        project.activeTab ?? (openTabs.includes(selectedProgram) ? selectedProgram : openTabs[0])
    if (project.activeTab !== undefined)
        check(openTabs.includes(activeTab), `activeTab ${activeTab} is not an open tab`)

    const fileViews =
        project.fileViews ??
        (project.openTabs === undefined && openTabs.includes(README)
            ? { [README]: 'preview' }
            : undefined)
    if (fileViews !== undefined && check(isRecord(fileViews), 'project.fileViews is not an object'))
        for (const [path, view] of Object.entries(fileViews)) {
            check(isFile(path), `the File view of ${path} is for no File`)
            check(FILE_VIEWS.includes(view), `unknown File view ${view} for ${path}`)
        }

    problems.push(...compilationProblems(compilations, projectFiles, target))

    if (
        buildOutputs !== undefined &&
        check(Array.isArray(buildOutputs), 'buildOutputs is not a list')
    ) {
        check(target.name === 'X86', `only x86 saves a Build output, not ${target.name}`)
        for (const entry of buildOutputs)
            check(entries.includes(entry), `buildOutputs names ${entry}, which is no Program`)
    }
    if (problems.length) throw new ExampleError(problems)

    const manifest = {
        version: ARCHIVE_VERSION,
        project: {
            id: '',
            name,
            description,
            language: target.name,
            createdAt: PACKED_AT,
            updatedAt: PACKED_AT,
            programs: Object.fromEntries(
                Object.entries(programs).map(([entry, program]) => [
                    entry,
                    { settings: {}, testcases: [], ...program }
                ])
            ),
            selectedProgram,
            ...(compilations.length ? { compilations } : {}),
            openTabs,
            activeTab,
            ...(fileViews ? { fileViews } : {})
        },
        files: Object.fromEntries(
            paths.map((path) => [path, { encoding: projectFiles[path].encoding }])
        )
    }
    const manifestBytes = new TextEncoder().encode(JSON.stringify(manifest, null, 2))
    if (manifestBytes.length > MANIFEST_BYTE_LIMIT)
        throw new ExampleError([`its project.json is ${manifestBytes.length} bytes, over 1 MiB`])
    const entriesOfZip = {}
    for (const path of paths)
        entriesOfZip[`files/${path}`] = [new Uint8Array(sources[path]), { mtime: PACKED_AT }]
    entriesOfZip['project.json'] = [manifestBytes, { mtime: PACKED_AT }]
    const archive = zipSync(entriesOfZip, { level: 6 })

    const archivePath = `${target.folder}/${id}${variant ? `-${variant}` : ''}.asmproj`
    return {
        archivePath,
        archive,
        manifest,
        files: projectFiles,
        port: {
            target: target.name,
            archive: archivePath,
            bytes: archive.length,
            ...(variant ? { variant } : {}),
            ...(buildOutputs?.length ? { buildOutputs } : {})
        }
    }
}

export function isRecord(value) {
    return typeof value === 'object' && value !== null && !Array.isArray(value)
}
