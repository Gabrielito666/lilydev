CC = gcc
CFLAGS = -Wall -Iinclude
SRC = main.c $(wildcard src/*.c)
LEX_SRC = $(wildcard views/*) scripts/build-html.js

all: bin/lilydev html/index.html

bin/lilydev: $(SRC)
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $(SRC)

html/index.html: $(LEX_SRC)
	pnpm run build-html

html:
	pnpm run build-html

dev: bin/lilydev html/index.html
	LILYDEV_HTML=./html/index.html ./bin/lilydev ./fixture.ly

deb: bin/lilydev html/index.html
	rm -f debian/files
	dpkg-buildpackage -b -nc -uc -us
	mkdir -p dist
	-mv ../lilydev_* ../lilydev-* dist/ 2>/dev/null || true

clean:
	rm -rf bin html dist

.PHONY: all dev clean html deb
