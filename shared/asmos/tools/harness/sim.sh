#!/bin/sh
# sim.sh <RISC-V|MIPS> <script name> <shots dir>: builds asmOS for MIPS or RISC-V on the editor's
# MARS or RARS and plays scripts/<name>.json. The units are compiled with a local Clang 21.1.0
# (ASMOS_CLANG) into BUILD (default /tmp/asmos-<target>), or taken from there for the units KEEP
# lists ("os/main.c,os/apps/apps.c,os/ide/ide.c,os/doom/doom_game.c,..."); USE_ASM=1 takes the
# tree's Generated assembly.
# AUTOSTART="app arg;app arg" starts applications.
T=$1; HERE=$(cd "$(dirname "$0")" && pwd)
BUILD=${BUILD:-/tmp/asmos-$T}
mkdir -p "$3" "$BUILD"
cd "$(. "$(dirname "$0")/../editor.sh")/packages/asm-editor" && \
EXAMPLES_ROOT="$(cd "$HERE/../../../.." && pwd)" OS_TARGET=$T OS_SCRIPT=$HERE/scripts/$2.json OS_OUT=$3 \
OS_ASM_OUT=$BUILD OS_KEEP="$KEEP" OS_USE_ASM="$USE_ASM" OS_AUTOSTART="$AUTOSTART" \
node_modules/.bin/vp test run --config .cache/asmos.config.mts src/lib/languages/mars/MarsHarness.test.ts --disable-console-intercept 2>&1 \
 | grep -v "^ *$" | grep -v "RUN \|Test Files\|Tests \|Start at\|Duration"
