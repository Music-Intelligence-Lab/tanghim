# Image Lightbox — Design

**Date:** 2026-07-06
**Status:** Approved, ready for implementation

## Goal

Let a visitor click any content screenshot on the landing pages to view it full-size
in a centered overlay. Dismiss via Esc, backdrop click, or a close button. One image
at a time — no gallery navigation, zoom, or captions.

## Scope

**In scope** — all content screenshots on the three landing pages (`/`, `/fr/`, `/ar/`):
- The hero image (`tanghim-ui.png`) in each `index.astro`.
- Every `Showcase.astro` image (the `shot-*.png` feature screenshots).
- Every `FeatureRow.astro` image.

**Out of scope (YAGNI):**
- Prev/next gallery navigation, arrow keys cycling between images.
- Zoom/pan inside the lightbox, captions, thumbnails, swipe gestures.
- Images inside the docs/articles (Starlight/markdown-rendered) — a separate surface.

## Approach

Native `<dialog>` element + a small amount of vanilla JS. No framework, no external
library, no dependency. Matches the project's self-contained, no-JS-framework ethos and
works with the existing plain `<img>` tags.

Rationale for `<dialog>` over alternatives:
- `showModal()` gives focus-trap, `Esc`-to-close, inert background, and `aria-modal`
  **for free** (native, accessible) — no manual focus management.
- CSS-only (`:target`/checkbox) was rejected: poor accessibility, awkward Esc/focus.
- A JS library (PhotoSwipe/GLightbox) was rejected: bundle weight + external dependency
  for a handful of screenshots; conflicts with the self-contained ethos.

## Architecture

Two pieces: **one shared component** (the overlay) + **a trigger convention** (marked images).

### 1. `Lightbox.astro` (new component)

Rendered **once per page**, before `</body>` in each index page. Contains:

- A single native `<dialog id="lightbox">` with:
  - one `<img>` (empty `src`/`alt`, filled in by JS at open time),
  - a close `<button>` (`×`) with a localized `aria-label`,
  - backdrop styling via `::backdrop`.
- An inline `<script>` (~40 lines) that:
  - Attaches **one** delegated `click` listener on `document` for `[data-lightbox]`
    triggers (so it covers every screenshot without per-image wiring).
  - On trigger activation: reads the source image's `currentSrc || src` and `alt`,
    sets them on the dialog's `<img>`, then calls `dialog.showModal()`.
  - Closes on: close-button click, **backdrop click** (event target is the `<dialog>`
    itself, not the inner image), and `Esc` (native — no code needed).
  - Restores focus to the trigger on close (native `<dialog>` returns focus to the
    element that had it before `showModal()`; the trigger is that element).

One dialog is reused for all images — no per-image dialogs, no index/state.

### 2. Trigger convention

Each in-scope `<img>` is wrapped in a focusable `<button type="button" data-lightbox>`
(transparent, no chrome) so it is:
- **keyboard-focusable** and activatable with Enter/Space (button semantics — free),
- **click-activatable** with the mouse.

The button carries a localized `aria-label` (e.g. "View image full size"). The image
inside keeps its descriptive `alt`.

**Layout constraint:** the existing images sit inside `position: relative` wrappers with
an absolutely-positioned glow sibling (`.showcase-glow`, `.row-glow`, `.hero-glow`) and
carry `w-full` + rounded/border classes. The wrapping `<button>` must be
`display: block; width: 100%`, carry no default button chrome (border/background/padding
reset to none), and preserve the image's own classes and the `relative z-[1]` stacking so
the glow still renders behind it. The button replaces the `<img>` in the DOM at the same
position (glow sibling stays put); it must not introduce a new stacking/positioning
context that hides the glow.

**Affordance:** `cursor: zoom-in` on the button, plus a subtle hover lift
(scale ~1.01 + slight brightness) — matches existing hover conventions,
`prefers-reduced-motion`-safe. A visible gold focus ring (the project's
`outline: 2px solid var(--brand-gold)` convention) on `:focus-visible`.

## Data flow

```
[button data-lightbox] click / Enter
  → JS reads inner <img>.currentSrc + .alt
  → sets dialog <img>.src + .alt
  → dialog.showModal()          (focus trap + inert bg, native)
close (× / backdrop / Esc)
  → dialog.close()              (focus returns to trigger, native)
```

No application state, no image index, no multiple dialogs.

## Styling (Tailwind + theme tokens)

- Dialog centered; image `max-width: 90vw; max-height: 90vh; width/height: auto`.
- `::backdrop`: navy at ~85% opacity (`--brand-navy-900` / a dark scrim).
- Image: rounded corners + subtle gold-tinted border, matching the existing on-page
  image treatment.
- Close button: gold-tinted (`--brand-gold`), positioned with **logical** properties
  (`inset-block-start` / `inset-inline-end`) so it lands top-right under LTR and
  top-left under RTL without a direction override.
- Open animation: a short fade/scale-in, disabled under `prefers-reduced-motion`.

## Accessibility

- Native `<dialog>` + `showModal()`: focus trap, `Esc`, inert background, `aria-modal` —
  all provided by the platform.
- Trigger is a real `<button>`: keyboard-focusable, Enter/Space activation, visible gold
  focus ring.
- Dialog `<img>` receives the source image's `alt`. Close button has a localized
  `aria-label`.
- `prefers-reduced-motion` disables the open animation and the hover lift.

## Localization

`Lightbox.astro` needs localized strings for the close-button `aria-label` and the
trigger `aria-label`. It determines the locale the same way `Footer.astro` does — from
`Astro.url.pathname` — with a small three-locale label set (en/ar/fr). No new i18n
machinery.

Suggested strings:
- close: EN "Close" · AR "إغلاق" · FR "Fermer"
- trigger: EN "View image full size" · AR "عرض الصورة بالحجم الكامل" · FR "Voir l'image en plein écran"

RTL: the dialog is centered (direction-agnostic); the close button uses logical inset
properties so it mirrors correctly in AR.

## Integration points

1. **`Lightbox.astro`** — new component.
2. **`Showcase.astro`** — wrap its `<img>` in the `data-lightbox` button + affordance.
3. **`FeatureRow.astro`** — same wrapping for its `<img>`.
4. **Hero image** — in each of `index.astro`, `fr/index.astro`, `ar/index.astro`, wrap
   the hero `<img>` the same way.
5. **Render `<Lightbox />` once** before `</body>` in each of the three index pages.

The Showcase/FeatureRow images (steps 2–3) are shared components, so wrapping them once
covers all their instances across all three locales. Only the hero (step 4) and the
`<Lightbox />` include (step 5) are touched per-page.

## Testing / verification

- **Build:** `npx astro build` compiles with no errors across all 67 pages.
- **Rendered DOM:** each index page contains exactly one `<dialog id="lightbox">`;
  every in-scope image is wrapped in a `[data-lightbox]` button.
- **Manual (dev server):** click a screenshot → opens full-size; Esc, backdrop click, and
  the close button each dismiss it; keyboard Tab reaches the image button and Enter opens
  it; focus returns to the trigger on close; AR page mirrors the close button to top-left.
- **Reduced motion:** with the OS setting on, no open animation / hover lift.

## Non-goals reminder

No gallery, no zoom/pan, no captions, no docs-image coverage. If those are wanted later
they are additive and can extend `Lightbox.astro` without reworking this design.
