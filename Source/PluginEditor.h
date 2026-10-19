#pragma once
#include "PluginProcessor.h"
#include <memory>
#include <vector>

/*
 * The panel, laid out like the browser build's "wide" drawing (web/emu/index.html):
 * a 1000 x 624 logical canvas scaled to the window. Button ids and encoder
 * roles are the firmware's own (panel.c order), as the browser page uses them.
 */
class GhoulboxEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit GhoulboxEditor (GhoulboxProcessor&);
    ~GhoulboxEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyStateChanged (bool isKeyDown) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void timerCallback() override;

    GhoulboxProcessor& proc;
    juce::Image screenImage { juce::Image::ARGB, GB_SCREEN_W, GB_SCREEN_H, true };
    std::vector<unsigned short> raw;
    unsigned int ledState[8] {};
    uint32_t screenSerial = 0;

    struct Impl;
    std::unique_ptr<Impl> impl;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GhoulboxEditor)
};
