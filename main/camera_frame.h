/* SPDX-License-Identifier: CC0-1.0 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Nearest-neighbour downscale: RGB565 BE camera bytes -> RGB565 LE LCD bytes.
 * Source and destination must not overlap. Dimensions are 1..65535 pixels.
 * src_stride includes any padding between source rows; output is tightly packed.
 * Invalid sizes return false before writing to dst. */
bool camera_frame_to_rgb565(const uint8_t *src, size_t src_size,
                           unsigned src_width, unsigned src_height, size_t src_stride,
                           uint8_t *dst, size_t dst_size,
                           unsigned dst_width, unsigned dst_height, bool rotate_180);
