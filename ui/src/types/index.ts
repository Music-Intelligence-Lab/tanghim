/** Shared TypeScript types mirroring the C++ model structs. */

export interface PitchClassVariant {
  noteName: string;          // e.g. "segah"
  noteNameDisplay: string;   // e.g. "segāh" (with diacritics)
  englishName: string;       // e.g. "E-b3"
  solfege: string;           // e.g. "Mi -b3", "Do 2", "Sol 1"
  midiNoteNumber: number;
  midiCentsDeviation: number;
  cents: number;
  fraction: string;
  ipnReference: string;      // "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
}

export interface ChromaticSlot {
  ipnRef: string;            // e.g. "E"
  selectedIndex: number;
  isLocked: boolean;
  variants: PitchClassVariant[];
  centsOffset: number;       // Current cents deviation from 12-EDO (source of truth for tuning output)
}

export interface MaqamPreset {
  isAssigned: boolean;
  maqamId: string;
  maqamDisplay: string;
  tonicIpn: string;
  tonicSolfege: string;
  baseMaqamId: string;
  isTransposed: boolean;
  tonicNote: string;
  setIndex: number;
  sliderPositions: number[];
  centsOffsets: number[];  // custom tuning values (source of truth for modified presets)
  degreeNames: string[];   // ascending degree PAO names for compatibility checking
  tuningSystemId: string;  // tuning system ID when preset was saved (for modified presets)
  startingNote: string;    // starting note when preset was saved (for modified presets)
}

export interface MtsStatusUpdate {
  isMtsTransmitter: boolean;
  mtsNativeCount: number;
  mpeCount: number;
  monoPbCount: number;
}

export interface MaqamMidiDragData {
  filename: string;
  midiBase64: string;
  maqamDisplay: string;
  tonicDisplay: string;
}

export interface TuningState {
  systemId: string;
  startingNote: string;
  isMtsTransmitter: boolean;
  oscillatorEnabled: boolean;
  heptEnabled: boolean;
  mtsReceivers: number;
  mtsNativeCount: number;
  mpeCount: number;
  monoPbCount: number;
  pluginVersion: string;
  buildTimestamp: string;
  /** Current reference frequency offset in cents (0 = default). */
  referenceFreqCents: number;
  /** Current reference frequency in Hz (with offset applied). */
  referenceFreqHz: number;
  /** Default reference frequency in Hz (at 0 offset). */
  referenceDefaultHz: number;
  /** PAO display name of the reference/tonic note (e.g. "yegāh"). */
  referenceNoteName: string;
  slots: ChromaticSlot[];
  presets: MaqamPreset[];
  /** chromaticIndex → { octave → PAO display name } (all octaves). */
  noteNames: Record<string, Record<string, string>>;
  /** Per-MIDI-note variant overrides. Key = MIDI note number (string),
   *  value = variant index. Absent key = use chromatic slot default. */
  perNoteOverrides: Record<string, number>;
  /** Per-MIDI-note cents overrides. Key = MIDI note number (string),
   *  value = cents deviation. Absent key = use chromatic slot centsOffset. */
  perNoteCentsOverrides: Record<string, number>;
  /** PAO idName → chromaticIndex (0-11) for ALL pitch classes across all octaves.
   *  Covers register-specific names (kirdan, muhayyar, etc.) that are absent
   *  from slot variants (which only contain MIDI 48-59 pitch classes). */
  paoNameMap: Record<string, number>;
  /** Unique PAO idNames in ascending MIDI note order (first occurrence per name).
   *  Used to sort transposition tonics in the tuning system's native pitch order. */
  paoOrder: string[];
  /** PAO idName → { englishName, solfege, octave } for display in transposition dropdown. */
  paoNameInfo: Record<string, { englishName: string; solfege: string; octave: number }>;
  /** Degree-aware IPN references from maqam detail API.
   *  Key = chromatic index (0-11) as string, value = IPN letter name (e.g. "Gb", "F#").
   *  Only populated for degree slots when a maqam is selected. */
  degreeIpnMap: Record<string, string>;
  /** Degree-aware solfege from maqam detail API.
   *  Key = chromatic index (0-11) as string, value = solfege string (e.g. "Sol-b3", "Fa#3").
   *  Only populated for degree slots when a maqam is selected. */
  degreeSolfegeMap: Record<string, string>;
  /** Currently selected maqam ID (e.g. "maqam_rast"), empty if none. */
  selectedMaqamId: string;
  /** Transposition index within the maqam (-1 = base tonic). */
  transpositionIndex: number;
  /** Active preset index (-1 = none). */
  activePresetIndex: number;
  /** Scroll position (MIDI note at left edge of slider bank). */
  startMidi: number;
  /** Ascending degree PAO names for degree highlighting. */
  degreeNames: string[];
  /** True while setStateInformation async load is in progress. */
  sessionRecallInProgress: boolean;
  /** True once session state has been fully restored from DAW data. */
  hasRecalledSessionState: boolean;
  /** MIDI channel for preset triggering. 0 = any channel, 1-16 = specific. */
  midiPresetChannel?: number;
  /** Preset index currently in MIDI Learn mode (-1 if none). */
  midiLearnTarget?: number;
  /** Per-preset MIDI note mappings. Array of 16 ints, -1 = unmapped. */
  midiPresetNotes?: number[];
}

export interface StartingNote {
  id: string;
  displayName: string;
  isCached?: boolean;
}

export interface TuningSystem {
  id: string;
  displayName: string;
  shortName: string;
  year: number;
  yearStr: string;    // e.g. "950a" — preserves letter suffix for chronological sort
  startingNotes: StartingNote[];
}

// ── Maqam list types (from /tuning-systems/{id}/{note}/maqamat?includeMaqamDegrees=true&includeTranspositions=true) ──

export interface MaqamDegrees {
  ascending: string[];   // PAO noteName idNames, e.g. ["rast", "dugah", "segah"]
  descending: string[];
}

export interface MaqamTransposition {
  tonicId: string;       // "chahargah"
  tonicDisplay: string;  // "chahārgāh"
  degrees: MaqamDegrees;
}

export interface MaqamListEntry {
  maqamId: string;        // "maqam_rast"
  maqamDisplay: string;   // "maqām rāst"
  familyId: string;       // "rast"
  familyDisplay: string;  // "rāst"
  tonicId: string;        // "rast"
  tonicDisplay: string;   // "rāst"
  degrees: MaqamDegrees;
  transpositions: MaqamTransposition[];
}

// ── JUCE 8 WebView bridge types ──────────────────────────────────────────────

declare global {
  interface Window {
    __JUCE__?: {
      backend: {
        addEventListener: (event: string, handler: (data: unknown) => void) => void;
        removeEventListener: (event: string, handler: (data: unknown) => void) => void;
        emitEvent: (event: string, data: unknown) => void;
      };
      initialisationData?: {
        __juce__functions: string[];
        __juce__sliders: string[];
        __juce__toggles: string[];
        __juce__comboBoxes: string[];
        __juce__platform: string[];
      };
    };
  }
}
