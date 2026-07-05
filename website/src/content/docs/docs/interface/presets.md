---
title: Presets
description: Saving, recalling, and managing maqam presets.
---

The preset bar has 8 preset slots. Each preset saves:

- The selected maqam and its degree positions
- All 12 slider cent offsets
- Optionally, the tuning system and starting note name (for modified presets)

## Using Presets

- **Save a preset**: select a maqam, then click an empty slot to save the current tuning state.
- **Activate a preset**: click a saved preset to load it.
- **Deactivate a preset**: click the active preset again to deactivate it (clears the maqam and resets sliders).
- **Delete a preset**: click the **x** on a saved preset to clear that slot.
- **Map a preset to MIDI**: **Shift+click** a preset to enter MIDI learn mode (requires a MIDI device and channel to be selected first; see [MIDI Preset Mapping](/docs/performance/midi-preset-mapping)).

## Modified Presets

If you adjust sliders after loading a maqam, the preset becomes "modified":

- The name shows an asterisk (`*`).
- Modified slider thumbs turn cyan.
- The tuning system is stored with the preset so it can reload correctly.

## Preset Storage

Presets are stored locally and shared across all DAWs: a preset saved in one DAW will be available in any other. To save and load different groups of presets along with all other settings (reference frequency, tuning system, etc.), use the [Save/Load State Files](/docs/performance/state-files) menu to export a `.tanghim` file.

## Auto-Activation

When you select a maqam from the dropdown, the plugin automatically activates a matching unmodified preset if one exists.
