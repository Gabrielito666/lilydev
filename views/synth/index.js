export default function createSynth()
{
	let notes = [];
	let duration = 0;
	let offset = 0;
	let playing = false;
	let ctx = null;
	let master = null;
	let volume = 1;
	let startedAt = 0;
	let nodes = [];

	function parse(buffer)
	{
		notes = [];
		const dv = new DataView(buffer);
		let p = 0;

		const u8 = () => dv.getUint8(p++);
		const be16 = () => { const v = dv.getUint16(p); p += 2; return v; };
		const be32 = () => { const v = dv.getUint32(p); p += 4; return v; };
		const str = (n) => { let s = ""; for (let i = 0; i < n; i++) s += String.fromCharCode(u8()); return s; };
		const varlen = () => { let v = 0, c; do { c = u8(); v = (v << 7) | (c & 0x7f); } while (c & 0x80); return v; };

		if (str(4) != "MThd") return;
		be32();
		be16();
		const ntrks = be16();
		let division = be16();
		if (division & 0x8000) division = 0x10000;

		let tempo = 500000;

		for (let t = 0; t < ntrks; t++)
		{
			if (str(4) != "MTrk") break;
			const len = be32();
			const end = p + len;
			let tick = 0, running = 0;

			while (p < end)
			{
				tick += varlen();
				let status = dv.getUint8(p);
				if (status & 0x80) { p++; running = status; }
				else status = running;

				const type = status & 0xf0;
				if (type == 0x90)
				{
					const key = u8(), vel = u8();
					if (vel > 0)
						notes.push({ tick: tick, key: key, vel: vel, ch: status & 0x0f });
				}
				else if (type == 0x80 || type == 0xa0 || type == 0xb0 || type == 0xe0) { u8(); u8(); }
				else if (type == 0xc0 || type == 0xd0) u8();
				else if (status == 0xff)
				{
					const meta = u8();
					const len = varlen();
					if (meta == 0x51 && len == 3)
						tempo = (dv.getUint8(p) << 16) | (dv.getUint8(p + 1) << 8) | dv.getUint8(p + 2);
					p += len;
				}
				else if (status == 0xf0 || status == 0xf7) p += varlen();
			}
			p = end;
		}

		notes.sort((a, b) => a.tick - b.tick);

		const last = {};
		for (const n of notes)
		{
			const prev = last[n.ch];
			if (prev && prev.dur == null)
				prev.dur = (n.tick - prev.tick) / division * tempo / 1e6;
			last[n.ch] = n;
		}
		for (const n of notes)
		{
			if (n.dur == null) n.dur = 0.5;
			n.dur = Math.min(n.dur, 4);
			n.time = n.tick / division * tempo / 1e6;
		}

		duration = notes.reduce((m, n) => Math.max(m, n.time + n.dur), 0);
	}

	function stopAll()
	{
		for (const n of nodes)
			try { n.stop(); n.disconnect(); } catch (e) {}
		nodes = [];
	}

	function startAt(when)
	{
		offset = when;
		stopAll();
		startedAt = ctx.currentTime;

		for (const n of notes)
		{
			if (n.time + n.dur <= when) continue;
			const t0 = ctx.currentTime + Math.max(0, n.time - when);
			const t1 = t0 + Math.max(0.05, n.time + n.dur - Math.max(n.time, when));

			const osc = ctx.createOscillator();
			const gain = ctx.createGain();
			osc.type = "square";
			osc.frequency.value = 440 * Math.pow(2, (n.key - 69) / 12);
			gain.gain.setValueAtTime((n.vel / 127) * 0.25, t0);
			gain.gain.exponentialRampToValueAtTime(0.001, t1);
			osc.connect(gain).connect(master);
			osc.start(t0);
			osc.stop(t1 + 0.02);
			nodes.push(osc);
		}
	}

	return {
		load(buffer)
		{
			this.pause();
			parse(buffer);
			offset = 0;
		},
		play()
		{
			if (!duration || playing) return;
			if (!ctx)
			{
				ctx = new AudioContext();
				master = ctx.createGain();
				master.gain.value = volume;
				const comp = ctx.createDynamicsCompressor();
				comp.threshold.value = -20;
				comp.ratio.value = 8;
				master.connect(comp).connect(ctx.destination);
			}
			ctx.resume();
			playing = true;
			if (offset >= duration) offset = 0;
			startAt(offset);
		},
		pause()
		{
			if (!playing) return;
			offset = this.time;
			playing = false;
			stopAll();
		},
		seek(t)
		{
			offset = Math.max(0, Math.min(duration, t));
			if (playing) startAt(offset);
		},
		setVolume(v)
		{
			volume = Math.max(0, Math.min(1.5, v));
			if (master) master.gain.value = volume;
		},
		get time()
		{
			return playing ? Math.min(duration, offset + ctx.currentTime - startedAt) : offset;
		},
		get duration() { return duration; },
		get playing() { return playing; }
	};
}
