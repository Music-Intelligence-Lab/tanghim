PROJECT IDEA

# Arabic Maqām Tuner
Simple maqam tuning VST plugin for all OSs and DAWs that uses DiArMaqAr API for the tuning data (https://diarmaqar.netlify.app/docs/api/).

Interface is:
A selector for tuning system
12 buttons that are maqām presets (which can be set by the user depending on maqamat and their transpositions available in tuning system)
12 sliders, one for each chromatic key on a midi keyboard from C to B
Each slider represents the variations available for each note based on our 12-pitch class sets (https://diarmaqar.netlify.app/docs/api/playground#classifyMaqamat12PitchClassSets) where by the slider can change the tuning of a note snapped to the variations of that note which are possible in the tuning system based on the IPN note name, the same way we render the 12-pitch-class sets.

IMPORTANT: 
Arabic Musicological Logic: 
IPN references respect Arabic maqām theory where microtonal modifiers indicate what the pitch is a variant OF, not mathematical proximity to 12-EDO semitones

For example this is the Ibn Sina tuning system:

Ibn Sīnā  (1037) 7-Fret Oud 17-Tone [ʿushayrān] - Dīwān (octave) 1

Note Name	ʿushayrān	ʿajam ʿushayrān	ʿirāq	kawasht	rāst	nīm zīrgūleh	zīrgūleh	dūgāh	kurdī	segāh	būselīk/ʿushshāq	chahārgāh	nīm ḥijāz	ḥijāz	nawā	ḥiṣār	tīk ḥiṣār
English Name	A2	Bb2	B-b2	B2	C3	C-#3	C#3	D3	Eb3	E-b3	E3	F3	F-#3	F#3	G3	Ab3	A-b3
Fraction Ratio	1/1	273/256	13/12	9/8	32/27	39/32	81/64	4/3	91/64	13/9	3/2	128/81	13/8	27/16	16/9	91/48	52/27
MIDI & Deviation	45 0.00	46 +11.31	36 +1038.57	47 +3.91	48 -5.87	50 -157.52	49 +7.82	50 -1.96	51 +9.35	53 -163.38	52 +1.96	53 -7.82	55 -159.47	54 +5.87	55 -3.91	56 +7.40	59 -265.34

The IPN Note E can have 2 variations, E and E-b, whereas Eb only has one and C only has one etc... 

