# Expandable, centered "What is X?" interludes

**Date:** 2026-07-05
**Component:** `website/src/components/Interlude.astro`
**Status:** approved design → ready for implementation plan

## Goal

Redesign the three no-glyph "What is X?" explainer interludes so their text is
center-justified and the body is collapsible: the eyebrow becomes a title, the
body shows the first two lines trimmed with an ellipsis, and a centered chevron
button below expands the full text with a smooth animation. Applies to all three
languages (EN/FR/AR, incl. RTL).

## Scope

**In scope — the three no-glyph explainer interludes only:**
- Archive — "What is the Digital Arabic Maqām Archive?"
- MTS-ESP — "What is MTS-ESP?"
- Diwān — "The Diwān of Arabic maqām theory" (eyebrow reworded to a question, see Copy)

**Out of scope — unchanged:**
- The glyph gloss interlude ("On the name", with the large `تنغيم` illuminated
  initial). Keeps its two-column left-aligned layout entirely.

## Component API

Add one boolean prop to `Interlude.astro`:

```ts
interface Props {
  eyebrow?: string;
  glyph?: string;
  expandable?: boolean;  // NEW — default false
}
```

- `expandable` is only honoured on **no-glyph** interludes. If `glyph` is set,
  `expandable` is ignored (the glyph gloss never collapses).
- When `expandable` is false (default), the component renders exactly as today —
  no behavioural change to any existing non-expandable usage.

The three explainer instances in the page files pass `expandable`.

## Layout (expandable variant)

Collapsed:
```
              ——                    short centered gold accent line
     What is MTS-ESP?               title (was eyebrow), centered, larger

  MTS-ESP is a tuning protocol
   developed by ODDSound (…)        body, center-justified, clamped to 2 lines + …

           ( › )                    centered round chevron button
```

Expanded: the 2-line clamp releases, the full body (all paragraphs) eases open,
and the chevron rotates 90° to point down. Collapsing reverses it.

### Visual details
- **Accent line:** short (~2.5rem) horizontal gold rule, centered above the
  title. Replaces the current `border-inline-start` gold rule (which does not
  suit centered, symmetric content). Symmetric → no LTR/RTL edge concern.
- **Title:** the eyebrow text, but rendered larger than the current 0.7rem
  eyebrow — sized to read as a section title (approx `clamp(1.05rem, 2vw, 1.35rem)`,
  tuned by eye), still gold, centered. Keeps `text-transform: uppercase` for EN/FR;
  AR is unaffected by uppercase and keeps `letter-spacing: normal` (existing RTL rule).
- **Body:** `text-align: center`, existing prose styles (links, inline Arabic
  terms, `<strong>`, `<em>`) preserved. `max-width` retained so long lines stay
  at reading measure even when centered.
- **Button:** ~32px round, 1px gold border, transparent fill, gold chevron `›`
  drawn in CSS. Rotates 90° (`transform: rotate(90deg)`) when expanded. Hover:
  subtle gold-tint background. Focus-visible: gold outline (matches existing link
  focus style).

## Mechanism — Tailwind-first, minimal JS

Astro has **no** built-in in-page expand/collapse animation (its View
Transitions / `transition:animate` are for page navigation only — verified
against Astro docs). So this is standard CSS transition + a tiny JS toggle.

Styling is done with **Tailwind utility classes** (arbitrary values where needed,
matching the page files' existing convention). Only the few states that cannot be
expressed as pure utilities live in a small scoped `<style>` block:
- the collapsed line-clamp (2 lines)
- the `grid-template-rows` collapse/expand transition
- the chevron rotation on the open state

### Markup shape
```
<div class="interlude-inner interlude-inner--expandable reveal">
  <div class="interlude-text">   (centered)
    <span class="accent-line" aria-hidden="true"></span>
    <h2 class="interlude-title">{eyebrow}</h2>
    <div id="interlude-body-{id}" class="interlude-collapse">
      <div class="interlude-body"><slot /></div>
    </div>
    <button class="interlude-toggle"
            aria-expanded="false"
            aria-controls="interlude-body-{id}"
            aria-label={expandLabel}>
      <span class="chevron" aria-hidden="true">›</span>
    </button>
  </div>
</div>
```
- `id` is generated per instance (e.g. `crypto.randomUUID()` at build time, or a
  slug of the eyebrow) so `aria-controls` is unique when several interludes render.
- The body `<slot>` is rendered **once** (no prose duplication). The collapse
  wrapper is a sibling of the button, not inside a `<summary>`.

### Animation
- Collapse wrapper uses the `grid-template-rows` technique:
  - collapsed: `grid-template-rows: <2-line height>` (a fixed floor, not 0, so
    two lines always show)
  - expanded (`.is-open`): `grid-template-rows: 1fr`
  - `transition: grid-template-rows 0.35s ease` + body opacity.
- The 2-line floor is achieved by clamping the inner `.interlude-body` to
  `-webkit-line-clamp: 2` when collapsed and releasing (`line-clamp: none`) when
  `.is-open`.
- **Browser-behaviour risk to verify in the dev server before finalising:**
  animating `grid-template-rows` from a fixed row value to `1fr`. If the
  transition from a fixed height to `1fr` does not ease cleanly across target
  browsers, fall back to animating `max-height` (collapsed = 2-line max-height,
  expanded = a large max-height) — a well-worn accordion pattern. Pick whichever
  renders smoothly; do not ship an approach that visibly janks.

### JavaScript (progressive enhancement)
- A single small inline script, gated on `body.js-ready` (same pattern the
  `.reveal` scroll animations already use).
- On toggle click: flip `aria-expanded`, toggle `.is-open` on the interlude,
  swap the button `aria-label` between expand/collapse (localised).
- Script is generic: it selects all `.interlude-toggle` buttons and wires each to
  its `aria-controls` target — one script handles every expandable interlude on
  the page.

### No-JS / graceful degradation
- Without `body.js-ready`, the collapse wrapper is **fully open and the clamp is
  off** (full text visible), and the toggle button is hidden. No content is ever
  trapped behind a dead button. This mirrors how `.reveal` only hides content
  when JS is confirmed running.

## Accessibility
- Toggle is a real `<button>` with `aria-expanded` + `aria-controls`.
- Collapsed body is clamped (still in the DOM and accessibility tree), not
  `display:none`, so it remains findable / screen-reader reachable.
- Localised `aria-label` per language: EN "Expand" / "Collapse",
  FR "Développer" / "Réduire", AR "توسيع" / "طيّ".
- Respects `prefers-reduced-motion`: disable the expand transition (instant
  open/close) under reduced motion, matching the existing `.reveal` treatment.

## RTL
- Center-justified text is direction-neutral — no per-direction alignment needed.
- The accent line is centered (not edge-anchored), so it needs no logical-property
  handling.
- Existing RTL rules preserved: no letter-spacing on Arabic title; AR `<em>`
  emphasis stays upright gold (from the earlier fix).

## Copy change (bundled)

Reword the **Diwān** eyebrow to a question in all three languages (the Archive and
MTS-ESP eyebrows are already questions):

| Lang | From | To |
|---|---|---|
| EN | `The Diwān of Arabic maqām theory` | `What is the Diwān of Arabic maqām theory?` |
| FR | `Le dīwān de la théorie du maqām arabe` | `Qu'est-ce que le dīwān de la théorie du maqām arabe ?` |
| AR | `ديوان نظرية المقام العربي` | `ما هو ديوان نظرية المقام العربي؟` |

## Files touched
- `src/components/Interlude.astro` — add `expandable` prop, title/accent/toggle
  markup, collapse + chevron CSS, generic inline toggle script.
- `src/pages/index.astro` — add `expandable` to the 3 explainers; reword Diwān eyebrow.
- `src/pages/fr/index.astro` — same.
- `src/pages/ar/index.astro` — same.

## Verification
- Dev-server eyeball (user) across EN/FR/AR: collapsed shows exactly 2 lines +
  ellipsis; chevron rotates; expand animation is smooth; RTL centered correctly.
- Confirm the closed-state clamp actually renders 2 lines (browser behaviour).
- No-JS check: full text visible, no trapped content.
- `prefers-reduced-motion`: no animation, still expandable.
- Production build passes.
```
