# Phase 3: fuzzing harness + regression corpus + robustness on adversarial
# inputs. make test runs the plain harness; make fuzz-san runs it under
# ASan/UBSan so any memory error aborts with a nonzero exit code.

FUZZ="$ROOT/tools/fuzz"
FUZZ_DIR="$D/fuzz"
mkdir -p "$FUZZ_DIR"

if [ ! -x "$FUZZ" ]; then
    fail "fuzz harness not built (run 'make')"
else
    # --- Regression corpus: one valid carrier per format ---
    for f in test.bmp test.png test.gif test.jpg test.ppm test.tga test.tiff test.ico; do
        [ -f "$D/$f" ] && cp "$D/$f" "$FUZZ_DIR/val_$f"
    done

    # --- Adversarial seeds ---
    _w() { printf "$1" > "$FUZZ_DIR/$2"; }

    # PNG with an unknown filter byte (5) in the scanline
    _w '\211PNG\r\n\032\n\000\000\000\015IHDR\000\000\000\001\000\000\000\001\010\002\000\000\000\000\000\000\000\017IDAT\170\001\001\004\000\373\377\005\000\000\000\006\000\007\000\000\000\000\000\000\000\000IEND\000\000\000\000' adv_png_filter5.png
    # PNG with huge dimensions (65535x65535) and a tiny IDAT
    _w '\211PNG\r\n\032\n\000\000\000\015IHDR\000\000\377\377\000\000\377\377\010\002\000\000\000\000\000\000\000\017IDAT\170\001\001\004\000\373\377\000\000\000\000\001\000\001\000\000\000\000\000\000\000\000IEND\000\000\000\000' adv_png_huge.png
    # PNG with a zero dimension
    _w '\211PNG\r\n\032\n\000\000\000\015IHDR\000\000\000\000\000\000\000\001\010\002\000\000\000\000\000\000\000\017IDAT\170\001\001\004\000\373\377\000\000\000\000\001\000\001\000\000\000\000\000\000\000\000IEND\000\000\000\000' adv_png_zerodim.png
    # BMP with absurd dimensions/offset (pixel data "out of range")
    _w 'BM\000\000\000\000\000\000\000\000\360\377\377\377\050\000\000\000\177\377\377\177\177\377\377\177\001\000\030\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000\000' adv_bmp_huge.bmp
    # GIF declaring a global palette larger than the file
    _w 'GIF89a\001\000\001\000\207\000\000' adv_gif_truncpal.gif
    # GIF without a global palette
    _w 'GIF89a\001\000\001\000\000\000\000' adv_gif_nopal.gif
    # JPEG with a COM segment length past EOF
    _w '\377\330\377\376\377\377\377\330\000\000\000\000\000\000' adv_jpeg_comhuge.jpg
    # TGA with a color map (unsupported)
    _w '\000\001\001\000\000\000\000\000\000\000\000\000\001\000\001\000\030\000' adv_tga_colormap.tga
    # Big-endian TIFF
    _w 'MM\000\052\000\000\000\010\001\000\000\001\000\001\000\000\000\000' adv_tiff_be.tiff
    # TIFF with the IFD pointing past EOF
    _w 'II\052\000\377\377\377\377' adv_tiff_ifd.tiff
    # ICO entry pointing past EOF
    _w '\000\000\001\000\001\000\020\020\000\000\000\000\001\000\040\000\104\004\000\000\377\377\377\377' adv_ico_badoff.ico
    # PPM with maxval != 255
    _w 'P6\n1 1\n256\n\000\000\000' adv_ppm_maxval.ppm
    # PAM without ENDHDR
    _w 'P7\nWIDTH 1\nHEIGHT 1\nDEPTH 3\nMAXVAL 255\n' adv_pam_noend.pam

    pass "fuzz corpus prepared"

    ITERS=${FUZZ_ITERS:-400}
    # shellcheck disable=SC2086
    "$FUZZ" "$ITERS" "$FUZZ_DIR"/* >/dev/null 2>&1
    assert_eq "fuzz harness clean run (rc 0)" 0 $?
fi

# --- Robustness: adversarial seeds must never crash the CLI ---
for f in "$FUZZ_DIR"/adv_*; do
    [ -f "$f" ] || continue
    _name=$(basename "$f")
    "$BIN" decode -i "$f" >/dev/null 2>&1
    _rc=$?
    case "$_rc" in
        1|3) pass "decode rejects $_name (rc $_rc)" ;;
        *)   fail "decode $_name (unexpected rc $_rc)" ;;
    esac
    "$BIN" capacity -i "$f" >/dev/null 2>&1
    _rc=$?
    case "$_rc" in
        0|3) pass "capacity safe on $_name (rc $_rc)" ;;
        *)   fail "capacity $_name (unexpected rc $_rc)" ;;
    esac
done