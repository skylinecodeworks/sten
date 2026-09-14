# Assertion helpers for the sten test suite.
# run.sh must define BIN, GEN and D before sourcing this file.

TESTS=0
FAILED=0
SKIPPED=0
RTN=0
FAILED_LOG="$D/.failures"
: > "$FAILED_LOG"

pass() {
    TESTS=$((TESTS + 1))
    printf '  ok   %s\n' "$1"
}

fail() {
    TESTS=$((TESTS + 1))
    FAILED=$((FAILED + 1))
    printf '  FAIL %s\n' "$1"
    printf '%s\n' "$1" >> "$FAILED_LOG"
}

skip() {
    SKIPPED=$((SKIPPED + 1))
    printf '  skip %s\n' "$1"
}

assert_eq() {
    if [ "$2" = "$3" ]; then
        pass "$1"
    else
        fail "$1 (expected [$2], got [$3])"
    fi
}

assert_ne() {
    if [ "$2" != "$3" ]; then
        pass "$1"
    else
        fail "$1 (should not be [$2])"
    fi
}

assert_contains() {
    case "$2" in
        *"$3"*) pass "$1" ;;
        *)      fail "$1 (does not contain [$3]: [$2])" ;;
    esac
}

assert_file_eq() {
    if cmp -s "$2" "$3"; then
        pass "$1"
    else
        fail "$1 (files differ: $2 vs $3)"
    fi
}

# assert_rc <name> <expected_rc> <command...>
assert_rc() {
    _name=$1
    _exp=$2
    shift 2
    "$@" >/dev/null 2>&1
    _rc=$?
    assert_eq "$_name" "$_exp" "$_rc"
}

# Encodes and decodes, comparing the decoded text with the original.
# check_roundtrip <name> <image> <message> [key]
check_roundtrip() {
    _name=$1
    _img=$2
    _msg=$3
    _key=${4-}
    _out="$D/rt_$RTN.out"
    RTN=$((RTN + 1))
    if [ -n "$_key" ]; then
        "$BIN" encode -i "$_img" -o "$_out" -m "$_msg" -k "$_key" >/dev/null 2>&1
        _erc=$?
    else
        "$BIN" encode -i "$_img" -o "$_out" -m "$_msg" >/dev/null 2>&1
        _erc=$?
    fi
    if [ "$_erc" -ne 0 ]; then
        fail "$_name (encode failed)"
        return
    fi
    if [ -n "$_key" ]; then
        _got=$("$BIN" decode -i "$_out" -k "$_key" 2>/dev/null)
    else
        _got=$("$BIN" decode -i "$_out" 2>/dev/null)
    fi
    assert_eq "$_name" "$_msg" "$_got"
}

# Same as check_roundtrip but from a binary file, comparing bytes.
# check_roundtrip_file <name> <image> <message_file> [key]
check_roundtrip_file() {
    _name=$1
    _img=$2
    _msgfile=$3
    _key=${4-}
    _out="$D/rt_$RTN.out"
    _dec="$D/rt_$RTN.dec"
    _exp="$D/rt_$RTN.exp"
    RTN=$((RTN + 1))
    if [ -n "$_key" ]; then
        "$BIN" encode -i "$_img" -o "$_out" -f "$_msgfile" -k "$_key" >/dev/null 2>&1
    else
        "$BIN" encode -i "$_img" -o "$_out" -f "$_msgfile" >/dev/null 2>&1
    fi
    if [ $? -ne 0 ]; then
        fail "$_name (encode failed)"
        return
    fi
    if [ -n "$_key" ]; then
        "$BIN" decode -i "$_out" -k "$_key" > "$_dec" 2>/dev/null
    else
        "$BIN" decode -i "$_out" > "$_dec" 2>/dev/null
    fi
    cat "$_msgfile" > "$_exp"
    printf '\n' >> "$_exp"
    assert_file_eq "$_name" "$_exp" "$_dec"
}

# --- Optional validation of output images with Pillow ---
HAVE_PIL=0
if command -v python3 >/dev/null 2>&1 && python3 -c 'import PIL' >/dev/null 2>&1; then
    HAVE_PIL=1
fi

# check_image <name> <file> <expected_format> <width> <height>
check_image() {
    if [ "$HAVE_PIL" != 1 ]; then
        skip "$1 (no python3/Pillow)"
        return
    fi
    _info=$(python3 - "$2" <<'PY' 2>/dev/null
import sys
from PIL import Image
im = Image.open(sys.argv[1])
im.load()
print(im.format, im.size[0], im.size[1])
PY
)
    assert_eq "$1" "$3 $4 $5" "$_info"
}

summary() {
    printf '\n----------------------------------------\n'
    if [ "$FAILED" -eq 0 ]; then
        printf 'RESULT: %d tests OK, %d skipped\n' "$TESTS" "$SKIPPED"
    else
        printf 'RESULT: %d/%d failed (%d skipped)\n' "$FAILED" "$TESTS" "$SKIPPED"
        printf 'Failures:\n'
        while IFS= read -r _f; do
            printf '  - %s\n' "$_f"
        done < "$FAILED_LOG"
    fi
    printf '%s\n' '----------------------------------------'
    [ "$FAILED" -eq 0 ]
}
