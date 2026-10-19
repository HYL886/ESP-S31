/* SPDX-License-Identifier: CC0-1.0 */
#include "camera_preview.h"

#include <errno.h>
#include <fcntl.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <unistd.h>
#include "bsp/esp32_s31_korvo_1.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_video_device.h"
#include "esp_video_ioctl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "camera_frame.h"
#include "ui_fonts.h"

#define CAPTURE_WIDTH 640
#define CAPTURE_HEIGHT 480
#define PREVIEW_WIDTH 480
#define PREVIEW_HEIGHT 360
#define PREVIEW_BYTES (PREVIEW_WIDTH * PREVIEW_HEIGHT * 2)
#define CAPTURE_BUFFERS 2
#define FRAME_TIMEOUT_US 3000000

static const char *TAG = "camera_preview";
static lv_obj_t *s_overlay;
static lv_obj_t *s_image;
static lv_obj_t *s_status;
static TaskHandle_t s_worker;
static uint32_t s_generation;
static _Atomic uint32_t s_requested_session;

/* Only the camera worker reads/writes these driver states. BSP 1.0.1 has no
 * matching camera deinit/XCLK handle, so initialize once per boot, even on error. */
static bool s_init_attempted;
static esp_err_t s_init_result;
static bool s_stop_failed;

static bool is_current(uint32_t session)
{
    return atomic_load(&s_requested_session) == session;
}

static void show_status(uint32_t session, const char *text)
{
    if (bsp_display_lock(0)) {
        if (is_current(session) && s_status) lv_label_set_text(s_status, text);
        bsp_display_unlock();
    }
}

static bool publish_frame(uint32_t session, lv_image_dsc_t *frame)
{
    bool published = false;
    if (bsp_display_lock(20)) {
        if (is_current(session) && s_image) {
            /* A descriptor is reused every other frame. Invalidate its old cache. */
            lv_image_cache_drop(frame);
            lv_image_set_src(s_image, frame);
            lv_obj_remove_flag(s_image, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(s_status, "实时预览 640x480 / 屏幕 480x360");
            published = true;
        }
        bsp_display_unlock();
    }
    return published;
}

static void capture_session(uint32_t session)
{
    int fd = -1;
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    bool streaming = false;
    const char *error_text = NULL;
    uint8_t *capture[CAPTURE_BUFFERS] = {0};
    size_t capture_size[CAPTURE_BUFFERS] = {0};
    uint8_t *pixels[2] = {0};
    lv_image_dsc_t frames[2] = {0};

    if (s_stop_failed) {
        show_status(session, "停止采集失败，请重启开发板。");
        return;
    }
    if (!s_init_attempted) {
        s_init_attempted = true;
        bsp_camera_cfg_t config = {0};
        s_init_result = bsp_camera_start(&config);
        ESP_LOGI(TAG, "Camera initialization: %s", esp_err_to_name(s_init_result));
    }
    if (s_init_result != ESP_OK) {
        show_status(session, "初始化失败，请断电检查摄像头排线后重启。");
        return;
    }
    if (!is_current(session)) return;

    fd = open(BSP_CAMERA_DEVICE, O_RDWR);
    if (fd < 0) {
        error_text = "无法打开摄像头，请查看串口日志。";
        goto cleanup;
    }
    struct v4l2_format format = {.type = V4L2_BUF_TYPE_VIDEO_CAPTURE};
    if (ioctl(fd, VIDIOC_G_FMT, &format) != 0) {
        error_text = "无法读取摄像头格式，请重试。";
        goto cleanup;
    }
    if (format.fmt.pix.width != CAPTURE_WIDTH || format.fmt.pix.height != CAPTURE_HEIGHT ||
        format.fmt.pix.pixelformat != V4L2_PIX_FMT_RGB565X) {
        ESP_LOGE(TAG, "Expected RGB565 BE 640x480; got %lux%lu, " V4L2_FMT_STR,
                 (unsigned long)format.fmt.pix.width, (unsigned long)format.fmt.pix.height,
                 V4L2_FMT_STR_ARG(format.fmt.pix.pixelformat));
        error_text = "摄像头格式不符，请检查工程配置后重新编译。";
        goto cleanup;
    }
    size_t stride = format.fmt.pix.bytesperline;
    if (!stride) stride = CAPTURE_WIDTH * 2;

    /* O_NONBLOCK is ignored by esp_video 2.2.0. Bound DQBUF explicitly instead. */
    struct timeval timeout = {.tv_sec = 0, .tv_usec = 250000};
    if (ioctl(fd, VIDIOC_S_DQBUF_TIMEOUT, &timeout) != 0) {
        error_text = "无法设置采集超时，请查看串口日志。";
        goto cleanup;
    }
    for (unsigned i = 0; i < 2; ++i) {
        pixels[i] = heap_caps_malloc(PREVIEW_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!pixels[i]) {
            error_text = "预览内存不足，请重启开发板后重试。";
            goto cleanup;
        }
        frames[i] = (lv_image_dsc_t){
            .header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = LV_COLOR_FORMAT_RGB565,
                       .w = PREVIEW_WIDTH, .h = PREVIEW_HEIGHT, .stride = PREVIEW_WIDTH * 2},
            .data_size = PREVIEW_BYTES,
            .data = pixels[i],
        };
    }
    struct v4l2_requestbuffers request = {
        .count = CAPTURE_BUFFERS, .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
        .memory = V4L2_MEMORY_MMAP,
    };
    if (ioctl(fd, VIDIOC_REQBUFS, &request) != 0 || request.count != CAPTURE_BUFFERS) {
        error_text = "采集内存不足，请查看串口日志后重试。";
        goto cleanup;
    }
    for (unsigned i = 0; i < CAPTURE_BUFFERS; ++i) {
        struct v4l2_buffer buffer = {
            .type = V4L2_BUF_TYPE_VIDEO_CAPTURE, .memory = V4L2_MEMORY_MMAP, .index = i,
        };
        if (ioctl(fd, VIDIOC_QUERYBUF, &buffer) != 0) {
            error_text = "无法获取采集缓冲区，请重试。";
            goto cleanup;
        }
        capture[i] = mmap(NULL, buffer.length, PROT_READ | PROT_WRITE, MAP_SHARED,
                          fd, buffer.m.offset);
        capture_size[i] = buffer.length;
        if (!capture[i] || capture[i] == MAP_FAILED || ioctl(fd, VIDIOC_QBUF, &buffer) != 0) {
            error_text = "无法准备采集缓冲区，请重试。";
            goto cleanup;
        }
    }
    if (!is_current(session)) goto cleanup;
    if (ioctl(fd, VIDIOC_STREAMON, &type) != 0) {
        error_text = "无法开始采集，请查看串口日志后重试。";
        goto cleanup;
    }
    streaming = true;
    ESP_LOGI(TAG, "Preview started: RGB565 BE 640x480 -> RGB565 LE 480x360");
    unsigned back = 0;
    int64_t last_frame_us = esp_timer_get_time();
    while (is_current(session)) {
        struct v4l2_buffer buffer = {
            .type = V4L2_BUF_TYPE_VIDEO_CAPTURE, .memory = V4L2_MEMORY_MMAP,
        };
        if (ioctl(fd, VIDIOC_DQBUF, &buffer) != 0) {
            /* This pinned driver maps an empty timed receive to ESP_FAIL/EPERM. */
            if (errno != EPERM && errno != ETIMEDOUT && errno != EAGAIN) {
                error_text = "采集出错，请查看串口日志后重试。";
                break;
            }
        } else {
            if (buffer.index >= CAPTURE_BUFFERS) {
                error_text = "采集缓冲区编号异常，请重试。";
                break;
            }
            bool valid = (buffer.flags & V4L2_BUF_FLAG_DONE) &&
                         !(buffer.flags & V4L2_BUF_FLAG_ERROR) &&
                         buffer.bytesused <= capture_size[buffer.index];
            if (valid) {
                valid = camera_frame_to_rgb565(capture[buffer.index], buffer.bytesused,
                    CAPTURE_WIDTH, CAPTURE_HEIGHT, stride, pixels[back], PREVIEW_BYTES,
                    PREVIEW_WIDTH, PREVIEW_HEIGHT, BSP_CAMERA_ROTATION == 180);
            }
            /* Return even an invalid frame. Otherwise the driver runs out of buffers. */
            if (ioctl(fd, VIDIOC_QBUF, &buffer) != 0) {
                error_text = "无法归还采集缓冲区，请重试。";
                break;
            }
            if (valid) {
                last_frame_us = esp_timer_get_time();
                if (publish_frame(session, &frames[back])) back ^= 1;
            }
        }
        if (esp_timer_get_time() - last_frame_us >= FRAME_TIMEOUT_US) {
            error_text = "未收到有效画面，请检查排线后重试。";
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }

cleanup:
    if (error_text) ESP_LOGE(TAG, "%s (errno=%d)", error_text, errno);
    if (streaming && ioctl(fd, VIDIOC_STREAMOFF, &type) != 0) {
        /* Do not free DMA buffers if the controller failed to stop. Keep its fd
         * alive and block further sessions until reboot to prevent use-after-free. */
        ESP_LOGE(TAG, "STREAMOFF failed (errno=%d); keeping capture buffers until reboot", errno);
        s_stop_failed = true;
        error_text = "停止采集失败，请重启开发板。";
    } else if (fd >= 0 && close(fd) != 0) {
        ESP_LOGE(TAG, "Camera close failed (errno=%d)", errno);
        s_stop_failed = true;
        error_text = "关闭摄像头失败，请重启开发板。";
    }

    /* No descriptor or pixel memory may outlive its image source reference.
     * LVGL rendering uses this same lock; conversion uses only the back buffer. */
    if (bsp_display_lock(0)) {
        if (is_current(session) && s_image) {
            lv_obj_add_flag(s_image, LV_OBJ_FLAG_HIDDEN);
            lv_image_set_src(s_image, NULL);
            if (error_text) lv_label_set_text(s_status, error_text);
        }
        for (unsigned i = 0; i < 2; ++i) lv_image_cache_drop(&frames[i]);
        bsp_display_unlock();
    }
    for (unsigned i = 0; i < 2; ++i) heap_caps_free(pixels[i]);
    ESP_LOGI(TAG, "Preview ended; display buffers released");
}

static void camera_worker(void *argument)
{
    (void)argument;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        for (;;) {
            uint32_t session = atomic_load(&s_requested_session);
            if (!session) break;
            ulTaskNotifyTake(pdTRUE, 0);
            capture_session(session);
            /* An error waits for Retry. A new generation starts after old cleanup. */
            if (is_current(session)) break;
        }
    }
}

/* These UI callbacks already run with LVGL's display lock held. */
static void start_requested_session(lv_event_t *event)
{
    (void)event;
    if (!s_overlay) return;
    lv_obj_add_flag(s_image, LV_OBJ_FLAG_HIDDEN);
    lv_image_set_src(s_image, NULL);
    lv_label_set_text(s_status, "正在启动摄像头…");
    if (!s_worker && xTaskCreate(camera_worker, "camera_preview", 6144, NULL, 4,
                                &s_worker) != pdPASS) {
        lv_label_set_text(s_status, "无法创建摄像头任务，内存不足。");
        return;
    }
    if (++s_generation == 0) ++s_generation;
    atomic_store(&s_requested_session, s_generation);
    xTaskNotifyGive(s_worker);
}

static void close_preview(lv_event_t *event)
{
    (void)event;
    if (!s_overlay) return;
    lv_obj_t *old_overlay = s_overlay;
    lv_obj_add_flag(old_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_image, LV_OBJ_FLAG_HIDDEN);
    lv_image_set_src(s_image, NULL);
    atomic_store(&s_requested_session, 0);
    s_overlay = s_image = s_status = NULL;
    if (s_worker) xTaskNotifyGive(s_worker);
    lv_obj_delete_async(old_overlay);
}

static lv_obj_t *make_button(const char *text, int x, lv_event_cb_t callback)
{
    lv_obj_t *button = lv_button_create(s_overlay);
    lv_obj_set_size(button, 100, 40);
    lv_obj_align(button, LV_ALIGN_TOP_RIGHT, x, 8);
    lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, NULL);
    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    return button;
}

void camera_preview_open(lv_event_t *event)
{
    (void)event;
    if (s_overlay) return;
    s_overlay = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(s_overlay);
    lv_obj_set_size(s_overlay, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(s_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(s_overlay, lv_color_hex(0x142030), 0);
    lv_obj_set_style_text_font(s_overlay, ui_font_normal(), 0);
    lv_obj_set_style_text_color(s_overlay, lv_color_white(), 0);
    lv_obj_remove_flag(s_overlay, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(s_overlay);
    lv_label_set_text(title, "摄像头");
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 18, 14);
    make_button("重试", -130, start_requested_session);
    make_button("返回", -18, close_preview);

    s_image = lv_image_create(s_overlay);
    lv_obj_set_size(s_image, PREVIEW_WIDTH, PREVIEW_HEIGHT);
    lv_obj_align(s_image, LV_ALIGN_TOP_MID, 0, 60);
    lv_obj_add_flag(s_image, LV_OBJ_FLAG_HIDDEN);
    s_status = lv_label_create(s_overlay);
    lv_obj_set_style_text_font(s_status, ui_font_small(), 0);
    lv_obj_set_width(s_status, 764);
    lv_obj_set_style_text_align(s_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s_status, LV_ALIGN_BOTTOM_MID, 0, -16);
    start_requested_session(NULL);
}
