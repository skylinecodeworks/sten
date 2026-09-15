# Phase 2: passphrase-based encryption (-p) and --capacity reporting.

M="secret phase 2 message"
P="correct horse battery staple"

check_roundtrip_pass() {
    _name=$1
    _img=$2
    _msg=$3
    _pass=$4
    _out="$D/pw_$RTN.out"
    RTN=$((RTN + 1))
    "$BIN" encode -i "$_img" -o "$_out" -m "$_msg" -p "$_pass" >/dev/null 2>&1
    if [ $? -ne 0 ]; then
        fail "$_name (encode failed)"
        return
    fi
    _got=$("$BIN" decode -i "$_out" -p "$_pass" 2>/dev/null)
    assert_eq "$_name" "$_msg" "$_got"
}

check_roundtrip_pass "passphrase BMP roundtrip" "$D/test.bmp" "$M" "$P"
check_roundtrip_pass "passphrase PNG roundtrip" "$D/test.png" "$M" "$P"
check_roundtrip_pass "passphrase GIF roundtrip" "$D/test.gif" "$M" "$P"
check_roundtrip_pass "passphrase PPM roundtrip" "$D/test.ppm" "$M" "$P"
check_roundtrip_pass "passphrase JPEG roundtrip" "$D/test.jpg" "$M" "$P"
check_roundtrip_pass "passphrase short" "$D/test.bmp" "ab" "x"
check_roundtrip_pass "passphrase unicode" "$D/test.bmp" "cifrado con ñandú" "clave ñ"

"$BIN" encode -i "$D/test.bmp" -o "$D/pw.bmp" -m "$M" -p "$P" >/dev/null 2>&1
assert_rc "passphrase wrong pass -> 1" 1 "$BIN" decode -i "$D/pw.bmp" -p "not the pass"
assert_rc "passphrase missing pass -> 1" 1 "$BIN" decode -i "$D/pw.bmp"
assert_rc "passphrase wrong key -k -> 1" 1 "$BIN" decode -i "$D/pw.bmp" -k whatever

printf 'binary\0encrypted\377\376 msg' > "$D/pw_bin.dat"
check_roundtrip_file_pass() {
    _name=$1
    _img=$2
    _msgfile=$3
    _pass=$4
    _out="$D/pwf_$RTN.out"
    _dec="$D/pwf_$RTN.dec"
    _exp="$D/pwf_$RTN.exp"
    RTN=$((RTN + 1))
    "$BIN" encode -i "$_img" -o "$_out" -f "$_msgfile" -p "$_pass" >/dev/null 2>&1
    if [ $? -ne 0 ]; then
        fail "$_name (encode failed)"
        return
    fi
    "$BIN" decode -i "$_out" -p "$_pass" > "$_dec" 2>/dev/null
    cat "$_msgfile" > "$_exp"
    printf '\n' >> "$_exp"
    assert_file_eq "$_name" "$_exp" "$_dec"
}
check_roundtrip_file_pass "passphrase binary file roundtrip" "$D/test.png" "$D/pw_bin.dat" "$P"

"$BIN" encode -i "$D/test.bmp" -o "$D/pwd1.bmp" -m "$M" -p "$P" >/dev/null 2>&1
"$BIN" encode -i "$D/test.bmp" -o "$D/pwd2.bmp" -m "$M" -p "$P" >/dev/null 2>&1
assert_file_neq "passphrase random salt non-deterministic" "$D/pwd1.bmp" "$D/pwd2.bmp"

"$BIN" encode -i "$D/test.bmp" -o "$D/pwd3.bmp" -m "$M" -p "$P" >/dev/null 2>&1
_got_pwd3=$("$BIN" decode -i "$D/pwd3.bmp" -p "$P" 2>/dev/null)
assert_eq "passphrase random salt roundtrip" "$M" "$_got_pwd3"

assert_rc "tags -k and -p exclusive -> 2" 2 "$BIN" encode -i "$D/test.bmp" -o "$D/x.bmp" -m hola -k k1 -p p1
assert_rc "tags -k and -p exclusive decode -> 2" 2 "$BIN" decode -i "$D/test.bmp" -k k1 -p p1

# --capacity
_help=$("$BIN" --help 2>&1)
assert_contains "help mentions -p/--passphrase" "$_help" "passphrase"
assert_contains "help mentions capacity" "$_help" "capacity"

_cap=$("$BIN" capacity -i "$D/test.bmp" 2>/dev/null)
case "$_cap" in
    ''|*[!0-9]*) fail "capacity output is a number (got [$_cap])" ;;
    *)            pass "capacity output is a number" ;;
esac
if [ "$_cap" -gt 0 ] 2>/dev/null; then
    pass "capacity > 0"
else
    fail "capacity > 0 (got $_cap)"
fi

assert_rc "capacity on unknown format -> 3" 3 "$BIN" capacity -i "$D/notimg.txt"
assert_rc "capacity without -i -> 2" 2 "$BIN" capacity

head -c "$_cap" /dev/urandom > "$D/cap_exact.dat"
"$BIN" encode -i "$D/test.bmp" -o "$D/cap_full.bmp" -f "$D/cap_exact.dat" >/dev/null 2>&1
assert_eq "capacity exact fit encode rc" 0 $?
"$BIN" decode -i "$D/cap_full.bmp" > "$D/cap_full.dec" 2>/dev/null
cp "$D/cap_exact.dat" "$D/cap_exact.exp"
printf '\n' >> "$D/cap_exact.exp"
assert_file_eq "capacity exact fit roundtrip" "$D/cap_exact.exp" "$D/cap_full.dec"

_cap2=$((_cap + 1))
head -c "$_cap2" /dev/urandom > "$D/cap_over.dat"
assert_rc "capacity over limit -> 3" 3 "$BIN" encode -i "$D/test.bmp" -o "$D/x.bmp" -f "$D/cap_over.dat"

_cap_png=$("$BIN" capacity -i "$D/test.png" 2>/dev/null)
case "$_cap_png" in
    ''|*[!0-9]*) fail "capacity PNG output is a number (got [$_cap_png])" ;;
    *)            pass "capacity PNG output is a number" ;;
esac