import Lex from "@lek-js/lex";

const FileSelectors = ({ section, pdfSelect, midiSelect, onPdf, onMidi }) =>
{
	return <section id="selectors" ref={section} hidden>
		<label>pdf
			<select ref={pdfSelect} onChange={(e) => onPdf(e.target.value)}></select>
		</label>
		<label>midi
			<select ref={midiSelect} onChange={(e) => onMidi(e.target.value)}></select>
		</label>
	</section>
}

export default FileSelectors;
