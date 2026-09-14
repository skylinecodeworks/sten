# Malformed, truncated or non-image inputs: must not crash.

head -c 200 "$D/test.png" > "$D/trunc.png"
assert_rc "PNG truncated -> 3" 3 "$BIN" decode -i "$D/trunc.png"

printf '\x89PNG\r\n\x1a\ngarbage' > "$D/bad.png"
assert_rc "PNG with magic and garbage -> 3" 3 "$BIN" decode -i "$D/bad.png"

head -c 100 "$D/test.jpg" > "$D/trunc.jpg"
assert_rc "JPEG truncated -> 1" 1 "$BIN" decode -i "$D/trunc.jpg"

head -c 100 "$D/test.gif" > "$D/trunc.gif"
"$BIN" decode -i "$D/trunc.gif" >/dev/null 2>&1
_rc=$?
assert_ne "GIF truncated does not decode" "0" "$_rc"

head -c 4096 /dev/urandom > "$D/random.bin"
assert_rc "random file -> 3" 3 "$BIN" decode -i "$D/random.bin"

: > "$D/empty.bin"
assert_rc "empty file -> 3" 3 "$BIN" decode -i "$D/empty.bin"
assert_rc "encode of empty file -> 3" 3 "$BIN" encode -i "$D/empty.bin" -o "$D/x" -m hola

assert_rc "PNG 1x1 no capacity -> 3" 3 "$BIN" encode -i "$D/tiny.png" -o "$D/x.png" -m "does not fit"

# A directory as input must not hang.
assert_rc "directory as input -> 3" 3 "$BIN" decode -i "$D"

# Detection is by content: the wrong extension does not matter.
cp "$D/test.png" "$D/mal_extension.bmp"
check_roundtrip "content-based detection (png with .bmp)" "$D/mal_extension.bmp" "content wins"

# A marked image that is then truncated must not decode cleanly.
"$BIN" encode -i "$D/test.bmp" -o "$D/full.bmp" -m "robust message" >/dev/null 2>&1
head -c 60 "$D/full.bmp" > "$D/cut.bmp"
"$BIN" decode -i "$D/cut.bmp" >/dev/null 2>&1
_rc=$?
assert_ne "marked BMP truncated does not decode" "0" "$_rc"

# Decoding a clean image different from the one used to embed must fail.
"$BIN" encode -i "$D/test.bmp" -o "$D/marked.bmp" -m "only for test.bmp" >/dev/null 2>&1
assert_rc "different fingerprint does not decode -> 1" 1 "$BIN" decode -i "$D/test32.bmp"
