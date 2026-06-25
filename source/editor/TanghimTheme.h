#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

/**
 * Shared theme colours and look-and-feel for the Tanghim native UI.
 * Matches the React CSS variables exactly.
 */
namespace Theme
{
    // ── Base colours (from index.css CSS variables) ────────────────────────
    static const juce::Colour bg         { 0xff0f0f1a };  // --bg
    static const juce::Colour surface    { 0xff1a1a2e };  // --surface
    static const juce::Colour surface2   { 0xff16213e };  // --surface2
    static const juce::Colour accent     { 0xffe94560 };  // --accent (red, tuning system)
    static const juce::Colour text       { 0xffe0e0e0 };  // --text
    static const juce::Colour textMuted  { 0xff888888 };  // --text-muted
    static const juce::Colour border     { 0xff2a2a4a };  // --border
    static const juce::Colour sliderTrack{ 0xff2a2a4a };  // --slider-track
    static const juce::Colour sliderThumb{ 0xffe94560 };  // --slider-thumb (red)
    static const juce::Colour accent2    { 0xff0f3460 };  // --accent2 (assigned preset border)

    // ── Semantic colours ───────────────────────────────────────────────────
    // Maqam context (gold)
    static const juce::Colour gold       { 0xffffc947 };  // degree highlight, snap markers
    static const juce::Colour goldMaqam  { 0xffd4a843 };  // maqam selector border/text
    static const juce::Colour goldPreset { 0xffe8b339 };  // preset/MIDI config

    // Modification indicators
    static const juce::Colour cyan       { 0xff26c6da };  // modified maqam degree
    static const juce::Colour teal       { 0xff00b0a0 };  // modified non-degree
    static const juce::Colour violet     { 0xffb07aff };  // per-note cents override
    static const juce::Colour blue       { 0xff50b4ff };  // per-note variant override

    // Status badges
    static const juce::Colour mtsEsp     { 0xff26a69a };  // MTS-ESP native (teal)
    static const juce::Colour oscAmber   { 0xffffa726 };  // oscillator badge
    static const juce::Colour heptMagenta{ 0xffd050e0 };  // heptatonic badge
    static const juce::Colour mpeBadge   { 0xff64b5f6 };  // MPE badge (blue)
    static const juce::Colour monoPbBadge{ 0xff66bb6a };  // Mono PB badge (green)
    static const juce::Colour connected  { 0xff81c784 };  // connection status (green)
    static const juce::Colour tuningText { 0xffb0b0cc };  // tuning info text (Receiver)

    // Piano key indicators
    static const juce::Colour whiteKey   { 0x99ffffff };  // rgba(255,255,255,0.6)
    static const juce::Colour blackKey   { 0x26ffffff };  // rgba(255,255,255,0.15)

    // ── Layout constants ───────────────────────────────────────────────────
    static constexpr int kSlotWidthPx       = 68;
    static constexpr int kBankLeftOffsetPx  = 16;
    static constexpr int kStatusBarHeight   = 26;
    static constexpr int kMinWidth          = 832;
    static constexpr int kMinHeight         = 620;
    static constexpr int kMaxWidth          = 2400;
    static constexpr int kMaxHeight         = 4000;

    // ── Font scale ─────────────────────────────────────────────────────────
    // JUCE font points render slightly smaller than CSS px on macOS Retina.
    // Apply a uniform scale to match WebView visual size.
    static constexpr float kFontScale = 1.15f;

    /** Scaled font for UI elements (matches WebView visual size). */
    static inline juce::Font scaledFont (float cssPx, int style = juce::Font::plain)
    {
        return juce::FontOptions (cssPx * kFontScale, style);
    }

    // ── Slider constants ───────────────────────────────────────────────────
    static constexpr double kCentsRange     = 150.0;  // ±150 cents
    static constexpr double kTrackHalfPct   = 45.0;   // 45% of track on each side of center
    static constexpr int kTrackWidth        = 24;
    static constexpr int kThumbDiameter     = 20;
    static constexpr int kSnapMarkerSize    = 7;

    // ── Helper functions ───────────────────────────────────────────────────

    /** Convert cents deviation to track percentage (0% = top, 100% = bottom).
     *  0 cents = 50% (center). Positive = above center (lower %). */
    static inline double centsToTrackPct (double cents)
    {
        double pct = 50.0 - (cents / kCentsRange) * kTrackHalfPct;
        return juce::jlimit (5.0, 95.0, pct);
    }

    /** Inverse: track percentage to cents deviation. */
    static inline double trackPctToCents (double pct)
    {
        return -(pct - 50.0) * kCentsRange / kTrackHalfPct;
    }
}

//==============================================================================
/** A Label whose text the user can copy by clicking it.
    Briefly displays "Copied!" feedback before reverting. */
class CopyableLabel : public juce::Label,
                      private juce::Timer
{
public:
    CopyableLabel()
    {
        setInterceptsMouseClicks (true, false);
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void setSourceText (const juce::String& newText)
    {
        sourceText = newText;
        if (! showingFeedback)
            setText (sourceText, juce::dontSendNotification);
    }

private:
    void mouseUp (const juce::MouseEvent&) override
    {
        if (sourceText.isEmpty())
            return;
        juce::SystemClipboard::copyTextToClipboard (sourceText);
        showingFeedback = true;
        setText ("Copied!", juce::dontSendNotification);
        startTimer (900);
    }

    void timerCallback() override
    {
        stopTimer();
        showingFeedback = false;
        setText (sourceText, juce::dontSendNotification);
    }

    juce::String sourceText;
    bool showingFeedback = false;
};

//==============================================================================
/** Custom LookAndFeel for the Tanghim transmitter UI. */
class TanghimLookAndFeel : public juce::LookAndFeel_V4
{
public:
    TanghimLookAndFeel()
    {
        // Window
        setColour (juce::ResizableWindow::backgroundColourId, Theme::bg);

        // Labels
        setColour (juce::Label::textColourId, Theme::text);

        // ComboBox
        setColour (juce::ComboBox::backgroundColourId,    Theme::surface);
        setColour (juce::ComboBox::textColourId,           Theme::text);
        setColour (juce::ComboBox::outlineColourId,        Theme::border);
        setColour (juce::ComboBox::arrowColourId,          Theme::textMuted);
        setColour (juce::ComboBox::focusedOutlineColourId, Theme::accent);

        // PopupMenu
        setColour (juce::PopupMenu::backgroundColourId,            Theme::surface);
        setColour (juce::PopupMenu::textColourId,                  Theme::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, Theme::accent);
        setColour (juce::PopupMenu::highlightedTextColourId,       juce::Colours::white);

        // Buttons
        setColour (juce::TextButton::buttonColourId,  Theme::surface2);
        setColour (juce::TextButton::buttonOnColourId, Theme::accent);
        setColour (juce::TextButton::textColourOffId,  Theme::text);
        setColour (juce::TextButton::textColourOnId,   juce::Colours::white);

        // TextEditor
        setColour (juce::TextEditor::backgroundColourId,    Theme::surface);
        setColour (juce::TextEditor::textColourId,           Theme::text);
        setColour (juce::TextEditor::outlineColourId,        Theme::border);
        setColour (juce::TextEditor::focusedOutlineColourId, Theme::accent);
        setColour (juce::TextEditor::highlightColourId,      Theme::accent.withAlpha (0.4f));

        // Caret
        setColour (juce::CaretComponent::caretColourId, Theme::accent);

        // Slider
        setColour (juce::Slider::backgroundColourId,       Theme::sliderTrack);
        setColour (juce::Slider::thumbColourId,             Theme::sliderThumb);
        setColour (juce::Slider::trackColourId,             Theme::sliderTrack);
    }

    juce::Font getPopupMenuFont() override
    {
        return juce::FontOptions (12.0f);
    }

    void drawPopupMenuBackground (juce::Graphics& g, int width, int height) override
    {
        g.setColour (findColour (juce::PopupMenu::backgroundColourId));
        g.fillRoundedRectangle (0.0f, 0.0f, (float) width, (float) height, 4.0f);
        g.setColour (Theme::border.brighter (0.3f));
        g.drawRoundedRectangle (0.5f, 0.5f, (float) width - 1.0f, (float) height - 1.0f, 4.0f, 1.0f);
    }

    int getPopupMenuBorderSize() override { return 4; }
};
