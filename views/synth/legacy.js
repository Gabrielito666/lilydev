import { BasicMIDI, SoundBankLoader, SpessaSynthProcessor, SpessaSynthSequencer } from "spessasynth_core";

const QUANTUM = 128;

/* Soundfont engine that does not need AudioWorklet (Chromium only exposes
   audioWorklet in secure contexts, so plain http://<lan-ip> has none).
   Drives spessasynth_core's raw processor from a ScriptProcessorNode on the
   main thread. Returns the same engine adapter the worklet path provides. */
export default async function createLegacyEngine(ctx, sfBuffer, onEnd)
{
	const processor = new SpessaSynthProcessor(ctx.sampleRate);
	const bank = SoundBankLoader.fromArrayBuffer(sfBuffer);
	processor.soundBankManager.addSoundBank(bank, "GeneralUser GS");
	await processor.ready;

	const seq = new SpessaSynthSequencer(processor);
	seq.loopCount = 0;
	seq.onEventCall = (e) =>
	{
		if (e.type == "songEnded" && onEnd)
			onEnd();
	};

	const node = ctx.createScriptProcessor(4096, 0, 2);
	node.onaudioprocess = (e) =>
	{
		const out = e.outputBuffer;
		const l = out.getChannelData(0);
		const r = out.getChannelData(1);
		l.fill(0);
		r.fill(0);
		/* process() renders one 128-sample quantum at a time and adds to
		   the buffers, so tick + render the whole block in quanta */
		for (let off = 0; off < out.length; off += QUANTUM)
		{
			seq.processTick();
			processor.process(l, r, off, Math.min(QUANTUM, out.length - off));
		}
	};
	/* must reach destination for the callback to fire */
	node.connect(ctx.destination);

	return {
		load(b)
		{
			seq.loadNewSongList(b ? [BasicMIDI.fromArrayBuffer(b)] : []);
		},
		play(t)
		{
			ctx.resume();
			seq.currentTime = t;
			seq.play();
		},
		pause()
		{
			if (!seq.paused)
				seq.pause();
		},
		seek(t)
		{
			seq.currentTime = t;
		},
		time()
		{
			return seq.currentTime;
		},
		paused()
		{
			return seq.paused;
		},
		setVolume(v)
		{
			processor.setSystemParameter("gain", v);
		}
	};
}
