#include "PluginEditor.h"
#include <cmath>

namespace
{
    const juce::Colour kBody (0xff1d1d1f), kTray (0xff141415), kPart (0xff2a2a2d), kEdge (0xff3a3a3e),
                       kLabel (0xffd6d6d8);
    const juce::Colour kLitFace (0xfffff4d6);

    // firmware button bits (panel.c order, as in the browser page)
    enum { B_FX, B_SCL, B_ENV, B_LFO, B_EDIT, B_GLO, B_HOME, B_SAVE, B_ARP, B_SEQ, B_PLAY, B_REC, B_OCTDN, B_OCTUP, B_COUNT };
    // encoder roles
    enum { EN_SELECT, EN_ALGORITHM, EN_PRESETS, EN_K1, EN_K2, EN_K3, EN_K4 };

    // the 27 keys: two rows of capsules, like the drawing
    const int kWhite[16]      = { 0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26 };
    const int kBlack[11]      = { 1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25 };
    const int kBlackAfter[11] = { 0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14 };

    struct KeyMap { char ch; bool isKey; int v; };
    const KeyMap kKeyMap[] = {
        { 'z',1,0 },{ 'x',1,2 },{ 'c',1,4 },{ 'v',1,6 },{ 'b',1,7 },{ 'n',1,9 },{ 'm',1,11 },{ ',',1,12 },{ '.',1,14 },{ '/',1,16 },
        { 's',1,1 },{ 'd',1,3 },{ 'f',1,5 },{ 'h',1,8 },{ 'j',1,10 },{ 'l',1,13 },{ ';',1,15 },{ '\'',1,17 },
        { 'q',1,18 },{ 'w',1,19 },{ 'e',1,20 },{ 'r',1,21 },{ 't',1,22 },{ 'y',1,23 },{ 'u',1,24 },{ 'i',1,25 },{ 'o',1,26 },
        { '1',0,0 },{ '2',0,1 },{ '3',0,2 },{ '4',0,3 },{ '5',0,4 },{ '6',0,5 },{ '7',0,6 },{ '8',0,7 },{ '9',0,8 },{ '0',0,9 },
        { '-',0,10 },{ '=',0,11 },{ '[',0,12 },{ ']',0,13 } };
    constexpr int kNumKeyMap = (int) (sizeof (kKeyMap) / sizeof (kKeyMap[0]));

    // ---------------------------------------------------------------- a panel button
    struct Btn : juce::Component
    {
        juce::String text, sub;
        bool red = false, held = false, latched = false, mouseDownFlag = false;
        int led = 0;          // 0 off, 1 dim, 2 lit, 3 green (PLAY), 4 breathing
        float breath = 0.0f;
        std::function<void (bool)> onDown;
        std::function<void()> onLatch;

        void paint (juce::Graphics& g) override
        {
            const float sc = (float) getWidth() / 54.0f;
            auto r = getLocalBounds().toFloat().reduced (1.0f);
            const juce::Colour dim (0xff3a3934);
            juce::Colour face = kPart;
            bool bright = false;
            switch (led)
            {
                case 1: face = dim; break;
                case 2: face = kLitFace; bright = true; break;
                case 3: face = juce::Colour (0xff5fe08a); bright = true; break;
                case 4: face = dim.interpolatedWith (juce::Colour (0xffd8cfb6), breath); bright = breath > 0.6f; break;
                default: break;
            }
            g.setColour (face);
            g.fillRoundedRectangle (r, 8.0f * sc);
            g.setColour (latched ? juce::Colour (0xff79b8ff) : (held ? kLitFace : kEdge));
            g.drawRoundedRectangle (r, 8.0f * sc, (latched || held) ? 3.0f : 1.5f);
            g.setColour (bright ? juce::Colour (0xff1d1d1f) : (red ? juce::Colour (0xffff453a) : kLabel));
            g.setFont (juce::Font (juce::FontOptions (11.5f * sc, juce::Font::bold)));
            if (sub.isEmpty()) g.drawText (text, getLocalBounds(), juce::Justification::centred);
            else
            {
                auto b = getLocalBounds();
                g.drawText (text, b.removeFromTop (b.getHeight() / 2 + 2), juce::Justification::centredBottom);
                g.setFont (juce::Font (juce::FontOptions (9.5f * sc, juce::Font::bold)));
                g.drawText (sub, b, juce::Justification::centredTop);
            }
        }
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.mods.isShiftDown() || e.mods.isPopupMenu() || latched) { if (onLatch) onLatch(); return; }
            mouseDownFlag = true;
            if (onDown) onDown (true);
        }
        void mouseUp (const juce::MouseEvent&) override
        {
            if (mouseDownFlag) { mouseDownFlag = false; if (onDown) onDown (false); }
        }
    };

    // ---------------------------------------------------------------- a knob (encoder or MASTER pot)
    struct Knob : juce::Component
    {
        juce::String label;
        float r = 22.0f;                       // logical radius
        juce::AudioParameterFloat* param = nullptr;   // MASTER only
        std::function<void (int)> onTurn;      // encoders only
        float angle = 0.0f, acc = 0.0f;
        int lastY = 0;

        void paint (juce::Graphics& g) override
        {
            const float sc = (float) getWidth() / (2.0f * r + 24.0f);
            const float cx = (float) getWidth() * 0.5f, cy = (r + 26.0f) * sc, rr = r * sc;
            g.setColour (kTray);  g.fillEllipse (cx - rr - 5 * sc, cy - rr - 5 * sc, 2 * (rr + 5 * sc), 2 * (rr + 5 * sc));
            g.setColour (kPart);  g.fillEllipse (cx - rr, cy - rr, 2 * rr, 2 * rr);
            g.setColour (kEdge);  g.drawEllipse (cx - rr, cy - rr, 2 * rr, 2 * rr, 2.0f);
            const float a = param ? juce::degreesToRadians (-135.0f + 270.0f * param->get()) : angle;
            g.setColour (juce::Colours::white);
            g.drawLine (cx + rr * 0.35f * std::sin (a), cy - rr * 0.35f * std::cos (a),
                        cx + (rr - 4 * sc) * std::sin (a), cy - (rr - 4 * sc) * std::cos (a), 3.5f * sc);
            g.setColour (kLabel);
            g.setFont (juce::Font (juce::FontOptions (13.0f * sc, juce::Font::bold)));
            g.drawText (label, 0, 0, getWidth(), (int) (cy - rr - 6 * sc), juce::Justification::centred);
        }
        void turn (int d)
        {
            if (param)
            {
                param->beginChangeGesture();
                param->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, param->get() + 0.016f * (float) d));
                param->endChangeGesture();
            }
            else
            {
                angle += 0.31f * (float) d;
                if (onTurn) onTurn (d);
            }
            repaint();
        }
        void mouseDown (const juce::MouseEvent& e) override { lastY = e.y; acc = 0; }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            acc += (float) (lastY - e.y); lastY = e.y;
            const float step = param ? 3.0f : 8.0f;
            while (acc >=  step) { turn (+1); acc -= step; }
            while (acc <= -step) { turn (-1); acc += step; }
        }
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
        {
            if (w.deltaY > 0) turn (+1); else if (w.deltaY < 0) turn (-1);
        }
    };

    // ---------------------------------------------------------------- the key bed (27 capsule keys, two rows)
    struct Keybed : juce::Component
    {
        uint32_t down = 0, litK = 0, dimK = 0, breathK = 0, midK = 0;
        float breath = 0.0f;
        std::function<void (int, bool)> onKey;
        int mouseKey = -1;

        juce::Rectangle<float> keyRect (int k) const
        {
            const float sc = (float) getWidth() / 928.0f, pitch = (928.0f - 24.0f) / 16.0f;
            const float kw = pitch * 0.78f, kh = 256.0f * 0.425f;
            float cx = 0, cy = 0;
            for (int i = 0; i < 16; ++i) if (kWhite[i] == k) { cx = 12.0f + ((float) i + 0.5f) * pitch; cy = 256.0f * 0.75f; }
            for (int j = 0; j < 11; ++j) if (kBlack[j] == k) { cx = 12.0f + ((float) kBlackAfter[j] + 1.0f) * pitch; cy = 256.0f * 0.25f; }
            return { (cx - kw / 2) * sc, (cy - kh / 2) * sc, kw * sc, kh * sc };
        }
        int hit (juce::Point<float> p) const
        {
            for (int k = 0; k < 27; ++k) if (keyRect (k).expanded (3.0f).contains (p)) return k;
            return -1;
        }
        void paint (juce::Graphics& g) override
        {
            const float sc = (float) getWidth() / 928.0f;
            g.setColour (kTray);
            g.fillRoundedRectangle (getLocalBounds().toFloat(), 18.0f * sc);
            for (int k = 0; k < 27; ++k)
            {
                auto r = keyRect (k);
                const bool on = (down >> k) & 1u;
                g.setColour (juce::Colour (0xff0c0c0d)); g.fillRoundedRectangle (r, r.getWidth() / 2);
                g.setColour (on ? kLitFace : kEdge);      g.drawRoundedRectangle (r, r.getWidth() / 2, on ? 3.0f : 1.5f);
                const float sw = juce::jmax (3.0f, r.getWidth() * 0.14f);
                juce::Colour slit (0xff3a3a3c);
                if      ((litK >> k) & 1u)    slit = juce::Colour (0xffececee);
                else if ((midK >> k) & 1u)    slit = juce::Colour (0xff9a9a9e);
                else if ((breathK >> k) & 1u) slit = slit.interpolatedWith (juce::Colour (0xffc8c8ca), breath);
                else if ((dimK >> k) & 1u)    slit = juce::Colour (0xff77777b);
                g.setColour (slit);
                g.fillRoundedRectangle (r.getCentreX() - sw / 2, r.getY() + r.getWidth() * 0.4f, sw,
                                        r.getHeight() - r.getWidth() * 0.8f, sw / 2);
            }
        }
        void mouseDown (const juce::MouseEvent& e) override { mouseKey = hit (e.position); if (mouseKey >= 0 && onKey) onKey (mouseKey, true); }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            const int k = hit (e.position);
            if (k == mouseKey) return;
            if (mouseKey >= 0 && onKey) onKey (mouseKey, false);
            mouseKey = k;
            if (k >= 0 && onKey) onKey (k, true);
        }
        void mouseUp (const juce::MouseEvent&) override { if (mouseKey >= 0 && onKey) onKey (mouseKey, false); mouseKey = -1; }
    };
}

// ==================================================================== Impl
struct GhoulboxEditor::Impl
{
    GhoulboxEditor& ed;
    GhoulboxProcessor& proc;

    std::vector<std::unique_ptr<Btn>> buttons;
    std::vector<std::unique_ptr<Knob>> knobs;
    Keybed keybed;
    std::vector<std::pair<juce::Component*, juce::Rectangle<float>>> rects;   // logical bounds

    uint32_t mouseKeys = 0, kbKeys = 0, sentKeys = 0, mouseBtn = 0, kbBtn = 0, latched = 0, sentBtn = 0;
    bool kbDown[kNumKeyMap] {};

    Impl (GhoulboxEditor& e, GhoulboxProcessor& p) : ed (e), proc (p) {}

    void sync()
    {
        const uint32_t k = mouseKeys | kbKeys, b = mouseBtn | kbBtn | latched;
        const int nk = juce::jmin (27, proc.fwNumKeys()), nb = juce::jmin ((int) B_COUNT, proc.fwNumButtons());
        for (int i = 0; i < nk; ++i)
            if (((k >> i) & 1u) != ((sentKeys >> i) & 1u)) proc.pushUiEvent ({ UiEvent::Key, i, (int) ((k >> i) & 1u) });
        for (int i = 0; i < nb; ++i)
            if (((b >> i) & 1u) != ((sentBtn >> i) & 1u)) proc.pushUiEvent ({ UiEvent::Button, i, (int) ((b >> i) & 1u) });
        sentKeys = k; sentBtn = b;
        keybed.down = k; keybed.repaint();
        for (size_t i = 0; i < buttons.size(); ++i)
        {
            buttons[i]->held = (b >> i) & 1u;
            buttons[i]->latched = (latched >> i) & 1u;
            buttons[i]->repaint();
        }
    }

    // LEDs as the browser page draws them (lit / dim / breathing / mid, PLAY green)
    void applyLeds (const unsigned int* l)
    {
        const float phase = (float) (juce::Time::getMillisecondCounter() % 1130u) / 1130.0f;
        const float breath = (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * phase)) * (l[4] ? 0.55f : 0.85f);
        for (size_t i = 0; i < buttons.size(); ++i)
        {
            if (! buttons[i]) continue;
            const bool lit = (l[0] >> i) & 1u, green = (i == (size_t) B_PLAY) && ((l[0] >> 14) & 1u);
            buttons[i]->led = green ? 3 : lit ? 2 : ((l[5] >> i) & 1u) ? 4 : ((l[2] >> i) & 1u) ? 1 : 0;
            buttons[i]->breath = breath;
            buttons[i]->repaint();
        }
        keybed.litK = l[1]; keybed.dimK = l[3]; keybed.breathK = l[6]; keybed.midK = l[7];
        keybed.breath = breath;
        keybed.repaint();
    }

    void addButton (int bit, const char* text, const char* sub, juce::Rectangle<float> r, bool red = false)
    {
        auto b = std::make_unique<Btn>();
        b->text = text; b->sub = sub; b->red = red;
        b->onDown  = [this, bit] (bool d) { ed.grabKeyboardFocus(); if (d) mouseBtn |= 1u << bit; else mouseBtn &= ~(1u << bit); sync(); };
        b->onLatch = [this, bit] { latched ^= 1u << bit; sync(); };
        ed.addAndMakeVisible (*b);
        rects.push_back ({ b.get(), r });
        if ((int) buttons.size() <= bit) buttons.resize ((size_t) bit + 1);
        buttons[(size_t) bit] = std::move (b);
    }

    void addKnob (const char* label, float cx, float cy, float r, int role)
    {
        auto k = std::make_unique<Knob>();
        k->label = label; k->r = r;
        if (role < 0) k->param = proc.getMasterParam();
        else k->onTurn = [this, role] (int d) { proc.pushUiEvent ({ UiEvent::Encoder, role, d }); };
        ed.addAndMakeVisible (*k);
        rects.push_back ({ k.get(), { cx - r - 12, cy - r - 26, 2 * r + 24, 2 * r + 38 } });
        knobs.push_back (std::move (k));
    }
};

// ==================================================================== editor
GhoulboxEditor::GhoulboxEditor (GhoulboxProcessor& p)
    : AudioProcessorEditor (&p), proc (p), raw (GB_SCREEN_W * GB_SCREEN_H, 0), impl (new Impl (*this, p))
{
    auto& I = *impl;

    I.addKnob ("MASTER",    105, 100, 24, -1);
    I.addKnob ("SELECT",    215, 100, 24, EN_SELECT);
    I.addKnob ("PRESETS",   105, 205, 24, EN_PRESETS);
    I.addKnob ("ALGORITHM", 215, 205, 24, EN_ALGORITHM);
    I.addKnob ("KNOB 1", 610, 100, 22, EN_K1);
    I.addKnob ("KNOB 2", 714, 100, 22, EN_K2);
    I.addKnob ("KNOB 3", 818, 100, 22, EN_K3);
    I.addKnob ("KNOB 4", 922, 100, 22, EN_K4);

    I.addButton (B_OCTDN, "OCT-", "", { 60, 258, 90, 34 });
    I.addButton (B_OCTUP, "OCT+", "", { 170, 258, 90, 34 });
    const char* row1[] = { "FX", "SCL", "ENV", "LFO", "EDIT", "GLO" };
    const int   bit1[] = { B_FX, B_SCL, B_ENV, B_LFO, B_EDIT, B_GLO };
    const char* row2[] = { "HOME", "SAVE", "ARP", "SEQ", "PLAY", "REC" };
    const int   bit2[] = { B_HOME, B_SAVE, B_ARP, B_SEQ, B_PLAY, B_REC };
    for (int i = 0; i < 6; ++i)
    {
        I.addButton (bit1[i], row1[i], "", { 582.0f + 63.0f * (float) i, 174.0f, 54.0f, 48.0f });
        I.addButton (bit2[i], row2[i], i == 4 ? "STOP" : "", { 582.0f + 63.0f * (float) i, 236.0f, 54.0f, 48.0f }, i == 5);
    }

    I.keybed.onKey = [&I] (int k, bool d)
    {
        if (d) I.mouseKeys |= 1u << k; else I.mouseKeys &= ~(1u << k);
        I.sync();
    };
    addAndMakeVisible (I.keybed);
    I.rects.push_back ({ &I.keybed, { 36, 332, 928, 256 } });

    setWantsKeyboardFocus (true);
    setResizable (true, true);
    setResizeLimits (500, 312, 2000, 1248);
    getConstrainer()->setFixedAspectRatio (1000.0 / 624.0);
    setSize (860, 537);
    startTimerHz (30);
}

GhoulboxEditor::~GhoulboxEditor() { stopTimer(); }

void GhoulboxEditor::resized()
{
    const float s = (float) getWidth() / 1000.0f;
    for (auto& [c, r] : impl->rects)
        c->setBounds ((r * s).toNearestInt());
}

void GhoulboxEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff0b0b0c));
    g.addTransform (juce::AffineTransform::scale ((float) getWidth() / 1000.0f));
    g.setColour (kBody);  g.fillRoundedRectangle (4, 4, 992, 616, 40);
    g.setColour (kTray);  g.fillRoundedRectangle (50, 250, 220, 50, 14);
    g.fillRoundedRectangle (568, 160, 400, 140, 14);
    g.setColour (juce::Colour (0xff0b0b0c));  g.fillRoundedRectangle (292, 44, 256, 256, 18);

    g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);   // crisp pixels
    g.drawImage (screenImage, 300, 52, 240, 240, 0, 0, GB_SCREEN_W, GB_SCREEN_H);

    if (! proc.isActive())
    {
        g.setColour (juce::Colours::black.withAlpha (0.88f));
        g.fillRect (300, 52, 240, 240);
        g.setColour (juce::Colours::orange);
        g.setFont (juce::Font (juce::FontOptions (13.0f)));
        g.drawFittedText (proc.getFirmwareError().isNotEmpty() ? proc.getFirmwareError() : juce::String ("Starting..."),
                          310, 62, 220, 220, juce::Justification::centred, 8);
    }
}

void GhoulboxEditor::timerCallback()
{
    for (auto& k : impl->knobs) if (k->param) k->repaint();   // MASTER follows host automation
    if (! proc.isActive()) return;
    const bool newScreen = proc.fetchFrame (raw.data(), ledState, screenSerial);
    impl->applyLeds (ledState);
    if (! newScreen) return;

    juce::Image::BitmapData bd (screenImage, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < GB_SCREEN_H; ++y)
        for (int x = 0; x < GB_SCREEN_W; ++x)
        {
            const unsigned v = raw[(size_t) (y * GB_SCREEN_W + x)];   // standard RGB565
            bd.setPixelColour (x, y, juce::Colour ((juce::uint8) (((v >> 11) & 31) * 255 / 31),
                                                   (juce::uint8) (((v >> 5) & 63) * 255 / 63),
                                                   (juce::uint8) ((v & 31) * 255 / 31)));
        }
    repaint (juce::Rectangle<int> (300, 52, 240, 240).transformedBy (juce::AffineTransform::scale ((float) getWidth() / 1000.0f)));
}

// Computer keys exactly as the browser page: Z.. / S D F H J L ; ' / Q..O for the keys,
// 1..0 - = for the twelve buttons, [ ] for OCT- / OCT+.
bool GhoulboxEditor::keyStateChanged (bool)
{
    auto& I = *impl;
    bool used = false, changed = false;
    for (int i = 0; i < kNumKeyMap; ++i)
    {
        const bool d = juce::KeyPress::isKeyCurrentlyDown (kKeyMap[i].ch);
        if (d == I.kbDown[i]) continue;
        I.kbDown[i] = d; used = changed = true;
        uint32_t& mask = kKeyMap[i].isKey ? I.kbKeys : I.kbBtn;
        if (d) mask |= 1u << kKeyMap[i].v; else mask &= ~(1u << kKeyMap[i].v);
    }
    if (changed) I.sync();
    return used;
}

// Arrow keys: up/down = SELECT, left/right = ALGORITHM (as in the browser page)
bool GhoulboxEditor::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::upKey)         { proc.pushUiEvent ({ UiEvent::Encoder, EN_SELECT, +1 });    return true; }
    if (k == juce::KeyPress::downKey)       { proc.pushUiEvent ({ UiEvent::Encoder, EN_SELECT, -1 });    return true; }
    if (k == juce::KeyPress::rightKey)      { proc.pushUiEvent ({ UiEvent::Encoder, EN_ALGORITHM, +1 }); return true; }
    if (k == juce::KeyPress::leftKey)       { proc.pushUiEvent ({ UiEvent::Encoder, EN_ALGORITHM, -1 }); return true; }
    return false;
}
