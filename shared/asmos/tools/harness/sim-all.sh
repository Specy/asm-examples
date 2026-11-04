#!/bin/sh
# sim-all.sh <RISC-V|MIPS> <shots dir>: compiles once, then plays every sim- script from that build
# but DOOM's. Without source annotations: with them the Generated assembly of the seven units takes
# the Project over the 16 MiB the editor's file system holds.
P=$(cd "$(dirname "$0")" && pwd)
export OS_ANNOTATIONS=0
$P/sim.sh $1 boot $2 | grep "^\[\|built\|FAIL\|error"
export KEEP=os/main.c,os/apps/apps.c,os/ide/ide.c,os/doom/doom_game.c,os/doom/doom_play.c,os/doom/doom_render.c,os/doom/doom_world.c
for s in desktop windows notepad timing stack-guard; do echo "== $s"; $P/sim.sh $1 sim-$s $2 | grep "^\[\|errors"; done
for app in terminal files paint calc mines pacman blocks; do
  echo "== $app"; AUTOSTART=$app $P/sim.sh $1 sim-$app $2 | grep "^\[\|errors"
done
echo "== fib"; AUTOSTART="asm code/fib.riscv" $P/sim.sh $1 sim-fib $2 | grep "^\[\|errors"
echo "== music"; AUTOSTART="music music/canon.mid" $P/sim.sh $1 sim-music $2 | grep "^\[\|errors"
echo "== video"; AUTOSTART="video videos/bad-apple.vid" $P/sim.sh $1 sim-video $2 | grep "^\[\|errors"
echo "== wallpaper"; AUTOSTART=settings $P/sim.sh $1 sim-wallpaper $2 | grep "^\[\|errors"
echo "== busy"; AUTOSTART=terminal $P/sim.sh $1 sim-busy $2 | grep "^\[\|errors"
