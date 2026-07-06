# Docs FAQ Page — Design

**Date:** 2026-07-06
**Status:** Approved, ready for implementation

## Goal

Add a single FAQ page to the Starlight docs, covering a mix of conceptual and
practical questions. It sits in its own "FAQ" sidebar section, placed right after
"Getting Started". English-only for now; AR/FR body translation is deferred.

## Scope

**In scope:**
- One new Markdown page: `src/content/docs/docs/faq.md` (English body).
- One new sidebar section "FAQ" inserted between "Getting Started" and "The
  Interface" in `astro.config.mjs`, with AR/FR translations for the section label
  and the page label (nav stays consistent even though the body is EN-only).

**Out of scope (YAGNI):**
- Accordion / collapsible `<details>` UI — questions are plain `##` headings.
- AR/FR translation of the page *body* (deferred to a later pass).
- Per-question feedback widgets, search-specific tuning.

## Content & format

- Standard Starlight Markdown with `title: FAQ` and a one-line `description`.
- Each question is an `##` heading. Starlight auto-anchors headings and lists them
  in the right-hand "On this page" ToC. Matches the existing Troubleshooting page.
- Answers are short and link out to the detailed doc page where one exists (e.g.
  installation, receiver, Ableton), rather than duplicating that content. The FAQ
  is for cross-cutting questions that lack a natural home elsewhere.
- Mixed conceptual + practical. Draft question set (~12–15), loosely ordered:
  - **About the plugin:** What is Tanghīm? · Is it free / open source? · What are
    the two plugins (Transmitter vs Receiver)?
  - **About maqām & tuning:** What is a maqām? · What's a quarter-tone / why not
    just 12-EDO? · What is tanghīm vs "tasyīk / tashrīq"?
  - **Data:** Where does the tuning data come from (DiArMaqAr)? · Do I need an
    internet connection?
  - **Getting sound out:** What is MTS-ESP and why use it? · My synth isn't
    MTS-ESP — what do I do? (→ Receiver)
  - **Practical:** How do I use it in Ableton Live? (→ Ableton) · Which plugin
    format should I install (VST3 / AU / CLAP)? · Can I switch maqām while
    playing? · Can I edit a tuning by ear?
  - Final wording/answers are drafted from the existing docs, the landing-page
    copy, and project knowledge, then reviewed by the user before finalising.

## Sidebar placement

Insert a new section object in the `sidebar` array in `astro.config.mjs`,
**between** the "Getting Started" block and the "The Interface" block:

```js
{
  label: 'FAQ',
  translations: { ar: 'الأسئلة الشائعة', fr: 'FAQ' },
  items: [
    { slug: 'docs/faq', translations: { ar: 'الأسئلة الشائعة', fr: 'Foire aux questions' } },
  ],
},
```

(Section label and page label translated so the AR/FR sidebars read naturally.)

## AR/FR behaviour (EN-only body)

The page body ships in English only. Under the AR and FR docs, Starlight will show
the **English FAQ page as a fallback** when the user follows the FAQ link (its
standard behaviour for an untranslated page under a locale root). This is accepted
for this pass. When ready, translation means adding
`src/content/docs/ar/docs/faq.md` and `src/content/docs/fr/docs/faq.md` — no config
change needed, Starlight resolves them by path.

## Verification

- `npx astro build` compiles with no errors (all 67+ pages).
- `/docs/faq` renders with each question as an anchored `##` heading and a working
  "On this page" ToC.
- The "FAQ" section appears in the sidebar **after Getting Started** in all three
  locale sidebars, with the label translated; following it under ar/fr shows the
  EN page as fallback.
- No em dashes in the FAQ prose (per the site-wide convention just established) —
  use commas / parentheses / colons.
