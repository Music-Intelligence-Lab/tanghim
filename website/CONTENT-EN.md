# Tanghīm Landing Page — English Content (for editing)

> Edit the text below, then hand it back. I'll place your edited copy and use it as the basis for AR/FR translation.
>
> **How to specify each showcase's layout:** add a `Layout:` line using ONE of these values:
>
> - `centered` — big centered visual with centered copy above/below (current Showcase style)
> - `image-left` — visual on the left, text on the right (two-column row)
> - `image-right` — visual on the right, text on the left (two-column row)
> - `text-only` — no visual (e.g. the feature grid cards)
>
> `<strong>...</strong>` marks the gold-accent word(s) in a title — keep or move the tags as you like.
> Lines starting with `#` or `>` are notes to me; leave them or delete them.
>
> **Section order — momentum first, depth as reward:** lead with the strongest capability, keep the
> feature tour flowing, and drop the explainer interludes (MTS-ESP, DiArMaqAr, the name gloss) between
> features where a hooked scroller starts asking "how does this work / where's this from?". The name
> gloss lands late as a grace note before the CTA.

---



## HERO

Eyebrow: Finally!
Headline: A **thousand years** of Arabic maqām intonation at your fingertips  
Subhead: Tanghīm is a **free, open-source MIDI plugin** that brings historical Arabic maqām tunings from the **Digital Arabic Maqām Archive** directly to any DAW

Primary button: Download  
Secondary link: Read the docs  
Screenshot alt text: The Tanghīm plugin interface: a bank of tuning sliders showing maqām degrees in gold against a navy panel, with maqām selector, preset grid, and reference-frequency controls.

---



## TRUST STRIP

Origin line: Conceived and designed by [Khyam Allami](https://khyamallami.com/) at the [Music Intelligence Lab](https://musicintelligencelab.com/)/[Center for Advanced Mathematical Sciences](https://www.aub.edu.lb/cams/), American University of Beirut, 2026. Free and [open source](https://github.com/Music-Intelligence-Lab/tanghim), licensed under [CC BY-NC-SA 4.0](https://creativecommons.org/licenses/by-nc-sa/4.0/).

---



## DIARMAQAR STRIP

Title: What is the Digital Arabic Maqām Archive (DiArMaqAr)?  
Copy: Tanghīm draws its data from the [Digital Arabic Maqām Archive (DiArMaqAr)](https://diarmaqar.net/) — an open-source repository of historically documented tuning systems and maqāmāt from the music of the Arabic-speaking region, each with full bibliographic attribution. DiArMaqAr was researched, designed, and developed by Khyam Allami with Ibrahim El Khansa at the Music Intelligence Lab, American University of Beirut. Tanghīm queries its REST API and caches the data locally, so the tunings you play are the same source-attributed values the archive holds. An update button tells you when the archive has changed.

---



## SHOWCASE

Layout: centered  
Title: Arabic maqām tuning, **your sounds**  
Subtitle: Three tuning methods, from software synths to hardware.  
Copy: MTS-ESP retunes compatible synths automatically and immediately from the main *transmitter* plugin over shared memory. Our dedicated *receiver* plugin enables per-note pitch bend using MPE (MIDI Polyphonic Expression), or Monophonic Pitch Bend for single-MIDI-channel synths.  
Badges: MTS-ESP · MPE · Mono PB  

---



## MTS-ESP STRIP

Title: What is MTS-ESP?  
Copy: MTS-ESP is a tuning protocol developed by [ODDSound](https://github.com/ODDSound/MTS-ESP). One transmitter plugin sets the tuning, and any number of compatible instruments connect to it automatically, retuning together with no manual setup. The tuning updates in real time, so a change of maqām or transposition follows every connected instrument as it plays.

---



## SHOWCASE

Layout: centered  
Title: Edit with your **ears** and play  
Subtitle: Theoretical tuning data, in practice  
Copy: Twelve sliders, one per chromatic step, provide a simple and immediate way to explore the tunings. Each slider includes snap markers to explore the different intervals, whilst letting you adjust any note by ear.

---



## SHOWCASE

Layout: centered  
Title: Choose your maqām then **modulate**  
Subtitle: MIDI-mappable and automatable presets.  
Copy: Save up to eight presets of any maqām in any available transposition, and switch between them by MIDI-mapping your controller, sending Program Change messages, or using DAW automation.

---



## SHOWCASE

Layout: centered  
Title: Hear it **instantly**  
Subtitle: Audition any tuning with no instrument loaded.  
Copy: The built-in triangle-wave oscillator (Osc) lets you hear a tuning immediately, with no external instrument loaded — so you can explore a maqām the moment you select it.  
Badges: Osc  

---



## SHOWCASE

Layout: centered  
Title: Diwānayn: from Yegāh to Saham  
Subtitle: Expand to full width for a two-octave view.  
Copy: Widen the plugin and the slider bank grows with it, laying out the tuning system across the two dawāwīn — the lower and upper octave registers of Arabic maqām theory. Every degree is visible at once, with its note names and solfege so you can see the whole system.

---



## MUSICOLOGY STRIP — DAWĀWĪN

> No glyph, no eyebrow — body copy only.
> Copy: Arabic maqām theory gives unique Persian-Arab-Ottoman names for each of its notes/pitches — rāst, dūgāh, sīkāh, and so on — across two dawāwīn (دواوين), a lower and an upper diwān (octave). Expand Tanghīm to its full width to see the tuning systems and maqāmāt across the two octaves.

---



## FEATURE

Layout: image-left  
Title: Heptatonic mode for quick play  
Copy: Heptatonic mode (Hept) remaps the keyboard so every maqām is performable on the diatonic (white) keys alone — the white keys play the maqām degrees in order from the tonic. Any maqām becomes easy to hear and produce, with no complex fingerings to learn.

---



## FEATURE

Layout: image-right  
Title: Set your **reference pitch**  
Copy: Set the tuning's reference pitch by ear, exact Hz, or cents to match your favourite recordings or your preference. The reference retains across maqāmāt and tuning systems, so you can set it and forget it.

---



## FEATURE

Layout: image-left  
Title: Save and load your maqām tunings  
Copy: Tanghīm lets you edit, save, and reload maqām tunings and preset settings as human-readable .tanghim files — including per-note overrides across different octaves.

---



## FEATURE

Layout: image-left  
Title: Grab and process the **MIDI notes**  
Copy: Drag the current maqām's MIDI notes out as a standard MIDI file — onto your timeline, or as a clip to feed arpeggiators and other MIDI processors. A Program Change is embedded so the preset re-arms on playback.

---



## ABLETON CALL-OUT

Title: A musically meaningful maqām tuning method for Ableton Live  
Copy: Ableton Live doesn't allow third-party MIDI-effect plug-ins. To solve this, Tanghīm ships with two Max for Live devices — an MPE Receiver and a Mono Pitch Bend Receiver — that deliver the tuning inside Live. No extra configuration: install, load, and the tuning follows.

Link 1: Read the article for the story  →  
Link 2: Ableton Live setup guide →

---



## CARDS GRID

Card 1 — Per-note overrides: Give the same note a different intonation in any octave.  
Card 2 — Reference frequency: Set the tuning's base pitch by knob, exact Hz, or in cents; retains across tuning systems.  
Card 3 — Range scroller: Pan the 128-note keyboard with magnetic snap to C, G, and A.  
Card 4 — DAW automation: Slider cents, reference offset, and preset are exposed as automatable parameters.  
Card 5 — Save & load: Store and recall complete tuning states as portable .tanghim files.  
Card 6 — Program-change switching: Switch presets from your DAW with MIDI Program Change messages.

---



## MUSICOLOGY STRIP — NAME GLOSS

Name gloss: **Tanghīm** (تنغيم) is an Arabic *maṣdar* (verbal noun), denoting the act of intoning or setting pitch. In linguistics it is the standard equivalent of *intonation*, the melodic contour of speech. Here it names intonational tuning, the pitch relationships that make up a system, as distinct from the Arabic-language usage of *dūzān* (دوزان) for the mechanical tuning of an instrument's strings. Dūzān is a borrowing from Ottoman Turkish düzen ("order, arrangement, tuning"), from the verb düzmek ("to arrange, put in order").

---



## FINAL CTA BAND

Headline: Free, open-source, and ready for your music.  
Buttons: Download  ·  Read the docs

---



## FOOTER

Credit: Conceived and designed by Khyam Allami at the [Music Intelligence Lab](https://musicintelligencelab.com/)/[Center for Advanced Mathematical Sciences](https://www.aub.edu.lb/cams/), American University of Beirut, 2026.  
Open source line: Free and [open source](https://github.com/Music-Intelligence-Lab/tanghim), licensed under [CC BY-NC-SA 4.0](https://creativecommons.org/licenses/by-nc-sa/4.0/).   

Link groups:  
Documentation: Docs · Article  
Project: GitHub · Downloads  
Related: Digital Arabic Maqām Archive (DiArMaqAr) · khyamallami.com

---



## NAV

Wordmark: Tanghīm
Links: Features · Docs · Article · GitHub · [Download button]
Language switcher: English · العربية · Français