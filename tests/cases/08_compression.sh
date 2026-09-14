# Phase 1: real deflate keeps the PNG output compact on compressible input,
# and message payloads are compressed before embedding.

check_roundtrip "flat PNG roundtrip" "$D/flat.png" "compression test message"
check_roundtrip "flat PNG roundtrip long" "$D/flat.png" "ABCDEFGHIJKLMNOPQRSTUVWXYZ abcdefghijklmnopqrstuvwxyz 0123456789 0123456789 0123456789 0123456789 0123456789 0123456789 0123456789 0123456789 0123456789 0123456789"

"$BIN" encode -i "$D/flat.png" -o "$D/flat_out.png" -m "compression test message" >/dev/null 2>&1
_sz_in=$(wc -c < "$D/flat.png")
_sz_out=$(wc -c < "$D/flat_out.png")
if [ "$_sz_out" -lt "$_sz_in" ]; then
    pass "flat PNG output shrinks ($_sz_in -> $_sz_out)"
else
    fail "flat PNG output shrinks ($_sz_in -> $_sz_out)"
fi

# A large compressible message roundtrips exactly from a file.
: > "$D/repeat.txt"
for _i in 1 2 3 4 5 6 7 8 9 10; do
    printf 'the quick brown fox jumps over the lazy dog. ' >> "$D/repeat.txt"
done
check_roundtrip_file "flat PNG compressed message from file" "$D/flat.png" "$D/repeat.txt"

# Determinism holds under the new compressed payload path.
"$BIN" encode -i "$D/flat.png" -o "$D/detc1.png" -m "deterministic compressed key abc" -k "key-bmp" >/dev/null 2>&1
"$BIN" encode -i "$D/flat.png" -o "$D/detc2.png" -m "deterministic compressed key abc" -k "key-bmp" >/dev/null 2>&1
assert_file_eq "compressed payload deterministic" "$D/detc1.png" "$D/detc2.png"

"$BIN" encode -i "$D/flat.png" -o "$D/flat_chk.png" -m "compression test message" >/dev/null 2>&1
check_image "flat PNG output is valid image" "$D/flat_chk.png" "PNG" 64 64