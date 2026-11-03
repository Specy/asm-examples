# Prints the asm-editor checkout the tools use: ASM_EDITOR, else the editor asm-examples is a
# submodule of (packages/examples), else asm-editor beside asm-examples. Sourced by the scripts one
# folder down from here, so $0 is one of them.
examples="$(cd "$(dirname "$0")/../../../.." && pwd)"
if [ -n "$ASM_EDITOR" ]; then echo "$ASM_EDITOR"
elif [ -d "$examples/../../packages/asm-editor" ]; then (cd "$examples/../.." && pwd)
else (cd "$examples/../asm-editor" && pwd)
fi
