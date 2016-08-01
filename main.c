#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include "mca.h"

int main(int argc, char const *argv[])
{
    printf("mca2wav by dasding\n");

    if (argc < 2) {
        printf("usage:\nmca2wav inputwave [loopcount]\n");
        return -1;
    }

    uint32_t loop_count = 1;
    if (argc > 2) {
        loop_count = atoi(argv[2]);
    }
    FILE *f = fopen(argv[1], "rb");
    if (f == NULL) {
        printf("File not found: %s\n", argv[1]);
        return -1;
    }

    WAVE wave = decode(f);
    fclose(f);

    loop(&wave, loop_count);

    uint8_t buffer[8];

    char fn[128];
    strcpy(fn, "");
    strcat(fn, argv[1]);
    strcat(fn, ".wav");
    f = fopen(fn, "wb");
    fwrite("RIFF", 1, 4, f);

    uint32_t filesize = wave.size + 36;
    buffer[0] = filesize & 0xFF;
    buffer[1] = filesize >> 8 & 0xFF;
    buffer[2] = filesize >> 16 & 0xFF;
    buffer[3] = filesize >> 24 & 0xFF;
    fwrite(buffer, 1, 4, f);


    fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f);
    fwrite("\x10\x00\x00\x00", 1, 4, f);
    fwrite("\x01\x00", 1, 2, f);

    buffer[0] = wave.channels;
    buffer[1] = 0;
    fwrite(buffer, 1, 2, f);

    buffer[0] = wave.samplerate & 0xFF;
    buffer[1] = wave.samplerate >> 8 & 0xFF;
    buffer[2] = wave.samplerate >> 16 & 0xFF;
    buffer[3] = wave.samplerate >> 24 & 0xFF;
    fwrite(buffer, 1, 4, f);

    uint32_t bytespersec = wave.samplerate * wave.channels * 2;
    buffer[0] = bytespersec & 0xFF;
    buffer[1] = bytespersec >> 8 & 0xFF;
    buffer[2] = bytespersec >> 16 & 0xFF;
    buffer[3] = bytespersec >> 24 & 0xFF;
    fwrite(buffer, 1, 4, f);

    uint16_t blockalign = wave.channels * 2;
    buffer[0] = blockalign & 0xFF;
    buffer[1] = blockalign >> 8 & 0xFF;
    fwrite(buffer, 1, 2, f);

    buffer[0] = 16;
    buffer[1] = 0;
    fwrite(buffer, 1, 2, f);

    fwrite("data", 1, 4, f);

    buffer[0] = wave.size & 0xFF;
    buffer[1] = wave.size >> 8 & 0xFF;
    buffer[2] = wave.size >> 16 & 0xFF;
    buffer[3] = wave.size >> 24 & 0xFF;
    fwrite(buffer, 1, 4, f);


    fwrite(wave.buffer, 1, wave.size, f);
    fclose(f);
}
