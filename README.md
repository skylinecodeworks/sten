# README

`sten` — ScatterBit steganography for images (pure C, no external libraries).

## Usage

```
sten encode -i image -o output (-m "text" | -f file) [-k key | -p passphrase]
sten decode -i image [-k key | -p passphrase]
sten capacity -i image
```

Options:

| Option | Description |
| --- | --- |
| `-i, --input`  | input image (BMP, PNG, GIF, JPEG, PNM/PAM, TGA, TIFF, ICO) |
| `-o, --output` | output image (encode only) |
| `-m, --message`| text message to hide |
| `-f, --file`   | read the message from a file |
| `-k, --key`    | optional key (derives the bit path) |
| `-p, --passphrase` | optional passphrase (encrypts the payload) |
| `-h, --help`   | show this help |

If neither `-m` nor `-f` is given, the message is read from stdin.

The `capacity` command prints the maximum message size (in bytes) that fits
in the given image, using the default redundancy 3.

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

## Encryption (phase 2)

Passing `-p` activates payload encryption. The ChaCha20 key is derived from
the passphrase with PBKDF2-HMAC-SHA256 (100 000 iterations), and the same
derived key is also used as the ScatterBit path key, so a wrong passphrase
fails both the path and the payload integrity check. The payload carries a
fresh salt/nonce pair (deterministic per image+key), and each encrypted
payload ends with a CRC32 tag over the plaintext.

- `-k` and `-p` are mutually exclusive.
- Compression (phase 1) happens before encryption, so short messages shrink
  first and are then encrypted.
- Encrypted payloads use the `SBT2` header with the `encoded` flag; plain
  payloads remain fully backward compatible.

## Build

```
make            # builds ./sten
make test       # builds and runs the full test suite
make clean      # removes build artifacts
```

Requires a C99/C11 compiler and `make`. Runs on any POSIX system.

## Design

- `ScatterBit`: message bits are spread across carrier bytes selected by a
  PRNG seeded from a content fingerprint plus an optional key. Each carrier
  byte contributes 1-3 LSBs depending on local texture (adaptive depth).
  A `SBT2` header holds flags, payload length and a CRC32 so extraction can
  be verified and truncated/clean images are rejected. Legacy `SBT1`
  payloads are still extracted.
- Phase 1 adds real zlib deflate (LZ77 + fixed Huffman blocks, with a stored
  fallback) so compressible messages shrink before embedding.
- Decode probes redundancy 3 -> 2 -> 1 automatically, so an image embedded
  with any redundancy extracts without flags.
- All crypto (SHA-256, HMAC-SHA256, PBKDF2, ChaCha20), hashing (CRC32,
  Adler32, FNV-1a) and zlib inflate are implemented from scratch; no external
  library is used.

## Testing

Shell-driven integration suite (`tests/run.sh`) plus C unit tests
(`tests/unit.c`). `make test` must stay green before merging any phase.

See `ROADMAP.md` for the development roadmap.