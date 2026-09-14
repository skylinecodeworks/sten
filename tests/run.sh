#!/bin/sh
set -u

ROOT=$(cd "$(dirname "$0")/.." && pwd)
BIN="$ROOT/sten"
GEN="$ROOT/tools/gen"
UNIT="$ROOT/tests/build/unit"
D="$ROOT/tests/build"
export BIN GEN UNIT D

mkdir -p "$D"
. "$ROOT/tests/lib.sh"

if [ ! -x "$BIN" ]; then
    echo "binary $BIN not found (run 'make' first)" >&2
    exit 1
fi

"$GEN" "$D" || {
    echo "could not generate fixtures" >&2
    exit 1
}

for c in 01_cli 02_bmp 03_png 04_gif 05_jpeg 06_roundtrip 07_robustness; do
    printf '\n== %s ==\n' "$c"
    . "$ROOT/tests/cases/$c.sh"
done

printf '\n== unit (C) ==\n'
if [ -x "$UNIT" ]; then
    "$UNIT" > "$D/unit.out" 2> "$D/unit.err"
    _urc=$?
    while IFS= read -r line; do
        case "$line" in
            "ok "*)     pass "${line#ok }" ;;
            "not ok "*) fail "${line#not ok }" ;;
            *)          printf '  %s\n' "$line" ;;
        esac
    done < "$D/unit.out"
    if [ "$_urc" -ne 0 ]; then
        printf '  (unit termino con exit=%s)\n' "$_urc"
    fi
else
    skip "unit tests not built (run 'make test')"
fi

summary
