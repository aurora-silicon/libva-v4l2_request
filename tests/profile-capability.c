/* SPDX-License-Identifier: GPL-3.0-or-later */
/* 10-bit profiles need their coded format and a usable 10-bit 4:2:0
 * CAPTURE format on the same decoder. */
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/resource.h>

#include "v4l2_request.h"

static unsigned long reject;
static uint32_t capture_format;
static uint32_t selected_format;
static unsigned int controls_seen;

int __wrap_ioctl(int fd, unsigned long request, ...)
{
    (void)fd;
    va_list ap;
    va_start(ap, request);
    void *arg = va_arg(ap, void *);
    va_end(ap);

    if (request == reject) {
        errno = EINVAL;
        return -1;
    }
    if (request == VIDIOC_S_FMT) {
        struct v4l2_format *format = arg;
        selected_format = v4l2r_format_pixelformat(format);
        if (V4L2_TYPE_IS_MULTIPLANAR(format->type)) {
            format->fmt.pix_mp.width = 1280;
            format->fmt.pix_mp.height = 720;
        } else {
            format->fmt.pix.width = 1280;
            format->fmt.pix.height = 720;
        }
        return 0;
    }
#if HAVE_V4L2_CTRL_HEVC
    if (request == VIDIOC_S_EXT_CTRLS) {
        struct v4l2_ext_controls *controls = arg;
        assert(controls->count == 1);
        assert(controls->controls[0].id == V4L2_CID_STATELESS_HEVC_SPS);
        struct v4l2_ctrl_hevc_sps *sps = controls->controls[0].ptr;
        assert(sps->bit_depth_luma_minus8 == 2);
        assert(sps->bit_depth_chroma_minus8 == 2);
        assert(sps->pic_width_in_luma_samples == 1280);
        assert(sps->pic_height_in_luma_samples == 720);
        controls_seen++;
        return 0;
    }
#endif

    assert(request == VIDIOC_ENUM_FMT);
    struct v4l2_fmtdesc *format = arg;
    if (format->index) {
        errno = EINVAL;
        return -1;
    }
    format->pixelformat = capture_format;
    return 0;
}

#if HAVE_V4L2_CTRL_VP9
static int entrypoints(struct VADriverContext *va, VAProfile profile)
{
    VAEntrypoint entry[4];
    int count = -1;
    assert(v4l2r_QueryConfigEntrypoints(va, profile, entry, &count) == VA_STATUS_SUCCESS);
    if (count == 1)
        assert(entry[0] == VAEntrypointVLD);
    return count;
}
#endif

int main(void)
{
    struct rlimit core = {0, 0};
    assert(!setrlimit(RLIMIT_CORE, &core));

#if HAVE_V4L2_PIX_FMT_P010 && HAVE_V4L2_CTRL_HEVC
    capture_format = V4L2_PIX_FMT_P010;
    assert(v4l2r_probe_hevc_10bit(123, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE));
    assert(selected_format == V4L2_PIX_FMT_HEVC_SLICE && controls_seen == 1);
    capture_format = V4L2_PIX_FMT_NV12;
    assert(!v4l2r_probe_hevc_10bit(123, V4L2_BUF_TYPE_VIDEO_OUTPUT));
    reject = VIDIOC_S_EXT_CTRLS;
    assert(!v4l2r_probe_hevc_10bit(123, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE));
    reject = VIDIOC_S_FMT;
    assert(!v4l2r_probe_hevc_10bit(123, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE));
    reject = 0;
#endif

#if HAVE_V4L2_PIX_FMT_P010 && HAVE_V4L2_CTRL_VP9
    capture_format = V4L2_PIX_FMT_P010;
    assert(v4l2r_probe_vp9_10bit(123, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE));
    assert(selected_format == V4L2_PIX_FMT_VP9_FRAME);
    capture_format = V4L2_PIX_FMT_NV12;
    assert(!v4l2r_probe_vp9_10bit(123, V4L2_BUF_TYPE_VIDEO_OUTPUT));
    reject = VIDIOC_S_FMT;
    assert(!v4l2r_probe_vp9_10bit(123, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE));
    reject = 0;

    struct v4l2r_driver drv = {.nb_decoders = 1, .converter_probed = true};
    struct VADriverContext va = {.pDriverData = &drv};
    assert(!pthread_mutex_init(&drv.mutex, NULL));
    drv.decoders[0].pixelformats[0] = V4L2_PIX_FMT_VP9_FRAME;
    drv.decoders[0].nb_pixelformats = 1;
    assert(entrypoints(&va, VAProfileVP9Profile0) == 1);
    assert(entrypoints(&va, VAProfileVP9Profile2) == 0);
    drv.decoders[0].vp9_10bit = true;
    assert(entrypoints(&va, VAProfileVP9Profile2) == 1);
    pthread_mutex_destroy(&drv.mutex);
#endif
    return 0;
}
