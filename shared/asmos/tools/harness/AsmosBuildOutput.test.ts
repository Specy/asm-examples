/**
 * Scratch (delete before committing): Builds asmOS from its archive as the editor's Build does, and
 * adds the program it links, `os/main`, with its Build record, to the archive.
 */
import { readFileSync, writeFileSync } from 'node:fs'
import { unzipSync, zipSync } from 'fflate'
import { expect, it } from 'vite-plus/test'
import { buildOutputStale, isUntouchedBuildOutput, makeBuildRecord } from '#lib/buildOutput.ts'
import { programBuildSources } from '#lib/buildSources.ts'
import { FileSystem } from '#lib/languages/peripherals/FileSystem.ts'
import { X86Emulator } from '#lib/languages/X86/X86Emulator.svelte.ts'
import { projectFileFromBytes, type ProjectFile, type ProjectFiles } from '#lib/projectFiles.ts'

const ARCHIVE = process.env.OS_ARCHIVE

it.skipIf(!ARCHIVE)('adds the Build output to the asmOS archive', async () => {
    const zip = unzipSync(readFileSync(ARCHIVE!))
    const manifest = JSON.parse(new TextDecoder().decode(zip['project.json']))
    const files: Record<string, ProjectFile> = {}
    for (const [path, info] of Object.entries(manifest.files) as [string, { encoding: 'plain' | 'base64' }][])
        files[path] = projectFileFromBytes(zip[`files/${path}`], info.encoding)
    const project = { ...manifest.project, files: files as ProjectFiles }
    const sources = programBuildSources(project)
    console.log('CONFIG', JSON.stringify({ ...sources, files: undefined }))
    let saved: { path: string; bytes: Uint8Array } | undefined
    const emulator = await X86Emulator(sources, {
        automaticChecking: false,
        display: { width: 640, height: 480 },
        peripherals: { fileSystem: new FileSystem(structuredClone(files) as ProjectFiles) }
    })
    emulator.setBuildOutputHost({
        claim: async () => 'save',
        save: (_sources, path, bytes, format) => {
            expect(format).toBe('executable')
            saved = { path, bytes: bytes.slice() }
        },
        isBuildOutput: () => false
    })
    await emulator.compile(0, sources)
    expect(emulator.errors).toEqual([])
    expect(saved).toBeDefined()
    const { path, bytes } = saved!
    const output = projectFileFromBytes(bytes, 'base64')
    const record = makeBuildRecord(sources, path, output, [])
    const withOutput = { ...files, [path]: output } as ProjectFiles
    //as the editor reads it after importing: untouched, and built from these very sources
    expect(isUntouchedBuildOutput(record, withOutput[path])).toBe(true)
    expect(buildOutputStale(record, programBuildSources({ ...project, files: withOutput }), withOutput)).toBe(
        false
    )
    manifest.files[path] = { encoding: 'base64' }
    manifest.project.builds = [record]
    const mtime = zip['project.json'] && Date.UTC(2026, 9, 23)
    const entries: Record<string, [Uint8Array, { mtime: number }]> = {}
    for (const name of Object.keys(zip)) if (name !== 'project.json') entries[name] = [zip[name], { mtime }]
    entries[`files/${path}`] = [bytes, { mtime }]
    entries['project.json'] = [new TextEncoder().encode(JSON.stringify(manifest, null, 2)), { mtime }]
    const archive = zipSync(entries, { level: 6 })
    writeFileSync(ARCHIVE!, archive)
    console.log(`${path}: ${bytes.length} bytes; archive ${archive.length} bytes; ${Object.keys(record.inputs).length} inputs`)
    emulator.dispose()
}, 120_000)

it.skipIf(!process.env.OS_CHECK)('imports the archive with an up-to-date Build output', async () => {
    const { makeProjectFromArchive } = await import('#lib/projectArchive.ts')
    const { buildRecordAt } = await import('#lib/buildOutput.ts')
    const project = (await makeProjectFromArchive(readFileSync(process.env.OS_CHECK!))).project
    const record = buildRecordAt(project.builds, 'os/main')
    expect(record).toBeDefined()
    expect(isUntouchedBuildOutput(record!, project.files['os/main'])).toBe(true)
    const live = programBuildSources(project)
    expect(buildOutputStale(record!, live, project.files)).toBe(false)
    console.log('IMPORTED', Object.keys(project.files).length, 'Files;', project.compilations?.length, 'compilations;',
        'programs', Object.keys(project.programs).join(','), '; tabs', project.openTabs.join(','))
})
