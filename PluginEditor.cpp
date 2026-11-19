#include "PluginEditor.h"

using namespace juce;

namespace ui
{
    Font font (float size, bool bold) { return Font (FontOptions ("Tahoma", size, bold ? Font::bold : Font::plain)); }
    String t (const char* utf8) { return String::fromUTF8 (utf8); }

    void sunken (Graphics& g, Rectangle<float> r)
    {
        g.setColour (well);
        g.fillRect (r);
        g.setColour (Colours::black);                       // dark top/left
        g.fillRect (r.getX(), r.getY(), r.getWidth(), 2.f);
        g.fillRect (r.getX(), r.getY(), 2.f, r.getHeight());
        g.setColour (goldDeep);                             // lit bottom/right
        g.fillRect (r.getX(), r.getBottom() - 1.f, r.getWidth(), 1.f);
        g.fillRect (r.getRight() - 1.f, r.getY(), 1.f, r.getHeight());
    }
}

static constexpr int kWidth = 560, kBaseHeight = 344, kWaveHeight = 114;
static constexpr int kTitleH = 46, kStatusH = 26;

//==============================================================================
// XP trackbar
int APLookAndFeel::getSliderThumbRadius (Slider& s) { return s.getProperties()["big"] ? 8 : 7; }

void APLookAndFeel::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float sliderPos, float, float,
                                      Slider::SliderStyle, Slider& s)
{
    const bool big = s.getProperties()["big"];
    const float cy = (float) y + (float) h * 0.5f - (big ? 3.f : 2.f);
    const float th = big ? 8.f : 6.f;
    const float r = (float) getSliderThumbRadius (s);

    auto track = Rectangle<float> ((float) x, cy - th * 0.5f, (float) w, th);
    ui::sunken (g, track);
    auto filled = track.reduced (2.f, 2.f).withRight (sliderPos);
    g.setGradientFill (ColourGradient (ui::goldLight, 0.f, filled.getY(), ui::goldDark, 0.f, filled.getBottom(), false));
    g.fillRect (filled);

    // thumb: rectangle with a pointer at the bottom
    const float tw = r * 2.f, tH = big ? 28.f : 22.f, tip = 6.f;
    const float tx = sliderPos - r, ty = cy - (tH - tip) * 0.5f - 2.f;
    Path p;
    p.startNewSubPath (tx, ty);
    p.lineTo (tx + tw, ty);
    p.lineTo (tx + tw, ty + tH - tip);
    p.lineTo (sliderPos, ty + tH);
    p.lineTo (tx, ty + tH - tip);
    p.closeSubPath();
    g.setColour (Colours::black.withAlpha (0.5f));
    g.fillPath (p, AffineTransform::translation (1.5f, 1.5f));
    const bool hot = s.isMouseOverOrDragging();
    g.setGradientFill (ColourGradient (hot ? Colours::white : ui::goldLight, tx, ty, ui::goldDark, tx, ty + tH, false));
    g.fillPath (p);
    g.setColour (Colour (0xff2a1e00));
    g.strokePath (p, PathStrokeType (1.f));
    g.setColour (Colours::white.withAlpha (0.55f));
    g.drawLine (tx + 2.f, ty + 2.f, tx + tw - 2.f, ty + 2.f, 1.f);
    g.setColour (Colour (0xff2a1e00).withAlpha (0.5f));       // grip lines
    for (int i = -1; i <= 1; ++i)
        g.drawLine (sliderPos + (float) i * 3.f, ty + 6.f, sliderPos + (float) i * 3.f, ty + tH - tip - 4.f, 1.f);
}

//==============================================================================
XPButton::XPButton (const String& label, bool led) : Button (label), hasLed (led)
{
    setClickingTogglesState (true);
}

void XPButton::paintButton (Graphics& g, bool over, bool down)
{
    auto b = getLocalBounds().toFloat().reduced (0.5f);
    const bool on = getToggleState() || down;
    const float rad = 3.f;

    if (on)
        g.setGradientFill (ColourGradient (ui::goldDark, 0.f, b.getY(), ui::gold, 0.f, b.getBottom(), false));
    else
    {
        ColourGradient grad (Colour (0xff3e3e3e), 0.f, b.getY(), Colour (0xff0e0e0e), 0.f, b.getBottom(), false);
        grad.addColour (0.45, Colour (0xff222222));
        grad.addColour (0.5, Colour (0xff161616));
        g.setGradientFill (grad);
    }
    g.fillRoundedRectangle (b, rad);

    if (! on)   // glossy top highlight
    {
        g.setColour (Colours::white.withAlpha (0.07f));
        g.fillRoundedRectangle (b.reduced (2.f).withHeight (b.getHeight() * 0.42f), 2.f);
    }
    g.setColour (Colours::black);
    g.drawRoundedRectangle (b, rad, 1.f);
    g.setColour ((over || on) ? ui::gold : ui::goldDark.withAlpha (0.8f));
    g.drawRoundedRectangle (b.reduced (1.f), rad - 1.f, over && ! on ? 1.6f : 1.f);

    g.setFont (ui::font (14.5f, true));
    const auto label = getName();
    const float tw = GlyphArrangement::getStringWidth (g.getCurrentFont(), label);
    float x = b.getCentreX() - tw * 0.5f;
    if (hasLed)
    {
        x += 10.f;
        auto led = Rectangle<float> (10.f, 10.f).withCentre ({ x - 14.f, b.getCentreY() });
        if (getToggleState()) { g.setColour (ui::red.withAlpha (0.35f)); g.fillRect (led.expanded (3.f)); }
        g.setColour (getToggleState() ? ui::red : Colour (0xff4a1410));
        g.fillRect (led);
        g.setColour (Colours::black);
        g.drawRect (led, 1.f);
    }
    g.setColour (on ? Colours::black : ui::text);
    g.drawText (label, Rectangle<float> (x, b.getY(), tw + 4.f, b.getHeight()), Justification::centredLeft);
}

void XPCheckBox::paintButton (Graphics& g, bool over, bool)
{
    auto b = getLocalBounds().toFloat();
    auto box = Rectangle<float> (14.f, 14.f).withCentre ({ b.getX() + 8.f, b.getCentreY() });
    g.setColour (ui::well);
    g.fillRect (box);
    g.setColour (over ? ui::gold : ui::goldDark);
    g.drawRect (box, 1.f);
    if (getToggleState())
    {
        Path tick;
        tick.startNewSubPath (box.getX() + 3.f, box.getCentreY());
        tick.lineTo (box.getX() + 6.f, box.getBottom() - 3.5f);
        tick.lineTo (box.getRight() - 3.f, box.getY() + 3.5f);
        g.setColour (ui::gold);
        g.strokePath (tick, PathStrokeType (2.f));
    }
    g.setFont (ui::font (13.f));
    g.setColour (ui::text);
    g.drawText (getName(), b.withTrimmedLeft (22.f), Justification::centredLeft);
}

//==============================================================================
void WaveformView::push (const WaveBin& b)
{
    bins[(size_t) write] = b;
    write = (write + 1) % kBins;
}

void WaveformView::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    ui::sunken (g, b);
    auto area = b.reduced (8.f, 8.f);
    const float mid = area.getCentreY();
    g.setColour (ui::goldDeep.withAlpha (0.6f));
    g.drawHorizontalLine ((int) mid, area.getX(), area.getRight());

    const float colW = area.getWidth() / (float) kBins;
    auto toH = [&] (float v) {
        const float db = Decibels::gainToDecibels (v, -60.f);
        return jlimit (0.f, 1.f, (db + 60.f) / 60.f) * area.getHeight() * 0.5f;
    };
    for (int i = 0; i < kBins; ++i)
    {
        const auto& bin = bins[(size_t) ((write + i) % kBins)];
        const float x = area.getX() + (float) i * colW;
        const float hgt = toH (bin.peak);
        g.setColour (Colour (0xff6b6250));
        g.fillRect (x, mid - hgt, jmax (1.f, colW - 0.4f), hgt * 2.f);
        if (bin.repaired)
        {
            g.setColour (ui::gold.withAlpha (0.18f));
            g.fillRect (x - 2.f, area.getY(), colW + 4.f, area.getHeight());
            const float rh = jmax (3.f, toH (bin.removed));
            g.setColour (ui::gold);
            g.fillRect (x - 0.5f, mid - rh, jmax (2.f, colW + 1.f), rh * 2.f);
        }
    }
    g.setFont (ui::font (11.f));
    g.setColour (ui::textDim);
    g.drawText (ui::t ("altın çizgiler = silinen click'ler"), area.removeFromTop (14.f), Justification::topRight);
}

//==============================================================================
void HelpOverlay::paint (Graphics& g)
{
    g.fillAll (ui::body);
    auto t = getLocalBounds().toFloat().reduced (28.f, 18.f);
    struct Line { const char* head; const char* body; };
    const Line lines[] = {
        { "HASSASİYET",       "Bir sesin click sayılması için ne kadar keskin olması gerektiği. Click'ler gidene kadar artır; 's' ve 't' sesleri körelmeye başlarsa azalt." },
        { "FREKANS",          "Click'lerin arandığı bölge. Bunun altındaki her şey onarım sırasında olduğu gibi korunur. Daha boğuk ağız seslerini yakalamak için düşür." },
        { "ONARIMI GENİŞLET", "Her onarımı iki yönde de genişletir. Onarımdan sonra hafif bir 'tık' kalıyorsa kullan." },
        { "DİNLE",            "Sadece silinen sesi çalar. Kelime değil, sadece click duymalısın." },
    };
    for (const auto& l : lines)
    {
        g.setFont (ui::font (13.f, true)); g.setColour (ui::gold);
        g.drawText (ui::t (l.head), t.removeFromTop (18.f), Justification::centredLeft);
        g.setFont (ui::font (13.5f)); g.setColour (ui::text.withAlpha (0.9f));
        g.drawFittedText (ui::t (l.body), t.removeFromTop (36.f).toNearestInt(), Justification::topLeft, 2, 1.0f);
        t.removeFromTop (8.f);
    }
    g.setFont (ui::font (11.5f)); g.setColour (ui::textDim);
    g.drawText (ui::t ("Kapatmak için herhangi bir yere tıkla"), t.removeFromTop (18.f), Justification::centredLeft);
}

//==============================================================================
APDeClickEditor::APDeClickEditor (APDeClickProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      listenBtn (ui::t ("DİNLE"), true), bypassBtn (ui::t ("DEVRE DIŞI"), false),
      waveBox (ui::t ("Dalga formu")), helpBtn ("?", false)
{
    setLookAndFeel (&lnf);

    for (auto* s : { &sensitivity, &frequency, &widen })
    {
        s->setSliderStyle (Slider::LinearHorizontal);
        s->setTextBoxStyle (Slider::NoTextBox, true, 0, 0);
        s->setPaintingIsUnclipped (true);
        addAndMakeVisible (*s);
    }
    sensitivity.getProperties().set ("big", true);
    sensitivity.setDoubleClickReturnValue (true, 5.0);
    frequency.setDoubleClickReturnValue (true, 3000.0);
    widen.setDoubleClickReturnValue (true, 0.0);

    sensA  = std::make_unique<SA> (proc.apvts, "sensitivity", sensitivity);
    freqA  = std::make_unique<SA> (proc.apvts, "frequency", frequency);
    widenA = std::make_unique<SA> (proc.apvts, "widen", widen);

    addAndMakeVisible (listenBtn);
    addAndMakeVisible (bypassBtn);
    listenA = std::make_unique<BA> (proc.apvts, "listen", listenBtn);
    bypassA = std::make_unique<BA> (proc.apvts, "bypass", bypassBtn);

    addAndMakeVisible (waveBox);
    waveBox.onClick = [this] { updateSize(); };

    addAndMakeVisible (helpBtn);
    addChildComponent (wave);
    addChildComponent (help);
    helpBtn.onClick = [this] { help.setVisible (helpBtn.getToggleState()); help.toFront (false); };
    help.onClose = [this] { helpBtn.setToggleState (false, sendNotification); };

    setSize (kWidth, kBaseHeight);
    startTimerHz (30);
}

APDeClickEditor::~APDeClickEditor() { setLookAndFeel (nullptr); }

void APDeClickEditor::updateSize()
{
    wave.setVisible (waveBox.getToggleState());
    setSize (kWidth, kBaseHeight + (waveBox.getToggleState() ? kWaveHeight : 0));
}

void APDeClickEditor::resized()
{
    const int w = getWidth();
    waveBox.setBounds (w - 160, 12, 110, 22);
    helpBtn.setBounds (w - 40, 11, 26, 24);

    groupArea = { 16, kTitleH + 16, w - 32, 194 };
    const int px = groupArea.getX() + 20, pw = groupArea.getWidth() - 40;
    sensitivity.setBounds (px, groupArea.getY() + 50, pw, 40);
    const int half = (pw - 36) / 2;
    frequency.setBounds (px, groupArea.getY() + 146, half, 34);
    widen.setBounds (px + half + 36, groupArea.getY() + 146, half, 34);

    const int by = groupArea.getBottom() + 14;
    listenBtn.setBounds (16, by, 150, 36);
    bypassBtn.setBounds (176, by, 150, 36);

    wave.setBounds (16, by + 50, w - 32, kWaveHeight - 14);
    help.setBounds (getLocalBounds().withTrimmedTop (kTitleH).withTrimmedBottom (kStatusH));
}

static void drawLogo (Graphics& g, Rectangle<float> r)
{
    g.setGradientFill (ColourGradient (ui::goldLight, r.getX(), r.getY(), ui::goldDark, r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 4.f);
    g.setColour (Colours::black);
    g.drawRoundedRectangle (r.reduced (0.5f), 4.f, 1.f);

    Path wave;
    const float cx = r.getCentreX(), cy = r.getCentreY() + 1.f, hw = r.getWidth() * 0.34f;
    wave.startNewSubPath (cx - hw, cy);
    for (int i = 0; i <= 24; ++i)
    {
        const float tt = (float) i / 24.f;
        wave.lineTo (cx - hw + tt * 2.f * hw, cy + std::sin (tt * 12.f) * std::sin (tt * MathConstants<float>::pi) * r.getHeight() * 0.14f);
    }
    g.setColour (Colours::black);
    g.strokePath (wave, PathStrokeType (2.f, PathStrokeType::curved, PathStrokeType::rounded));
    // the click being cut: a spike with a slash through it
    const float sx = cx + hw * 0.45f;
    g.drawLine (sx, cy - r.getHeight() * 0.36f, sx, cy - r.getHeight() * 0.12f, 2.f);
    g.drawLine (sx - 4.f, cy - r.getHeight() * 0.2f, sx + 4.f, cy - r.getHeight() * 0.3f, 1.5f);
}

void APDeClickEditor::paint (Graphics& g)
{
    const int w = getWidth(), h = getHeight();
    g.fillAll (ui::body);

    // ---- title bar (glossy black, XP style) ----
    {
        auto tb = Rectangle<float> (0.f, 0.f, (float) w, (float) kTitleH);
        ColourGradient grad (Colour (0xff3a3a3a), 0.f, 0.f, Colour (0xff050505), 0.f, tb.getBottom(), false);
        grad.addColour (0.48, Colour (0xff1c1c1c));
        grad.addColour (0.52, Colour (0xff0a0a0a));
        g.setGradientFill (grad);
        g.fillRect (tb);
        g.setColour (Colours::white.withAlpha (0.12f));
        g.drawHorizontalLine (0, 0.f, (float) w);
        g.setGradientFill (ColourGradient (ui::goldDark, 0.f, 0.f, ui::gold, (float) w * 0.5f, 0.f, true));
        g.fillRect (0, kTitleH - 2, w, 2);

        drawLogo (g, { 12.f, 9.f, 28.f, 28.f });
        g.setFont (ui::font (19.f, true));
        g.setColour (Colours::black.withAlpha (0.8f));
        g.drawText ("AP Mouth DeClick", 51, 10, 300, 26, Justification::centredLeft);
        g.setColour (ui::gold);
        g.drawText ("AP Mouth DeClick", 50, 9, 300, 26, Justification::centredLeft);
        const float nameW = GlyphArrangement::getStringWidth (g.getCurrentFont(), "AP Mouth DeClick");
        g.setFont (ui::font (11.5f));
        g.setColour (ui::textDim);
        g.drawText ("by Albert Punchline", (int) (58.f + nameW), 13, 160, 22, Justification::centredLeft);
    }

    // ---- group box "Ayarlar" ----
    {
        auto gb = groupArea.toFloat();
        const String title = ui::t (" Ayarlar ");
        g.setFont (ui::font (13.f, true));
        const float tw = GlyphArrangement::getStringWidth (g.getCurrentFont(), title);
        auto frame = gb.withTrimmedTop (8.f);
        // etched line: dark + light
        g.setColour (Colours::black);
        g.drawRoundedRectangle (frame.translated (1.f, 1.f), 3.f, 1.f);
        g.setColour (ui::goldDeep);
        g.drawRoundedRectangle (frame, 3.f, 1.f);
        g.setColour (ui::body);
        g.fillRect (gb.getX() + 10.f, gb.getY(), tw, 17.f);
        g.setColour (ui::gold);
        g.drawText (title, Rectangle<float> (gb.getX() + 10.f, gb.getY(), tw, 17.f), Justification::centredLeft);
    }

    const int px = groupArea.getX() + 20, pw = groupArea.getWidth() - 40;
    auto label = [&] (const String& s, int x, int y, int lw) {
        g.setFont (ui::font (13.f, true)); g.setColour (ui::text);
        g.drawText (s, x, y, lw, 22, Justification::centredLeft);
    };
    auto valueBox = [&] (const String& s, int x, int y, int bw) {
        auto r = Rectangle<float> ((float) x, (float) y, (float) bw, 22.f);
        ui::sunken (g, r);
        g.setFont (ui::font (13.5f, true)); g.setColour (ui::gold);
        g.drawText (s, r.reduced (7.f, 0.f), Justification::centredRight);
    };

    label (ui::t ("HASSASİYET"), px, groupArea.getY() + 24, 200);
    valueBox (String (sensitivity.getValue(), 1), px + pw - 74, groupArea.getY() + 24, 74);

    // XP trackbar ticks
    {
        const int r = 8;
        const float x0 = (float) sensitivity.getX() + (float) r, x1 = (float) sensitivity.getRight() - (float) r;
        const float ty = (float) sensitivity.getBottom() - 2.f;
        g.setFont (ui::font (11.f));
        for (int v = 0; v <= 10; ++v)
        {
            const float xx = x0 + (x1 - x0) * (float) v / 10.f;
            g.setColour (ui::goldDark);
            g.drawLine (xx, ty, xx, ty + (v % 2 == 0 ? 6.f : 3.f), 1.f);
            if (v % 2 == 0)
            {
                g.setColour (ui::textDim);
                g.drawText (String (v), Rectangle<float> (xx - 15.f, ty + 6.f, 30.f, 14.f), Justification::centred);
            }
        }
    }

    const int half = (pw - 36) / 2;
    label (ui::t ("FREKANS"), px, groupArea.getY() + 120, half - 90);
    valueBox (String (frequency.getValue() / 1000.0, 1) + " kHz", px + half - 86, groupArea.getY() + 120, 86);
    label (ui::t ("ONARIMI GENİŞLET"), px + half + 36, groupArea.getY() + 120, half - 76);
    valueBox (String (widen.getValue(), 1) + " ms", px + half + 36 + half - 72, groupArea.getY() + 120, 72);

    // ---- status bar ----
    {
        auto sb = Rectangle<float> (0.f, (float) (h - kStatusH), (float) w, (float) kStatusH);
        g.setGradientFill (ColourGradient (Colour (0xff222222), 0.f, sb.getY(), Colour (0xff111111), 0.f, sb.getBottom(), false));
        g.fillRect (sb);
        g.setColour (Colours::black);
        g.drawHorizontalLine ((int) sb.getY(), 0.f, (float) w);
        auto right = sb.reduced (3.f).removeFromRight (150.f);
        auto left = sb.reduced (3.f).withTrimmedRight (156.f);
        ui::sunken (g, left);
        ui::sunken (g, right);
        g.setFont (ui::font (12.f));
        g.setColour (ui::text.withAlpha (0.85f));
        g.drawText (statusText(), left.reduced (8.f, 0.f), Justification::centredLeft);
        const double sr = proc.getSampleRate() > 0 ? proc.getSampleRate() : 48000.0;
        const int ms = (int) std::round (1000.0 * declick::Channel::lookaheadSamples (sr) / sr);
        g.setColour (ui::textDim);
        g.drawText ("Gecikme: " + String (ms) + " ms", right.reduced (8.f, 0.f), Justification::centredLeft);
    }
}

String APDeClickEditor::statusText() const
{
    if (bypassBtn.getToggleState())  return ui::t ("Devre dışı: orijinal ses çalıyor");
    if (listenBtn.getToggleState())  return ui::t ("Sadece silinen sesler çalıyor");
    if (sensitivity.getValue() < 0.05) return ui::t ("Hassasiyet 0: işlem yapılmıyor");
    if (! proc.hasPlayed.load())     return ui::t ("Hazır. Çalmaya başla, click'ler otomatik temizlenir");
    return ui::t ("Çalışıyor: ağız sesleri temizleniyor");
}

void APDeClickEditor::timerCallback()
{
    const int n = proc.popWaveBins (scratch.data(), (int) scratch.size());
    for (int i = 0; i < n; ++i) wave.push (scratch[(size_t) i]);
    if (n > 0 && wave.isVisible()) wave.repaint();
    repaint (groupArea);                                   // value readouts
    repaint (0, getHeight() - kStatusH, getWidth(), kStatusH);
}
