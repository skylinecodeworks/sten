# ROADMAP

Hoja de ruta de `sten`, esteganografía ScatterBit para imágenes (C puro, sin librerías externas).

Decisiones de alcance:

- Objetivo predominante: **capacidad y utilidad** (por delante de esteganálisis).
- Todas las dependencias son **vendored** (se implementan dentro del repo; no se enlaza nada externo).
- JPEG mantiene el **segmento COM** (no recomprime píxeles); se formaliza su alcance y se documentan sus límites.

## Estado actual (base)

- CLI C11: `encode`/`decode` para BMP, PNG, GIF y JPEG (vía segmento COM).
- Algoritmo ScatterBit: fingerprint de contenido + ruta PRNG derivada de clave, redundancia adaptativa 1-3, cabecera `SBT1` + longitud + CRC32, auto-extract probando redundancia 3->1.
- Inflate zlib propio completo; deflate solo con bloques *stored* (sin compresión).
- Suite de tests: 7 casos shell + unit (120 tests), fixtures generados, validación Pillow opcional.
- Deuda: sin commits, sin README, sin man page, sin CI.

## Fase 0 — Fundación mínima

- Commit inicial del repo y README (uso, capacidades por formato, alcance del JPEG/COM).
- `make test` en verde como puerta de entrada de cada fase.

## Fase 1 — Capacidad: compresión real

Objetivo principal. Hoy `deflate_zlib_stored` no comprime e infla los PNG de salida.

1. Encoder deflate con LZ77 + Huffman (fijo primero, dinámico después) para PNG; el inflate ya existe.
2. Compresión del mensaje antes de incrustar (mismo deflate), con flag en cabecera.
3. Ampliar cabecera `SBT1` a versión + flags; seguir leyendo mensajes legados v1 (retrocompatibilidad).
4. Tests: `tamano_salida_PNG <= tamano_entrada + X` y roundtrips con mensajes grandes.

## Fase 2 — Utilidad: cifrado y nuevos formatos

1. Cifrado opcional vendored: ChaCha20 (test vectors RFC 7539) + PBKDF2-HMAC-SHA256 como KDF.
   - Nueva opción `-p/--passphrase` (deriva la clave); `-k` queda para clave cruda.
   - Tests de vectores de referencia e integridad (clave/contraseña errónea -> rc 1).
2. Formatos de bajo esfuerzo: PPM/PGM/PAM (sin compresión), TGA, TIFF sin compresión, ICO (reusa el adaptador PNG).
   - Nuevos ficheros `src/ppm.c`, `src/tga.c`, `src/tiff.c`, `src/ico.c` siguiendo el patrón de `bmp.c` + cases de tests.
3. `--capacity`: reporta bytes máx. embebibles para una imagen (y clave) dada, reutilizando `capacity()` de scatter.

## Fase 3 — Seguridad defensiva (para no regresar)

- Harness de fuzzing para los parsers (BMP/PNG/GIF/JPEG) + corpus de regresión.
- Ampliar `07_robustness.sh` con los hallazgos.
- CI con ASan/UBSan sobre `make test`.
- Endurecer checks de límites conocidos (overflow `off+plen` BMP, `pos+clen` PNG, paleta GIF).

## Fase 4 — UX y entrega

- `--inspect` (formato, dimensiones, tipo de color, capacidad), `--verbose`, `--version`.
- Man page `sten(1)` y `make install`.
- Benchmarks de capacidad/rendimiento por formato + tabla comparativa en docs.

## Backlog (fuera del objetivo actual, registrado)

- Esteganálisis: zonas sin tocar, preservación de histograma.
- Incrustación basada en DCT (JPEG que sobrevive re-encode); descartada de momento.