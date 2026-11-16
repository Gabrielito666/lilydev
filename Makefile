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

clean:
	rm -rf bin html

.PHONY: all dev clean html
