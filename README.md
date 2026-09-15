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

## Encryption (passphrase)

Passing `-p` encrypts the stored payload with ChaCha20. A fresh 16-byte salt
and 12-byte nonce are drawn from the OS entropy source for every encoding; the
encryption key is derived from the passphrase with PBKDF2-HMAC-SHA256
(100 000 iterations) over that per-message salt and mixed through SHA-256.
Each encrypted payload ends with a CRC32 tag over the plaintext, which also
serves as the wrong-passphrase detector.

- The bit path is independent from the passphrase: with `-p` the ScatterBit
  path is derived from the image alone, and the encrypted cargo is what
  protects the message. `-k` controls the path but does not encrypt.
- `-k` and `-p` are mutually exclusive.
- Compression happens before encryption, so short messages shrink first and
  are then encrypted.
- The salt is random per encoding, so the output is non-deterministic: the
  same passphrase and image produce a different file on every run. Salt and
  nonce are stored in the payload header.
- Encrypted payloads use the `SBT2` header with the `encoded` flag; plain
  payloads remain fully backward compatible.

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

## Installation / Downloads

Prebuilt packages are attached to every tagged **GitHub Release**:

| Channel | Asset | Install |
| --- | --- | --- |
| Debian/Ubuntu | `sten_<ver>_amd64.deb` | `sudo apt install ./sten_<ver>_amd64.deb` |
| Fedora/RHEL | `sten-<ver>-1.x86_64.rpm` | `sudo dnf install ./sten-<ver>-1.x86_64.rpm` |
| Alpine | `sten-<ver>-r0.apk` | `apk add ./sten-<ver>-r0.apk` |
| Arch | `PKGBUILD` (AUR) | `makepkg -si` on the AUR package |
| Any Linux | `sten-<ver>-linux-musl-x86_64` | static binary; drop it in `$PATH` |
| Source | `sten-<ver>.tar.gz` | tarball for `make && sudo make install` |

Every release ships a `SHA256SUMS` file for verification.
Artifacts are produced by the `releases` GitHub Actions workflow on each
`v*` tag; the `.rpm`/`.apk`/`PKGBUILD` are built from the same tarball in
Fedora and Alpine containers.

## Creating a release

Tag a commit and push the tag; `release.yml` builds all packages and opens a
draft Release with the artifacts attached:

```
git tag v2.0
git push origin v2.0
```

Publish the draft from the GitHub Releases page once the build is green.

## Design

- `ScatterBit`: message bits are spread across carrier bytes selected by a
  PRNG seeded from a content fingerprint plus an optional key. Each carrier
  byte contributes 1-3 LSBs depending on local texture (adaptive depth).
  A `SBT2` header holds flags, payload length and a CRC32 so extraction can
  be verified and truncated/clean images are rejected. Legacy `SBT1`
  payloads are still extracted.
- Phase 1 adds real zlib deflate (LZ77 + fixed Huffman blocks, with a stored
  fallback) so compressible messages shrink before embedding.
- With `-p` the payload is encrypted and the path is independent of the
  passphrase; `/dev/urandom` supplies the per-message salt and nonce.
- Decode probes redundancy 3 -> 2 -> 1 automatically, so an image embedded
  with any redundancy extracts without flags.
- All crypto (SHA-256, HMAC-SHA256, PBKDF2, ChaCha20), hashing (CRC32,
  Adler32, FNV-1a) and zlib inflate are implemented from scratch; no external
  library is used.

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