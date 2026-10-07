/*
 * SensorHub Layout Regression Checks
 *
 * Tests the DT layout ABI and calibration image padding on the host.
 *
 * Authors:
 * worryzu <worryzu@gmail.com> @LinearTeam
 *
 * Copyright (C) 2026 Evarentha
 * SPDX-License-Identifier: MIT
 */
#include <assert.h>
#define main loader_entry
#include "sensorhub-boot.c"
#undef main

int main(void)
{
    struct load_segment regions[2] = {
        { "pm_sys", 0x800000, 0x3b000 }, { "cali_lib", 0x83b000, 0x5000 }
    };
    assert(valid_layout(regions));
    regions[0].base = 0x1000000; regions[0].size = 0x20000;
    regions[1].base = 0x1020000; regions[1].size = 0x3000;
    assert(valid_layout(regions)); /* No compiled-in board address. */
    regions[1].base++;
    assert(!valid_layout(regions));
    regions[1].base -= 2;
    assert(!valid_layout(regions));
    regions[1].base++;
    regions[1].size = 0;
    assert(!valid_layout(regions));
    regions[0].base = 0xffffe000; regions[0].size = 0x1000;
    regions[1].base = 0xfffff000; regions[1].size = 0x2000;
    assert(!valid_layout(regions));
    regions[1].size = 0x1000;
    assert(valid_layout(regions));
    regions[0].name[0] = 'x';
    assert(!valid_layout(regions));

    FILE *file = tmpfile();
    uint8_t *image;
    size_t length;
    assert(file);
    int fd = fileno(file);
    assert(read_cali_image(fd, 100, &image, &length));
    uint8_t input[71]; memset(input, 0x5a, sizeof(input));
    assert(write(fd, input, sizeof(input)) == sizeof(input));
    assert(!read_cali_image(fd, 100, &image, &length));
    assert(length == 71 && !memcmp(input, image, 71));
    for (size_t i = 71; i < 100; i++) assert(image[i] == 0);
    free(image);
    assert(read_cali_image(fd, 70, &image, &length));
    assert(!read_cali_image(fd, 71, &image, &length));
    free(image); fclose(file);
    puts("PASS layout: relocated resources, gaps, overlap, 32-bit overflow, names, empty and padding bounds");
    return 0;
}
