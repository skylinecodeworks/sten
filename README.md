# README

`sten` — ScatterBit steganography for images (pure C, no external libraries).

## Usage

```
sten encode -i image -o output (-m "text" | -f file) [-k key | -p passphrase]
sten decode -i image [-k key | -p passphrase]
sten capacity -i image
sten inspect -i image
```

Options:

| Option | Description |
| --- | --- |
| `-i, --input`  | input image (BMP, PNG, GIF, JPEG, PNM/PAM, TGA, TIFF, ICO) |
| `-o, --output` | output image (encode only) |
| `-m, --message`| text message to hide |
| `-f, --file`   | read the message from a file |
| `-k, --key`    | optional key (derives the bit path) |
| `-p, --passphrase` | optional passphrase (encrypts the payload; ChaCha20 + PBKDF2, random per-message salt) |
| `-v, --verbose`| print the detected format details to stderr |
| `-h, --help`   | show this help |
| `-V, --version`| print the version and exit |

If neither `-m` nor `-f` is given, the message is read from stdin.

The `capacity` command prints the maximum message size (in bytes) that fits
in the given image, using the default redundancy 3. The `inspect` command
reports the format, dimensions, color layout and capacity of an image, so
you can pick the best carrier for a payload.

## Exit codes

| Code | Meaning |
| --- | --- |
| 0 | success (or help shown) |
| 1 | no message found (clean image or wrong key) |
| 2 | usage error (including using both `-k` and `-p`) |
| 3 | operational error (I/O, unsupported format, oversized message) |

## Supported formats

| Format | Carrier | Notes |
| --- | --- | --- |
| BMP    | pixel LSBs (24/32 bpp) | in-place, bit depth per pixel kept |
| PNG    | RGB/RGBA 8-bit raw pixels | re-deflated on output |
| GIF    | global palette bytes | reduced capacity (LSB of palette) |
| JPEG   | COM segment | **does not touch pixels**; see below |
| PNM/PAM| P5/P6 binary, P7 (maxval 255) | in-place |
| TGA    | type 2 (24/32-bit), type 3 (8-bit gray) | in-place |
| TIFF   | little-endian uncompressed RGB strip | in-place |
| ICO    | first PNG entry | header/entries rebuilt, size may change |

### JPEG scope

The JPEG adapter embeds data in a `0xFF 0xFE` (COM) marker inserted right
after the SOI marker. It never re-encodes pixels, so the output remains a
valid JPEG that decodes identically to the input. Consequences:

- The message is stored in the file's comment segment, not in pixel data.
- Capacity is limited (~16 KB of payload).
- The embedded data survives copying, renaming and a re-save by tools that
  preserve COM markers, but not aggressive re-encoding that strips comments.

## Build

```
make            # builds ./sten
make test       # builds and runs the full test suite
make fuzz-san   # mutation fuzzer over the parsers under ASan/UBSan
make test-san   # full suite rebuilt with ASan/UBSan
make install    # installs ./sten and docs/sten.1 under /usr/local
make clean      # removes build artifacts
```

`make install` honours `PREFIX` (default `/usr/local`) and `DESTDIR`, and
installs the manual page to `$(PREFIX)/share/man/man1/sten.1`.

Requires a C99/C11 compiler and `make`. Runs on any POSIX system.

## Testing

Shell-driven integration suite (`tests/run.sh`) plus C unit tests
(`tests/unit.c`). `make test` must stay green before merging any phase.

Phase 3 adds defensive checks:

- All parsers (BMP, PNG, GIF, JPEG, PNM/PAM, TGA, TIFF, ICO) reject
  truncated input, overflowed dimensions/offsets and oversized decompression
  targets; a malicious stream cannot allocate beyond its declared size.
- `tools/fuzz` is a mutation-based harness that runs every adapter against
  thousands of mutated images; `make fuzz-san` runs it under ASan/UBSan.
  The regression corpus lives in `tests/cases/14_fuzz.sh` (valid carriers
  plus adversarial seeds for each format).
- `make test-san` rebuilds the whole suite with ASan/UBSan and is the CI
  gate for memory safety.

## Benchmarks

`make bench` runs `tools/bench.sh` against the generated carriers:
256×256 for BMP/PNG; the test fixtures for the rest. It hides a 1 KB payload
at redundancy 3 and measures wall-clock time on this machine (single run,
results are indicative). `capacity` is the exact capacity reported by
`sten capacity`.

| format | image (B) | capacity (B) | ratio | encode (ms) | decode (ms) |
| --- | --- | --- | --- | --- | --- |
| BMP | 196662 | 24563 | 12.5 % | 4 | 4 |
| PNG | 196947 | 24563 | 12.5 % | 16 | 6 |
| GIF | 799 | 127 | 15.9 % | 2 | 2 |
| JPEG | 369 | 16370 | 4435 % | 2 | 2 |
| PNM/PAM | 1549 | 179 | 11.6 % | 2 | 2 |
| TGA | 1554 | 179 | 11.5 % | 2 | 2 |
| TIFF | 1676 | 179 | 10.7 % | 2 | 2 |
| ICO | 1130 | 115 | 10.2 % | 2 | 2 |

JPEG capacity dwarfs the others because the message lives in a COM segment
and pixels are never touched. For raster carriers, capacity per byte is the
adaptive 1-3 LSB scheme capped by the `SBT2` overhead.

See `ROADMAP.md` for the development roadmap.