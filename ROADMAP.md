# ROADMAP

Hoja de ruta de `sten`, esteganografía ScatterBit para imágenes (C puro, sin librerías externas).

Decisiones de alcance:

- Objetivo predominante: **capacidad y utilidad** (por delante de esteganálisis).
- Todas las dependencias son **vendored** (se implementan dentro del repo; no se enlaza nada externo).
- JPEG mantiene el **segmento COM** (no recomprime píxeles); se formaliza su alcance y se documentan sus límites.

## Estado actual

- CLI C11: `encode`/`decode`/`capacity`/`inspect`, opciones `-k`/`-p`/`-v`/`-V`/`-h`.
- Formatos: BMP, PNG, GIF, JPEG (vía segmento COM), PNM/PAM, TGA (sin compresión), TIFF, ICO (reusa el adaptador PNG).
- Algoritmo ScatterBit con payload `SBT2`: fingerprint de contenido + ruta PRNG derivada de clave (o de la clave derivada por PBKDF2 con `-p`), redundancia adaptativa 1-3, compresión zlib (LZ77 + Huffman fija) y cifrado ChaCha20 opcionales vía flags, CRC32 de verificación y auto-extract probando redundancia 3->1. Los mensajes legados `SBT1` siguen leyéndose.
- Suite de tests: 15 cases shell + unit C (101 tests OK), fixtures generados, validación Pillow opcional, fuzzer de mutación `tools/fuzz` y puertas `make fuzz-san`/`make test-san` con ASan/UBSan.
- Docs: README, man page `docs/sten.1`, `make install` (PREFIX/DESTDIR) y benchmarks `make bench`.
- Git: fases 0-4 commitadas y mergeadas a `main`; historial limpio.

## Fase 0 — Fundación mínima ✅ completada

- Commit inicial del repo y README (uso, capacidades por formato, alcance del JPEG/COM). Commit `25d1e23`.
- `make test` en verde como puerta de entrada de cada fase.

## Fase 1 — Capacidad: **compresión real** ✅ completada

Objetivo principal de la fase: hoy `deflate_zlib_stored` no comprime e infla los PNG de salida.

1. Encoder deflate con LZ77 + Huffman fija para PNG; el inflate ya existía (stored, fijo y dinámico).
2. Compresión del mensaje antes de incrustar (mismo deflate), con flag en cabecera.
3. Cabecera `SBT1` ampliada a versión + flags; siguen leyéndose mensajes legados v1 (retrocompatibilidad).
4. Tests: `tamano_salida_PNG <= tamano_entrada + X` y roundtrips con mensajes grandes.

Pendiente heredado: el encoder **Huffman dinámico** (queda solo la fija). Ver `## Pendiente`.

## Fase 2 — Utilidad: cifrado y nuevos formatos ✅ completada

1. Cifrado opcional vendored: **ChaCha20** (test vectors RFC 7539) + PBKDF2-HMAC-SHA256 como KDF.
   - Opción `-p/--passphrase` (deriva la clave); `-k` queda para clave cruda.
   - Tests de vectores de referencia e integridad (clave/contraseña errónea -> rc 1).
2. Formatos de bajo esfuerzo: PPM/PGM/PAM (sin compresión), TGA, TIFF sin compresión, ICO (reusa el adaptador PNG).
   - Nuevos ficheros `src/ppm.c`, `src/tga.c`, `src/tiff.c`, `src/ico.c` siguiendo el patrón de `bmp.c` + cases de tests.
3. `--capacity`: reporta bytes máx. embebibles para una imagen (y clave) dada, reutilizando `capacity()` de scatter.

## Fase 3 — Seguridad defensiva (para no regresar) ✅ completada

- Harness de **fuzzing** para los parsers (BMP/PNG/GIF/JPEG) + corpus de regresión (`tools/fuzz.c`, `tests/cases/14_fuzz.sh`).
- Ampliar `07_robustness.sh` con los hallazgos.
- CI con ASan/UBSan sobre `make test` (`make test-san`, `make fuzz-san`; pipeline externo opcional).
- Endurecer checks de límites conocidos (overflow `off+plen` BMP, `pos+clen` PNG, paleta GIF).

## Fase 4 — UX y entrega ✅ completada

- `--inspect` (formato, dimensiones, tipo de color, capacidad), `--verbose`, `--version`.
- Man page `sten(1)` y `make install`.
- Benchmarks de capacidad/rendimiento por formato + tabla comparativa en docs.

## Pendiente

Cierre de deudas detectadas al completar las fases:

- Encoder deflate **Huffman dinámico** (prometido en Fase 1 "después"): solo existe fixed+stored. Mejoraría el ratio en mensajes grandes; requiere su `deflate_dynamic` + tests de tamaño de salida y vectores de referencia.
- CI externo (GitHub Actions) que ejecute `make test`, `make test-san` y `make fuzz-san` en cada push/PR como complemento de los targets locales.

## Backlog (fuera del objetivo actual, registrado)

- Esteganálisis: zonas sin tocar, preservación de histograma.
- Incrustación basada en DCT (JPEG que sobrevive re-encode); descartada de momento.