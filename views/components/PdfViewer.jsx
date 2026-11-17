import Lex from "@lek-js/lex";

const PdfViewer = ({ section, scroll, pages, onZoom }) =>
{
	return <section id="pdf-viewer" ref={section} hidden>
		<div className="toolbar">
			<button onClick={() => onZoom(-1)}>-</button>
			<button onClick={() => onZoom(0)}>fit</button>
			<button onClick={() => onZoom(1)}>+</button>
		</div>
		<div id="scroll" ref={scroll}>
			<div id="pages" ref={pages}></div>
		</div>
	</section>
}

export default PdfViewer;
