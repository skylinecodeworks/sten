# Phase 2: Netpbm adapters (PPM P6, PGM P5, PAM P7 binary).

M="netpbm message"

check_roundtrip "PPM roundtrip" "$D/test.ppm" "$M"
check_roundtrip "PPM with key" "$D/test.ppm" "$M" "key-ppm"
check_roundtrip "PGM roundtrip" "$D/test.pgm" "$M"
check_roundtrip "PGM with key" "$D/test.pgm" "$M" "key-pgm"
check_roundtrip "PAM roundtrip" "$D/test.pam" "$M"
check_roundtrip "PAM with key" "$D/test.pam" "$M" "key-pam"

"$BIN" encode -i "$D/test.ppm" -o "$D/sz.ppm" -m "$M" >/dev/null 2>&1
assert_rc "PPM direct decode -> 0" 0 "$BIN" decode -i "$D/sz.ppm"
_after=$(wc -c < "$D/sz.ppm")
assert_eq "PPM keeps file size" "$(wc -c < "$D/test.ppm")" "$_after"

printf 'P6\n3 2\n255\n\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b' > "$D/bad.ppm"
assert_rc "PPM short pixel data -> 3" 3 "$BIN" encode -i "$D/bad.ppm" -o "$D/x.ppm" -m hola

printf 'P3\n1 1\n255\n1 2 3\n' > "$D/text.ppm"
assert_rc "PPM P3 text rejected -> 3" 3 "$BIN" encode -i "$D/text.ppm" -o "$D/x.ppm" -m hola

printf 'P5\n1 1\n255\nabc' > "$D/bad.pgm"
assert_rc "PGM short data -> 3" 3 "$BIN" encode -i "$D/bad.pgm" -o "$D/x.pgm" -m hola

printf 'P7\nWIDTH 1\nHEIGHT 1\nDEPTH 3\nMAXVAL 255\nTUPLTYPE RGB\nENDHDR\nabc' > "$D/bad.pam"
assert_rc "PAM short data -> 3" 3 "$BIN" encode -i "$D/bad.pam" -o "$D/x.pam" -m hola

"$BIN" encode -i "$D/test.ppm" -o "$D/imgchk.ppm" -m "$M" >/dev/null 2>&1
check_image "PPM output is valid image" "$D/imgchk.ppm" "PPM" 32 16

_head=$(od -An -tc -N 3 "$D/test.pgm" | tr -d ' \n')
case "$_head" in
    P5*) pass "PGM header preserved" ;;
    *)    fail "PGM header preserved (got $_head)" ;;
esac
_head=$(od -An -tc -N 3 "$D/test.pam" | tr -d ' \n')
case "$_head" in
    P7*) pass "PAM header preserved" ;;
    *)    fail "PAM header preserved (got $_head)" ;;
esac

assert_rc "PPM oversized -> 3" 3 "$BIN" encode -i "$D/test.ppm" -o "$D/x.ppm" -f "$D/big.txt"