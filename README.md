# LilyDev

Writing sheet music should feel like writing a web app with hot reload:
edit a plain-text `.ly` file in vim (or any editor), save, and the score
redraws itself in the browser — no export steps, no "render and check"
cycle. The PDF viewer and the MIDI playback update live over a WebSocket.

## Requirements

- Runtime: `lilypond`, `ghostscript`
- Build: `gcc`, `make`, `debhelper` (v13), `node` + `pnpm` (to bundle the web UI)

## Build the .deb

```sh
git clone https://github.com/Gabrielito666/lilydev.git
cd lilydev
pnpm install
curl -L -o sounds/GeneralUser-GS.sf2 \
	https://raw.githubusercontent.com/mrbumpy409/GeneralUser-GS/main/GeneralUser-GS.sf2
make deb
```

The package lands next to the repo: `../lilydev_0.1.4_amd64.deb`.
It includes the GeneralUser GS soundfont, so the .deb is ~32 MB.

## Install & use (Debian)

```sh
sudo apt install ../lilydev_0.1.4_amd64.deb
cd your-project
lilydev piece.ly
```

Open `http://localhost:8080`. LilyDev watches the current directory
recursively; saving any `.ly` or `.ily` triggers a recompile and a live
reload.
Use `-p`/`--port` to change the port.

## Soundfont playback

The web player renders MIDI through
[SpessaSynth](https://github.com/spessasus/spessasynth_lib)
(Apache-2.0) using [GeneralUser GS](https://www.schristiancollins.com/generaluser)
by S. Christian Collins (redistributable unmodified). The server exposes
the soundfont as `/soundfont.sf2` and the AudioWorklet processor as
`/synth.js`.

Path resolution (first hit wins):

1. `$LILYDEV_SOUNDFONT` — e.g. `make dev` sets it to `./sounds/GeneralUser-GS.sf2`
2. `/usr/share/lilydev/sounds/GeneralUser-GS.sf2` (installed by the .deb)

`sounds/` is git-ignored; if no soundfont is found, the player falls back
to a simple built-in synth. `make dev` runs the server against `./html`
and `./sounds` directly, no installation needed.

## Next steps

- Improve response times (lilypond cold-start dominates the reload loop)
