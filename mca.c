#include "mca.h"
#include <stdlib.h>
#include <string.h>

#define FRAME_SIZE 8
#define SAMPLES_PER_FRAME 14
#define INTERLEAVE_BLOCK_SIZE 0x100
#define COEF_SPACING 0x30
#define META_SIZE 0x14

uint32_t HEADER_SIZE = 0x30;

uint32_t SAMPLES_PER_BLOCK = (INTERLEAVE_BLOCK_SIZE / FRAME_SIZE) * SAMPLES_PER_FRAME;

int32_t nibble_to_int[16] = {0,1,2,3,4,5,6,7,-8,-7,-6,-5,-4,-3,-2,-1};

int16_t clamp16(int32_t val) {
    if (val > 32767) {
        return 32767;
    }
    if (val < -32768) {
        return -32768;
    }
    return val;
}

typedef struct _channel {
    uint32_t offset;
    uint8_t * buffer;
    uint32_t buf_idx;
    int16_t adpcm_coef[16];
    int16_t adpcm_history[2];
} Channel;

uint8_t read_byte(FILE *f, uint32_t offset) {
    fseek(f, offset, SEEK_SET);
    uint8_t byte1 = (uint8_t) (fgetc(f) & 0xFF);
    return byte1;
}

uint16_t read_word(FILE *f, uint32_t offset) {
    fseek(f, offset, SEEK_SET);
    uint16_t byte1 = (uint16_t) (fgetc(f) & 0xFF);
    uint16_t byte2 = (uint16_t) (fgetc(f) & 0xFF);
    return byte1 + (byte2 << 8);
}


int16_t read_sword(FILE *f, uint32_t offset) {
    fseek(f, offset, SEEK_SET);
    int16_t byte1 = (int16_t) (fgetc(f) & 0xFF);
    int16_t byte2 = (int16_t) (fgetc(f) & 0xFF);
    return byte1 + (byte2 << 8);
}


uint32_t read_dword(FILE *f, uint32_t offset) {
    fseek(f, offset, SEEK_SET);
    uint16_t byte1 = (uint16_t) (fgetc(f) & 0xFF);
    uint16_t byte2 = (uint16_t) (fgetc(f) & 0xFF);
    uint16_t byte3 = (uint16_t) (fgetc(f) & 0xFF);
    uint16_t byte4 = (uint16_t) (fgetc(f) & 0xFF);
    return byte1 + (byte2 << 8) + (byte3 << 16) + (byte4 << 24);
}

WAVE decode(FILE *f) {
    fseek(f, 0, SEEK_END);
    uint32_t file_size = ftell(f);
    rewind(f);
    uint16_t version     = read_word(f, 0x04);
    uint8_t num_channels = read_byte(f, 0x08);
    uint32_t num_samples = read_dword(f, 0x0C);
    uint16_t samplerate  = read_word(f, 0x10);
    uint32_t loop_start  = read_dword(f, 0x14);
    uint32_t loop_end    = read_dword(f, 0x18);

    uint32_t data_size   = read_dword(f, 0x20);
    uint32_t length   = read_dword(f, 0x24);
    uint16_t num_meta    = read_word(f, 0x28);
    uint32_t data_offset = read_dword(f, 0x2C);


    uint32_t coef_offset;

    if (version == 5) {
        if (data_offset > 0x500) {
            HEADER_SIZE = 0x38;
            data_offset = file_size - data_size;
        }
        coef_offset = HEADER_SIZE + num_meta * META_SIZE;
    } else if (version == 4) {
        data_offset = file_size - data_size;
        coef_offset = data_offset - COEF_SPACING * num_channels;
    } else {
        printf("version unsupported\n");
        exit(1);
    }

    Channel channel[num_channels];

    for (int i = 0; i < num_channels; ++i) {
        channel[i].offset = data_offset + INTERLEAVE_BLOCK_SIZE * i;
        channel[i].buffer = (uint8_t *) malloc(num_samples * 2);
        channel[i].buf_idx = 0;
        channel[i].adpcm_history[0] = 0;
        channel[i].adpcm_history[1] = 0;
        for (int j = 0; j < 16; ++j) {
            channel[i].adpcm_coef[j] = read_sword(f, coef_offset + i * COEF_SPACING + j * 2);
            // printf("%d\n", channel[i].adpcm_coef[j]);
        }
    }


    uint32_t samples_written = 0;
    uint32_t samples_written_in_block = 0;

    while (samples_written < num_samples) {
        int32_t samples_todo = SAMPLES_PER_FRAME;

        if (samples_written + samples_todo > num_samples) {
            samples_todo = num_samples - samples_written;
        }

        uint32_t frames_written = samples_written_in_block / SAMPLES_PER_FRAME;

        for (int chan_idx = 0; chan_idx < num_channels; ++chan_idx) {
            uint32_t frame_offset = frames_written * FRAME_SIZE + channel[chan_idx].offset;

            uint8_t header = read_byte(f, frame_offset);
            uint32_t scale = 1 << (header & 0xF);
            uint8_t coef_idx = (header >> 4) & 0xF;
            int16_t hist1 = channel[chan_idx].adpcm_history[0];
            int16_t hist2 = channel[chan_idx].adpcm_history[1];
            int16_t coef1 = channel[chan_idx].adpcm_coef[coef_idx * 2];
            int16_t coef2 = channel[chan_idx].adpcm_coef[coef_idx * 2 + 1];

            for (int i = 0; i < samples_todo; ++i) {
                uint8_t sample_byte = read_byte(f, frames_written * FRAME_SIZE + channel[chan_idx].offset + 1 + i / 2);
                int32_t nibble;
                if (i % 2 == 1) {
                    nibble = nibble_to_int[sample_byte & 0xF];
                } else {
                    nibble = nibble_to_int[sample_byte >> 4];
                }

                int32_t sample = (nibble * scale) << 11;
                sample += 1024;
                sample += (coef1 * hist1 + coef2 * hist2);
                sample = sample >> 11;
                sample = clamp16(sample);

                channel[chan_idx].buffer[channel[chan_idx].buf_idx] = sample & 0xFF;
                channel[chan_idx].buffer[channel[chan_idx].buf_idx + 1] = sample >> 8 & 0xFF;
                channel[chan_idx].buf_idx += 2;

                hist2 = hist1;
                hist1 = sample;
            }

            channel[chan_idx].adpcm_history[0] = hist1;
            channel[chan_idx].adpcm_history[1] = hist2;
        }

        samples_written          += samples_todo;
        samples_written_in_block += samples_todo;

        if (samples_written % 1000 == 0) {
            printf("\rDECODE [ %d / %d ]          ", samples_written, num_samples);
        }


        if (samples_written_in_block == SAMPLES_PER_BLOCK) {
            for (int i = 0; i < num_channels; ++i) {
                channel[i].offset += INTERLEAVE_BLOCK_SIZE * num_channels;
            }
            samples_written_in_block = 0;
        }
    }

    printf("\nDONE! %d samples processed\n", num_samples);

    uint8_t *final = (uint8_t *) malloc(num_samples * 2 * num_channels);
    uint32_t final_idx = 0;
    for (int i = 0; i < num_samples; ++i) {
        for (int chan_idx = 0; chan_idx < num_channels; ++chan_idx) {
            final[final_idx + 0] = channel[chan_idx].buffer[i*2];
            final[final_idx + 1] = channel[chan_idx].buffer[i*2 + 1];
            final_idx += 2;
        }
    }

    for (int chan_idx = 0; chan_idx < num_channels; ++chan_idx) {
        free(channel[chan_idx].buffer);
    }

    WAVE wave;
    wave.buffer = final;
    wave.size = num_samples * 2 * num_channels;

    wave.samples = num_samples;
    wave.samplerate = samplerate;
    wave.channels = num_channels;

    wave.loop_start = loop_start;
    wave.loop_end = loop_end;
    return wave;
}


void loop(WAVE *wave, uint32_t loop_count) {
    uint32_t preloop_length = wave->loop_start * 2 * wave->channels;
    uint8_t *preloop = (uint8_t*) malloc(preloop_length);

    uint32_t inloop_length = (wave->loop_end - wave->loop_start) * 2 * wave->channels;
    uint8_t *inloop = (uint8_t*) malloc(inloop_length);

    uint32_t postloop_length = (wave->samples - wave->loop_end) * 2 * wave->channels;
    uint8_t *postloop = (uint8_t*) malloc(postloop_length);

    uint8_t *newbuffer = (uint8_t*) malloc(preloop_length + inloop_length * loop_count + postloop_length);

    memcpy(preloop, wave->buffer, preloop_length);
    memcpy(inloop, &wave->buffer[preloop_length], inloop_length);
    memcpy(postloop, &wave->buffer[preloop_length + inloop_length], postloop_length);

    memcpy(newbuffer, preloop, preloop_length);

    uint32_t sample_idx = 0;
    for (int i = 0; i < loop_count; ++i) {
        sample_idx = preloop_length + inloop_length * i;
        memcpy(&newbuffer[sample_idx], inloop, inloop_length);
    }
    sample_idx = preloop_length + inloop_length * loop_count;
    memcpy(&newbuffer[sample_idx], postloop, postloop_length);

    wave->size = (preloop_length + inloop_length * loop_count + postloop_length);
    wave->samples = wave->loop_start + (wave->loop_end - wave->loop_start) * loop_count + (wave->samples - wave->loop_end);
    free(wave->buffer);
    wave->buffer = newbuffer;
}
