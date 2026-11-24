import Lex from "@lek-js/lex";
import PdfViewer from "./components/PdfViewer.jsx";
import PlayerBar from "./components/PlayerBar.jsx";
import FileSelectors from "./components/FileSelectors.jsx";
import createSynth from "./synth/index.js";

const refs = {
	viewer: { current: null },
	scroll: { current: null },
	pages: { current: null },
	player: { current: null },
	playBtn: { current: null },
	track: { current: null },
	fill: { current: null },
	timeLabel: { current: null },
	selectors: { current: null },
	pdfSelect: { current: null },
	midiSelect: { current: null },
	status: { current: null },
	busy: { current: null },
};

const state = {
	pdfs: [],
	midis: [],
	pdf: null,
	midi: null,
	zoom: 1,
	baseH: 1,
	synth: null,
	task: null,
	renderToken: 0,
	pdfjs: null,
};

function url(f)
{
	return "/" + f.name + "?h=" + f.hash;
}

function applyZoom()
{
	for (const c of refs.pages.current.children)
		c.style.width = Math.round(parseInt(c.dataset.w) * state.zoom) + "px";
}

function zoomBy(d)
{
	if (d == 0)
		state.zoom = (refs.scroll.current.clientHeight - 16) / state.baseH;
	else
		state.zoom = Math.min(6, Math.max(0.25, state.zoom + d * 0.25));
	applyZoom();
}

async function showPdf()
{
	const token = ++state.renderToken;

	if (state.task)
	{
		state.task.destroy();
		state.task = null;
	}

	if (!state.pdf)
	{
		refs.pages.current.replaceChildren();
		return;
	}

	const task = state.pdfjs.getDocument({ url: url(state.pdf) });
	state.task = task;

	let doc;
	try
	{
		doc = await task.promise;
	}
	catch
	{
		return;
	}

	if (token != state.renderToken)
	{
		task.destroy();
		return;
	}

	/* Double buffering: render every page into a detached fragment so
	   the old PDF stays on screen until the new one is fully ready,
	   then swap atomically (no blank flash) */
	const frag = document.createDocumentFragment();

	try
	{
		for (let n = 1; n <= doc.numPages; n++)
		{
			if (token != state.renderToken)
				return;

			const page = await doc.getPage(n);
			const vp = page.getViewport({ scale: 2 });
			const canvas = <canvas></canvas>;
			canvas.width = Math.floor(vp.width);
			canvas.height = Math.floor(vp.height);
			canvas.dataset.w = String(Math.floor(vp.width / 2));
			canvas.style.width = Math.round(parseInt(canvas.dataset.w) * state.zoom) + "px";
			if (n == 1) state.baseH = vp.height / 2;

			await page.render({ canvas, viewport: vp }).promise;
			frag.appendChild(canvas);
		}
	}
	catch
	{
		return;
	}

	if (token != state.renderToken)
		return;

	const prevTop = refs.scroll.current.scrollTop;
	const prevLeft = refs.scroll.current.scrollLeft;
	refs.pages.current.replaceChildren(frag);
	applyZoom();
	refs.scroll.current.scrollTop = prevTop;
	refs.scroll.current.scrollLeft = prevLeft;
}

function fillOptions(select, files, current)
{
	select.innerHTML = "";
	for (const f of files)
		select.appendChild(<option value={f.name}>{f.name}</option>);
	if (current)
		select.value = current.name;
}

function loadMidi()
{
	state.synth.pause();
	if (!state.midi) return;
	fetch(url(state.midi))
		.then((r) => r.arrayBuffer())
		.then((b) => { state.synth.load(b); paintPlayer(); });
}

function render()
{
	refs.viewer.current.hidden = state.pdfs.length == 0;
	showPdf();

	refs.player.current.hidden = state.midis.length == 0;
	loadMidi();

	refs.selectors.current.hidden = state.pdfs.length < 2 && state.midis.length < 2;
	fillOptions(refs.pdfSelect.current, state.pdfs, state.pdf);
	fillOptions(refs.midiSelect.current, state.midis, state.midi);
}

function pick(files, current)
{
	return files.find((f) => f.name == current?.name) ?? files[0] ?? null;
}

async function fetchFiles()
{
	return await (await fetch("/files/")).json();
}

async function refreshPdf()
{
	const data = await fetchFiles();
	state.pdfs = data.pdfs;
	state.pdf = pick(state.pdfs, state.pdf);

	refs.viewer.current.hidden = state.pdfs.length == 0;
	refs.selectors.current.hidden = state.pdfs.length < 2 && state.midis.length < 2;
	fillOptions(refs.pdfSelect.current, state.pdfs, state.pdf);
	showPdf();
}

async function refreshMidi()
{
	const data = await fetchFiles();
	state.midis = data.midis;
	state.midi = pick(state.midis, state.midi);

	refs.player.current.hidden = state.midis.length == 0;
	refs.selectors.current.hidden = state.pdfs.length < 2 && state.midis.length < 2;
	fillOptions(refs.midiSelect.current, state.midis, state.midi);
	loadMidi();
}

async function refresh()
{
	const data = await fetchFiles();
	state.pdfs = data.pdfs;
	state.midis = data.midis;
	state.pdf = pick(state.pdfs, state.pdf);
	state.midi = pick(state.midis, state.midi);
	render();
}

function toggle()
{
	if (!state.synth.duration) return;
	if (state.synth.playing) state.synth.pause();
	else state.synth.play();
	paintPlayer();
}

function seek(e)
{
	if (!state.synth.duration) return;
	state.synth.seek((e.offsetX / refs.track.current.clientWidth) * state.synth.duration);
	paintPlayer();
}

function paintPlayer()
{
	const s = state.synth;
	refs.playBtn.current.textContent = s.playing ? "\u275A\u275A" : "\u25B6";
	refs.fill.current.style.width = (s.duration ? s.time / s.duration : 0) * 100 + "%";
	refs.timeLabel.current.textContent = s.time.toFixed(1) + "s";
}

Lex.useClient(async () =>
{
	await import("pdfjs-dist/legacy/build/pdf.worker.min.mjs");
	state.pdfjs = await import("pdfjs-dist/legacy/build/pdf.mjs");

	state.synth = createSynth();
	refresh();

	setInterval(() =>
	{
		if (state.synth.playing && state.synth.time >= state.synth.duration)
			state.synth.pause();
		paintPlayer();
	}, 200);

	const ws = new WebSocket((location.protocol == "https:" ? "wss" : "ws") + "://" + location.host);
	ws.onmessage = (e) =>
	{
		const m = JSON.parse(e.data);
		if (m.event == "compiling")
		{
			refs.busy.current.style.visibility = "visible";
			return;
		}
		refs.busy.current.style.visibility = "hidden";
		if (m.event == "error")
		{
			refs.status.current.hidden = false;
			refs.status.current.textContent = m.message || "lilypond error";
			return;
		}
		refs.status.current.hidden = true;
		if (m.file && m.file.endsWith(".pdf"))
			refreshPdf();
		else if (m.file)
			refreshMidi();
		else
			refresh();
	};
});

const Page = () =>
{
	return <div id="app">
		<div id="busy" ref={refs.busy}>...typesetting...</div>

		<PlayerBar
			section={refs.player}
			playBtn={refs.playBtn}
			track={refs.track}
			fill={refs.fill}
			timeLabel={refs.timeLabel}
			onToggle={toggle}
			onSeek={seek}
			onVolume={(v) => state.synth.setVolume(v)} />

		<PdfViewer
			section={refs.viewer}
			scroll={refs.scroll}
			pages={refs.pages}
			onZoom={zoomBy} />

		<FileSelectors
			section={refs.selectors}
			pdfSelect={refs.pdfSelect}
			midiSelect={refs.midiSelect}
			onPdf={(v) => { state.pdf = state.pdfs.find((f) => f.name == v); showPdf(); }}
			onMidi={(v) => { state.midi = state.midis.find((f) => f.name == v); loadMidi(); paintPlayer(); }} />

		<footer id="status" ref={refs.status} hidden></footer>
	</div>
}

export default Page;
