# Commands: inspect, verbose, version, man page / install target.
. "$ROOT/tests/lib.sh"

# --version prints the version
_version=$("$BIN" --version 2>/dev/null)
assert_contains "--version prints version" "$_version" "sten"
_verrc=$("$BIN" --version >/dev/null 2>&1; echo $?)
assert_eq "--version exits 0" 0 "$_verrc"

# --help still works
_help=$("$BIN" --help 2>/dev/null)
assert_contains "--help mentions inspect" "$_help" "inspect"
assert_contains "--help mentions verbose" "$_help" "verbose"

# inspect: BMP reports format, dimensions and capacity
_bmp=$("$BIN" inspect -i "$D/test.bmp" 2>/dev/null)
assert_contains "inspect bmp format" "$_bmp" "format: BMP"
assert_contains "inspect bmp width" "$_bmp" "width: 64"
assert_contains "inspect bmp height" "$_bmp" "height: 48"
assert_contains "inspect bmp channels" "$_bmp" "channels: 3"
assert_contains "inspect bmp capacity" "$_bmp" "capacity: "
_cap=$("$BIN" capacity -i "$D/test.bmp" 2>/dev/null)
assert_contains "inspect bmp capacity == capacity" "$_bmp" "capacity: $_cap bytes"

# inspect: PNG
_png=$("$BIN" inspect -i "$D/test.png" 2>/dev/null)
assert_contains "inspect png format" "$_png" "format: PNG"
assert_contains "inspect png width/height" "$_png" "width: 64"
assert_contains "inspect png height" "$_png" "height: 64"

# inspect: GIF
_gif=$("$BIN" inspect -i "$D/test.gif" 2>/dev/null)
assert_contains "inspect gif format" "$_gif" "format: GIF"

# inspect: JPEG (SOF scan)
_jpg=$("$BIN" inspect -i "$D/test.jpg" 2>/dev/null)
assert_contains "inspect jpeg format" "$_jpg" "format: JPEG"
assert_contains "inspect jpeg width" "$_jpg" "width: 1"

# inspect: PNM / TGA / TIFF / ICO
assert_contains "inspect ppm format" "$("$BIN" inspect -i "$D/test.ppm" 2>/dev/null)" "format: PNM/PAM"
assert_contains "inspect tga format" "$("$BIN" inspect -i "$D/test.tga" 2>/dev/null)" "format: TGA"
assert_contains "inspect tiff format" "$("$BIN" inspect -i "$D/test.tiff" 2>/dev/null)" "format: TIFF"
assert_contains "inspect ico format" "$("$BIN" inspect -i "$D/test.ico" 2>/dev/null)" "format: ICO"
_ico=$("$BIN" inspect -i "$D/test.ico" 2>/dev/null)
assert_contains "inspect ico width" "$_ico" "width: 16"

# inspect on a corrupt image fails
printf 'garbage-not-an-image' > "$D/ux_bad.bin"
assert_rc "inspect corrupt image rc" 3 "$BIN" inspect -i "$D/ux_bad.bin"
rm -f "$D/ux_bad.bin"

# verbose: encode/decode print the format to stderr
_err=$(printf 'hola ux' | "$BIN" encode -i "$D/test.png" -o "$D/ux_v.png" -v 2>&1 >/dev/null)
assert_contains "verbose encode mentions format" "$_err" "encoding into PNG"
_err=$(printf 'hola ux' | "$BIN" encode -i "$D/test.png" -o "$D/ux_v.png" 2>/dev/null)
_verr=$("$BIN" decode -i "$D/ux_v.png" -v 2>&1 >/dev/null)
assert_contains "verbose decode mentions format" "$_verr" "decoding PNG"
rm -f "$D/ux_v.png"

# exhaustive field names so future regressions are visible
_i=$("$BIN" inspect -i "$D/test.bmp" 2>/dev/null)
for _k in "format:" "width:" "height:" "channels:" "bits per channel:" "capacity:"; do
    case "$_i" in
        *"$_k"*) pass "inspect field [$_k]" ;;
        *)       fail "inspect missing field [$_k]" ;;
    esac
done

# Makefile install target exists
_mf=$(cat "$ROOT/Makefile")
assert_contains "Makefile has install target" "$_mf" "install:"