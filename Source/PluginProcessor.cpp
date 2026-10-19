#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <cstring>

GhoulboxProcessor::GhoulboxProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      screen (GB_SCREEN_W * GB_SCREEN_H, 0),
      pendL (kPendCap + kChunk, 0.0f), pendR (kPendCap + kChunk, 0.0f)
{
    addParameter (master = new juce::AudioParameterFloat ({ "master", 1 }, "Master",
                                                          juce::NormalisableRange<float> (0.0f, 1.0f), 0.68f));
    // every instance loads its own copy of the firmware library, so each has its own globals
    fw = GbFirmware::create (fwError);
    if (fw != nullptr)
    {
        ready = true;
        nativeRate = fw->init();
        snapBuf.resize ((size_t) fw->state_max());
        booted = (fw->booted() != 0);
    }
}

GhoulboxProcessor::~GhoulboxProcessor() = default;

bool GhoulboxProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void GhoulboxProcessor::bootIfNeeded (const unsigned char* state, int len)
{
    if (! ready) return;
    std::lock_guard<std::mutex> lock (bootMutex);
    if (booted.load()) return;
    fw->master (master->get());         // the pot's level from the first block, as the browser does
    lastMaster = master->get();
    fw->boot (state, len);              // non-realtime thread; flash sectors go in before boot
    booted = true;
}

void GhoulboxProcessor::prepareToPlay (double sampleRate, int)
{
    hostRate = sampleRate;
    resampling = std::abs (sampleRate - (double) nativeRate) > 0.5;
    interpL.reset(); interpR.reset();
    pendCount = 0;
    samplesSinceFrame = 0;
    bootIfNeeded (nullptr, 0);          // no saved state arrived first: boot clean
}

// ---------------------------------------------------------------- UI plumbing
void GhoulboxProcessor::pushUiEvent (const UiEvent& e)
{
    int s1, n1, s2, n2;
    fifo.prepareToWrite (1, s1, n1, s2, n2);
    if (n1 > 0)      events[(size_t) s1] = e;
    else if (n2 > 0) events[(size_t) s2] = e;
    fifo.finishedWrite (n1 + n2);
}

void GhoulboxProcessor::applyUiEvents()
{
    int s1, n1, s2, n2;
    fifo.prepareToRead (fifo.getNumReady(), s1, n1, s2, n2);
    auto apply = [this] (const UiEvent& e)
    {
        switch (e.type)
        {
            case UiEvent::Key:     fw->key (e.a, e.b);     break;
            case UiEvent::Button:  fw->button (e.a, e.b);  break;
            case UiEvent::Encoder: fw->encoder (e.a, e.b); break;
        }
    };
    for (int i = 0; i < n1; ++i) apply (events[(size_t) (s1 + i)]);
    for (int i = 0; i < n2; ++i) apply (events[(size_t) (s2 + i)]);
    fifo.finishedRead (n1 + n2);
}

bool GhoulboxProcessor::fetchFrame (unsigned short* dst, unsigned int* ledsOut, uint32_t& lastSerial)
{
    const juce::SpinLock::ScopedLockType sl (screenLock);
    std::memcpy (ledsOut, ledsPub, sizeof ledsPub);
    if (! screenValid || serial == lastSerial) return false;
    std::memcpy (dst, screen.data(), screen.size() * sizeof (unsigned short));
    lastSerial = serial;
    return true;
}

void GhoulboxProcessor::publishScreen (int numSamples)
{
    samplesSinceFrame += numSamples;
    if (samplesSinceFrame < (int) (hostRate / 30.0)) return;
    // take the lock FIRST: fw->screen_changed() clears the firmware's dirty flag, so a frame must never be
    // consumed without being stored (a static screen would stay stale)
    if (const juce::SpinLock::ScopedTryLockType sl (screenLock); sl.isLocked())
    {
        samplesSinceFrame = 0;
        if (fw->screen_changed()) { fw->copy_screen (screen.data()); screenValid = true; ++serial; }
        fw->leds (ledsPub);
    }
}

// ---------------------------------------------------------------- audio
void GhoulboxProcessor::renderSegment (float* l, float* r, int n)
{
    if (! resampling) { fw->render (l, r, n); return; }

    const double ratio = (double) nativeRate / hostRate;   // firmware samples per host sample
    while (n > 0)
    {
        const int out = juce::jmin (n, kChunk);
        const int need = (int) std::ceil (out * ratio) + 4;
        while (pendCount < need)
        {
            fw->render (pendL.data() + pendCount, pendR.data() + pendCount, 64);
            pendCount += 64;
        }
        const int used = interpL.process (ratio, pendL.data(), l, out);
        interpR.process (ratio, pendR.data(), r, out);
        const int left = pendCount - used;
        std::memmove (pendL.data(), pendL.data() + used, (size_t) left * sizeof (float));
        std::memmove (pendR.data(), pendR.data() + used, (size_t) left * sizeof (float));
        pendCount = left;
        l += out; r += out; n -= out;
    }
}

void GhoulboxProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    if (! ready || ! booted.load()) { midi.clear(); return; }

    if (const juce::SpinLock::ScopedTryLockType sl (stateLock); sl.isLocked() && statePending)
    {
        fw->restore (pendingState.data(), (int) pendingState.size());
        statePending = false;
    }

    if (snapReq.exchange (false))       // getStateInformation asked for a consistent snapshot
    {
        snapLen = fw->state_save (snapBuf.data(), (int) snapBuf.size());
        snapDone.signal();
    }

    const float m = master->get();
    if (m != lastMaster) { fw->master (m); lastMaster = m; }
    applyUiEvents();

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);

    int pos = 0;
    for (const auto meta : midi)
    {
        const int t = juce::jlimit (pos, n, meta.samplePosition);
        if (t > pos) { renderSegment (L + pos, R + pos, t - pos); pos = t; }
        const auto msg = meta.getMessage();
        fw->midi_in (msg.getRawData(), msg.getRawDataSize());
    }
    if (pos < n) renderSegment (L + pos, R + pos, n - pos);
    midi.clear();

    publishScreen (n);
}

// ---------------------------------------------------------------- state
void GhoulboxProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (! ready || ! booted.load()) return;

    snapDone.reset();
    snapReq = true;
    if (snapDone.wait (300))            // audio thread filled snapBuf
    {
        dest.append (snapBuf.data(), (size_t) juce::jmax (0, snapLen.load()));
        return;
    }
    snapReq = false;                    // audio not running: read directly
    std::vector<unsigned char> buf ((size_t) fw->state_max());
    dest.append (buf.data(), (size_t) juce::jmax (0, fw->state_save (buf.data(), (int) buf.size())));
}

void GhoulboxProcessor::setStateInformation (const void* data, int size)
{
    if (! ready || size <= 0) return;
    if (! booted.load())
    {
        bootIfNeeded ((const unsigned char*) data, size);   // flash sectors + music, before the first audio block
        return;
    }
    const juce::SpinLock::ScopedLockType sl (stateLock);    // already running: restore the live music only
    pendingState.assign ((const unsigned char*) data, (const unsigned char*) data + size);
    statePending = true;
}

juce::AudioProcessorEditor* GhoulboxProcessor::createEditor() { return new GhoulboxEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GhoulboxProcessor(); }
