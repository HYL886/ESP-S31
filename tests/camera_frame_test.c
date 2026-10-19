/* SPDX-License-Identifier: CC0-1.0 */
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "camera_frame.h"

static void colours_and_rotation(void)
{
    /* Red, green / blue, white, in camera byte order. */
    const uint8_t source[] = {0xf8, 0x00, 0x07, 0xe0, 0x00, 0x1f, 0xff, 0xff};
    const uint8_t expected[] = {0x00, 0xf8, 0xe0, 0x07, 0x1f, 0x00, 0xff, 0xff};
    const uint8_t rotated[] = {0xff, 0xff, 0x1f, 0x00, 0xe0, 0x07, 0x00, 0xf8};
    uint8_t output[8];
    assert(camera_frame_to_rgb565(source, sizeof(source), 2, 2, 4,
                                 output, sizeof(output), 2, 2, false));
    assert(memcmp(output, expected, sizeof(output)) == 0);
    assert(camera_frame_to_rgb565(source, sizeof(source), 2, 2, 4,
                                 output, sizeof(output), 2, 2, true));
    assert(memcmp(output, rotated, sizeof(output)) == 0);
}

static void downscale_with_padding(void)
{
    /* Four pixels per row, one padding pixel between rows. Last row needs no pad. */
    const uint8_t source[] = {
        0x11, 0x01, 0x22, 0x02, 0x33, 0x03, 0x44, 0x04, 0xaa, 0xaa,
        0x55, 0x05, 0x66, 0x06, 0x77, 0x07, 0x88, 0x08,
    };
    uint8_t output[6] = {0};
    assert(camera_frame_to_rgb565(source, sizeof(source), 4, 2, 10,
                                 output, sizeof(output), 3, 1, false));
    const uint8_t expected[] = {0x01, 0x11, 0x02, 0x22, 0x03, 0x33};
    assert(memcmp(output, expected, sizeof(output)) == 0);
    assert(camera_frame_to_rgb565(source, sizeof(source), 4, 2, 10,
                                 output, sizeof(output), 3, 1, true));
    const uint8_t rotated[] = {0x08, 0x88, 0x07, 0x77, 0x06, 0x66};
    assert(memcmp(output, rotated, sizeof(output)) == 0);
}

static void invalid_sizes_do_not_write(void)
{
    const uint8_t source[8] = {0};
    uint8_t output[8];
    memset(output, 0xa5, sizeof(output));
    assert(!camera_frame_to_rgb565(NULL, 8, 2, 2, 4, output, 8, 2, 2, false));
    assert(!camera_frame_to_rgb565(source, 8, 2, 2, 4, NULL, 8, 2, 2, false));
    assert(!camera_frame_to_rgb565(source, 8, 0, 2, 4, output, 8, 2, 2, false));
    assert(!camera_frame_to_rgb565(source, 8, 2, 0, 4, output, 8, 2, 2, false));
    assert(!camera_frame_to_rgb565(source, 8, 2, 2, 4, output, 8, 0, 2, false));
    assert(!camera_frame_to_rgb565(source, 8, 2, 2, 4, output, 8, 2, 0, false));
    assert(!camera_frame_to_rgb565(source, 8, 2, 2, 4, output, 8, 3, 2, false));
    assert(!camera_frame_to_rgb565(source, 8, 2, 2, 3, output, 8, 2, 2, false));
    assert(!camera_frame_to_rgb565(source, 7, 2, 2, 4, output, 8, 2, 2, false));
    assert(!camera_frame_to_rgb565(source, 8, 2, 2, 4, output, 7, 2, 2, false));
    assert(!camera_frame_to_rgb565(source, SIZE_MAX, 2, 2, SIZE_MAX,
                                 output, 8, 2, 2, false));
    assert(!camera_frame_to_rgb565(source, 8, UINT_MAX, 2, 4, output, 8, 2, 2, false));
    for (unsigned i = 0; i < sizeof(output); ++i) assert(output[i] == 0xa5);
}

static void full_preview_bounds(void)
{
    static uint8_t source[640 * 480 * 2];
    static uint8_t output[480 * 360 * 2 + 2];
    for (size_t i = 0; i < sizeof(source); i += 2) {
        source[i] = 0xf8;
        source[i + 1] = 0x00;
    }
    output[0] = output[sizeof(output) - 1] = 0xa5;
    assert(camera_frame_to_rgb565(source, sizeof(source), 640, 480, 1280,
                                 output + 1, sizeof(output) - 2, 480, 360, true));
    for (size_t i = 1; i < sizeof(output) - 1; i += 2) {
        assert(output[i] == 0x00 && output[i + 1] == 0xf8);
    }
    assert(output[0] == 0xa5 && output[sizeof(output) - 1] == 0xa5);
}

int main(void)
{
    colours_and_rotation();
    downscale_with_padding();
    invalid_sizes_do_not_write();
    full_preview_bounds();
    puts("camera_frame: all tests passed");
    return 0;
}
