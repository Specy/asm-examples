// Brings a Project exported from the editor back into its example: the Files it changed, added or
// (with --delete) removed, and its Compilation records into compilations.json, which is how a C
// example's Generated assembly gets the fingerprints the build checks. example.json and build.mjs
// stay hand-written: where the exported Programs, tabs or File views differ from what they give,
// sync says so.
//
//   npm run sync -- <archive.asmproj> <example> [--variant <variant>] [--delete]
//
// <example> is an example's id or folder (pacman, risc-v/pacman, shared/flappy-bird). The port is
// the archive's Target; a shared example without build.mjs gets a new port folder for a Target it
// lacks. Open the example's archive from dist/, change it and Compile in the editor, export it,
// sync, then build.
import { existsSync, mkdirSync, readFileSync, readdirSync, rmSync, writeFileSync } from 'node:fs'
import { dirname, join, relative } from 'node:path'
import { fileURLToPath } from 'node:url'
import { parseArgs } from 'node:util'
import { unzipSync } from 'fflate'
import { buildExample, exampleFiles, findExamples } from './lib/examples.mjs'
import {
    BUILD_FILE,
    COMPILATIONS_FILE,
    EXAMPLE_FILE,
    ExampleError,
    readProjectFile
} from './lib/files.mjs'
import { isRecord, packPort } from './lib/pack.mjs'
import { findTarget, targetByFolder } from './lib/targets.mjs'

const root = fileURLToPath(new URL('..', import.meta.url))
const { values: options, positionals } = parseArgs({
    allowPositionals: true,
    options: { variant: { type: 'string' }, delete: { type: 'boolean', default: false } }
})
if (positionals.length !== 2)
    fail('usage: npm run sync -- <archive.asmproj> <example> [--variant <variant>] [--delete]')
const [archivePath, name] = positionals

const exported = readArchive(archivePath)
const target = findTarget(exported.project.language)
if (!target)
    fail(`the archive is a ${exported.project.language} Project, which no Target folder holds`)

const { examples } = findExamples(root)
const example = examples.find(
    (candidate) => candidate.id === name || candidate.path === name.replace(/\/+$/, '')
)
if (!example) fail(`there is no example ${name}`)
if (example.target && example.target !== target)
    fail(
        `${example.path} is for ${example.target.name}, and the archive is ${target.name}: move the example to shared/ first`
    )

let built
try {
    built = await buildExample(example, { capture: true })
} catch (error) {
    if (!(error instanceof ExampleError)) throw error
    fail(`${example.path}: ${error.problems.join('\n')}`)
}
const ports = built.captured.filter(
    (port) =>
        port.target === target &&
        (options.variant === undefined || port.variant === options.variant)
)
//where each archive path comes from, and where a File the archive adds goes
let port = ports[0]
let newFolder = example.folder
if (ports.length > 1)
    fail(
        `${example.path} packs ${ports.length} ${target.name} archives: choose one with --variant (${ports.map((port) => port.variant ?? 'none').join(', ')})`
    )
if (!port) {
    if (example.target || existsSync(join(example.folder, BUILD_FILE)))
        fail(
            `${example.path} packs no ${target.name} archive${options.variant ? ` with the variant ${options.variant}` : ''}`
        )
    //a new port of a shared example: the shared Files, and a Target folder for the rest
    const portFolders = readdirSync(example.folder, { withFileTypes: true })
        .filter((entry) => entry.isDirectory() && targetByFolder.has(entry.name))
        .map((entry) => entry.name + '/')
    port = {
        target,
        files: exampleFiles(example.folder)({ exclude: portFolders }),
        project: undefined
    }
    newFolder = join(example.folder, target.folder)
    console.log(`a new ${target.name} port: ${relative(root, newFolder)}/`)
} else if (!example.target && !existsSync(join(example.folder, BUILD_FILE))) {
    newFolder = join(example.folder, target.folder)
}

//the Build outputs the editor saved are made by a Build, never kept here
const derived = new Set((exported.project.builds ?? []).map((record) => record.outputPath))
const changed = []
const added = []
const destinations = Object.create(null)
for (const path of Object.keys(exported.files).sort()) {
    if (derived.has(path)) {
        console.log(`left out ${path}: a Build output`)
        continue
    }
    const destination = port.files[path] ?? join(newFolder, path)
    if (
        dirname(destination) === example.folder &&
        [EXAMPLE_FILE, COMPILATIONS_FILE, BUILD_FILE].includes(path)
    )
        fail(`the archive has a File ${path}, which would replace the example's own ${path}`)
    destinations[path] = destination
    const bytes = exported.files[path].bytes
    const exists = existsSync(destination)
    if (!exists) added.push(path)
    else if (!readFileSync(destination).equals(bytes)) changed.push(path)
    if (!exists || changed.at(-1) === path) {
        mkdirSync(dirname(destination), { recursive: true })
        writeFileSync(destination, bytes)
    }
    const rule = encodingProblem(destination, path, exported.files[path].encoding)
    if (rule) console.warn(rule)
}
const removed = Object.keys(port.files).filter((path) => !(path in exported.files))
const sharedFolder = example.target ? undefined : newFolder
for (const path of removed) {
    const source = port.files[path]
    if (!options.delete) continue
    if (sharedFolder && !source.startsWith(sharedFolder + '/')) {
        console.log(`kept ${relative(root, source)}: the example's other ports have it too`)
        continue
    }
    rmSync(source)
}

const records = exported.project.compilations ?? []
const compilations = { ...built.compilations }
if (records.length) compilations[target.folder] = records
else delete compilations[target.folder]
const compilationsPath = join(example.folder, COMPILATIONS_FILE)
//the editor writes a record's fields in an order of its own: only what they say counts
const canonical = (value) =>
    JSON.stringify(value, (_key, item) =>
        isRecord(item)
            ? Object.fromEntries(Object.entries(item).sort(([a], [b]) => a.localeCompare(b)))
            : item
    )
const recordsChanged = canonical(compilations) !== canonical(built.compilations)
if (recordsChanged) {
    if (Object.keys(compilations).length)
        writeFileSync(compilationsPath, JSON.stringify(compilations, null, 4) + '\n')
    else rmSync(compilationsPath, { force: true })
}

const describe = (paths) => paths.map((path) => `  ${path}`).join('\n')
if (changed.length) console.log(`changed:\n${describe(changed)}`)
if (added.length)
    console.log(`added:\n${describe(added.map((path) => relative(root, destinations[path])))}`)
if (removed.length)
    console.log(
        `${options.delete ? 'removed' : 'not in the archive (--delete removes them)'}:\n${describe(removed)}`
    )
if (recordsChanged)
    console.log(`${COMPILATIONS_FILE}: ${records.length} ${target.name} Compilation records`)
if (!changed.length && !added.length && !recordsChanged && !(options.delete && removed.length))
    console.log('nothing to change')
if (exported.annotated)
    console.warn('the archive has renames or comments, which examples do not keep')
compareProject()

/** Where the archive's Programs, tabs and File views differ from what the example gives. */
function compareProject() {
    const files = Object.fromEntries(
        Object.entries(port.files).filter(([path]) => !(options.delete && removed.includes(path)))
    )
    for (const path of added) files[path] = destinations[path]
    let packed
    try {
        packed = packPort({
            id: example.id,
            target,
            files,
            name: port.name ?? built.meta.name,
            description: port.description ?? built.meta.description,
            project: port.project,
            compilations: records,
            buildOutputs: port.buildOutputs,
            variant: port.variant
        })
    } catch (error) {
        if (!(error instanceof ExampleError)) throw error
        console.warn(`\nthe build will fail:\n${describe(error.problems)}`)
        return
    }
    const source = existsSync(join(example.folder, BUILD_FILE)) ? BUILD_FILE : EXAMPLE_FILE
    const differences = []
    for (const field of ['programs', 'selectedProgram', 'openTabs', 'activeTab', 'fileViews']) {
        const theirs = normalize(field, exported.project[field])
        const ours = normalize(field, packed.manifest.project[field])
        if (JSON.stringify(theirs) !== JSON.stringify(ours))
            differences.push(
                `  ${field}: the archive has ${JSON.stringify(theirs)}, ${source} gives ${JSON.stringify(ours)}`
            )
    }
    if (differences.length)
        console.log(`\nto keep, copy into ${source} by hand:\n${differences.join('\n')}`)
}

function normalize(field, value) {
    if (field !== 'programs' || !isRecord(value)) return value ?? null
    return Object.fromEntries(
        Object.entries(value)
            .sort(([a], [b]) => a.localeCompare(b))
            .map(([entry, program]) => [entry, { settings: {}, testcases: [], ...program }])
    )
}

/**
 * The exported Project: its manifest's project and each File's bytes and encoding. Programs are
 * read from every archive version the editor writes, the oldest of them holding one Program in
 * the project itself.
 */
function readArchive(path) {
    let zip
    try {
        zip = unzipSync(new Uint8Array(readFileSync(path)))
    } catch (error) {
        fail(`${path} is not a Project archive: ${error.message}`)
    }
    if (!zip['project.json']) fail(`${path} has no project.json`)
    const manifest = JSON.parse(new TextDecoder().decode(zip['project.json']))
    if (![1, 2, 3, 4].includes(manifest.version))
        fail(`${path} is an archive of version ${manifest.version}`)
    const project = { ...manifest.project }
    if (manifest.version < 3) {
        const { entry, settings, testcases, display } = project
        project.programs = { [entry]: { settings, testcases, ...(display ? { display } : {}) } }
        project.selectedProgram = entry
    }
    const files = {}
    let annotated = false
    for (const [name, info] of Object.entries(manifest.files)) {
        if (info.edits?.length)
            fail(`${name} has Byte edits: export it again from a current editor`)
        if (info.annotations) annotated = true
        const bytes = zip[`files/${name}`]
        if (!bytes) fail(`${path} lists ${name} but does not hold it`)
        files[name] = { bytes: Buffer.from(bytes), encoding: info.encoding }
    }
    return { project, files, annotated }
}

/** A warning when the repository would store the File otherwise than the editor did. */
function encodingProblem(destination, path, encoding) {
    try {
        const ours = readProjectFile(destination, path).encoding
        if (ours !== encoding)
            return `${path}: the editor kept it as ${encoding === 'plain' ? 'text' : 'bytes'}, the build reads it as ${ours === 'plain' ? 'text' : 'bytes'}`
    } catch {
        //a File that is not UTF-8 is a problem the build reports
    }
}

function fail(message) {
    console.error(message)
    process.exit(1)
}
