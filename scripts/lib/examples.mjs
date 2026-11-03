import { existsSync, readdirSync, readFileSync } from 'node:fs'
import { join } from 'node:path'
import { pathToFileURL } from 'node:url'
import { BUILD_FILE, COMPILATIONS_FILE, EXAMPLE_FILE, ExampleError, walk } from './files.mjs'
import { VARIANT, isRecord, packPort } from './pack.mjs'
import { SHARED_FOLDER, TARGETS, findTarget, targetByFolder } from './targets.mjs'

/** Folders at the repository's root that hold no examples. */
const OTHER_FOLDERS = new Set(['scripts', 'node_modules', 'dist'])
const ID = VARIANT
const EXAMPLE_FIELDS = ['name', 'description', 'tags', 'license', 'project', 'ports']
const PORT_FIELDS = ['name', 'description', 'project']
const DEFAULT_LICENSE = 'MIT'
const LICENSE_FILES = ['LICENSE', 'LICENSE.md', 'LICENSE.txt', 'COPYING']

/**
 * The examples of the repository at `root`: each folder of a Target folder, written for that
 * Target, and each folder of `shared/`, written for several. Anything else at the root, or loose
 * in those folders, is a problem, so that a misspelled Target folder cannot hide its examples.
 */
export function findExamples(root) {
    const examples = []
    const problems = []
    for (const entry of readdirSync(root, { withFileTypes: true })) {
        if (!entry.isDirectory() || entry.name.startsWith('.') || OTHER_FOLDERS.has(entry.name))
            continue
        const shared = entry.name === SHARED_FOLDER
        if (!shared && !targetByFolder.has(entry.name)) {
            problems.push(
                `${entry.name}/ is neither a Target folder (${TARGETS.map((target) => target.folder).join(', ')}) nor ${SHARED_FOLDER}/`
            )
            continue
        }
        for (const child of readdirSync(join(root, entry.name), { withFileTypes: true })) {
            const path = `${entry.name}/${child.name}`
            if (!child.isDirectory()) {
                problems.push(`${path}: only example folders belong in ${entry.name}/`)
                continue
            }
            if (!ID.test(child.name)) {
                problems.push(`${path}: an example's folder name is lower case words joined by -`)
                continue
            }
            examples.push({
                id: child.name,
                path,
                folder: join(root, path),
                target: shared ? undefined : targetByFolder.get(entry.name)
            })
        }
    }
    const seen = new Map()
    for (const example of examples) {
        const other = seen.get(example.id)
        if (other) problems.push(`${example.path} and ${other} are both called ${example.id}`)
        else seen.set(example.id, example.path)
    }
    examples.sort((a, b) => a.id.localeCompare(b.id))
    return { examples, problems }
}

/** The example's `example.json`, checked. */
export function readExampleFile(example) {
    const path = join(example.folder, EXAMPLE_FILE)
    if (!existsSync(path)) throw new ExampleError([`it has no ${EXAMPLE_FILE}`])
    const meta = readJson(path, EXAMPLE_FILE)
    if (!isRecord(meta)) throw new ExampleError([`${EXAMPLE_FILE} is not an object`])
    const problems = []
    const check = (condition, problem) => condition || problems.push(problem)
    for (const field of Object.keys(meta))
        check(EXAMPLE_FIELDS.includes(field), `unknown field ${field} in ${EXAMPLE_FILE}`)
    check(typeof meta.name === 'string' && meta.name.trim() !== '', `${EXAMPLE_FILE} has no name`)
    check(
        typeof meta.description === 'string' && meta.description.trim() !== '',
        `${EXAMPLE_FILE} has no description`
    )
    check(
        meta.tags === undefined ||
            (Array.isArray(meta.tags) && meta.tags.every((tag) => typeof tag === 'string')),
        'tags is not a list of words'
    )
    const license = meta.license ?? DEFAULT_LICENSE
    check(typeof license === 'string' && license !== '', 'license is not an SPDX expression')
    if (license !== DEFAULT_LICENSE)
        check(
            LICENSE_FILES.some((name) => existsSync(join(example.folder, name))),
            `its license is ${license}, not the repository's ${DEFAULT_LICENSE}, so it needs a LICENSE File`
        )
    if (example.target) {
        check(meta.ports === undefined, `ports is for ${SHARED_FOLDER}/ examples; use project`)
    } else {
        check(
            meta.project === undefined,
            `a ${SHARED_FOLDER}/ example gives each port's project under ports`
        )
        if (meta.ports !== undefined && check(isRecord(meta.ports), 'ports is not an object'))
            for (const [folder, port] of Object.entries(meta.ports)) {
                check(targetByFolder.has(folder), `ports.${folder} is no Target folder`)
                if (!check(isRecord(port), `ports.${folder} is not an object`)) continue
                for (const field of Object.keys(port))
                    check(PORT_FIELDS.includes(field), `unknown field ${field} in ports.${folder}`)
            }
    }
    if (problems.length) throw new ExampleError(problems)
    return { ...meta, tags: meta.tags ?? [], license }
}

/** The example's `compilations.json`: its Compilation records by Target folder. */
export function readCompilationsFile(example) {
    const path = join(example.folder, COMPILATIONS_FILE)
    if (!existsSync(path)) return {}
    const records = readJson(path, COMPILATIONS_FILE)
    if (!isRecord(records)) throw new ExampleError([`${COMPILATIONS_FILE} is not an object`])
    const unknown = Object.keys(records).filter((folder) => !targetByFolder.has(folder))
    if (unknown.length)
        throw new ExampleError([
            `${COMPILATIONS_FILE} has records for ${unknown.join(', ')}, which are no Target folders`
        ])
    return records
}

/**
 * `files(options)` as an example's `build.mjs` receives it: the Files of the example folder, or of
 * its `from` subfolder, as archive path → file path. `include` keeps only the paths it lists and
 * `exclude` drops some; an entry ending in `/` is a folder and anything under it. An entry that
 * matches nothing is a problem, as a misspelled one would be.
 */
export function exampleFiles(folder) {
    return ({ from = '', include, exclude = [] } = {}) => {
        const base = from ? join(folder, from) : folder
        if (!existsSync(base)) throw new ExampleError([`files(): there is no ${from}`])
        let paths = walk(base)
        const matches = (path, entry) =>
            entry.endsWith('/') ? path.startsWith(entry) : path === entry
        for (const entry of [...(include ?? []), ...exclude])
            if (!paths.some((path) => matches(path, entry)))
                throw new ExampleError([
                    `files(): ${entry} matches no File of ${from || 'the example'}`
                ])
        if (include) paths = paths.filter((path) => include.some((entry) => matches(path, entry)))
        paths = paths.filter((path) => !exclude.some((entry) => matches(path, entry)))
        return Object.fromEntries(paths.map((path) => [path, join(base, path)]))
    }
}

/**
 * Builds one example: its `build.mjs` if it has one, else the default. Each port is one archive;
 * the result is the example's index entry and the archives to write. With `capture`, nothing is
 * packed or checked: the result is what each `pack` call asked for, which `sync` maps Files with.
 */
export async function buildExample(example, { capture = false } = {}) {
    const meta = readExampleFile(example)
    const compilations = readCompilationsFile(example)
    const files = exampleFiles(example.folder)
    const packed = []
    const captured = []
    const usedRecords = new Set()
    const pack = ({
        target: name,
        variant,
        files: portFiles,
        project,
        buildOutputs,
        draft = false,
        ...rest
    } = {}) => {
        const unknown = Object.keys(rest).filter((key) => key !== 'name' && key !== 'description')
        if (unknown.length) throw new ExampleError([`pack(): unknown option ${unknown.join(', ')}`])
        const target = findTarget(name)
        if (!target) throw new ExampleError([`pack(): ${name} is no Target`])
        if (!isRecord(portFiles)) throw new ExampleError(['pack(): files is not an object'])
        if (typeof draft !== 'boolean') throw new ExampleError(['pack(): draft must be a boolean'])
        usedRecords.add(target.folder)
        if (capture) {
            captured.push({
                target,
                variant,
                files: portFiles,
                project,
                buildOutputs,
                draft,
                name: rest.name ?? meta.name,
                description: rest.description ?? meta.description
            })
            return
        }
        if (draft) return
        const portName = rest.name ?? meta.name
        const portDescription = rest.description ?? meta.description
        const port = packPort({
            id: example.id,
            target,
            files: portFiles,
            name: portName,
            description: portDescription,
            project,
            compilations: compilations[target.folder] ?? [],
            buildOutputs,
            variant
        })
        if (packed.some((other) => other.archivePath === port.archivePath))
            throw new ExampleError([`two archives are ${port.archivePath}: give one a variant`])
        //a port named or described apart from its example says so in the index too
        if (portName !== meta.name) port.port.name = portName
        if (portDescription !== meta.description) port.port.description = portDescription
        packed.push(port)
        return port
    }

    if (existsSync(join(example.folder, BUILD_FILE))) {
        const module = await import(pathToFileURL(join(example.folder, BUILD_FILE)).href)
        if (typeof module.default !== 'function')
            throw new ExampleError([`${BUILD_FILE} has no default export function`])
        await module.default({ id: example.id, folder: example.folder, example: meta, files, pack })
        if (!packed.length && !captured.length)
            throw new ExampleError([`${BUILD_FILE} packed nothing`])
    } else if (example.target) {
        pack({ target: example.target.folder, files: files(), project: meta.project })
    } else {
        defaultSharedBuild(example, meta, files, pack)
    }
    if (capture) return { meta, compilations, captured }

    const unused = Object.keys(compilations).filter((folder) => !usedRecords.has(folder))
    if (unused.length)
        throw new ExampleError([
            `${COMPILATIONS_FILE} has records for ${unused.join(', ')}, which it has no port for`
        ])
    const order = (port) => TARGETS.findIndex((target) => target.name === port.target)
    const ports = packed
        .map((port) => port.port)
        .sort((a, b) => order(a) - order(b) || (a.variant ?? '').localeCompare(b.variant ?? ''))
    return {
        entry: {
            name: meta.name,
            description: meta.description,
            tags: meta.tags,
            license: meta.license,
            path: example.path,
            ports
        },
        archives: packed.map(({ archivePath, archive }) => ({ archivePath, archive }))
    }
}

/**
 * A shared example without `build.mjs`: each Target folder in it is a port, whose Files are its
 * own and every File of the example outside the Target folders, at their paths from there.
 */
function defaultSharedBuild(example, meta, files, pack) {
    const folders = readdirSync(example.folder, { withFileTypes: true })
        .filter((entry) => entry.isDirectory() && targetByFolder.has(entry.name))
        .map((entry) => entry.name)
    if (!folders.length)
        throw new ExampleError([
            `no Target folder (${TARGETS.map((target) => target.folder).join(', ')}) and no ${BUILD_FILE}`
        ])
    for (const folder of Object.keys(meta.ports ?? {}))
        if (!folders.includes(folder))
            throw new ExampleError([`ports.${folder} is for no ${folder}/ folder`])
    const common = files({ exclude: folders.map((folder) => folder + '/') })
    const problems = []
    for (const folder of folders) {
        const own = files({ from: folder })
        const clashes = Object.keys(own).filter((path) => path in common)
        if (clashes.length) {
            problems.push(
                ...clashes.map(
                    (path) => `${folder}/${path} and the shared ${path} would be the same File`
                )
            )
            continue
        }
        const port = meta.ports?.[folder] ?? {}
        try {
            pack({
                target: folder,
                files: { ...common, ...own },
                name: port.name,
                description: port.description,
                project: port.project
            })
        } catch (error) {
            if (!(error instanceof ExampleError)) throw error
            problems.push(...error.problems.map((problem) => `${folder}: ${problem}`))
        }
    }
    if (problems.length) throw new ExampleError(problems)
}

function readJson(path, name) {
    try {
        return JSON.parse(readFileSync(path, 'utf8'))
    } catch (error) {
        throw new ExampleError([`${name} is not JSON: ${error.message}`])
    }
}
