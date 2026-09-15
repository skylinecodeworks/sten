#!/bin/sh
# Build a .deb with dpkg-deb (no debhelper needed).
# Usage: build-deb.sh <version>
set -eu

VERSION="$1"
OUT="pkg/sten_${VERSION}_amd64.deb"
ROOT="pkg/deb/sten_${VERSION}_amd64"

rm -rf "$ROOT" "$OUT"
mkdir -p "$ROOT/DEBIAN" "$ROOT/usr/bin" "$ROOT/usr/share/man/man1"

make clean >/dev/null
make sten >/dev/null

install -m 0755 sten "$ROOT/usr/bin/sten"
gzip -9c docs/sten.1 > "$ROOT/usr/share/man/man1/sten.1.gz"

sed "s/@VERSION@/$VERSION/g" packaging/deb/control > "$ROOT/DEBIAN/control"

dpkg-deb --build --root-owner-group "$ROOT" "$OUT" >/dev/null

# Smoke test after an install simulation.
dpkg-deb --info "$OUT" >/dev/null
echo "ok: $OUT"
