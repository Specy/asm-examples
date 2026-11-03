#!/bin/sh
# asm.sh <srcdir> <builddir> : copies the sources into <builddir>/proj, compiles every unit with the
# host's gcc (OS_CFLAGS added) and puts the Generated assemblies at their .c.asm paths in the copy.
SRC=$1; B=$2
mkdir -p $B/proj $B/asm
rsync -a --delete --exclude '*.c.asm' --exclude os/main $SRC/ $B/proj/
echo '[{"stats":"built"}]' > $B/none.json
cd "$(. "$(dirname "$0")/../editor.sh")/packages/asm-editor" && \
OS_DIR=$B/proj OS_ENTRY=os/main.c OS_ASM_OUT=$B/asm \
OS_SOURCES=os/main.c,os/apps/apps.c,os/ide/ide.c,os/doom/doom_game.c,os/doom/doom_play.c,os/doom/doom_render.c,os/doom/doom_world.c \
OS_OPT=1 OS_OPTS=os/ide/ide.c=2,os/doom/doom_game.c=2,os/doom/doom_play.c=2,os/doom/doom_render.c=2,os/doom/doom_world.c=2 \
OS_SCRIPT=$B/none.json OS_OUT=$B \
node_modules/.bin/vp test run --config .cache/asmos.config.mts src/lib/languages/X86/AsmosHarness.test.ts --disable-console-intercept 2>&1 \
 | grep -v "^ *$" | grep -v "RUN \|Test Files\|Tests \|Start at\|Duration\|verified only on GCC"
cp $B/asm/main.c.asm $B/proj/os/main.c.asm
cp $B/asm/apps.c.asm $B/proj/os/apps/apps.c.asm
cp $B/asm/ide.c.asm $B/proj/os/ide/ide.c.asm
for p in game play render world; do cp $B/asm/doom_$p.c.asm $B/proj/os/doom/doom_$p.c.asm; done
