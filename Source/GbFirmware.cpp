// SPDX-License-Identifier: GPL-3.0-only
#include "GbFirmware.h"
#include <mutex>

#if JUCE_WINDOWS
 static const char* const kLibExt = ".dll";
#elif JUCE_MAC
 static const char* const kLibExt = ".dylib";
#else
 static const char* const kLibExt = ".so";
#endif

std::unique_ptr<GbFirmware> GbFirmware::create (juce::String& error)
{
    // the library sits next to the plugin binary inside the bundle
    const auto src = juce::File::getSpecialLocation (juce::File::currentExecutableFile)
                         .getSiblingFile (juce::String ("ghoulbox_fw") + kLibExt);
    if (! src.existsAsFile())
    {
        error = "Firmware library missing.\nCopy the whole Ghoulbox.vst3 folder,\nnot just the file inside it:\n" + src.getFullPathName();
        return nullptr;
    }

    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("GhoulboxFW");
    dir.createDirectory();

    // once per process: remove copies left behind by a crash (a copy still loaded elsewhere can't be deleted)
    static std::once_flag swept;
    std::call_once (swept, [&dir]
    {
        for (auto& f : dir.findChildFiles (juce::File::findFiles, false, "gbfw_*"))
            f.deleteFile();
    });

    const auto copy = dir.getChildFile ("gbfw_" + juce::String::toHexString (juce::Random::getSystemRandom().nextInt64()) + kLibExt);
    if (! src.copyFileTo (copy))
    {
        error = "Could not copy the firmware library to\n" + copy.getFullPathName();
        return nullptr;
    }

    std::unique_ptr<GbFirmware> fw (new GbFirmware());
    fw->tempCopy = copy;                       // the destructor removes it, also if loading fails below

    if (! fw->lib.open (copy.getFullPathName()))
    {
        error = "Could not load the firmware library.";
        return nullptr;
    }

    const bool ok = fw->bind (fw->pInit, "gbx_init") && fw->bind (fw->pBooted, "gbx_booted")
                 && fw->bind (fw->pBoot, "gbx_boot") && fw->bind (fw->pRender, "gbx_render")
                 && fw->bind (fw->pMidi, "gbx_midi_in") && fw->bind (fw->pKey, "gbx_key")
                 && fw->bind (fw->pButton, "gbx_button") && fw->bind (fw->pEncoder, "gbx_encoder")
                 && fw->bind (fw->pMaster, "gbx_master") && fw->bind (fw->pLeds, "gbx_leds")
                 && fw->bind (fw->pNumKeys, "gbx_num_keys") && fw->bind (fw->pNumButtons, "gbx_num_buttons")
                 && fw->bind (fw->pNumEncoders, "gbx_num_encoders") && fw->bind (fw->pScreenChanged, "gbx_screen_changed")
                 && fw->bind (fw->pCopyScreen, "gbx_copy_screen") && fw->bind (fw->pStateMax, "gbx_state_max")
                 && fw->bind (fw->pStateSave, "gbx_state_save") && fw->bind (fw->pRestore, "gbx_restore");
    if (! ok)
    {
        error = "The firmware library is missing functions\n(rebuild the plugin).";
        return nullptr;
    }
    return fw;
}

GbFirmware::~GbFirmware()
{
    lib.close();                               // unload first, then the file can be deleted
    if (tempCopy != juce::File())
        tempCopy.deleteFile();
}
