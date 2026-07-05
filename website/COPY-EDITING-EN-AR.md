# Music-Technology Copy Editing & Translation — English → Arabic

A general style guide for **writing and translating music-technology copy in Arabic**
(websites, app UI, documentation, marketing). It covers voice, register, the gender
problem, how to handle technical terms, a verification workflow, RTL/typography, and a
reusable glossary.

It is **product-agnostic**. Use it for any maqām/tuning/DAW/plugin/synth material, whether
you are translating from an English source or writing fresh Arabic.

---

## 1. The one-line brief

> **Accurate, but accessible.** Direct, clear, explanatory, and educational — so a non-expert
> feels at ease reading it and a beginner learns the concepts from the text itself.

Everything below serves that sentence.

---

## 2. Voice & style principles

1. **Accurate but accessible.** Be technically correct without sounding like a spec sheet.
   If a choice trades a little precision for a lot of clarity, take it — but never at the
   cost of being *wrong*.

2. **Direct and plain.** Short sentences. Active constructions. Cut formal connective filler
   (`ومن ثمّ`, `علاوةً على ذلك`, `يتمثّل في`…). Everyday words the practitioners actually use,
   not classical/literary vocabulary.

3. **Explanatory & educational.** Assume the reader is smart but new. Explain the *how* and
   the *why*, not only the *what*. A good sentence leaves the reader knowing one more thing
   about how music technology works.

4. **Concrete over abstract.** Prefer specific, tangible wording.
   - `نغمة لونية` (a chromatic *note*) over `خطوة لونية` (a chromatic *step*).
   - `تحفظ نسخة من البيانات على جهازك` (saves a copy on your device) over `تخزّن البيانات محلياً`
     (caches locally).

5. **Functional framing.** Say what a control *lets you do*, not just what it is.
   - `يسمح كل مزلاق باستكشاف الأبعاد المختلفة` (each slider lets you explore the different
     intervals) over `يتضمّن كل مزلاق نقاط انجذاب` (each slider contains snap points).

6. **Warm and reader-facing.** Address the reader (`يتيح لكم…`, `لتروا…`) — subject to the
   gender rules in §4.

7. **Light on tashkīl (ـَ ـُ ـِ ـّ).** Modern web Arabic runs mostly unvocalized. Add a vowel
   mark only where it resolves a genuine reading ambiguity. Do not decorate the text with
   full case endings.

8. **Teach through glossing.** Introduce a term with a bracketed gloss so the reader learns
   the vocabulary as they read — see §5.

### A worked example (formal → clear)

> **Before:** `يستعلم البرنامج من واجهة برمجة التطبيقات الخاصة به ويقوم بتخزين البيانات محلياً،
> مما يضمن تطابق القيم المعزوفة مع تلك الموثّقة في المصدر.`
>
> **After:** `يتّصل البرنامج بواجهة الأرشيف البرمجية (REST API) ويحفظ نسخة من بياناته على جهازك،
> لتكون النغمات المعزوفة مطابقة تماماً لِما في الأرشيف.`

Same facts; the second version is shorter, concrete (`نسخة على جهازك`), teaches (`REST API`
glossed), and reads like a person explaining, not a manual.

### A worked example (faithful translation → native prose)

This one is subtler: both versions are *correct*, but only the second reads like a native
writer. The source is the “On the name” gloss — an etymological aside explaining that
**Tanghīm** is a maṣdar meaning *intonation* (the speech sense) and, here, musical tuning
(as distinct from *dūzān*, mechanical string tuning).

> **Faithful (mirrors the English structure):** `تنغيم مصدرٌ عربي له معنيان: في الكلام، هو تنغيم
> الصوت — أي المنحنى اللحني الذي ترتفع به النبرة وتنخفض أثناء النطق (وهو ما يُعرف في علم اللغة بـintonation).
> وفي الموسيقى، هو ضبط العلاقات بين درجات الصوت. وهذا المعنى الموسيقي هو المقصود هنا: العلاقات النغمية التي
> يتكوّن منها النظام…`
>
> **Native (leaner, music-first, in-register):** `تنغيم مصدر عربي يدل على فعل ضبط النغمات الموسيقية
> وعلاقاتها، وفي الدراسات اللغوية يعني المنحنى اللحني للكلام (intonation). يشير الاسم هنا إلى ضبط النغمات
> التي يتكوّن منها الديوان الموسيقي، تمييزاً عن «الدوزان» الذي يُستعمل لضبط أوتار الآلة ميكانيكياً…`

Four things separate them:

1. **Lead with the relevant sense, not the English order.** The product is *musical*, so the
   musical meaning goes first (`ضبط النغمات الموسيقية`) and the linguistic sense is background
   colour. Faithfully mirroring the English (speech-sense first, because English anchors the
   exotic Arabic word to the familiar “intonation”) inverts the emphasis for an Arabic reader.

2. **Don’t over-explain a word the reader already owns.** The faithful version glosses
   *intonation* with a full mini-lecture — `المنحنى اللحني الذي ترتفع به النبرة وتنخفض أثناء النطق`.
   That “explain it thoroughly” instinct is right for a *foreign* concept (§1) but patronises for
   a *native* one. The native version trusts the reader: `المنحنى اللحني للكلام (intonation)` — done.

3. **Announce nothing; let the sentence carry the structure.** `له معنيان: …` is textbook
   scaffolding. Native prose flows the two senses in one clause without meta-labelling them.

4. **Reach for the domain’s own word.** `الديوان الموسيقي` (the register/octave — a maqām-theory
   term the reader is learning elsewhere on the page) is more concrete *and* more in-register than
   the generic `النظام`. Concrete-over-abstract (§2.4) includes preferring the field’s vocabulary.

The meta-lesson: **when the English source over-explains, a faithful translation carries the
over-explaining across.** Economy is not lost precision — for a native reader, the leaner version
is the *clearer* one. Translate the meaning and the register, not the sentence structure.

---

## 3. Register & vocabulary

- Choose the **common practitioner term** over the formal/encyclopedic one when they differ,
  but *know both* (see the glossary's Notes column). Example: a synth is `سنثسيزر` in practice;
  the encyclopedic term is `مُركِّب`.
- Avoid ornate synonyms. One clear word beats an elegant-but-obscure one.
- Keep a term **consistent** across a whole project. Pick one rendering per concept and reuse
  it — do not vary `مزلاق`/`شريحة` or `تصوير`/`تبديل` from paragraph to paragraph.
- Prefer verbs and verbal nouns that carry meaning plainly: `يضبط`, `يعزف`, `يحفظ`, `يوصّل`.

---

## 4. Gender-neutral address — the key Arabic decision

Arabic marks gender on verbs and pronouns, so “neutral” needs a deliberate strategy. The rule:

> **Only text that is *addressed to the reader* needs to be gender-neutral** (so it does not
> assume the reader is male or female). **Descriptive/expository text uses natural grammatical
> gender.**

### 4.1 Descriptive text → natural gender

When you describe a thing, agree with its grammatical gender.
- A product referred to by a **feminine noun** (e.g. it *is* an `إضافة`) takes feminine
  agreement: `… هي إضافة …`, `تتيح`, `تجلب`, `بياناتها`, `عرضها`.
- A masculine subject stays masculine: `يتيح المذبذب…`, `يعيد الوضع السباعي…`.
- Do **not** force nominalization/passive on descriptive sentences just to dodge gender —
  that is what made earlier drafts read stiff.

### 4.2 Reader-addressed text → gender-neutral, in this order of preference

1. **Plural (masculine) imperative / address** — the conventional inclusive form, and the most
   natural and energetic:
   `اضبطوا`, `اختاروا`, `اعزفوا`, `التقطوا`, and `يتيح لكم…`, `لتروا…`.
2. **Verbal noun (maṣdar)** — neutral but flatter; good for headings and captions:
   `ضبط النغمة المرجعية`, `اختيار المقام`.
3. **Passive / impersonal** — `تُضبط`, `يُمكن ضبط…`; use when 1 and 2 don't fit.

### 4.3 What is already neutral (keep it)

- **The possessive `ـك`** is written the same for masculine and feminine when unvocalized
  (`مقامك`, `ذوقك`, `جهازك`, `بأصواتك`). It is inherently neutral — keep it; you don't need to
  pluralize every possessive.

### 4.4 What to avoid

- The **singular imperative** and **singular 2nd-person verb**, which are gendered in spelling
  (masc `اضبط` vs fem `اضبطي` — the feminine adds ‎ي‎). If you're talking *to* the reader, don't
  use them; use §4.2.
- `أنتَ/أنتِ` distinctions — sidestep with the plural or by rephrasing.

> **Note:** the masculine plural is the *conventional* inclusive default in Modern Standard
> Arabic. If a project adopts a stricter inclusive policy, prefer maṣdar/passive (4.2 items 2–3)
> to avoid a masculine default entirely; decide this once, per project.

---

## 5. Technical terms & the glossing convention

### 5.1 Keep in Latin script (don't translate)

Established acronyms, protocol/format names, and message names stay in English:

> **MIDI · MPE · MTS-ESP · CLAP · VST3 · AU · Max for Live · Program Change · Pitch Bend ·
> REST API · IPN · PAO**, and abbreviations like **DAW**, **Osc**, **Hept**, **Mono PB**.

And **do not translate the expansion of an English acronym** — keep the whole thing English:
`MPE (MIDI Polyphonic Expression)`, not `MPE (تعبير MIDI متعدد الأصوات)`.

### 5.2 Translatable concepts → Arabic term + bracketed gloss

For a concept that *does* have a good Arabic term, write the **Arabic term and gloss the
English (or transliteration) in brackets** on first or key use. This is the educational move —
the reader learns the pairing:

- `محطة عمل الصوتيات الرقمية (Digital Audio Workstation)`
- `إضافة توسيعية (بلاجِن)`
- `نغمة لونية (كروماتيكية)`
- `واجهة الأرشيف البرمجية (REST API)`

Gloss once where it teaches; don't repeat the bracket every time — it clutters. After the first
mention, use the Arabic term (or a well-known abbreviation) alone.

### 5.3 Practitioner term vs formal term

When the everyday term differs from the encyclopedic one, prefer the everyday term for
reader-facing copy and note the formal one. Example: **synthesizer** → `سنثسيزر` (practice) /
`مُركِّب` (formal). Pick per audience and stay consistent.

### 5.4 Watch for collisions

- **`تحميل`** means both *download* and *load*. Reserve `تحميل` for **download**; use
  `فتح` / `إضافة` / `استرجاع` for loading a file, preset, or device — otherwise the reader
  can't tell which you mean.
- **`درجة`** does heavy duty (a maqām *degree*, and `درجة الصوت` = *pitch*). Keep the context
  clear.
- **`طبقة الصوت`** is *vocal register* (soprano/alto…), **not** a note's *pitch*. For pitch use
  `درجة الصوت` / `حِدّة الصوت`.

---

## 6. Verification workflow

Never guess a technical term. Before committing a translatable term:

1. **Check Arabic Wikipedia** (the default authority). Search the English term; the article
   title / interlanguage link gives the standard Arabic term. (For maqām theory, also cross-check
   a maqām-theory source, since practice sometimes differs from the encyclopedic entry.)
2. **Prefer the practitioner term** where it diverges, but record the formal one in the glossary
   Notes.
3. **Write the decision down** in the glossary so the whole project stays consistent and the next
   translator doesn't re-litigate it.
4. If **no standard Arabic term exists** (e.g. *pitch bend*), use a clear descriptive Arabic
   phrase plus the English in brackets: `ثني النغمة (Pitch Bend)`.

---

## 7. RTL & typography

- **Never letter-space Arabic.** Positive tracking breaks the cursive joins and can render text
  unreadable. (In CSS, neutralize any `letter-spacing`/`tracking` utility on Arabic elements and
  on `[dir="rtl"]`.)
- **Size Arabic up.** At the same point size Arabic reads optically smaller than Latin; bump
  Arabic body and headings a notch relative to the Latin sizes.
- **Latin/technical terms inside Arabic** are fine — the Unicode bidi algorithm lays them out
  left-to-right within the right-to-left line, including the parentheses around a gloss.
- **Use logical CSS** (`border-inline-start`, `padding-inline-start`, `text-align: start/end`,
  `order`) so layouts mirror correctly under `dir="rtl"`.
- **Arabic display vs body faces**: pair a display face for headings/wordmarks with a readable
  body face for running text; don't set long Arabic paragraphs in a display face.

---

## 8. Numerals, punctuation & orthography

- **Numerals:** Western/“Arabic” numerals (0–9) are standard and clearest for technical values —
  `128`, `440 Hz`, `4×2`, `2026`. Spell out small counts where it reads better (`ثمانية`,
  `اثنا عشر`).
- **Punctuation:** Arabic comma `،` and Arabic question mark `؟`. In RTL, use the arrow that
  points the reading direction for “next/more” (`←`).
- **Orthography — common fixes:**
  - `أو` (not `او`), `إلى` (not `الى`).
  - Form-VII/VIII verbal nouns take *hamzat al-waṣl* — a bare alif: `الانجذاب` (not `الإنجذاب`),
    `الاستقبال`, `الاستماع`.
  - Verb government: `يسمح بـ…` (with the ب), but `يتيح …` takes a direct object.
  - `تنوين` and hamza spelling matter in headings where they're most visible.

---

## 9. Brand & proper names

- **Product/brand names in Arabic pages:** where a brand is rendered in Arabic script, give it a
  **distinct visual style** (e.g. a display face + accent colour) so *title-use* of the word is
  visibly different from the *common word*. Keep a Latin logo/wordmark as a deliberate choice if
  the brand's identity is Latin.
- **Institution & archive names:** translate to the established Arabic form and keep it consistent
  (see glossary). Provide the English/acronym once in brackets if the audience may look it up.

---

## 10. Glossary (EN → AR)

Reusable base glossary. **Notes** flag the formal-vs-common split, collisions, and sourcing.
Extend it per project; keep one rendering per concept.

### 10.1 Music theory & maqām

| English | Arabic | Notes |
|---|---|---|
| maqām / maqāmāt | مقام / مقامات | |
| Arabic maqām (the tradition) | المقام العربي | Singular/collective. |
| the Arabic maqāmāt (plural sense) | المقامات العربية | Use the plural when you mean *the many maqāmāt*, not the tradition. |
| tuning / intonation | التنغيم | The act/system of setting pitch relationships. |
| tuning system(s) | نظام / أنظمة التنغيم | |
| a tuning / tunings | تنغيم / **تناغيم** | Plural of `تنغيم` is `تناغيم`. |
| to tune / retune | يضبط / يُعاد تنغيمه | `ضبط` for the act; passive `يُعاد تنغيمه` when neutral. |
| transposition | **تصوير** | Maqām-theory term. *Not* `تبديل` (that's *switching*). |
| degree (of a maqām) | درجة / درجات | |
| tonic (of a maqām) | **درجة استقرار المقام** | The finalis/resting degree. |
| pitch | درجة الصوت / حِدّة الصوت | *Not* `طبقة الصوت` (= vocal register). |
| reference pitch | النغمة المرجعية | |
| reference frequency | التردد المرجعي | |
| cents | سنت | |
| semitone | **نصف بعد** | Per Wikipedia; not `نصف نغمة`. |
| interval(s) | بُعد / أبعاد | |
| chromatic (step/note) | لوني — نغمة لونية | Gloss `(كروماتيكية)` on first use. `لوني` is the Wikipedia term (not `كروماتي`). |
| diatonic | دياتوني | e.g. `المفاتيح البيضاء (الدياتونية)`. |
| octave | أوكتاف / الجواب / الديوان | All valid; `الجواب`/`الديوان` are the maqām-theory terms. |
| dīwān / dawāwīn (octave registers) | ديوان / ديوانين / دواوين | The lower/upper octave registers. |
| heptatonic (mode) | سباعي — الوضع السباعي | |
| white / black keys | المفاتيح البيضاء / السوداء | |
| solfège | السولفيج | |
| note | نوتة / نغمة | `نوتة` (the symbol/MIDI note); `نغمة` (the tone). |
| scale | سلّم | |
| modulation (between maqāmāt) | التلوين / الانتقال | |

### 10.2 Audio & music technology

| English | Arabic | Notes |
|---|---|---|
| DAW (Digital Audio Workstation) | محطة عمل الصوتيات الرقمية | Gloss `(Digital Audio Workstation)`; `DAW` fine as the short form after. |
| plugin | إضافة / إضافة توسيعية (بلاجِن) | Common: `إضافة`. Formal: `برنامج مساعد`, `مكوّن إضافي`, `مِلحَق`. `بلاجِن` = spoken. Pick one per project. |
| synthesizer | سنثسيزر / سنثسيزرات | Practitioner term. Formal/Wikipedia: `مُركِّب`, `مولّد الأصوات`. |
| software synth | سنثسيزر مبرمج / السنثسيزرات المبرمجة | |
| hardware synth | سنثسيزر صلب / السنثسيزرات الصلبة | |
| software | برمجيات / برمجي | |
| hardware | العتاد / العتاد الصلب | |
| oscillator | مذبذب | |
| triangle / sine / square wave | موجة مثلثية / جيبية / مربّعة | |
| MIDI controller | وحدة التحكم | `وحدة تحكم MIDI` when disambiguation helps. |
| automation | أتمتة | |
| parameter | معامِل / معاملات | |
| preset(s) | إعداد مسبق / إعدادات مسبقة | |
| MIDI note | نوتة MIDI | Keep `MIDI` Latin. |
| pitch bend | ثني النغمة (Pitch Bend) | No standard Arabic entry → descriptive + English. |
| shared memory | الذاكرة المشتركة | |
| protocol | بروتوكول | |
| API / REST API | واجهة برمجية / واجهة … البرمجية (REST API) | Keep `REST API` Latin in brackets. |
| timeline | المسار الزمني / الخطّ الزمني | |
| arpeggiator | مولّد الأربيجيو / مولّدات الأربيجيو | |
| MIDI processor / effect | معالِج MIDI / مؤثّر MIDI | |
| transmitter / receiver | المُرسِل / المُستقبِل | Or `إضافة الإرسال / الاستقبال`. |
| offset | إزاحة | |
| override | تجاوز / تجاوزات | |

### 10.3 UI & actions

| English | Arabic | Notes |
|---|---|---|
| slider | مزلاق / مزالق | |
| knob | مقبض | |
| snap (magnetic) | انجذاب (مغناطيسي) | `نقاط الانجذاب` = snap points (spelling: `الانجذاب`). |
| download | تحميل | Reserve `تحميل` for download only. |
| load (file/preset/device) | فتح / استرجاع / إضافة | Avoid `تحميل` here (collision with *download*). |
| save | حفظ | |
| update | تحديث | |
| open source | مفتوح المصدر | |
| free (gratis) | مجاني | |

### 10.4 Institutions & proper names (example set)

| English | Arabic |
|---|---|
| Music Intelligence Lab | مختبر الذكاء الموسيقي |
| Center for Advanced Mathematical Sciences | مركز العلوم الرياضية المتقدمة |
| American University of Beirut | الجامعة الأمريكية في بيروت |
| Digital Arabic Maqām Archive (DiArMaqAr) | أرشيف المقام العربي الرقمي (DiArMaqAr) |

---

## 11. Quick checklist

**Do**
- Write short, direct, concrete sentences that teach.
- Address the reader with the **plural** (`اضبطوا`, `يتيح لكم`) when giving instructions.
- Use natural grammatical gender for descriptions.
- Gloss a translatable term once with `(English/transliteration)`.
- Verify every technical term against Arabic Wikipedia; keep the glossary consistent.
- Keep possessive `ـك` (it's neutral).

**Don't**
- Force passive/nominal on descriptive text just to avoid gender.
- Use the singular imperative when addressing the reader (it's gendered).
- Translate an acronym's English expansion.
- Letter-space Arabic, or set long paragraphs in a display face.
- Reuse `تحميل` for both *download* and *load*.
- Vary terminology for the same concept within a project.
