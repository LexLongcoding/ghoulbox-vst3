/*
 * gb_host.c - wraps web/emu/felucca_web.c (the browser emulator's firmware
 * glue) for the VST3. SPDX-License-Identifier: GPL-3.0-only
 *
 * felucca_web.c is included, not copied: it is the same translation unit, so
 * its statics (nor, web_fb, NB, NE, panel, ...) are visible here, and the
 * firmware stays byte-identical to the browser build.
 * Build flags (see CMakeLists.txt): -ffp-contract=off -std=gnu99 -w
 *   -I<root>/build/gen -I<root>/firmware/src -I<root>/web/emu
 */
#include <stdint.h>
#include <string.h>
#include "gb_host.h"
#include "felucca_web.c"

#define GBX_SECTOR  4096u
#define GBX_SECTORS ((uint32_t) (sizeof nor / GBX_SECTOR))
#define GBX_KEYS    27u

static uint32_t gbx_key_mask, gbx_btn_mask, gbx_last_draws = 0xFFFFFFFFu;

/* Panel changes wait in a queue, each held >= 24 ms of DEVICE time before the next goes in, exactly
 * as web/emu/worklet.js does: the firmware reads the contacts once per UI frame, so a click whose down and
 * up land in one audio block would otherwise be lost. */
#define GBX_HOLD_MS 24u
#define GBX_QN 256u
typedef struct { uint8_t is_key; uint32_t mask; } gbx_in_t;
static gbx_in_t gbx_q[GBX_QN];
static uint32_t gbx_qh, gbx_qt, gbx_next_ok;

static void gbx_apply (const gbx_in_t *e) { if (e->is_key) web_keys (e->mask); else web_buttons (e->mask); }
static void gbx_push (int is_key, uint32_t mask)
{
    gbx_in_t e;
    e.is_key = (uint8_t) is_key; e.mask = mask;
    if (gbx_qt - gbx_qh >= GBX_QN) { gbx_apply (&e); return; }   /* full: don't lose the newest state */
    gbx_q[gbx_qt++ % GBX_QN] = e;
}
static void gbx_pump (void)
{
    const uint32_t now = web_now_ms();
    while (gbx_qh != gbx_qt && (int32_t) (now - gbx_next_ok) >= 0)
    {
        gbx_apply (&gbx_q[gbx_qh++ % GBX_QN]);
        gbx_next_ok = now + GBX_HOLD_MS;
    }
}

int gbx_init (void)   { return (int) web_sample_rate(); }
int gbx_booted (void) { return (int) web_booted; }
int gbx_num_keys (void)     { return (int) GBX_KEYS; }
int gbx_num_buttons (void)  { return (int) NB; }
int gbx_num_encoders (void) { return (int) NE; }

/* ---- state blob: "GBX1", u32 n, n x {u32 sector, 4096 bytes}, u32 plen, project ---- */
static uint32_t rd32 (const uint8_t *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t) p[3] << 24); }
static void wr32 (uint8_t *p, uint32_t v) { p[0] = (uint8_t) v; p[1] = (uint8_t) (v >> 8); p[2] = (uint8_t) (v >> 16); p[3] = (uint8_t) (v >> 24); }

static int gbx_parse (const uint8_t *s, int len, int apply, const uint8_t **proj, uint32_t *plen)
{
    const uint8_t *p, *e;
    uint32_t n, i;
    *proj = 0; *plen = 0;
    if (!s || len < 12 || memcmp (s, "GBX1", 4)) return 0;
    p = s + 4; e = s + len;
    n = rd32 (p); p += 4;
    for (i = 0; i < n; i++)
    {
        uint32_t idx;
        if (e - p < (long) (4 + GBX_SECTOR)) return 0;
        idx = rd32 (p);
        if (idx >= GBX_SECTORS) return 0;
        if (apply) memcpy (nor + idx * GBX_SECTOR, p + 4, GBX_SECTOR);
        p += 4 + GBX_SECTOR;
    }
    if (e - p < 4) return 0;
    *plen = rd32 (p); p += 4;
    if ((uint32_t) (e - p) < *plen) { *plen = 0; return 0; }
    *proj = *plen ? p : 0;
    return 1;
}

int gbx_state_max (void) { return (int) (8u + GBX_SECTORS * (4u + GBX_SECTOR) + 4u + web_project_size()); }

int gbx_state_save (unsigned char *dst, int cap)
{
    uint8_t *p = dst, *cnt;
    uint32_t s, n = 0, i;
    const uint8_t *proj;
    if (!web_booted || cap < gbx_state_max()) return 0;
    memcpy (p, "GBX1", 4); p += 4;
    cnt = p; p += 4;
    for (s = 0; s < GBX_SECTORS; s++)
    {
        const uint8_t *q = nor + s * GBX_SECTOR;
        for (i = 0; i < GBX_SECTOR && q[i] == 0xFF; i++) {}
        if (i == GBX_SECTOR) continue;                 /* erased: skip */
        wr32 (p, s); memcpy (p + 4, q, GBX_SECTOR); p += 4 + GBX_SECTOR; n++;
    }
    wr32 (cnt, n);
    proj = web_project_capture();
    if (proj) { wr32 (p, web_project_size()); p += 4; memcpy (p, proj, web_project_size()); p += web_project_size(); }
    else      { wr32 (p, 0); p += 4; }
    return (int) (p - dst);
}

int gbx_boot (const unsigned char *s, int len)
{
    const uint8_t *proj; uint32_t plen; int ok;
    if (web_booted) return 1;
    web_nor_erase();
    ok = s && gbx_parse (s, len, 0, &proj, &plen);       /* validate, then apply */
    if (ok) gbx_parse (s, len, 1, &proj, &plen);
    web_boot();
    if (ok && proj && plen <= web_project_import_capacity())
    {
        memcpy (web_project_import_buffer(), proj, plen);
        web_project_restore (plen);
    }
    return 1;
}

int gbx_restore (const unsigned char *s, int len)
{
    const uint8_t *proj; uint32_t plen;
    if (!web_booted || !gbx_parse (s, len, 0, &proj, &plen) || !proj || plen > web_project_import_capacity()) return 0;
    memcpy (web_project_import_buffer(), proj, plen);
    return (int) web_project_restore (plen);
}

/* ---- audio ---- */
void gbx_render (float *l, float *r, int frames)
{
    while (frames > 0)
    {
        int c = frames > 128 ? 128 : frames;      /* a worklet quantum: the panel queue is fed between slices */
        gbx_pump();
        web_render ((uint32_t) c);
        memcpy (l, web_out_l(), (size_t) c * sizeof (float));
        memcpy (r, web_out_r(), (size_t) c * sizeof (float));
        l += c; r += c; frames -= c;
    }
}

/* ---- input ---- */
void gbx_midi_in (const unsigned char *b, int len)
{
    if (len < 1) return;
    web_midi (b[0], len > 1 ? b[1] : 0, len > 2 ? b[2] : 0);
}
void gbx_key (int i, int down)
{
    if (i < 0 || i >= (int) GBX_KEYS) return;
    if (down) gbx_key_mask |= 1u << i; else gbx_key_mask &= ~(1u << i);
    gbx_push (1, gbx_key_mask);
}
void gbx_button (int id, int down)
{
    if (id < 0 || id >= (int) NB) return;
    if (down) gbx_btn_mask |= 1u << id; else gbx_btn_mask &= ~(1u << id);
    gbx_push (0, gbx_btn_mask);
}
void gbx_encoder (int role, int delta) { if (role >= 0 && role < (int) NE) web_enc ((uint32_t) role, delta); }
void gbx_master (float v) { web_master ((uint32_t) (v * 1023.0f + 0.5f)); }

/* ---- screen: felucca_web.c stores RGB565 byte-swapped (panel order) ---- */
int gbx_screen_changed (void)
{
    if (web_draws == gbx_last_draws) return 0;
    gbx_last_draws = web_draws;
    return 1;
}
void gbx_copy_screen (unsigned short *dst)
{
    uint32_t i;
    for (i = 0; i < 240u * 240u; i++) { uint16_t v = web_fb[i]; dst[i] = (unsigned short) ((v >> 8) | (v << 8)); }
}

/* ---- LEDs (the same exports the browser page reads) ---- */
void gbx_leds (unsigned int out[8])
{
    out[0] = web_lit_buttons();   out[1] = web_lit_keys();
    out[2] = web_dim_buttons();   out[3] = web_dim_keys();
    out[4] = web_dim_level();
    out[5] = web_breath_buttons(); out[6] = web_breath_keys();
    out[7] = web_mid_keys();
}
