# Expandable Interludes Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the three no-glyph "What is X?" interludes (Archive, MTS-ESP, Diwān) center-justified and collapsible — eyebrow-as-title, 2-line clamped preview with ellipsis, and a centered chevron button that smoothly expands the full body — across EN/FR/AR.

**Architecture:** Add an `expandable` boolean prop to `Interlude.astro`. When set (and no glyph), the component renders the eyebrow as a centered title with a short gold accent line above it, wraps the body slot in a collapsible container clamped to 2 lines, and renders a chevron toggle button. A component-scoped `<script>` (gated on `body.js-ready`) wires every toggle to its `aria-controls` target and animates open/closed via a CSS class. No-JS and reduced-motion render fully expanded. Styling is Tailwind-first; only clamp/collapse/chevron states live in a scoped `<style>`.

**Tech Stack:** Astro 7, Tailwind (arbitrary-value utilities), scoped component CSS + one component `<script>`. No new dependencies.

## Global Constraints

- **Scope:** Only the three no-glyph explainer interludes get the treatment. The glyph gloss ("On the name", `glyph="تنغيم"`) is never expandable and its markup path is unchanged.
- **No fallbacks / graceful degradation in data logic** — but progressive enhancement for JS is required and expected here: no-JS = full text visible, button hidden (this is enhancement, not a data fallback).
- **Tailwind-first:** use Tailwind utility classes for styling; put in scoped `<style>` only what utilities can't express (line-clamp toggle, grid-rows/max-height transition, chevron rotation).
- **RTL:** center-justified text is direction-neutral. Preserve existing RTL rules (no letter-spacing on Arabic; AR `<em>` upright gold).
- **Accessibility:** real `<button>` with `aria-expanded` + `aria-controls`; collapsed body clamped (not `display:none`); localised `aria-label`; honour `prefers-reduced-motion`.
- **Localised aria-labels:** EN "Expand"/"Collapse", FR "Développer"/"Réduire", AR "توسيع"/"طيّ".
- **Copy:** reword the Diwān eyebrow to a question — EN `What is the Diwān of Arabic maqām theory?`, FR `Qu'est-ce que le dīwān de la théorie du maqām arabe ?`, AR `ما هو ديوان نظرية المقام العربي؟`.
- **Verification is browser + build, not unit tests** — the site has no frontend test harness. Each task ends with a build check and an explicit dev-server observation the user confirms.

## File Structure

- `src/components/Interlude.astro` — MODIFY. Add `expandable` prop, title/accent/toggle markup for the expandable path, collapse/chevron CSS, and a component `<script>` wiring the toggles. The existing non-expandable and glyph paths stay byte-for-byte the same.
- `src/pages/index.astro` — MODIFY. Add `expandable` to the 3 explainer `<Interlude>` calls; reword Diwān eyebrow.
- `src/pages/ar/index.astro` — MODIFY. Same.
- `src/pages/fr/index.astro` — MODIFY. Same.

The component owns all behaviour and styling; the page files only opt in + fix copy. This keeps the collapse logic in one place and the pages declarative.

---

### Task 1: Add `expandable` prop and expandable markup (collapsed-by-CSS, no JS yet)

Render the new centered layout for expandable interludes: accent line, title, clamped body, chevron button. Static CSS only — clicking does nothing yet. The default (non-expandable) and glyph paths are untouched.

**Files:**
- Modify: `src/components/Interlude.astro`

**Interfaces:**
- Consumes: nothing new.
- Produces: `Interlude` now accepts `expandable?: boolean` (default `false`). When `expandable` is true and `glyph` is unset, renders the expandable markup below. Later tasks rely on these class/attribute hooks: root `.interlude-inner--expandable`, button `.interlude-toggle` with `aria-expanded` + `aria-controls={bodyId}`, collapse wrapper `#{bodyId}.interlude-collapse`, and the open-state class `.is-open` applied to the root.

- [ ] **Step 1: Add the prop to the interface and destructure it**

In `src/components/Interlude.astro` frontmatter, change:

```ts
interface Props {
  eyebrow?: string;
  glyph?: string;
}

const { eyebrow, glyph } = Astro.props;
```

to:

```ts
interface Props {
  eyebrow?: string;
  glyph?: string;
  expandable?: boolean;
  /** Localised toggle labels; required when expandable. */
  expandLabel?: string;
  collapseLabel?: string;
}

const { eyebrow, glyph, expandable = false, expandLabel = 'Expand', collapseLabel = 'Collapse' } = Astro.props;

// Expandable only applies to no-glyph explainer interludes.
const isExpandable = expandable && !glyph;

// Stable unique id for aria-controls (crypto.randomUUID is available at build time in Astro/Node).
const bodyId = `interlude-body-${crypto.randomUUID().slice(0, 8)}`;
```

- [ ] **Step 2: Add the expandable markup branch**

Replace the current body block:

```astro
<div class:list={['interlude-inner reveal', { 'interlude-inner--no-glyph': !glyph }]}>
  {glyph && (
    <div class="interlude-glyph" aria-hidden="true">
      <span lang="ar" class="interlude-glyph-word">{glyph}</span>
    </div>
  )}
  <div class="interlude-text">
    {eyebrow && <p class="interlude-eyebrow">{eyebrow}</p>}
    <div class="interlude-body">
      <slot />
    </div>
  </div>
</div>
```

with:

```astro
<div class:list={['interlude-inner reveal', { 'interlude-inner--no-glyph': !glyph, 'interlude-inner--expandable': isExpandable }]}>
  {glyph && (
    <div class="interlude-glyph" aria-hidden="true">
      <span lang="ar" class="interlude-glyph-word">{glyph}</span>
    </div>
  )}
  {isExpandable ? (
    <div class="interlude-text interlude-text--expandable">
      <span class="interlude-accent-line" aria-hidden="true"></span>
      {eyebrow && <h2 class="interlude-title">{eyebrow}</h2>}
      <div id={bodyId} class="interlude-collapse">
        <div class="interlude-body interlude-body--centered">
          <slot />
        </div>
      </div>
      <button
        type="button"
        class="interlude-toggle"
        aria-expanded="false"
        aria-controls={bodyId}
        aria-label={expandLabel}
        data-expand-label={expandLabel}
        data-collapse-label={collapseLabel}
      >
        <span class="interlude-chevron" aria-hidden="true">›</span>
      </button>
    </div>
  ) : (
    <div class="interlude-text">
      {eyebrow && <p class="interlude-eyebrow">{eyebrow}</p>}
      <div class="interlude-body">
        <slot />
      </div>
    </div>
  )}
</div>
```

- [ ] **Step 3: Add the expandable CSS to the scoped `<style>` block**

Append inside the existing `<style>` block in `src/components/Interlude.astro` (before the closing `</style>`), just after the existing `.interlude-inner--no-glyph` rules is fine, or at the end:

```css
  /* ── Expandable "What is X?" interludes ─────────────────────────────────
     Centered, symmetric explainer: short gold accent line, eyebrow-as-title,
     center-justified body clamped to 2 lines until expanded, chevron toggle.
     Replaces the left gold rule (which doesn't suit centered content). */
  .interlude-inner--expandable {
    grid-template-columns: minmax(0, 1fr);
    max-width: 720px;
    text-align: center;
  }

  /* No left rule on the expandable variant — the no-glyph border rule must not
     apply here. (This wins by being later + equal specificity.) */
  .interlude-inner--expandable .interlude-text--expandable {
    border-inline-start: none;
    padding-inline-start: 0;
    display: flex;
    flex-direction: column;
    align-items: center;
  }

  .interlude-accent-line {
    display: block;
    width: 2.5rem;
    height: 2px;
    margin-bottom: 1rem;
    background: color-mix(in srgb, var(--brand-gold) 55%, transparent);
    border-radius: 1px;
  }

  .interlude-title {
    margin: 0 0 0.9rem;
    font-size: clamp(1.05rem, 2vw, 1.35rem);
    font-weight: 600;
    text-transform: uppercase;
    letter-spacing: 0.12em;
    color: var(--brand-gold);
    line-height: 1.3;
  }

  /* Arabic titles must not be letter-spaced (breaks cursive joins). */
  :global([dir='rtl']) .interlude-title {
    letter-spacing: normal;
    text-transform: none;
  }

  .interlude-body--centered {
    text-align: center;
    margin-inline: auto;
  }

  /* Collapsed: clamp the body to 2 lines with an ellipsis. The collapse wrapper
     animates in Task 3; here it just holds the clamp. */
  .interlude-collapse .interlude-body--centered {
    display: -webkit-box;
    -webkit-box-orient: vertical;
    -webkit-line-clamp: 2;
    overflow: hidden;
  }

  /* Chevron toggle button */
  .interlude-toggle {
    margin-top: 1rem;
    width: 2rem;
    height: 2rem;
    display: inline-flex;
    align-items: center;
    justify-content: center;
    border: 1px solid color-mix(in srgb, var(--brand-gold) 55%, transparent);
    border-radius: 50%;
    background: transparent;
    color: var(--brand-gold);
    cursor: pointer;
    transition: background 0.15s ease, border-color 0.15s ease;
  }

  .interlude-toggle:hover {
    background: color-mix(in srgb, var(--brand-gold) 12%, transparent);
    border-color: var(--brand-gold);
  }

  .interlude-toggle:focus-visible {
    outline: 2px solid var(--brand-gold);
    outline-offset: 2px;
  }

  .interlude-chevron {
    display: inline-block;
    font-size: 1.1rem;
    line-height: 1;
    transform: rotate(90deg); /* › points down when collapsed */
    transition: transform 0.3s ease;
  }
```

Note: `›` rotated 90° points down (indicating "expand"); Task 3 rotates it to 270° (points up) when open. Adjust base rotation by eye in verification.

- [ ] **Step 4: Build to verify no errors**

Run: `cd website && npx astro build`
Expected: build completes, no errors. (This proves the prop + markup compile; the interludes are not yet opted-in, so nothing changes on the pages yet.)

- [ ] **Step 5: Commit**

```bash
cd website
git add src/components/Interlude.astro
git commit -m "website: add expandable variant to Interlude (static markup + CSS)

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 2: Opt the three explainer interludes in + reword Diwān eyebrow (all languages)

Turn on `expandable` for Archive/MTS-ESP/Diwān in all three pages, pass localised toggle labels, and reword the Diwān eyebrow to a question. After this task the collapsed 2-line preview is visible in the browser (still no expand behaviour — that's Task 3).

**Files:**
- Modify: `src/pages/index.astro`
- Modify: `src/pages/ar/index.astro`
- Modify: `src/pages/fr/index.astro`

**Interfaces:**
- Consumes: `Interlude`'s `expandable`, `expandLabel`, `collapseLabel` props from Task 1.
- Produces: nothing consumed by later tasks.

- [ ] **Step 1: EN — opt in the three explainers and reword Diwān**

In `src/pages/index.astro`:

Archive (line ~118):
```astro
<Interlude eyebrow="What is the Digital Arabic Maqām Archive?" expandable expandLabel="Expand" collapseLabel="Collapse">
```

MTS-ESP (line ~145):
```astro
<Interlude eyebrow="What is MTS-ESP?" expandable expandLabel="Expand" collapseLabel="Collapse">
```

Diwān (line ~197) — reword AND opt in:
```astro
<Interlude eyebrow="What is the Diwān of Arabic maqām theory?" expandable expandLabel="Expand" collapseLabel="Collapse">
```

Leave the "On the name" glyph interlude (line ~292) untouched.

- [ ] **Step 2: AR — opt in and reword Diwān**

In `src/pages/ar/index.astro`:

Archive (line ~118): add props (keep the existing question eyebrow):
```astro
<Interlude eyebrow="ما هو أرشيف المقام العربي الرقمي؟" expandable expandLabel="توسيع" collapseLabel="طيّ">
```

MTS-ESP (line ~144):
```astro
<Interlude eyebrow="ما هو MTS-ESP؟" expandable expandLabel="توسيع" collapseLabel="طيّ">
```

Diwān (line ~196) — reword AND opt in:
```astro
<Interlude eyebrow="ما هو ديوان نظرية المقام العربي؟" expandable expandLabel="توسيع" collapseLabel="طيّ">
```

Leave "حول الاسم" glyph interlude (line ~291) untouched.

- [ ] **Step 3: FR — opt in and reword Diwān**

In `src/pages/fr/index.astro`:

Archive (line ~118):
```astro
<Interlude eyebrow="Qu'est-ce que le Digital Arabic Maqām Archive ?" expandable expandLabel="Développer" collapseLabel="Réduire">
```

MTS-ESP (line ~145):
```astro
<Interlude eyebrow="Qu'est-ce que MTS-ESP ?" expandable expandLabel="Développer" collapseLabel="Réduire">
```

Diwān (line ~197) — reword AND opt in:
```astro
<Interlude eyebrow="Qu'est-ce que le dīwān de la théorie du maqām arabe ?" expandable expandLabel="Développer" collapseLabel="Réduire">
```

Leave "À propos du nom" glyph interlude (line ~292) untouched.

- [ ] **Step 4: Build**

Run: `cd website && npx astro build`
Expected: build completes, no errors.

- [ ] **Step 5: Dev-server verification (user confirms)**

Run the dev server. On each of `/`, `/ar`, `/fr`:
- The three explainers show the centered accent line, title, and exactly **2 lines** of body text with an ellipsis, plus the chevron button.
- The Diwān eyebrow now reads as a question in each language.
- The "On the name" glyph interlude is unchanged (still two-column, left-aligned, large glyph).
- RTL (`/ar`): text is centered; Arabic title has no letter-spacing.

**If the 2-line clamp does NOT show** (body hidden or full) → note it; the clamp CSS from Task 1 needs adjustment before proceeding. Do not continue to Task 3 until the collapsed preview renders correctly.

- [ ] **Step 6: Commit**

```bash
cd website
git add src/pages/index.astro src/pages/ar/index.astro src/pages/fr/index.astro
git commit -m "website: make the 3 What-is-X interludes expandable; Diwān eyebrow as question

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

### Task 3: Toggle behaviour + smooth expand animation + graceful degradation

Wire the chevron to expand/collapse with a smooth CSS transition, gated on `body.js-ready`. No-JS and reduced-motion render fully expanded with the button hidden.

**Files:**
- Modify: `src/components/Interlude.astro`

**Interfaces:**
- Consumes: the `.interlude-toggle` / `#{bodyId}` / `.is-open` hooks from Task 1.
- Produces: final behaviour; nothing downstream.

- [ ] **Step 1: Add the collapse/expand animation CSS + JS-gate rules**

Append to the scoped `<style>` in `src/components/Interlude.astro`:

```css
  /* ── Expand/collapse animation ──────────────────────────────────────────
     Default (no JS): fully expanded, no clamp, button hidden — so text is
     never trapped. The .js-ready gate below re-enables the collapsed state. */
  .interlude-toggle { display: none; }

  :global(body.js-ready) .interlude-toggle { display: inline-flex; }

  /* With JS: the collapse wrapper uses a max-height transition. Collapsed =
     a 2-line max-height floor; open = a generous max-height. max-height is
     used (not grid 1fr) because it eases reliably from a fixed floor. */
  :global(body.js-ready) .interlude-collapse {
    max-height: calc(2em * 1.85 + 0.1em); /* ~2 lines at the body line-height */
    overflow: hidden;
    transition: max-height 0.35s ease;
  }

  :global(body.js-ready) .interlude-inner--expandable.is-open .interlude-collapse {
    max-height: 40rem; /* larger than any interlude body; eases open */
  }

  /* Release the line-clamp when open so the full prose shows. */
  :global(body.js-ready) .interlude-inner--expandable.is-open .interlude-collapse .interlude-body--centered {
    -webkit-line-clamp: unset;
    display: block;
    overflow: visible;
  }

  /* Chevron points up when open. */
  :global(body.js-ready) .interlude-inner--expandable.is-open .interlude-chevron {
    transform: rotate(270deg);
  }

  /* Reduced motion: no transition, and show everything expanded with no toggle. */
  @media (prefers-reduced-motion: reduce) {
    :global(body.js-ready) .interlude-collapse { transition: none; max-height: none; }
    :global(body.js-ready) .interlude-collapse .interlude-body--centered {
      -webkit-line-clamp: unset; display: block; overflow: visible;
    }
    :global(body.js-ready) .interlude-toggle { display: none; }
  }
```

Note the `max-height` floor and open value are tuned by eye in Step 4 — `2em * 1.85` matches the interlude body line-height (1.85); the open `40rem` must exceed the tallest interlude body (verify none clips).

- [ ] **Step 2: Add the component toggle script**

Add at the end of `src/components/Interlude.astro` (after `</style>`), a component `<script>` (Astro bundles and runs it once globally):

```astro
<script>
  // Expandable interludes: wire each toggle to its aria-controls target.
  // Runs only when JS is active; CSS above keeps content visible without it.
  document.addEventListener('DOMContentLoaded', () => {
    const toggles = document.querySelectorAll('.interlude-toggle');
    toggles.forEach((btn) => {
      btn.addEventListener('click', () => {
        const inner = btn.closest('.interlude-inner--expandable');
        if (!inner) return;
        const open = inner.classList.toggle('is-open');
        btn.setAttribute('aria-expanded', String(open));
        const expandLabel = btn.getAttribute('data-expand-label') || 'Expand';
        const collapseLabel = btn.getAttribute('data-collapse-label') || 'Collapse';
        btn.setAttribute('aria-label', open ? collapseLabel : expandLabel);
      });
    });
  });
</script>
```

- [ ] **Step 3: Build**

Run: `cd website && npx astro build`
Expected: build completes, no errors. The component script is bundled.

- [ ] **Step 4: Dev-server verification (user confirms)**

On `/`, `/ar`, `/fr`:
- Collapsed: 2 lines + ellipsis, chevron points down.
- Click chevron: body eases open smoothly to full text; chevron rotates to point up; `aria-label` swaps to the collapse label. No clipping of the full body (widen `max-height` open value if any interlude clips).
- Click again: eases closed back to 2 lines.
- RTL (`/ar`): animation + centering correct; chevron rotation reads sensibly.
- **No-JS check** (disable JS / view before hydration): full text visible, no button, nothing trapped.
- **Reduced-motion check** (OS setting): full text visible, no toggle, no animation.

If the `max-height` ease janks or an interlude clips, adjust the floor / open value and rebuild.

- [ ] **Step 5: Commit**

```bash
cd website
git add src/components/Interlude.astro
git commit -m "website: animate interlude expand/collapse; JS-gated, reduced-motion safe

Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>"
```

---

## Self-Review

**Spec coverage:**
- Centered + eyebrow-as-title → Task 1 (title markup + CSS), Task 2 (opt-in). ✓
- 2-line clamp + ellipsis → Task 1 CSS, verified Task 2 Step 5. ✓
- Chevron toggle, rotates → Task 1 (button + base rotation), Task 3 (open rotation). ✓
- Smooth animation → Task 3 (max-height transition). ✓
- Gold accent line replacing left rule → Task 1 CSS. ✓
- Scope: only 3 no-glyph explainers; glyph untouched → `isExpandable = expandable && !glyph` (Task 1), explicit "leave untouched" notes (Task 2). ✓
- All languages incl. RTL → Task 2 all three pages; RTL title rule (Task 1) + RTL verification (Tasks 2,3). ✓
- Tailwind-first / scoped CSS only for clamp/animation/chevron → honoured; note styling uses component CSS consistent with the existing Interlude `<style>` (the component is already CSS-in-`<style>`, not utility-classed, so matching it is the DRY choice; pages use utilities). ✓
- Diwān eyebrow reworded EN/FR/AR → Task 2 Steps 1–3. ✓
- Accessibility (button, aria-expanded/controls, clamp not display:none, localised aria-label) → Task 1 markup + Task 3 script. ✓
- Progressive enhancement (no-JS full text, button hidden) → Task 3 CSS gate + verification. ✓
- prefers-reduced-motion → Task 3 CSS + verification. ✓

**Placeholder scan:** No TBD/TODO. The "tune by eye" notes (title size, max-height values) are concrete starting values with a named adjustment step in verification, not placeholders.

**Type consistency:** Prop names `expandable`/`expandLabel`/`collapseLabel`, class hooks `.interlude-inner--expandable`/`.interlude-toggle`/`.interlude-collapse`/`.is-open`/`.interlude-body--centered`, and `bodyId`/`aria-controls` are used identically across Tasks 1–3. ✓

**Note on the Tailwind constraint (decided):** `Interlude.astro` renders a rich HTML `<slot>`, and Astro slots cannot receive Tailwind utility classes — slotted prose (paragraphs, links, `em`, inline Arabic terms) must be styled with `:global()` CSS regardless (verified against Astro's styling docs). Given a slot-based component can never be "full Tailwind," the decision is to **keep the component fully scoped-CSS** (its existing convention) and add the new expandable styles the same way. The page-level opt-in (`expandable`, eyebrow copy) stays declarative. The user's "use Tailwind" applies to page-level/new-markup work, which the page files already follow.
