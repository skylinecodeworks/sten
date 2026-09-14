# README

`sten` — ScatterBit steganography for images (pure C, no external libraries).

## Usage

```
sten encode -i image -o output (-m "text" | -f file) [-k key]
sten decode -i image [-k key]
```

Options:

| Option | Description |
| --- | --- |
| `-i, --input`  | input image (BMP, PNG, GIF, JPEG) |
| `-o, --output` | output image (encode only) |
| `-m, --message`| text message to hide |
| `-f, --file`   | read the message from a file |
| `-k, --key`    | optional key (derives the bit path) |
| `-h, --help`   | show this help |

If neither `-m` nor `-f` is given, the message is read from stdin.

## Exit codes

| Code | Meaning |
| --- | --- |
| 0 | success (or help shown) |
| 1 | no message found (clean image or wrong key) |
| 2 | usage error |
| 3 | operational error (I/O, unsupported format, oversized message) |

## Supported formats

| Format | Carrier | Notes |
| --- | --- | --- |
| BMP    | pixel LSBs (24/32 bpp) | in-place, bit depth per pixel kept |
| PNG    | RGB/RGBA 8-bit raw pixels | re-deflated on output |
| GIF    | global palette bytes | reduced capacity (LSB of palette) |
| JPEG   | COM segment | **does not touch pixels**; see below |

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
make clean      # removes build artifacts
```

Requires a C99/C11 compiler and `make`. Runs on any POSIX system.

## Design

- `ScatterBit`: message bits are spread across carrier bytes selected by a
  PRNG seeded from a content fingerprint plus an optional key. Each carrier
  byte contributes 1-3 LSBs depending on local texture (adaptive depth).
  A `SBT1` header holds payload length and a CRC32 so extraction can be
  verified and truncated/clean images are rejected.
- Decode probes redundancy 3 -> 2 -> 1 automatically, so an image embedded
  with any redundancy extracts without flags.
- All hashing (CRC32, Adler32, FNV-1a) and zlib inflate are implemented
  from scratch; no external library is used.

## Testing

Shell-driven integration suite (`tests/run.sh`) plus C unit tests
(`tests/unit.c`). `make test` must stay green before merging any phase.

See `ROADMAP.md` for the development roadmap.