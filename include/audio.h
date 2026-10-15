#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>
#include <stddef.h>

#define AUDIO_FORMAT_U8   1
#define AUDIO_FORMAT_S16  2

typedef struct {
    uint32_t sample_rate;
    uint8_t  format;   // AUDIO_FORMAT_U8 or AUDIO_FORMAT_S16
    uint8_t  channels; // 1 (mono) or 2 (stereo)
} audio_spec_t;

int  audio_init(void);
int  audio_open(uint32_t sample_rate, uint8_t format, uint8_t channels);
int  audio_write(const void* buffer, size_t bytes);
void audio_close(void);
void audio_play_tone(uint32_t freq_hz, uint32_t duration_ms);
int  audio_is_sb16_detected(void);
void audio_run_tests(void);

#endif
