// Copy to <asm-editor>/packages/asm-editor/.cache/asmos.config.mts: the harness runs as one of the
// editor's tests, against the editor's own x86 Core.
import { resolve } from 'node:path'
import { mergeConfig } from 'vite-plus'
import base from '../vite.config.ts'

export default mergeConfig(base, {
    cacheDir: 'node_modules/.vite-asmos',
    server: { fs: { allow: [resolve(import.meta.dirname, '../../..')] } }
})
