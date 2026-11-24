import { Sequencer, WorkletSynthesizer } from "spessasynth_lib";
import { BasicMIDI } from "spessasynth_core";
import createFallback from "./fallback.js";

const SOUNDFONT_URL = "/soundfont.sf2";
const WORKLET_URL = "/synth.js";

/* MIDI player backed by SpessaSynth + GeneralUser GS.
   Same public API as the fallback square-wave synth (load/play/pause/seek/
   setVolume/time/duration/playing) so the UI does not care which one runs. */
export default function createSynth(onStatus)
{
	let buffer = null;     /* last MIDI ArrayBuffer loaded */
	let duration = 0;
	let offset = 0;        /* playhead when paused / while the engine loads */
	let playing = false;
	let volume = 1;

	let spessa = null;     /* { ctx, synth, seq } once initialized */
	let songLoaded = false;
	let starting = null;   /* in-flight init promise */
	let fallback = null;   /* used when the soundfont is not served */

	function status(msg)
	{
		if (onStatus) onStatus(msg);
	}

	async function init()
	{
		const res = await fetch(SOUNDFONT_URL);
		if (!res.ok)
			throw new Error("soundfont not available");
		const sf = await res.arrayBuffer();

		const ctx = new AudioContext();
		await ctx.audioWorklet.addModule(WORKLET_URL);

		const synth = new WorkletSynthesizer(ctx);
		synth.connect(ctx.destination);
		synth.setSystemParameter("gain", volume);
		await synth.soundBankManager.addSoundBank(sf, "GeneralUser GS");
		await synth.isReady;

		const seq = new Sequencer(synth);
		seq.loopCount = 0;
		seq.eventHandler.addEvent("songEnded", "lilydev", () =>
		{
			playing = false;
			offset = duration;
		});

		spessa = { ctx, synth, seq };
		songLoaded = false;
	}

	function loadSong()
	{
		spessa.seq.loadNewSongList([{ binary: buffer, fileName: "piece.mid" }]);
		songLoaded = true;
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

			if (spessa)
			{
				if (buffer)
					loadSong();
				else
				{
					spessa.seq.loadNewSongList([]);
					songLoaded = false;
				}
			}
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
			if (spessa)
			{
				if (!songLoaded) loadSong();
				spessa.ctx.resume();
				spessa.seq.currentTime = offset;
				spessa.seq.play();
				return;
			}
			if (starting) return; /* the pending init will start playback */

			status("loading sound font...");
			starting = init()
				.then(() =>
				{
					status(null);
					loadSong();
					if (!playing) return; /* user paused while loading */
					spessa.ctx.resume();
					spessa.seq.currentTime = offset;
					spessa.seq.play();
				})
				.catch((e) =>
				{
					console.warn("SpessaSynth unavailable, using basic synth:", e);
					status("soundfont not found, using basic synth");
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
			if (spessa)
			{
				if (!spessa.seq.paused)
					spessa.seq.pause();
				offset = spessa.seq.currentTime;
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
			if (spessa && playing)
				spessa.seq.currentTime = t;
			else if (fallback)
				fallback.seek(t);
		},
		setVolume(v)
		{
			volume = Math.max(0, Math.min(1.5, v));
			if (spessa)
				spessa.synth.setSystemParameter("gain", volume);
			else if (fallback)
				fallback.setVolume(volume);
		},
		get time()
		{
			if (fallback) return fallback.time;
			if (spessa && playing && !spessa.seq.paused)
				return Math.min(duration, spessa.seq.currentTime);
			return offset;
		},
		get duration()
		{
			return duration;
		},
		get playing()
		{
			if (fallback) return fallback.playing;
			return playing && !!(spessa && !spessa.seq.paused);
		}
	};
}
