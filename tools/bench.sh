#!/bin/sh
set -u

ROOT=$(cd "$(dirname "$0")/.." && pwd)
BIN="$ROOT/sten"
GEN="$ROOT/tools/gen"
D="$ROOT/tests/build"
OUT=/tmp/sten_bench_msg.bin

mkdir -p "$D"
"$GEN" "$D" >/dev/null 2>&1

# Fixed 1024-byte payload (or less when the carrier cannot fit it).
printf 'a%.0s' 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 \
    22 23 24 25 26 27 28 29 30 31 32 33 34 35 36 37 38 39 40 41 42 43 \
    44 45 46 47 48 49 50 51 52 53 54 55 56 57 58 59 60 61 62 63 64 > "$OUT"

ns_ms() {
    awk -v a="$1" -v b="$2" 'BEGIN { printf "%.0f", (b - a) / 1000000 }'
}

printf '%-8s %12s %12s %10s %12s %12s\n' \
    "format" "image (B)" "capacity (B)" "ratio" "encode (ms)" "decode (ms)"

for spec in "BMP:bmp" "PNG:png" "GIF:gif" "JPEG:jpg" "PNM/PAM:ppm" "TGA:tga" "TIFF:tiff" "ICO:ico"; do
    name=${spec%%:*}
    ext=${spec##*:}
    case "$ext" in
        bmp) img="$D/big.bmp"; [ -f "$img" ] || img="$D/test.bmp" ;;
        png) img="$D/big.png"; [ -f "$img" ] || img="$D/test.png" ;;
        *)   img="$D/test.$ext" ;;
    esac
    if [ ! -f "$img" ]; then
        continue
    fi
    size=$(wc -c < "$img" | tr -d ' ')
    cap=$("$BIN" capacity -i "$img" 2>/dev/null)
    n=$(( cap < 1024 ? cap : 1024 ))
    if [ "$n" -le 0 ]; then
        continue
    fi
    msg="/tmp/sten_bench_${ext}.msg"
    if [ "$n" -eq 1024 ]; then
        cp "$OUT" "$msg"
    else
        head -c "$n" "$OUT" > "$msg"
    fi
    enc="/tmp/sten_bench_${ext}.enc"

    t0=$(date +%s%N)
    "$BIN" encode -i "$img" -o "$enc" -f "$msg" >/dev/null 2>&1
    t1=$(date +%s%N)
    "$BIN" decode -i "$enc" > /dev/null 2>&1
    t2=$(date +%s%N)

    ems=$(ns_ms "$t0" "$t1")
    dms=$(ns_ms "$t1" "$t2")
    printf '%-8s %12s %12s %10s %12s %12s\n' \
        "$name" "$size" "$cap" "$cap/$size" "$ems" "$dms"
done

rm -f "$OUT" /tmp/sten_bench_*.msg /tmp/sten_bench_*.enc