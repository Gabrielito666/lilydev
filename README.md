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
make deb
```

The package lands next to the repo: `../lilydev_0.1.3_amd64.deb`.

## Install & use (Debian)

```sh
sudo apt install ../lilydev_0.1.3_amd64.deb
cd your-project
lilydev piece.ly
```

Open `http://localhost:8080`. LilyDev watches the current directory
recursively; saving any `.ly` or `.ily` triggers a recompile and a live
reload.
Use `-p`/`--port` to change the port.

## Next steps

- Improve the MIDI engine (better sound than the current WebAudio square-wave synth)
- Improve response times (lilypond cold-start dominates the reload loop)
