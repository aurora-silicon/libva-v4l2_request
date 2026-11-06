/* SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/resource.h>
#include "v4l2_request.h"
static unsigned long reject; static uint32_t capture_format, selected_format; static unsigned int controls_seen;
int __wrap_ioctl(int fd, unsigned long request, ...)
{
 (void)fd; va_list ap; va_start(ap,request); void *arg=va_arg(ap,void *); va_end(ap);
 if(request==reject){errno=EINVAL;return -1;} if(request==VIDIOC_S_FMT){struct v4l2_format *x=arg;selected_format=v4l2r_format_pixelformat(x);if(V4L2_TYPE_IS_MULTIPLANAR(x->type)){x->fmt.pix_mp.width=1280;x->fmt.pix_mp.height=720;}else{x->fmt.pix.width=1280;x->fmt.pix.height=720;}return 0;}
#if HAVE_V4L2_CTRL_HEVC
 if(request==VIDIOC_S_EXT_CTRLS){struct v4l2_ext_controls *cs=arg;assert(cs->count==1&&cs->controls[0].id==V4L2_CID_STATELESS_HEVC_SPS);struct v4l2_ctrl_hevc_sps *s=cs->controls[0].ptr;assert(s->bit_depth_luma_minus8==2&&s->bit_depth_chroma_minus8==2);assert(s->pic_width_in_luma_samples==1280&&s->pic_height_in_luma_samples==720);controls_seen++;return 0;}
#endif
 assert(request==VIDIOC_ENUM_FMT);struct v4l2_fmtdesc *x=arg;if(x->index){errno=EINVAL;return -1;}x->pixelformat=capture_format;return 0;
}
static int entrypoints(struct VADriverContext *v,VAProfile p){VAEntrypoint e[4];int n=-1;assert(v4l2r_QueryConfigEntrypoints(v,p,e,&n)==VA_STATUS_SUCCESS);if(n==1)assert(e[0]==VAEntrypointVLD);return n;}
int main(void)
{
 struct rlimit core={0,0};assert(!setrlimit(RLIMIT_CORE,&core));
#if HAVE_V4L2_PIX_FMT_P010 && HAVE_V4L2_CTRL_HEVC
 capture_format=V4L2_PIX_FMT_P010;assert(v4l2r_probe_hevc_10bit(123,V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE));assert(selected_format==V4L2_PIX_FMT_HEVC_SLICE&&controls_seen==1);capture_format=V4L2_PIX_FMT_NV12;assert(!v4l2r_probe_hevc_10bit(123,V4L2_BUF_TYPE_VIDEO_OUTPUT));reject=VIDIOC_S_EXT_CTRLS;assert(!v4l2r_probe_hevc_10bit(123,V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE));reject=VIDIOC_S_FMT;assert(!v4l2r_probe_hevc_10bit(123,V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE));reject=0;
#endif
#if HAVE_V4L2_PIX_FMT_P010 && HAVE_V4L2_CTRL_VP9
 capture_format=V4L2_PIX_FMT_P010;assert(v4l2r_probe_vp9_10bit(123,V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE));assert(selected_format==V4L2_PIX_FMT_VP9_FRAME);capture_format=V4L2_PIX_FMT_NV12;assert(!v4l2r_probe_vp9_10bit(123,V4L2_BUF_TYPE_VIDEO_OUTPUT));reject=VIDIOC_S_FMT;assert(!v4l2r_probe_vp9_10bit(123,V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE));reject=0;struct v4l2r_driver d={.nb_decoders=1,.converter_probed=true};struct VADriverContext v={.pDriverData=&d};assert(!pthread_mutex_init(&d.mutex,NULL));d.decoders[0].pixelformats[0]=V4L2_PIX_FMT_VP9_FRAME;d.decoders[0].nb_pixelformats=1;assert(entrypoints(&v,VAProfileVP9Profile0)==1);assert(entrypoints(&v,VAProfileVP9Profile2)==0);d.decoders[0].vp9_10bit=true;assert(entrypoints(&v,VAProfileVP9Profile2)==1);pthread_mutex_destroy(&d.mutex);
#endif
 return 0;
}
