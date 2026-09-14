# Command line interface: usage, exit codes and option validation.

assert_rc "no arguments -> 2" 2 "$BIN"
assert_rc "unknown command -> 2" 2 "$BIN" frobnicate -i "$D/test.bmp"
assert_rc "encode without -i -> 2" 2 "$BIN" encode -o "$D/x.bmp" -m hola
assert_rc "encode without -o -> 2" 2 "$BIN" encode -i "$D/test.bmp" -m hola
assert_rc "decode without -i -> 2" 2 "$BIN" decode
assert_rc "encode with -m and -f -> 2" 2 "$BIN" encode -i "$D/test.bmp" -o "$D/x.bmp" -m hola -f "$D/test.bmp"

assert_rc "encode --help -> 0" 0 "$BIN" encode --help
assert_rc "decode --help -> 0" 0 "$BIN" decode --help
assert_rc "-h as command -> 0" 0 "$BIN" -h
assert_rc "--help as command -> 0" 0 "$BIN" --help

_help=$("$BIN" --help 2>&1)
assert_contains "help mentions encode" "$_help" "encode"
assert_contains "help mentions decode" "$_help" "decode"
assert_contains "help mentions -k" "$_help" "--key"

printf 'not an image\n' > "$D/notimg.txt"
assert_rc "encode of unknown format -> 3" 3 "$BIN" encode -i "$D/notimg.txt" -o "$D/x" -m hola
assert_rc "decode of unknown format -> 3" 3 "$BIN" decode -i "$D/notimg.txt"
assert_rc "encode with missing input -> 3" 3 "$BIN" encode -i "$D/missing.bmp" -o "$D/x.bmp" -m hola
assert_rc "decode with missing input -> 3" 3 "$BIN" decode -i "$D/missing.bmp"
assert_rc "encode to unwritable path -> 3" 3 "$BIN" encode -i "$D/test.bmp" -o "$D/no_dir/x.bmp" -m hola
