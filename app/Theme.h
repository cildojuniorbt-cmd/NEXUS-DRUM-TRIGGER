#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <functional>

// ============================================================================
//  Tema (cores personalizáveis) + LookAndFeel + componentes base
// ============================================================================
namespace theme {

inline juce::Colour bg{0xff070a09}, panelTop{0xff131a17}, panelBot{0xff0d1210}, edge{0xff1f2c26}, track{0xff1d2823};
inline juce::Colour accent{0xff39e639}, accentDim{0xff1b7a22}, hot{0xff9dff7a};
inline const juce::Colour text{0xfff2f5f3}, muted{0xff8a9690}, warn{0xffffb020}, danger{0xffff4d4d};

inline int bgStyle = 0;
inline const char* bgNames[] = {"Preto", "Grafite", "Azul-noite", "Roxo-noite"};

inline void setAccent(juce::Colour c) {
    accent = c;
    accentDim = c.withMultipliedBrightness(0.55f);
    hot = c.brighter(0.5f);
}
inline void setBackground(int style) {
    bgStyle = juce::jlimit(0, 3, style);
    switch (bgStyle) {
        case 1: bg = juce::Colour(0xff0e1012); panelTop = juce::Colour(0xff1a1d21); panelBot = juce::Colour(0xff131518); edge = juce::Colour(0xff2a2f36); track = juce::Colour(0xff262b31); break;
        case 2: bg = juce::Colour(0xff060912); panelTop = juce::Colour(0xff0f1626); panelBot = juce::Colour(0xff0a0f1b); edge = juce::Colour(0xff1c2842); track = juce::Colour(0xff192238); break;
        case 3: bg = juce::Colour(0xff0a0710); panelTop = juce::Colour(0xff17101f); panelBot = juce::Colour(0xff0f0a15); edge = juce::Colour(0xff2a1d3a); track = juce::Colour(0xff241a31); break;
        default: bg = juce::Colour(0xff070a09); panelTop = juce::Colour(0xff131a17); panelBot = juce::Colour(0xff0d1210); edge = juce::Colour(0xff1f2c26); track = juce::Colour(0xff1d2823); break;
    }
}
inline juce::Font font(float size, bool bold = false) {
    return juce::Font(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain));
}
inline juce::String u8(const char* s) { return juce::String::fromUTF8(s); }
inline juce::String upper(juce::String s) {
    s = s.toUpperCase();
    const char* from[] = {"ã", "õ", "á", "é", "í", "ó", "ú", "â", "ê", "ô", "ç"};
    const char* to[]   = {"Ã", "Õ", "Á", "É", "Í", "Ó", "Ú", "Â", "Ê", "Ô", "Ç"};
    for (int i = 0; i < 11; ++i) s = s.replace(u8(from[i]), u8(to[i]));
    return s;
}

inline juce::File settingsFile() {
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Nexus Drum Trigger").getChildFile("aparencia.xml");
}
inline void save() {
    juce::XmlElement x("Aparencia");
    x.setAttribute("accent", accent.toString());
    x.setAttribute("bg", bgStyle);
    settingsFile().getParentDirectory().createDirectory();
    x.writeTo(settingsFile());
}
inline void load() {
    if (auto x = juce::XmlDocument::parse(settingsFile())) {
        if (x->getTagName() == "Aparencia") {
            setBackground(x->getIntAttribute("bg", 0));
            setAccent(juce::Colour::fromString(x->getStringAttribute("accent", "ff39e639")));
        }
    }
}
} // namespace theme

// ---------------------------------------------------------------- LookAndFeel
class NexusLnF : public juce::LookAndFeel_V4 {
public:
    NexusLnF() { applyColours(); }

    void applyColours() {
        using namespace juce;
        setColour(ResizableWindow::backgroundColourId, theme::bg);
        setColour(PopupMenu::backgroundColourId, theme::panelTop);
        setColour(PopupMenu::textColourId, theme::text);
        setColour(PopupMenu::highlightedBackgroundColourId, theme::accentDim);
        setColour(PopupMenu::highlightedTextColourId, Colours::white);
        setColour(ComboBox::backgroundColourId, theme::panelTop);
        setColour(ComboBox::textColourId, theme::text);
        setColour(ComboBox::outlineColourId, theme::edge);
        setColour(ComboBox::arrowColourId, theme::accent);
        setColour(Label::textColourId, theme::text);
        setColour(TextButton::buttonColourId, theme::panelTop);
        setColour(TextButton::textColourOffId, theme::text);
        setColour(TextButton::textColourOnId, Colours::black);
        setColour(ToggleButton::tickColourId, theme::accent);
        setColour(ToggleButton::textColourId, theme::text);
        setColour(ListBox::backgroundColourId, Colours::transparentBlack);
        setColour(ScrollBar::thumbColourId, theme::edge.brighter(0.4f));
        setColour(Slider::thumbColourId, theme::accent);
        setColour(Slider::trackColourId, theme::accent);
        setColour(Slider::backgroundColourId, theme::track);
        setColour(TextEditor::backgroundColourId, theme::panelBot);
        setColour(TextEditor::textColourId, theme::text);
        setColour(TextEditor::outlineColourId, theme::edge);
        setColour(AlertWindow::backgroundColourId, theme::panelTop);
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override {
        using namespace juce;
        auto b = Rectangle<float>(static_cast<float>(x), static_cast<float>(y), static_cast<float>(w), static_cast<float>(h)).reduced(3.0f);
        const float d = jmin(b.getWidth(), b.getHeight());
        const auto c = b.getCentre();
        const float r = d * 0.5f, arcR = r - 3.0f, th = jmax(3.5f, d * 0.075f);
        Path bgArc; bgArc.addCentredArc(c.x, c.y, arcR, arcR, 0.0f, start, end, true);
        g.setColour(theme::track);
        g.strokePath(bgArc, PathStrokeType(th, PathStrokeType::curved, PathStrokeType::rounded));
        const float angle = start + pos * (end - start);
        if (pos > 0.002f) {
            Path v; v.addCentredArc(c.x, c.y, arcR, arcR, 0.0f, start, angle, true);
            g.setColour(theme::accent.withAlpha(0.22f));
            g.strokePath(v, PathStrokeType(th * 2.0f, PathStrokeType::curved, PathStrokeType::rounded));
            g.setColour(theme::accent);
            g.strokePath(v, PathStrokeType(th, PathStrokeType::curved, PathStrokeType::rounded));
        }
        const float br = r - th - 4.0f;
        g.setGradientFill(ColourGradient(Colour(0xff2b3430), c.x, c.y - br, Colour(0xff0a0d0c), c.x, c.y + br, false));
        g.fillEllipse(c.x - br, c.y - br, br * 2, br * 2);
        g.setColour(Colour(0xff3a4540));
        g.drawEllipse(c.x - br, c.y - br, br * 2, br * 2, 1.2f);
        Path p; p.addRoundedRectangle(-1.6f, -br + 3.0f, 3.2f, br * 0.5f, 1.6f);
        g.setColour(theme::accent);
        g.fillPath(p, AffineTransform::rotation(angle).translated(c.x, c.y));
    }

    void drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                          juce::Slider::SliderStyle style, juce::Slider& s) override {
        using namespace juce;
        if (style == Slider::LinearVertical) {
            const float cx = static_cast<float>(x) + static_cast<float>(w) * 0.5f;
            Rectangle<float> tr(cx - 3.0f, static_cast<float>(y) + 2.0f, 6.0f, static_cast<float>(h) - 4.0f);
            g.setColour(theme::track); g.fillRoundedRectangle(tr, 3.0f);
            auto fill = tr.withTop(pos);
            g.setColour(theme::accent.withAlpha(0.25f)); g.fillRoundedRectangle(fill.expanded(2.5f, 0), 5.0f);
            g.setColour(theme::accent); g.fillRoundedRectangle(fill, 3.0f);
            Rectangle<float> cap(cx - 15.0f, pos - 8.0f, 30.0f, 16.0f);
            g.setColour(Colour(0xfff4fff4)); g.fillRoundedRectangle(cap, 4.0f);
            g.setColour(theme::accent); g.drawRoundedRectangle(cap, 4.0f, 2.0f);
            g.setColour(Colours::black.withAlpha(0.5f)); g.fillRect(cx - 9.0f, pos - 0.5f, 18.0f, 1.2f);
            return;
        }
        if (style != Slider::LinearHorizontal) { LookAndFeel_V4::drawLinearSlider(g, x, y, w, h, pos, minPos, maxPos, style, s); return; }
        const float cy = static_cast<float>(y) + static_cast<float>(h) * 0.5f;
        Rectangle<float> tr(static_cast<float>(x) + 2.0f, cy - 3.0f, static_cast<float>(w) - 4.0f, 6.0f);
        g.setColour(theme::track); g.fillRoundedRectangle(tr, 3.0f);
        auto fill = tr.withRight(pos);
        g.setColour(theme::accent.withAlpha(0.25f)); g.fillRoundedRectangle(fill.expanded(0, 2.5f), 5.0f);
        g.setColour(theme::accent); g.fillRoundedRectangle(fill, 3.0f);
        g.setColour(theme::accent.withAlpha(0.28f)); g.fillEllipse(pos - 10.0f, cy - 10.0f, 20.0f, 20.0f);
        g.setColour(Colour(0xfff4fff4)); g.fillEllipse(pos - 6.5f, cy - 6.5f, 13.0f, 13.0f);
        g.setColour(theme::accent); g.drawEllipse(pos - 6.5f, cy - 6.5f, 13.0f, 13.0f, 2.0f);
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hl, bool down) override {
        using namespace juce;
        auto r = b.getLocalBounds().toFloat().reduced(0.5f);
        if (b.getProperties()["flat"]) { if (hl) { g.setColour(Colours::white.withAlpha(0.06f)); g.fillRoundedRectangle(r, 8.0f); } return; }
        const bool on = b.getToggleState();
        if (on) {
            g.setColour(theme::accent.withAlpha(0.25f)); g.fillRoundedRectangle(r.expanded(2.0f), 10.0f);
            g.setColour(theme::accent); g.fillRoundedRectangle(r, 8.0f);
        } else {
            g.setColour((down ? theme::panelBot : (hl ? theme::panelTop.brighter(0.12f) : theme::panelTop)));
            g.fillRoundedRectangle(r, 8.0f);
            g.setColour(theme::edge.brighter(hl ? 0.5f : 0.1f)); g.drawRoundedRectangle(r, 8.0f, 1.2f);
        }
    }
    juce::Font getTextButtonFont(juce::TextButton&, int h) override { return theme::font(juce::jmin(15.0f, static_cast<float>(h) * 0.5f), true); }

    void drawComboBox(juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override {
        using namespace juce;
        auto r = Rectangle<float>(0, 0, static_cast<float>(w), static_cast<float>(h)).reduced(0.5f);
        g.setColour(theme::panelBot); g.fillRoundedRectangle(r, 8.0f);
        g.setColour(box.hasKeyboardFocus(true) ? theme::accent : theme::edge.brighter(0.2f)); g.drawRoundedRectangle(r, 8.0f, 1.2f);
        Path arrow; const float ax = static_cast<float>(w) - 20.0f, ay = static_cast<float>(h) * 0.5f;
        arrow.startNewSubPath(ax, ay - 2.5f); arrow.lineTo(ax + 5.0f, ay + 2.5f); arrow.lineTo(ax + 10.0f, ay - 2.5f);
        g.setColour(theme::accent); g.strokePath(arrow, PathStrokeType(2.0f, PathStrokeType::curved, PathStrokeType::rounded));
    }
    juce::Font getComboBoxFont(juce::ComboBox&) override { return theme::font(14.0f); }
};

// ---------------------------------------------------------------- Botão liga/desliga
class PowerButton : public juce::Button {
public:
    PowerButton() : juce::Button("power") { setClickingTogglesState(true); }
    void paintButton(juce::Graphics& g, bool hl, bool) override {
        using namespace juce;
        auto r = getLocalBounds().toFloat().reduced(3.0f);
        const bool on = getToggleState();
        const Colour c = on ? theme::accent : theme::muted;
        if (on) { g.setColour(theme::accent.withAlpha(0.2f)); g.fillEllipse(r.expanded(2.5f)); }
        Path p; const auto cc = r.getCentre(); const float rad = r.getWidth() * 0.33f;
        p.addCentredArc(cc.x, cc.y + 0.5f, rad, rad, 0.0f, 0.65f, 2.0f * MathConstants<float>::pi - 0.65f, true);
        p.startNewSubPath(cc.x, cc.y - rad - 1.0f); p.lineTo(cc.x, cc.y - 1.0f);
        g.setColour(hl ? c.brighter(0.3f) : c);
        g.strokePath(p, PathStrokeType(2.2f, PathStrokeType::curved, PathStrokeType::rounded));
    }
};

// ---------------------------------------------------------------- Cartão (painel)
class Card : public juce::Component {
public:
    explicit Card(const juce::String& t = {}, bool withPower = false) : title(t) {
        if (withPower) { power = std::make_unique<PowerButton>(); addAndMakeVisible(*power); }
    }
    void setTitle(const juce::String& t) { title = t; repaint(); }
    juce::Rectangle<int> content() const { return getLocalBounds().reduced(14, 0).withTrimmedTop(title.isEmpty() ? 12 : 42).withTrimmedBottom(12); }
    void paint(juce::Graphics& g) override {
        using namespace juce;
        auto r = getLocalBounds().toFloat().reduced(0.5f);
        g.setGradientFill(ColourGradient(theme::panelTop, 0, 0, theme::panelBot, 0, static_cast<float>(getHeight()), false));
        g.fillRoundedRectangle(r, 12.0f);
        g.setColour(theme::edge); g.drawRoundedRectangle(r, 12.0f, 1.2f);
        if (title.isNotEmpty()) {
            g.setColour(theme::text); g.setFont(theme::font(15.5f, true));
            g.drawText(title, 16 + (power ? 34 : 0), 8, getWidth() - 100, 28, Justification::centredLeft);
            g.setColour(theme::edge); g.fillRect(14, 40, getWidth() - 28, 1);
        }
    }
    void resized() override { if (power) power->setBounds(10, 9, 30, 28); }
    std::unique_ptr<PowerButton> power;
protected:
    juce::String title;
};

// ---------------------------------------------------------------- Knob giratório
class Knob : public juce::Component {
public:
    Knob(const juce::String& name, double mn, double mx, double step, const juce::String& suffix, double def, int decimals = -1, double skewMid = 0.0) {
        title.setText(name, juce::dontSendNotification);
        title.setFont(theme::font(13.0f)); title.setColour(juce::Label::textColourId, theme::text);
        title.setJustificationType(juce::Justification::centred);
        value.setFont(theme::font(13.0f)); value.setColour(juce::Label::textColourId, theme::muted);
        value.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(title); addAndMakeVisible(value);
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters(1.25f * juce::MathConstants<float>::pi, 2.75f * juce::MathConstants<float>::pi, true);
        slider.setRange(mn, mx, step);
        if (skewMid > mn && skewMid < mx) slider.setSkewFactorFromMidPoint(skewMid);
        slider.setTextValueSuffix(suffix);
        if (decimals >= 0) slider.setNumDecimalPlacesToDisplay(decimals);
        slider.setDoubleClickReturnValue(true, def);
        slider.setMouseDragSensitivity(180);
        slider.onValueChange = [this] {
            updateText();
            if (set) set(static_cast<float>(slider.getValue()));
            if (afterChange) afterChange();
        };
        addAndMakeVisible(slider);
    }
    void bind(std::function<float()> g, std::function<void(float)> s) { get = std::move(g); set = std::move(s); refresh(); }
    void refresh() { if (get) { slider.setValue(get(), juce::dontSendNotification); updateText(); } }
    void resized() override {
        auto r = getLocalBounds();
        title.setBounds(r.removeFromTop(18));
        value.setBounds(r.removeFromBottom(18));
        slider.setBounds(r);
    }
    std::function<void()> afterChange;
    juce::Slider slider;
private:
    void updateText() { value.setText(slider.getTextFromValue(slider.getValue()), juce::dontSendNotification); }
    juce::Label title, value;
    std::function<float()> get;
    std::function<void(float)> set;
};

// ---------------------------------------------------------------- Slider horizontal (rótulo + valor)
class BarSlider : public juce::Component {
public:
    BarSlider(const juce::String& name, double mn, double mx, double step, const juce::String& suffix, int decimals = -1, double skewMid = 0.0) {
        title.setText(name, juce::dontSendNotification);
        title.setFont(theme::font(13.5f)); title.setColour(juce::Label::textColourId, theme::text);
        value.setFont(theme::font(13.5f, true)); value.setColour(juce::Label::textColourId, theme::text);
        value.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(title); addAndMakeVisible(value);
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setRange(mn, mx, step);
        if (skewMid > mn && skewMid < mx) slider.setSkewFactorFromMidPoint(skewMid);
        slider.setTextValueSuffix(suffix);
        if (decimals >= 0) slider.setNumDecimalPlacesToDisplay(decimals);
        slider.onValueChange = [this] {
            updateText();
            if (set) set(static_cast<float>(slider.getValue()));
            if (afterChange) afterChange();
        };
        addAndMakeVisible(slider);
    }
    void bind(std::function<float()> g, std::function<void(float)> s) { get = std::move(g); set = std::move(s); refresh(); }
    void refresh() { if (get) { slider.setValue(get(), juce::dontSendNotification); updateText(); } }
    void paint(juce::Graphics& g) override {
        auto p = pill.toFloat();
        g.setColour(theme::panelBot); g.fillRoundedRectangle(p, 7.0f);
        g.setColour(theme::edge.brighter(0.15f)); g.drawRoundedRectangle(p, 7.0f, 1.0f);
    }
    void resized() override {
        auto r = getLocalBounds();
        pill = r.removeFromRight(74).withSizeKeepingCentre(74, 28);
        r.removeFromRight(8);
        value.setBounds(pill);
        title.setBounds(r.removeFromTop(18));
        slider.setBounds(r);
    }
    std::function<void()> afterChange;
    juce::Slider slider;
private:
    void updateText() { value.setText(slider.getTextFromValue(slider.getValue()), juce::dontSendNotification); }
    juce::Label title, value;
    juce::Rectangle<int> pill;
    std::function<float()> get;
    std::function<void(float)> set;
};

// ---------------------------------------------------------------- Medidor vertical segmentado
class SegMeter : public juce::Component {
public:
    void setLevel(float lin) {
        const float db = juce::Decibels::gainToDecibels(lin, -80.0f);
        const float n = juce::jlimit(0.0f, 1.0f, juce::jmap(db, -54.0f, 0.0f, 0.0f, 1.0f));
        shown = juce::jmax(n, shown * 0.86f);
        if (n >= peak) { peak = n; hold = 24; } else if (hold > 0) { --hold; } else { peak = juce::jmax(0.0f, peak - 0.03f); }
        repaint();
    }
    void paint(juce::Graphics& g) override {
        const int segs = 18;
        const float gap = 2.0f, sh = (static_cast<float>(getHeight()) - gap * (segs - 1)) / segs;
        for (int i = 0; i < segs; ++i) {
            const float t = static_cast<float>(i + 1) / segs;
            const float y = static_cast<float>(getHeight()) - (static_cast<float>(i) + 1.0f) * sh - gap * static_cast<float>(i);
            const bool lit = shown >= t - 0.001f || (peak >= t - 0.03f && peak < t + 0.03f);
            juce::Colour c = t > 0.9f ? theme::danger : (t > 0.72f ? theme::warn : theme::accent);
            g.setColour(lit ? c : theme::track);
            g.fillRoundedRectangle(0.0f, y, static_cast<float>(getWidth()), sh, 1.5f);
        }
    }
private:
    float shown = 0.0f, peak = 0.0f; int hold = 0;
};
