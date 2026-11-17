import Lex from "@lek-js/lex";

const PlayerBar = ({ section, playBtn, track, fill, timeLabel, onToggle, onSeek, onVolume }) =>
{
	return <section id="player" ref={section} hidden>
		<button ref={playBtn} onClick={() => onToggle()}>&#9654;</button>
		<div id="track" ref={track} onClick={(e) => onSeek(e)}>
			<div id="fill" ref={fill}></div>
		</div>
		<span id="time" ref={timeLabel}>0.0s</span>
		<label id="vol">
			<span>vol</span>
			<input type="range" min="0" max="150" value="100"
				onInput={(e) => onVolume(e.target.value / 100)}
				onChange={(e) => onVolume(e.target.value / 100)} />
		</label>
	</section>
}

export default PlayerBar;
