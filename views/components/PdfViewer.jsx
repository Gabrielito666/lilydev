import Lex from "@lek-js/lex";

const PdfViewer = ({ section, scroll, pages, darkInput, onZoom, onDark }) =>
{
	return <section id="pdf-viewer" ref={section} hidden>
		<div className="toolbar">
			<button onClick={() => onZoom(-1)}>-</button>
			<button onClick={() => onZoom(0)}>fit</button>
			<button onClick={() => onZoom(1)}>+</button>
			<label id="dark" title="dark mode">
				<input type="checkbox" ref={darkInput} onChange={(e) => onDark(e.target.checked)} />
				<span></span>
			</label>
		</div>
		<div id="scroll" ref={scroll}>
			<div id="pages" ref={pages}></div>
		</div>
	</section>
}

export default PdfViewer;
