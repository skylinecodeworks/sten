# PNG adapter: RGB/RGBA, rejections and output validity.

M="test message for PNG 0123456789"

check_roundtrip "PNG RGB roundtrip" "$D/test.png" "$M"
check_roundtrip "PNG RGB with key" "$D/test.png" "$M" "key-png"
check_roundtrip "PNG RGBA roundtrip" "$D/rgba.png" "$M"
check_roundtrip "PNG RGBA with key" "$D/rgba.png" "$M" "key-rgba"
check_roundtrip "PNG big" "$D/big.png" "$M"

printf 'bin\0ario\377\376\002png' > "$D/bin_png.dat"
check_roundtrip_file "PNG binary message" "$D/test.png" "$D/bin_png.dat"

assert_rc "PNG clean does not decode -> 1" 1 "$BIN" decode -i "$D/test.png"
"$BIN" encode -i "$D/test.png" -o "$D/keyed.png" -m secret -k k1 >/dev/null 2>&1
assert_rc "PNG wrong key -> 1" 1 "$BIN" decode -i "$D/keyed.png" -k k2
assert_rc "PNG no key on keyed message -> 1" 1 "$BIN" decode -i "$D/keyed.png"

assert_rc "PNG paletted rejected -> 3" 3 "$BIN" encode -i "$D/pal.png" -o "$D/x.png" -m hola
assert_rc "PNG 16-bit rejected -> 3" 3 "$BIN" encode -i "$D/gray16.png" -o "$D/x.png" -m hola

dd if=/dev/zero bs=60000 count=1 2>/dev/null | tr '\0' 'A' > "$D/big.txt"
assert_rc "PNG oversized message -> 3" 3 "$BIN" encode -i "$D/test.png" -o "$D/x.png" -f "$D/big.txt"

"$BIN" encode -i "$D/rgba.png" -o "$D/imgchk.png" -m "$M" >/dev/null 2>&1
check_image "PNG output is valid image" "$D/imgchk.png" "PNG" 64 64

# The PNG output must not depend on the input compression: still a PNG.
"$BIN" decode -i "$D/imgchk.png" >/dev/null 2>&1
assert_eq "PNG output decodes" 0 $?
