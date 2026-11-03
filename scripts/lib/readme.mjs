import { TARGETS } from './targets.mjs'

export const TABLE_START = '<!-- examples:start -->'
export const TABLE_END = '<!-- examples:end -->'

/** The README's table of examples, from the index: one row an example, by name. */
export function examplesTable(index) {
    const cell = (text) => text.replace(/\|/g, '\\|').replace(/\s*\n\s*/g, ' ')
    const rows = Object.values(index.examples)
        .sort((a, b) => a.name.localeCompare(b.name))
        .map((example) => {
            const targets = TARGETS.filter((target) =>
                example.ports.some((port) => port.target === target.name)
            ).map((target) => target.name)
            return `| [${cell(example.name)}](${example.path}/) | ${targets.join(', ')} | ${cell(example.description)} | ${example.license} |`
        })
    return [
        '| Example | Targets | What it is | License |',
        '| ------- | ------- | ---------- | ------- |',
        ...rows
    ].join('\n')
}

/** `readme` with `table` between its markers, which it must have. */
export function withExamplesTable(readme, table) {
    const start = readme.indexOf(TABLE_START)
    const end = readme.indexOf(TABLE_END)
    if (start < 0 || end < start)
        throw new Error(
            `README.md needs ${TABLE_START} and ${TABLE_END} around the table of examples`
        )
    return `${readme.slice(0, start + TABLE_START.length)}\n${table}\n${readme.slice(end)}`
}
