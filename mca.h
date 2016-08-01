#include <stdio.h>
#include <inttypes.h>

typedef struct _f {
  uint8_t *buffer;
  uint32_t size;
  uint32_t samples;
  uint32_t samplerate;
  uint32_t channels;
  uint32_t loop_start;
  uint32_t loop_end;
} WAVE;

WAVE decode(FILE * f);

void loop(WAVE *wave, uint32_t loop_count);
