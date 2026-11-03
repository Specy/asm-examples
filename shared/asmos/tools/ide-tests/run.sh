#!/bin/sh
# The Assembly Editor's RISC-V engine (asmOS os/ide/rv_asm.c, rv_cpu.c), tested against RARS.
#   run.sh            build rvcli with gcc -O2 -Wall -Wextra -Werror and run every test
#   SAN=1 run.sh      the same with AddressSanitizer and UndefinedBehaviorSanitizer
#   run.sh NAME       only the corpus programs whose names contain NAME
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
SOURCE=${OS_DIR:-$HERE/../..}/os/ide
BIN=${BIN:-$HERE/bin}
mkdir -p "$BIN"
if [ "$SAN" = 1 ]; then
    CLI=$BIN/rvcli-san
    gcc -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined -Wall -Wextra -Werror -o "$CLI" "$SOURCE/rvcli.c"
else
    CLI=$BIN/rvcli
    gcc -O2 -Wall -Wextra -Werror -o "$CLI" "$SOURCE/rvcli.c"
fi
# the engine alone, as asmOS compiles it: its own allocator, C11, no host program around it
cat > "$BIN/asmos-like.c" <<'C'
#include <stdlib.h>
static void *test_alloc(size_t size) { return calloc(1, size); }
#define RV_ALLOC(size) test_alloc(size)
#define RV_REALLOC(pointer, size) realloc((pointer), (size))
#define RV_FREE(pointer) free(pointer)
#include "rv_asm.c"
#include "rv_cpu.c"
C
gcc -std=gnu11 -O2 -fno-pic -Wall -Wextra -Werror -Wstack-usage=32768 -I"$SOURCE" -c -o "$BIN/asmos-like.o" "$BIN/asmos-like.c"
if nm "$BIN/asmos-like.o" | grep -E ' [BbDdCcGgSs] ' ; then echo "FAIL: the engine has global or static variables"; exit 1; fi
echo "engine: builds as asmOS builds it, no global state, no stack frame over 32 KiB"

status=0
# the @screen comment, as the web editor reads it
while read -r name expected; do
    got=$("$CLI" --screen "$HERE/screen/$name" | grep '^SCREEN')
    if [ "$got" != "$expected" ]; then echo "FAIL screen/$name: $got, expected $expected"; status=1; fi
done < "$HERE/screen/expected.txt"
echo "screen directive: $(wc -l < "$HERE/screen/expected.txt") cases checked"

# Step back with a journal too small for the whole run: the oldest steps are dropped
for name in sort recursion strings print_long sieve mmio_echo; do
    keys=$(sed -n 's/^#[[:space:]]*@test.*keys=\([^ ]*\).*/\1/p' "$HERE/corpus/$name.s")
    got=$("$CLI" --stepback-check --journal 50 ${keys:+--keys "$keys"} "$HERE/corpus/$name.s" | grep '^STEPBACK')
    if [ "$got" != "STEPBACK ok" ]; then echo "FAIL small journal $name: $got"; status=1; fi
done
echo "small journal: 6 programs stepped back across a full ring"

node "$HERE/suite.mjs" "$CLI" "$1" || status=1
# asmOS's example programs: the same in RARS, unchanged (input and keys from tests/examples)
CORPUS="$SOURCE/../../code" INPUTS="$HERE/examples" node "$HERE/suite.mjs" "$CLI" "$1" || status=1
exit $status
