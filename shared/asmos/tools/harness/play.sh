#!/bin/sh
# play.sh <builddir> <script name> <shots dir> : builds from <builddir>/proj's saved assemblies and plays
# scripts/<name>.json; AUTOSTART="app arg;app arg" starts applications.
B=$1; S=${S:-$(cd "$(dirname "$0")" && pwd)}
mkdir -p $3
cd "${ASM_EDITOR:-$(cd "$(dirname "$0")/../../../../asm-editor" && pwd)}/packages/asm-editor" && \
OS_DIR=$B/proj OS_ENTRY=os/main.c OS_USE_ASM=1 OS_AUTOSTART="$AUTOSTART" \
OS_SOURCES=os/main.c,os/apps/apps.c,os/ide/ide.c,os/doom/doom_game.c,os/doom/doom_play.c,os/doom/doom_render.c,os/doom/doom_world.c \
OS_SCRIPT=$S/scripts/$2.json OS_OUT=$3 \
node_modules/.bin/vp test run --config .cache/asmos.config.mts src/lib/languages/X86/AsmosHarness.test.ts --disable-console-intercept 2>&1 \
 | grep -v "^ *$" | grep -v "RUN \|Test Files\|Tests \|Start at\|Duration\|verified only on GCC\|lines of NASM"
