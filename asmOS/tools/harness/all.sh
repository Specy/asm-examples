#!/bin/sh
# all.sh <builddir> <shots dir> : plays every script but DOOM's
P=${P:-$(cd "$(dirname "$0")" && pwd)}
for s in desktop notepad terminal files paint pacman windows; do
  echo "== $s"; $P/play.sh $1 $s $2 | grep -v "^asmOS: 640\|^TERMINAL:\|compiled in\|FILE os/main"
done
echo "== fib"; AUTOSTART="asm code/fib.riscv" $P/play.sh $1 fib $2 | grep -v "^asmOS: 640\|^TERMINAL:\|compiled in\|FILE os/main"
echo "== video"; AUTOSTART="video videos/bad-apple.vid" $P/play.sh $1 video $2 | grep -v "^asmOS: 640\|^TERMINAL:\|compiled in\|FILE os/main"
