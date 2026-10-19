/* SPDX-License-Identifier: CC0-1.0 */
#include "camera_frame.h"

bool camera_frame_to_rgb565(const uint8_t *src, size_t src_size,
                           unsigned src_width, unsigned src_height, size_t src_stride,
                           uint8_t *dst, size_t dst_size,
                           unsigned dst_width, unsigned dst_height, bool rotate_180)
{
    if (!src || !dst || !src_width || !src_height || !dst_width || !dst_height ||
        src_width > UINT16_MAX || src_height > UINT16_MAX ||
        dst_width > src_width || dst_height > src_height) {
        return false;
    }
    size_t row_bytes = (size_t)src_width * 2;
    size_t dst_row_bytes = (size_t)dst_width * 2;
    if (src_stride < row_bytes || src_size < row_bytes ||
        (src_height > 1 && src_stride > (src_size - row_bytes) / (src_height - 1)) ||
        dst_height > dst_size / dst_row_bytes) {
        return false;
    }

    for (unsigned y = 0; y < dst_height; ++y) {
        unsigned sy = y * src_height / dst_height;
        if (rotate_180) sy = src_height - 1 - sy;
        const uint8_t *row = src + (size_t)sy * src_stride;
        for (unsigned x = 0; x < dst_width; ++x) {
            unsigned sx = x * src_width / dst_width;
            if (rotate_180) sx = src_width - 1 - sx;
            const uint8_t *pixel = row + (size_t)sx * 2;
            *dst++ = pixel[1];
            *dst++ = pixel[0];
        }
    }
    return true;
}
