# ROADMAP

Development roadmap for `sten`, ScatterBit steganography for images
(pure C11, zero external dependencies).

Scope decisions:

- **Capacity and utility** take priority over steganalysis resistance.
- All dependencies are **vendored** (implemented in-repo; nothing is linked externally).
- JPEG uses the **COM segment** (no pixel re-encoding); scope is formalized and its limits documented.

## Current status

- C11 CLI: `encode` / `decode` / `capacity` / `inspect`, options `-k` / `-p` / `-v` / `-V` / `-h`.
- Formats: BMP, PNG, GIF, JPEG (COM segment), PNM/PAM, TGA (uncompressed), TIFF, ICO (reuses the PNG adapter).
- ScatterBit payload `SBT2`: content fingerprinting + PRNG path keyed by raw key (or PBKDF2-derived with `-p`), adaptive redundancy 1-3, optional zlib compression (LZ77 + fixed Huffman) and ChaCha20 encryption, CRC32 integrity check, auto-extract probing redundancy 3->1. Legacy `SBT1` messages still decode.
- Tests: 15 shell cases + C unit tests (101 passing), generated fixtures, optional Pillow validation, mutation fuzzer `tools/fuzz`, and ASan/UBSan gates (`make fuzz-san` / `make test-san`).
- Docs: README, man page `docs/sten.1`, `make install` (PREFIX/DESTDIR), benchmarks (`make bench`).
- Git: phases 0-4 committed and merged to `main`; clean history.

## Phase 0 — Minimal foundation (done)

- Initial repo commit, README (usage, per-format capacity, JPEG/COM scope). Commit `25d1e23`.
- `make test` green as the gate for every subsequent phase.

## Phase 1 — Capacity: real compression (done)

`deflate_zlib_stored` no longer inflates output PNGs.

1. Deflate encoder with LZ77 + fixed Huffman for PNG; inflate (stored/fixed/dynamic) already existed.
2. Message is compressed before embedding, flagged in the header.
3. `SBT1` header extended with version + flags; legacy v1 messages still read (backward compatible).
4. Tests: `output_PNG_size <= input_size + X` and large-message roundtrips.

Carried-over debt: dynamic-Huffman encoder still missing. See `Pending`.

## Phase 2 — Utility: encryption and new formats (done)

1. Optional vendored crypto: **ChaCha20** (RFC 7539 vectors) + PBKDF2-HMAC-SHA256 as KDF.
   - `-p/--passphrase` derives the key; `-k` stays for raw keys.
   - Reference-vector and integrity tests (wrong key/passphrase -> rc 1).
2. Low-effort formats: PPM/PGM/PAM (uncompressed), TGA, uncompressed TIFF, ICO (reuses the PNG adapter).
   - New `src/ppm.c`, `src/tga.c`, `src/tiff.c`, `src/ico.c` following the `bmp.c` pattern, plus test cases.
3. `--capacity`: reports max embeddable bytes for a given image (and key), reusing `scatter`'s `capacity()`.

## Phase 3 — Defensive security (regression guard) (done)

- **Fuzzing** harness for the parsers (BMP/PNG/GIF/JPEG) + regression corpus (`tools/fuzz.c`, `tests/cases/14_fuzz.sh`).
- `07_robustness.sh` extended with findings.
- ASan/UBSan CI running `make test` (`make test-san`, `make fuzz-san`; optional external pipeline).
- Hardened known boundary checks (BMP `off+plen`, PNG `pos+clen`, GIF palette overflow).

## Phase 4 — UX and delivery (done)

- `--inspect` (format, dimensions, color type, capacity), `--verbose`, `--version`.
- Man page `sten(1)` and `make install`.
- Per-format capacity/perf benchmarks + comparison table in docs.

## Pending

Debt found while completing the phases:

- Deflate **dynamic-Huffman** encoder (deferred since Phase 1): only fixed + stored exist today. Would improve the ratio on large messages; needs `deflate_dynamic` plus output-size and reference-vector tests.
- External CI (GitHub Actions) running `make test`, `make test-san` and `make fuzz-san` on each push/PR, complementing the local targets.

## Backlog (out of current scope, tracked)

- Steganalysis: untouched zones, histogram preservation.
- DCT-based embedding (re-encode-surviving JPEG); shelved for now.