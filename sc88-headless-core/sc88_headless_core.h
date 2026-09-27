#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum
{
    SC88_HEADLESS_SAMPLE_RATE = 32000,
    SC88_HEADLESS_CONTROL_ROM_SIZE = 0x80000,
    SC88_HEADLESS_WAVE_ROM_COUNT = 4,
    SC88_HEADLESS_WAVE_ROM_SIZE = 0x200000
};

typedef struct sc88_headless_context sc88_headless_context;

typedef struct sc88_headless_rom_set
{
    const uint8_t* control;
    size_t control_size;

    const uint8_t* wave[SC88_HEADLESS_WAVE_ROM_COUNT];
    size_t wave_size[SC88_HEADLESS_WAVE_ROM_COUNT];
} sc88_headless_rom_set;

sc88_headless_context* sc88_headless_create(
    const sc88_headless_rom_set* roms
);

void sc88_headless_destroy(
    sc88_headless_context* context
);

int sc88_headless_is_valid(
    const sc88_headless_context* context
);

int sc88_headless_boot(
    sc88_headless_context* context
);

int sc88_headless_play_short_message(
    sc88_headless_context* context,
    uint8_t port,
    uint8_t status,
    uint8_t data1,
    uint8_t data2
);

void sc88_headless_render_int16(
    sc88_headless_context* context,
    int16_t* stereo,
    uint32_t frames
);

uint32_t sc88_headless_sample_rate(void);

size_t sc88_headless_get_display_text(
    const sc88_headless_context* context,
    char* output,
    size_t capacity
);

size_t sc88_headless_midi_backlog(
    const sc88_headless_context* context
);

#ifdef __cplusplus
}
#endif
