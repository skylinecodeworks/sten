# Roundtrip properties: determinism, stdin, UTF-8, multiline and output.

# Determinism: the same input and message produce identical bytes.
"$BIN" encode -i "$D/test.png" -o "$D/det1.png" -m "deterministic" >/dev/null 2>&1
"$BIN" encode -i "$D/test.png" -o "$D/det2.png" -m "deterministic" >/dev/null 2>&1
assert_file_eq "PNG deterministic" "$D/det1.png" "$D/det2.png"

# Different keys produce different outputs.
"$BIN" encode -i "$D/test.png" -o "$D/k1.png" -m "same" -k one >/dev/null 2>&1
"$BIN" encode -i "$D/test.png" -o "$D/k2.png" -m "same" -k two >/dev/null 2>&1
if cmp -s "$D/k1.png" "$D/k2.png"; then
    fail "different keys produce different outputs"
else
    pass "different keys produce different outputs"
fi

# Message via stdin (without -m or -f).
printf 'message via stdin' | "$BIN" encode -i "$D/test.png" -o "$D/stdin.png" >/dev/null 2>&1
_got=$("$BIN" decode -i "$D/stdin.png" 2>/dev/null)
assert_eq "PNG message via stdin" "message via stdin" "$_got"

# UTF-8 and special characters.
U="áéíóú ñ 日本語 \$(pwd) \"quotes\" 100%"
check_roundtrip "PNG UTF-8" "$D/test.png" "$U"
check_roundtrip "BMP UTF-8" "$D/test.bmp" "$U"

# Multiline from file.
printf 'line1\nline2\nline3\n' > "$D/multi.txt"
check_roundtrip_file "PNG multiline" "$D/test.png" "$D/multi.txt"

# The decode output ends in exactly one newline.
printf 'no newline' > "$D/exp_nl.txt"
printf '\n' >> "$D/exp_nl.txt"
"$BIN" encode -i "$D/test.png" -o "$D/nl.png" -m "no newline" >/dev/null 2>&1
"$BIN" decode -i "$D/nl.png" > "$D/nl.out" 2>/dev/null
assert_file_eq "decode appends a single newline" "$D/exp_nl.txt" "$D/nl.out"

# Empty message in BMP and PNG.
check_roundtrip "BMP empty message" "$D/test.bmp" ""
check_roundtrip "PNG empty message" "$D/test.png" ""
