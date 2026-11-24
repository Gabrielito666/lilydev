CC = gcc
CFLAGS = -Wall -Iinclude
SRC = main.c $(wildcard src/*.c)
LEX_SRC = $(wildcard views/*) scripts/build-html.js

all: bin/lilydev html/index.html html/synth.js

bin/lilydev: $(SRC)
	mkdir -p bin
	$(CC) $(CFLAGS) -o $@ $(SRC)

html/index.html: $(LEX_SRC)
	pnpm run build-html

html/synth.js: node_modules/spessasynth_lib/dist/spessasynth_processor.min.js
	mkdir -p html
	cp $< $@

html: html/index.html html/synth.js

dev: bin/lilydev html/index.html html/synth.js
	LILYDEV_HTML=./html/index.html LILYDEV_SOUNDFONT=./sounds/GeneralUser-GS.sf2 ./bin/lilydev ./fixture.ly

deb: bin/lilydev html/index.html html/synth.js
	rm -f debian/files
	dpkg-buildpackage -b -nc -uc -us
	mkdir -p dist
	-mv ../lilydev_* ../lilydev-* dist/ 2>/dev/null || true

clean:
	rm -rf bin html dist

.PHONY: all dev clean html deb
