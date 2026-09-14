# Phase 2: TGA adapter (24-bit uncompressed true-color).

M="tga message"

check_roundtrip "TGA roundtrip" "$D/test.tga" "$M"
check_roundtrip "TGA with key" "$D/test.tga" "$M" "key-tga"
check_roundtrip "TGA binary message" "$D/test.tga" "bin\000tga"

"$BIN" encode -i "$D/test.tga" -o "$D/sz.tga" -m "$M" >/dev/null 2>&1
assert_eq "TGA keeps file size" "$(wc -c < "$D/test.tga")" "$(wc -c < "$D/sz.tga")"

printf '\x00\x00\x10\x00\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x01\x00\x08\x00\xff' > "$D/bad.tga"
assert_rc "TGA invalid type -> 3" 3 "$BIN" encode -i "$D/bad.tga" -o "$D/x.tga" -m hola

printf '\x00\x00\x02\x00\x00\x00\x00\x00\x00\x00\x00\x00\x20\x00\x10\x00\x18\x20\x01\x02\x03\x04' > "$D/tga.bin"
assert_rc "TGA truncated pixels -> 3" 3 "$BIN" encode -i "$D/tga.bin" -o "$D/x.tga" -m hola

"$BIN" encode -i "$D/test.tga" -o "$D/imgchk.tga" -m "$M" >/dev/null 2>&1
check_image "TGA output is valid image" "$D/imgchk.tga" "TGA" 32 16

assert_rc "TGA oversized -> 3" 3 "$BIN" encode -i "$D/test.tga" -o "$D/x.tga" -f "$D/big.txt"