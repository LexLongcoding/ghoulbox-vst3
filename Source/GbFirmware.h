// GbFirmware - one private, run-time-loaded copy of the Ghoulbox firmware per plugin instance.
//
// The firmware keeps its state in C globals. Compiled into the plugin, that would make every instance
// in a host share one synth. Instead it is built as ghoulbox_fw.(dll|so|dylib) next to the plugin binary,
// and each instance copies that library to a uniquely named temp file and loads the COPY: a module
// loaded from a different file has its own globals, so instances are fully independent.
// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <juce_core/juce_core.h>
#include <memory>
#include "gb_host.h"

class GbFirmware
{
public:
    /** Loads a fresh copy of the firmware. Returns nullptr and fills `error` on failure. */
    static std::unique_ptr<GbFirmware> create (juce::String& error);
    ~GbFirmware();

    // same names and meaning as the gbx_ C API in gb_host.h
    int  init()                                          { return pInit(); }
    int  booted()                                        { return pBooted(); }
    int  boot (const unsigned char* s, int n)            { return pBoot (s, n); }
    void render (float* l, float* r, int frames)         { pRender (l, r, frames); }
    void midi_in (const unsigned char* b, int n)         { pMidi (b, n); }
    void key (int i, int down)                           { pKey (i, down); }
    void button (int id, int down)                       { pButton (id, down); }
    void encoder (int role, int delta)                   { pEncoder (role, delta); }
    void master (float v01)                              { pMaster (v01); }
    void leds (unsigned int out[8])                      { pLeds (out); }
    int  num_keys()                                      { return pNumKeys(); }
    int  num_buttons()                                   { return pNumButtons(); }
    int  num_encoders()                                  { return pNumEncoders(); }
    int  screen_changed()                                { return pScreenChanged(); }
    void copy_screen (unsigned short* dst)               { pCopyScreen (dst); }
    int  state_max()                                     { return pStateMax(); }
    int  state_save (unsigned char* dst, int cap)        { return pStateSave (dst, cap); }
    int  restore (const unsigned char* s, int n)         { return pRestore (s, n); }

private:
    GbFirmware() = default;
    template <typename Fn> bool bind (Fn& f, const char* name)
    {
        f = reinterpret_cast<Fn> (lib.getFunction (name));
        return f != nullptr;
    }

    juce::DynamicLibrary lib;
    juce::File tempCopy;

    decltype (&gbx_init)          pInit = nullptr;
    decltype (&gbx_booted)        pBooted = nullptr;
    decltype (&gbx_boot)          pBoot = nullptr;
    decltype (&gbx_render)        pRender = nullptr;
    decltype (&gbx_midi_in)       pMidi = nullptr;
    decltype (&gbx_key)           pKey = nullptr;
    decltype (&gbx_button)        pButton = nullptr;
    decltype (&gbx_encoder)       pEncoder = nullptr;
    decltype (&gbx_master)        pMaster = nullptr;
    decltype (&gbx_leds)          pLeds = nullptr;
    decltype (&gbx_num_keys)      pNumKeys = nullptr;
    decltype (&gbx_num_buttons)   pNumButtons = nullptr;
    decltype (&gbx_num_encoders)  pNumEncoders = nullptr;
    decltype (&gbx_screen_changed) pScreenChanged = nullptr;
    decltype (&gbx_copy_screen)   pCopyScreen = nullptr;
    decltype (&gbx_state_max)     pStateMax = nullptr;
    decltype (&gbx_state_save)    pStateSave = nullptr;
    decltype (&gbx_restore)       pRestore = nullptr;

    JUCE_DECLARE_NON_COPYABLE (GbFirmware)
};
