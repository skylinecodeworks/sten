CC      ?= gcc
CFLAGS  ?= -O2
CFLAGS  += -std=c11 -Wall -Wextra

SRC     = src/main.c src/scatter.c src/util.c src/deflate.c src/crypto.c \
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

$(UNIT): tests/unit.c src/util.o src/deflate.o src/scatter.o src/crypto.o
	@mkdir -p tests/build
	$(CC) $(CFLAGS) -Isrc -o $@ tests/unit.c src/util.o src/deflate.o src/scatter.o src/crypto.o

test: $(BIN) tools/gen $(UNIT)
	sh tests/run.sh

clean:
	rm -f $(OBJ) $(BIN) tools/gen $(UNIT)
	rm -rf tests/build

.PHONY: all test clean
