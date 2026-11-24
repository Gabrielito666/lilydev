import { Sequencer, WorkletSynthesizer } from "spessasynth_lib";
import { BasicMIDI } from "spessasynth_core";
import createLegacyEngine from "./legacy.js";
import createFallback from "./fallback.js";

const SOUNDFONT_URL = "/soundfont.sf2";
const WORKLET_URL = "/synth.js";

/* MIDI player backed by SpessaSynth + GeneralUser GS.
   Engine chain, first one that works wins:
     1. AudioWorklet (off main thread; Chromium hides audioWorklet in
        insecure contexts, e.g. http://<lan-ip> from another device)
     2. main-thread spessasynth_core via ScriptProcessorNode (legacy.js)
     3. square-wave synth (fallback.js) when no soundfont is served
   Same public API everywhere: load/play/pause/seek/setVolume/time/duration/playing. */
export default function createSynth(onStatus)
{
	let buffer = null;     /* last MIDI ArrayBuffer loaded */
	let duration = 0;
	let offset = 0;        /* playhead when paused / while the engine loads */
	let playing = false;
	let volume = 1;

	let engine = null;     /* { load, play, pause, seek, time, paused, setVolume } */
	let starting = null;   /* in-flight init promise */
	let fallback = null;   /* square-wave, only if the soundfont is missing */

	function status(msg)
	{
		if (onStatus) onStatus(msg);
	}

	function onEnd()
	{
		playing = false;
		offset = duration;
	}

	async function init()
	{
		const res = await fetch(SOUNDFONT_URL);
		if (!res.ok)
			throw Object.assign(new Error("soundfont not served: HTTP " + res.status),
				{ soundfont: true });
		const sf = await res.arrayBuffer();

		const AudioCtx = window.AudioContext || window.webkitAudioContext;
		const ctx = new AudioCtx();

		/* 1. AudioWorklet path */
		if (ctx.audioWorklet)
		{
			try
			{
				await ctx.audioWorklet.addModule(WORKLET_URL);

				const synth = new WorkletSynthesizer(ctx);
				synth.connect(ctx.destination);
				await synth.soundBankManager.addSoundBank(sf, "GeneralUser GS");
				await synth.isReady;
				synth.setSystemParameter("gain", volume);

				const seq = new Sequencer(synth);
				seq.loopCount = 0;
				seq.eventHandler.addEvent("songEnded", "lilydev", onEnd);

				engine = {
					load(b) { seq.loadNewSongList(b ? [{ binary: b, fileName: "piece.mid" }] : []); },
					play(t) { ctx.resume(); seq.currentTime = t; seq.play(); },
					pause() { if (!seq.paused) seq.pause(); },
					seek(t) { seq.currentTime = t; },
					time() { return seq.currentTime; },
					paused() { return seq.paused; },
					setVolume(v) { synth.setSystemParameter("gain", v); }
				};
				return;
			}
			catch (e)
			{
				console.warn("audioWorklet engine failed, trying the main-thread engine:", e);
			}
		}
		else
		{
			/* Chromium only exposes audioWorklet in secure contexts:
			   accessing lilydev over LAN via plain http lands here */
			console.warn("no AudioWorklet (insecure context?), using the main-thread engine");
		}

		/* 2. main-thread engine */
		engine = await createLegacyEngine(ctx, sf, onEnd);
		engine.setVolume(volume);
	}

	function startPlay()
	{
		engine.play(offset >= duration ? 0 : offset);
	}

	function useFallback()
	{
		if (!fallback)
			fallback = createFallback();
		fallback.setVolume(volume);
		if (buffer)
			fallback.load(buffer);
	}

	return {
		load(newBuffer)
		{
			this.pause();
			buffer = newBuffer.byteLength ? newBuffer : null;
			duration = 0;
			offset = 0;
			if (buffer)
			{
				try { duration = BasicMIDI.fromArrayBuffer(buffer).duration; }
				catch (e) { console.warn("MIDI parse error:", e); }
			}
			if (engine)
				engine.load(buffer);
			else if (fallback && buffer)
				fallback.load(buffer);
		},
		play()
		{
			if (!duration || playing) return;
			playing = true;

			if (fallback)
			{
				fallback.play();
				return;
			}
			if (engine)
			{
				startPlay();
				return;
			}
			if (starting) return; /* the pending init will start playback */

			status("loading sound font...");
			starting = init()
				.then(() =>
				{
					status(null);
					if (buffer)
						engine.load(buffer);
					if (playing)
						startPlay();
				})
				.catch((e) =>
				{
					console.error("soundfont synth unavailable:", e);
					status(e.soundfont ? "soundfont not found, using basic synth"
						: "soundfont engine failed, using basic synth");
					useFallback();
					if (playing)
						fallback.play();
				})
				.finally(() => { starting = null; });
		},
		pause()
		{
			if (!playing) return;
			playing = false;
			if (engine)
			{
				engine.pause();
				offset = engine.time();
			}
			else if (fallback)
			{
				fallback.pause();
				offset = fallback.time;
			}
		},
		seek(t)
		{
			t = Math.max(0, Math.min(duration, t));
			offset = t;
			if (engine && playing)
				engine.seek(t);
			else if (fallback)
				fallback.seek(t);
		},
		setVolume(v)
		{
			volume = Math.max(0, Math.min(1.5, v));
			if (engine)
				engine.setVolume(volume);
			else if (fallback)
				fallback.setVolume(volume);
		},
		get time()
		{
			if (fallback) return fallback.time;
			if (engine && playing && !engine.paused())
				return Math.min(duration, engine.time());
			return offset;
		},
		get duration()
		{
			return duration;
		},
		get playing()
		{
			if (fallback) return fallback.playing;
			return playing && !!engine && !engine.paused();
		}
	};
}
