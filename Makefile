CC      ?= gcc
CFLAGS  ?= -O2
CFLAGS  += -std=c11 -Wall -Wextra

SRC     = src/main.c src/scatter.c src/util.c src/deflate.c src/crypto.c src/rand.c \
          src/bmp.c src/png.c src/gif.c src/jpeg.c src/format.c \
          src/ppm.c src/tga.c src/tiff.c src/ico.c
OBJ     = $(SRC:.c=.o)
BIN     = sten
UNIT    = tests/build/unit

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ)

%.o: %.c
	$(CC) $(CFLAGS) -Isrc -c -o $@ $<

tools/gen: tools/gen_test_images.c src/util.o src/deflate.o
	$(CC) $(CFLAGS) -Isrc -o $@ tools/gen_test_images.c src/util.o src/deflate.o

$(UNIT): tests/unit.c src/util.o src/deflate.o src/scatter.o src/crypto.o src/rand.o
	@mkdir -p tests/build
	$(CC) $(CFLAGS) -Isrc -o $@ tests/unit.c src/util.o src/deflate.o src/scatter.o src/crypto.o src/rand.o

FUZZOBJ = src/util.o src/deflate.o src/crypto.o src/format.o \
          src/bmp.o src/png.o src/gif.o src/jpeg.o src/ppm.o \
          src/tga.o src/tiff.o src/ico.o src/scatter.o src/rand.o

tools/fuzz: tools/fuzz.c $(FUZZOBJ)
	$(CC) $(CFLAGS) -Isrc -o $@ tools/fuzz.c $(FUZZOBJ)

SANFLAGS = -fsanitize=address,undefined -fno-sanitize-recover=all -O1 -g

# Full suite (CLI + gen + unit + fuzz harness) under ASan/UBSan.
test-san: clean
	$(MAKE) CFLAGS="$(CFLAGS) $(SANFLAGS)" test

# Standalone sanitized fuzzer over the generated fixtures.
fuzz-san: clean
	$(MAKE) CFLAGS="$(CFLAGS) $(SANFLAGS)" tools/gen tools/fuzz
	mkdir -p tests/build
	tools/gen tests/build
	./tools/fuzz 5000 tests/build/*.bmp tests/build/*.png tests/build/*.gif \
	    tests/build/*.jpg tests/build/*.ppm tests/build/*.tga \
	    tests/build/*.tiff tests/build/*.ico

fuzz: tools/fuzz

bench: $(BIN) tools/gen
	sh tools/bench.sh

test: $(BIN) tools/gen $(UNIT) tools/fuzz
	sh tests/run.sh

PREFIX  ?= /usr/local
MANDIR  ?= $(PREFIX)/share/man

install: $(BIN)
	install -d $(DESTDIR)$(PREFIX)/bin $(DESTDIR)$(MANDIR)/man1
	install -m 0755 $(BIN) $(DESTDIR)$(PREFIX)/bin/$(BIN)
	install -m 0644 docs/sten.1 $(DESTDIR)$(MANDIR)/man1/sten.1

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/$(BIN) $(DESTDIR)$(MANDIR)/man1/sten.1

clean:
	rm -f $(OBJ) $(BIN) tools/gen $(UNIT) tools/fuzz
	rm -rf tests/build

.PHONY: all test clean fuzz bench fuzz-san test-san install uninstall
