/*
 * gb_host.h - the plugin's view of the Ghoulbox firmware.
 *
 * Implemented in gb_host.c, which compiles the browser emulator's
 * web/emu/felucca_web.c UNCHANGED (same translation unit) and wraps its
 * web_* exports. Threading: gbx_render / input / state functions run on the
 * AUDIO thread; gbx_boot runs once on a non-realtime thread.
 *
 * The firmware keeps its state in globals, so each plugin instance loads its own copy of the library.
 */
#ifndef GB_HOST_H
#define GB_HOST_H
#ifdef __cplusplus
extern "C" {
#endif

/* The firmware is built as a shared library (GBX_BUILD_DLL); the plugin loads one private copy per
 * instance and binds these symbols at run time (see GbFirmware.h), so each instance has its own globals. */
#if defined(GBX_BUILD_DLL)
  #if defined(_WIN32)
    #define GBX_API __declspec(dllexport)
  #else
    #define GBX_API __attribute__((visibility("default")))
  #endif
#else
  #define GBX_API
#endif

#define GB_SCREEN_W 240
#define GB_SCREEN_H 240

GBX_API int  gbx_init (void);                 /* returns the firmware's native sample rate (Hz) */
GBX_API int  gbx_booted (void);
/* Boots the firmware once. `state` (may be NULL) is a blob from gbx_state_save:
 * its flash sectors are loaded BEFORE boot, its live music after. */
GBX_API int  gbx_boot (const unsigned char *state, int len);

GBX_API void gbx_render (float *l, float *r, int frames);
GBX_API void gbx_midi_in (const unsigned char *bytes, int len);   /* channel messages */

GBX_API void gbx_key (int index, int down);   /* 0 = F3 ... 26 = G5 (27 keys) */
GBX_API void gbx_button (int id, int down);   /* id = bit in the firmware's printed-label order */
GBX_API void gbx_encoder (int role, int delta);
GBX_API void gbx_master (float v01);          /* the MASTER pot (may be set before gbx_boot) */
/* LEDs as the browser page draws them: [0] lit buttons (bit 14 = PLAY green), [1] lit keys,
 * [2] dim buttons, [3] dim keys, [4] dim-low flag, [5] breathing buttons, [6] breathing keys, [7] mid keys */
GBX_API void gbx_leds (unsigned int out[8]);

GBX_API int  gbx_num_keys (void);
GBX_API int  gbx_num_buttons (void);
GBX_API int  gbx_num_encoders (void);

GBX_API int  gbx_screen_changed (void);                    /* true if redrawn since last call */
GBX_API void gbx_copy_screen (unsigned short *dst);        /* 240x240 RGB565, normal byte order */

GBX_API int  gbx_state_max (void);
GBX_API int  gbx_state_save (unsigned char *dst, int cap); /* returns bytes written */
GBX_API int  gbx_restore (const unsigned char *src, int len); /* after boot: live music only */

#ifdef __cplusplus
}
#endif
#endif
