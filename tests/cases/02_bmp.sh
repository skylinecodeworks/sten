# BMP adapter: 24/32 bit, in-place, rejections and limits.

M="test message for BMP 0123456789"

check_roundtrip "BMP 24-bit roundtrip" "$D/test.bmp" "$M"
check_roundtrip "BMP 24-bit with key" "$D/test.bmp" "$M" "key-bmp"
check_roundtrip "BMP 32-bit roundtrip" "$D/test32.bmp" "$M"
check_roundtrip "BMP 32-bit with key" "$D/test32.bmp" "$M" "key32"
check_roundtrip "BMP big" "$D/big.bmp" "$M"

printf 'message from file for bmp' > "$D/msg_bmp.txt"
check_roundtrip_file "BMP message from file" "$D/test.bmp" "$D/msg_bmp.txt"
printf 'bin\0ario\377\376\001bmp' > "$D/bin_bmp.dat"
check_roundtrip_file "BMP binary message" "$D/test.bmp" "$D/bin_bmp.dat"

# In-place: the same input and output file must keep the size.
cp "$D/test.bmp" "$D/inplace.bmp"
_before=$(wc -c < "$D/inplace.bmp")
"$BIN" encode -i "$D/inplace.bmp" -o "$D/inplace.bmp" -m "in place" >/dev/null 2>&1
assert_eq "BMP in-place encode rc" 0 $?
_after=$(wc -c < "$D/inplace.bmp")
assert_eq "BMP in-place keeps size" "$_before" "$_after"
_got=$("$BIN" decode -i "$D/inplace.bmp" 2>/dev/null)
assert_eq "BMP in-place roundtrip" "in place" "$_got"

# Re-embedding over an already marked image.
"$BIN" encode -i "$D/test.bmp" -o "$D/twice.bmp" -m "first" >/dev/null 2>&1
"$BIN" encode -i "$D/twice.bmp" -o "$D/twice2.bmp" -m "second" >/dev/null 2>&1
_got=$("$BIN" decode -i "$D/twice2.bmp" 2>/dev/null)
assert_eq "BMP re-embed overwrites" "second" "$_got"

assert_rc "BMP clean does not decode -> 1" 1 "$BIN" decode -i "$D/test.bmp"
"$BIN" encode -i "$D/test.bmp" -o "$D/keyed.bmp" -m secret -k k1 >/dev/null 2>&1
assert_rc "BMP wrong key -> 1" 1 "$BIN" decode -i "$D/keyed.bmp" -k k2
assert_rc "BMP no key on keyed message -> 1" 1 "$BIN" decode -i "$D/keyed.bmp"
assert_rc "BMP 8-bit rejected -> 3" 3 "$BIN" encode -i "$D/gray8.bmp" -o "$D/x.bmp" -m hola

dd if=/dev/zero bs=60000 count=1 2>/dev/null | tr '\0' 'A' > "$D/big.txt"
assert_rc "BMP oversized message -> 3" 3 "$BIN" encode -i "$D/test.bmp" -o "$D/x.bmp" -f "$D/big.txt"

"$BIN" encode -i "$D/test.bmp" -o "$D/imgchk.bmp" -m "$M" >/dev/null 2>&1
check_image "BMP output is valid image" "$D/imgchk.bmp" "BMP" 64 48
