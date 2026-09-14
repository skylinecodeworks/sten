# JPEG adapter: COM segment, size limit and rejections.

M="jpeg marker test abc XYZ"

check_roundtrip "JPEG roundtrip" "$D/test.jpg" "$M"
check_roundtrip "JPEG with key" "$D/test.jpg" "$M" "key-jpeg"
check_roundtrip "JPEG empty message" "$D/test.jpg" ""

printf 'bin\0ario\377jpeg' > "$D/bin_jpg.dat"
check_roundtrip_file "JPEG binary message" "$D/test.jpg" "$D/bin_jpg.dat"

assert_rc "JPEG clean does not decode -> 1" 1 "$BIN" decode -i "$D/test.jpg"
"$BIN" encode -i "$D/test.jpg" -o "$D/keyed.jpg" -m secret -k k1 >/dev/null 2>&1
assert_rc "JPEG wrong key -> 1" 1 "$BIN" decode -i "$D/keyed.jpg" -k k2
assert_rc "JPEG no key on keyed message -> 1" 1 "$BIN" decode -i "$D/keyed.jpg"

dd if=/dev/zero bs=20000 count=1 2>/dev/null | tr '\0' 'x' > "$D/jpg_big.txt"
assert_rc "JPEG oversized message -> 3" 3 "$BIN" encode -i "$D/test.jpg" -o "$D/x.jpg" -f "$D/jpg_big.txt"

"$BIN" encode -i "$D/test.jpg" -o "$D/imgchk.jpg" -m "$M" >/dev/null 2>&1
_magic=$(xxd -p -l 4 "$D/imgchk.jpg")
assert_eq "JPEG inserts COM marker after SOI" "ffd8fffe" "$_magic"
_tail=$(tail -c 2 "$D/imgchk.jpg" | xxd -p)
assert_eq "JPEG ends in EOI" "ffd9" "$_tail"
_size_in=$(wc -c < "$D/test.jpg")
_size_out=$(wc -c < "$D/imgchk.jpg")
if [ "$_size_out" -gt "$_size_in" ]; then
    pass "JPEG grows when inserting the COM segment"
else
    fail "JPEG grows when inserting the COM segment ($_size_in -> $_size_out)"
fi
