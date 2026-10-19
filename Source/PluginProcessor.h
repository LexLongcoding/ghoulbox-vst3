#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include <mutex>
#include <vector>
#include "gb_host.h"
#include "GbFirmware.h"
#include <memory>

struct UiEvent
{
    enum Type { Key, Button, Encoder } type;
    int a = 0, b = 0;   // Key: index,down  Button: id,down  Encoder: role,delta
};

class GhoulboxProcessor : public juce::AudioProcessor
{
public:
    GhoulboxProcessor();
    ~GhoulboxProcessor() override;

    void prepareToPlay (double sampleRate, int maxBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Ghoulbox"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }   // USB-MIDI out isn't emulated
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }   // HALL reverb

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- UI (message) thread ----
    void pushUiEvent (const UiEvent&);
    /** Latest LEDs always; the screen only if it changed since lastSerial (returns true then). */
    bool fetchFrame (unsigned short* dst, unsigned int* ledsOut, uint32_t& lastSerial);
    bool isActive() const { return ready && booted.load(); }
    const juce::String& getFirmwareError() const { return fwError; }
    int fwNumKeys() const    { return fw ? fw->num_keys() : 0; }
    int fwNumButtons() const { return fw ? fw->num_buttons() : 0; }
    juce::AudioParameterFloat* getMasterParam() { return master; }

private:
    void bootIfNeeded (const unsigned char* state, int len);
    void renderSegment (float* l, float* r, int n);
    void applyUiEvents();
    void publishScreen (int numSamples);

    std::unique_ptr<GbFirmware> fw;     // this instance's private copy of the firmware
    juce::String fwError;
    bool ready = false;                 // false if the firmware library could not be loaded
    std::atomic<bool> booted { false };
    std::mutex bootMutex;
    double hostRate = 44100.0;
    int nativeRate = 44100;
    bool resampling = false;
    juce::AudioParameterFloat* master = nullptr;
    float lastMaster = -1.0f;

    static constexpr int kFifo = 512;
    juce::AbstractFifo fifo { kFifo };
    std::array<UiEvent, kFifo> events;

    juce::SpinLock screenLock;
    std::vector<unsigned short> screen;
    bool screenValid = false;
    unsigned int ledsPub[8] {};
    uint32_t serial = 0;
    int samplesSinceFrame = 0;

    // live-music restore handed to the audio thread
    juce::SpinLock stateLock;
    std::vector<unsigned char> pendingState;
    bool statePending = false;

    // state snapshot taken on the audio thread (flash is written there)
    std::atomic<bool> snapReq { false };
    juce::WaitableEvent snapDone;
    std::vector<unsigned char> snapBuf;
    std::atomic<int> snapLen { 0 };

    static constexpr int kChunk = 256, kPendCap = 4096;
    juce::LagrangeInterpolator interpL, interpR;
    std::vector<float> pendL, pendR;
    int pendCount = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GhoulboxProcessor)
};
