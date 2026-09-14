# Documentation expectations for phase 0.

assert_file_exists() {
    if [ -f "$1" ]; then
        pass "$2"
    else
        fail "$2 (missing: $1)"
    fi
}

assert_file_contains() {
    if grep -qF "$3" "$2" 2>/dev/null; then
        pass "$1"
    else
        fail "$1 (missing in $2: [$3])"
    fi
}

ROOT=$(cd "$(dirname "$0")/.." && pwd)

assert_file_exists "$ROOT/README.md" "README exists"
assert_file_exists "$ROOT/ROADMAP.md" "ROADMAP exists"
assert_file_exists "$ROOT/Makefile" "Makefile exists"
assert_file_exists "$ROOT/docs/sten.1" "man page exists"

assert_file_contains "README documents usage" "$ROOT/README.md" "sten encode -i image -o output"
assert_file_contains "README documents options" "$ROOT/README.md" "optional key (derives the bit path)"
assert_file_contains "README documents exit codes" "$ROOT/README.md" "no message found (clean image or wrong key)"
assert_file_contains "README documents JPEG scope" "$ROOT/README.md" "does not touch pixels"
assert_file_contains "README documents formats" "$ROOT/README.md" "GIF"
assert_file_contains "README documents install" "$ROOT/README.md" "make install"
assert_file_contains "man page documents inspect" "$ROOT/docs/sten.1" "inspect"

assert_file_contains "ROADMAP defines phase 1 compression" "$ROOT/ROADMAP.md" "compresión real"
assert_file_contains "ROADMAP defines phase 2 crypto" "$ROOT/ROADMAP.md" "ChaCha20"
assert_file_contains "ROADMAP defines phase 3 fuzzing" "$ROOT/ROADMAP.md" "fuzzing"
assert_file_contains "ROADMAP defines phase 4 UX" "$ROOT/ROADMAP.md" "inspect"