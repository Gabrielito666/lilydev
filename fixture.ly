\version "2.24"

fluteMusic = {
	\tempo 4 = 96
	e''4\mf g'' a'' g'' |
	e'' a'' g'' e'' |
	f'' a'' c''' a'' |
	b'' g'' d''2 |
	e''4 g'' c''' b'' |
	a'' f'' c''2 |
	d''' b'' g'' b'' |
	c'''1 \bar "|."
}

pianoRH = {
	<c'' e'' g''>1\p |
	<c'' e'' a''> |
	<a' c'' f''> |
	<b' d'' g''> |
	<c'' e'' g''> |
	<a' c'' f''> |
	<b' d'' g''> |
	<c'' e'' g''> |
}

pianoLH = {
	<c, c'>2 <g g'> |
	<a a'>2 <e e'> |
	<f f'>2 <c c'> |
	<g g'>2 <d d'> |
	<c c'>2 <g g'> |
	<f f'>2 <c c'> |
	<g g'>2 <d d'> |
	<c, c'>1 |
}

\score {
	<<
		\new Staff \with { instrumentName = "Flute" midiInstrument = "flute" } { \fluteMusic }
		\new PianoStaff \with { instrumentName = "Piano" } <<
			\new Staff {
				\set Staff.midiInstrument = "acoustic grand"
				\pianoRH
			}
			\new Staff {
				\set Staff.midiInstrument = "acoustic grand"
				\clef bass
				\pianoLH
			}
		>>
	>>
	\layout {}
	\midi {}
}
