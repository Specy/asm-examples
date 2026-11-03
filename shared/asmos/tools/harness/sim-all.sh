#!/bin/sh
# sim-all.sh <RISC-V|MIPS> <shots dir>: compiles once, then plays every sim- script from that build
P=$(cd "$(dirname "$0")" && pwd)
$P/sim.sh $1 boot $2 | grep "^\[\|built\|FAIL\|error"
export KEEP=os/main.c,os/apps/apps.c,os/ide/ide.c
for s in desktop windows notepad timing; do echo "== $s"; $P/sim.sh $1 sim-$s $2 | grep "^\[\|errors"; done
for app in terminal files paint calc mines pacman; do
  echo "== $app"; AUTOSTART=$app $P/sim.sh $1 sim-$app $2 | grep "^\[\|errors"
done
echo "== fib"; AUTOSTART="asm code/fib.riscv" $P/sim.sh $1 sim-fib $2 | grep "^\[\|errors"
