# Phase 2: ICO adapter (PNG-encoded entry reused through the PNG adapter).

M="ico message"

check_roundtrip "ICO roundtrip" "$D/test.ico" "$M"
check_roundtrip "ICO with key" "$D/test.ico" "$M" "key-ico"

# The embedded ICO must have a PNG entry and still be a valid icon image.
"$BIN" encode -i "$D/test.ico" -o "$D/imgchk.ico" -m "$M" >/dev/null 2>&1
if [ -f "$D/imgchk.ico" ]; then
    _sig=$(od -An -tx1 -j 22 -N 8 "$D/imgchk.ico" | tr -d ' \n')
    case "$_sig" in
        89504e470d0a1a0a) pass "ICO output keeps PNG entry" ;;
        *)                fail "ICO output keeps PNG entry (got $_sig)" ;;
    esac
    check_image "ICO output is valid image" "$D/imgchk.ico" "ICO" 16 16
else
    fail "ICO output keeps PNG entry (no output)"
fi

printf '\x00\x00\x01\x00\x00\x00' > "$D/empty.ico"
assert_rc "ICO empty -> 3" 3 "$BIN" encode -i "$D/empty.ico" -o "$D/x.ico" -m hola

printf '\x00\x00\x01\x00\x01\x00\x10\x10\x00\x00\x01\x00\x20\x00\x68\x04\x00\x00\x16\x00\x00\x00' > "$D/bmp.ico"
assert_rc "ICO without PNG entry -> 3" 3 "$BIN" encode -i "$D/bmp.ico" -o "$D/x.ico" -m hola

assert_rc "ICO oversized -> 3" 3 "$BIN" encode -i "$D/test.ico" -o "$D/x.ico" -f "$D/big.txt"