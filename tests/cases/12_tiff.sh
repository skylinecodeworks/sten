# Phase 2: TIFF adapter (little-endian baseline, uncompressed RGB strip).

M="tiff message"

check_roundtrip "TIFF roundtrip" "$D/test.tiff" "$M"
check_roundtrip "TIFF with key" "$D/test.tiff" "$M" "key-tiff"
check_roundtrip "TIFF binary message" "$D/test.tiff" "bin\000tiff"

"$BIN" encode -i "$D/test.tiff" -o "$D/sz.tiff" -m "$M" >/dev/null 2>&1
assert_eq "TIFF keeps file size" "$(wc -c < "$D/test.tiff")" "$(wc -c < "$D/sz.tiff")"

printf 'MM\x00\x2a\x00\x00\x00\x08\x00\x00\x00\x01\x00\x00\x00\x00' > "$D/bigend.tiff"
assert_rc "TIFF big-endian rejected -> 3" 3 "$BIN" encode -i "$D/bigend.tiff" -o "$D/x.tiff" -m hola

printf 'II\x00\x2a\x00\x00\x00\x08\x00\x00\x00\x00' > "$D/nofd.tiff"
assert_rc "TIFF no entries -> 3" 3 "$BIN" encode -i "$D/nofd.tiff" -o "$D/x.tiff" -m hola

cp "$D/test.tiff" "$D/trunc.tiff"
head -c 40 "$D/test.tiff" > "$D/trunc.tiff"
assert_rc "TIFF truncated IFD -> 3" 3 "$BIN" encode -i "$D/trunc.tiff" -o "$D/x.tiff" -m hola

"$BIN" encode -i "$D/test.tiff" -o "$D/imgchk.tiff" -m "$M" >/dev/null 2>&1
check_image "TIFF output is valid image" "$D/imgchk.tiff" "TIFF" 32 16

assert_rc "TIFF oversized -> 3" 3 "$BIN" encode -i "$D/test.tiff" -o "$D/x.tiff" -f "$D/big.txt"