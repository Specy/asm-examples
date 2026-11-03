export type Target = { name: string; folder: string }

export type ExampleSource = {
    id: string
    path: string
    folder: string
    target?: Target
}

export type CapturedPort = {
    target: Target
    variant?: string
    files: Record<string, string>
    name: string
    description: string
    project?: Record<string, unknown>
    buildOutputs?: string[]
    draft: boolean
}

export function findExamples(root: string): { examples: ExampleSource[]; problems: string[] }

export function buildExample(
    example: ExampleSource,
    options: { capture: true }
): Promise<{
    meta: Record<string, unknown>
    compilations: Record<string, unknown[]>
    captured: CapturedPort[]
}>

export function buildExample(
    example: ExampleSource,
    options?: { capture?: false }
): Promise<{
    entry: Record<string, unknown>
    archives: { archivePath: string; archive: Uint8Array }[]
}>
