# GIF adapter: global palette, reduced capacity and rejections.

M="gif palette test 123"

check_roundtrip "GIF roundtrip" "$D/test.gif" "$M"
check_roundtrip "GIF with key" "$D/test.gif" "$M" "key-gif"
check_roundtrip "GIF empty message" "$D/test.gif" ""

printf 'bin\0ario\003gif' > "$D/bin_gif.dat"
check_roundtrip_file "GIF binary message" "$D/test.gif" "$D/bin_gif.dat"

assert_rc "GIF clean does not decode -> 1" 1 "$BIN" decode -i "$D/test.gif"
"$BIN" encode -i "$D/test.gif" -o "$D/keyed.gif" -m secret -k k1 >/dev/null 2>&1
assert_rc "GIF wrong key -> 1" 1 "$BIN" decode -i "$D/keyed.gif" -k k2

assert_rc "GIF without global palette rejected -> 3" 3 "$BIN" encode -i "$D/nopal.gif" -o "$D/x.gif" -m hola

head -c 5000 /dev/urandom > "$D/gif_big.txt"
assert_rc "GIF oversized message -> 3" 3 "$BIN" encode -i "$D/test.gif" -o "$D/x.gif" -f "$D/gif_big.txt"

"$BIN" encode -i "$D/test.gif" -o "$D/imgchk.gif" -m "$M" >/dev/null 2>&1
check_image "GIF output is valid image" "$D/imgchk.gif" "GIF" 1 1
