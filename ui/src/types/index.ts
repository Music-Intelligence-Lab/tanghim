/** Shared TypeScript types mirroring the C++ model structs. */

export interface PitchClassVariant {
  noteName: string;          // e.g. "segah"
  noteNameDisplay: string;   // e.g. "segāh" (with diacritics)
  englishName: string;       // e.g. "E-b3"
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
}

export interface MaqamPreset {
  isAssigned: boolean;
  maqamId: string;
  maqamDisplay: string;
  tonicIpn: string;
  baseMaqamId: string;
  isTransposed: boolean;
  tonicNote: string;
  setIndex: number;
  sliderPositions: number[];
  degreeNames: string[];   // ascending degree PAO names for compatibility checking
}

export interface MtsStatusUpdate {
  isMtsTransmitter: boolean;
  mtsNativeCount: number;
  mpeCount: number;
  monoPbCount: number;
}

export interface TuningState {
  systemId: string;
  startingNote: string;
  isMtsTransmitter: boolean;
  mtsReceivers: number;
  mtsNativeCount: number;
  mpeCount: number;
  monoPbCount: number;
  pluginVersion: string;
  slots: ChromaticSlot[];
  presets: MaqamPreset[];
  /** chromaticIndex → { octave → PAO display name } (all octaves). */
  noteNames: Record<string, Record<string, string>>;
  /** Per-MIDI-note variant overrides. Key = MIDI note number (string),
   *  value = variant index. Absent key = use chromatic slot default. */
  perNoteOverrides: Record<string, number>;
  /** PAO idName → chromaticIndex (0-11) for ALL pitch classes across all octaves.
   *  Covers register-specific names (kirdan, muhayyar, etc.) that are absent
   *  from slot variants (which only contain MIDI 48-59 pitch classes). */
  paoNameMap: Record<string, number>;
}

export interface StartingNote {
  id: string;
  displayName: string;
}

export interface TuningSystem {
  id: string;
  displayName: string;
  shortName: string;
  year: number;
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
