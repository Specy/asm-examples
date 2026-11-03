// Packs every example into its archives, dist/<target>/<example>.asmproj, and writes
// dist/index.json, which lists each example's ports and each Target's examples. It also brings the
// table of examples in README.md up to date. Nothing is written while any example has a problem.
//
//   npm run build            build, and rewrite README.md's table
//   npm run check            build, and fail if README.md's table is out of date (CI)
import { mkdirSync, readFileSync, rmSync, writeFileSync } from 'node:fs'
import { dirname, join } from 'node:path'
import { fileURLToPath } from 'node:url'
import { buildExample, findExamples } from './lib/examples.mjs'
import { ExampleError } from './lib/files.mjs'
import { examplesTable, withExamplesTable } from './lib/readme.mjs'
import { TARGETS } from './lib/targets.mjs'

const root = fileURLToPath(new URL('..', import.meta.url))
const checkOnly = process.argv.includes('--check')
const dist = join(root, 'dist')

const { examples, problems } = findExamples(root)
const index = { version: 1, examples: {}, targets: {} }
const archives = []
for (const example of examples) {
    try {
        const built = await buildExample(example)
        index.examples[example.id] = built.entry
        archives.push(...built.archives)
    } catch (error) {
        const found =
            error instanceof ExampleError ? error.problems : [error.stack ?? String(error)]
        problems.push(...found.map((problem) => `${example.path}: ${problem}`))
    }
}
for (const target of TARGETS) {
    const ids = Object.keys(index.examples).filter((id) =>
        index.examples[id].ports.some((port) => port.target === target.name)
    )
    if (ids.length) index.targets[target.name] = ids
}

const readme = readFileSync(join(root, 'README.md'), 'utf8')
let updated = readme
try {
    updated = withExamplesTable(readme, examplesTable(index))
    if (checkOnly && updated !== readme && !problems.length)
        problems.push('README.md: its table of examples is out of date; run npm run build')
} catch (error) {
    problems.push(error.message)
}
if (problems.length) {
    console.error(problems.join('\n'))
    console.error(
        `\n${problems.length} problem${problems.length === 1 ? '' : 's'}; nothing written`
    )
    process.exit(1)
}

rmSync(dist, { recursive: true, force: true })
for (const { archivePath, archive } of archives) {
    mkdirSync(dirname(join(dist, archivePath)), { recursive: true })
    writeFileSync(join(dist, archivePath), archive)
}
writeFileSync(join(dist, 'index.json'), JSON.stringify(index, null, 2) + '\n')
if (updated !== readme) writeFileSync(join(root, 'README.md'), updated)

const total = archives.reduce((sum, { archive }) => sum + archive.length, 0)
for (const { archivePath, archive } of archives)
    console.log(`${archivePath}  ${archive.length} bytes`)
console.log(
    `${examples.length} examples, ${archives.length} archives, ${total} bytes in dist/${updated !== readme ? '; README.md updated' : ''}`
)
