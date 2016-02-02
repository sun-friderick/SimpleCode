/*
 * ffplay : Simple Media Player based on the FFmpeg libraries
 * Copyright (c) 2003 Fabrice Bellard
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "config.h"
#include <inttypes.h>
#include <math.h>
#include <limits.h>
#include "libavutil/avstring.h"
#include "libavutil/colorspace.h"
#include "libavutil/pixdesc.h"
#include "libavutil/imgutils.h"
#include "libavutil/dict.h"
#include "libavutil/parseutils.h"
#include "libavutil/samplefmt.h"
#include "libavutil/avassert.h"
#include "libavformat/avformat.h"
#include "libavdevice/avdevice.h"
#include "libswscale/swscale.h"
#include "libavcodec/audioconvert.h"
#include "libavutil/opt.h"
#include "libavcodec/avfft.h"

#if CONFIG_AVFILTER
# include "libavfilter/avcodec.h"
# include "libavfilter/avfilter.h"
# include "libavfilter/avfiltergraph.h"
# include "libavfilter/vsink_buffer.h"
#endif

#include <SDL.h>
#include <SDL_thread.h>

#include "cmdutils.h"

#include <unistd.h>
#include <assert.h>

const char program_name[] = "ffplay";
const int program_birth_year = 2003;

#define MAX_QUEUE_SIZE (15 * 1024 * 1024)
#define MIN_AUDIOQ_SIZE (20 * 16 * 1024)
#define MIN_FRAMES 5

/* SDL audio buffer size, in samples. Should be small to have precise
   A/V sync as SDL does not have hardware buffer fullness info. */
#define SDL_AUDIO_BUFFER_SIZE 1024

/* no AV sync correction is done if below the AV sync threshold */
#define AV_SYNC_THRESHOLD 0.01
/* no AV correction is done if too big error */
#define AV_NOSYNC_THRESHOLD 10.0

#define FRAME_SKIP_FACTOR 0.05

/* maximum audio speed change to get correct sync */
#define SAMPLE_CORRECTION_PERCENT_MAX 10

/* we use about AUDIO_DIFF_AVG_NB A-V differences to make the average */
#define AUDIO_DIFF_AVG_NB   20

/* NOTE: the size must be big enough to compensate the hardware audio buffersize size */
#define SAMPLE_ARRAY_SIZE (2*65536)

static int sws_flags = SWS_BICUBIC;

typedef struct PacketQueue {
    AVPacketList *first_pkt, *last_pkt;
    int nb_packets;
    int size;
    int abort_request;
    SDL_mutex *mutex;
    SDL_cond *cond;
} PacketQueue;

#define VIDEO_PICTURE_QUEUE_SIZE 2
#define SUBPICTURE_QUEUE_SIZE 4

typedef struct VideoPicture {
    double pts;                                  ///<presentation time stamp for this picture
    double target_clock;                         ///<av_gettime() time at which this should be displayed ideally
    int64_t pos;                                 ///<byte position in file
    SDL_Overlay *bmp;
    int width, height; /* source height & width */
    int allocated;
    enum PixelFormat pix_fmt;

#if CONFIG_AVFILTER
    AVFilterBufferRef *picref;
#endif
} VideoPicture;

typedef struct SubPicture {
    double pts; /* presentation time stamp for this picture */
    AVSubtitle sub;
} SubPicture;

enum {
    AV_SYNC_AUDIO_MASTER, /* default choice */
    AV_SYNC_VIDEO_MASTER,
    AV_SYNC_EXTERNAL_CLOCK, /* synchronize to an external clock */
};

typedef struct VideoState {
    SDL_Thread *read_tid;
    SDL_Thread *video_tid;
    SDL_Thread *refresh_tid;
    AVInputFormat *iformat;
    int no_background;
    int abort_request;
    int paused;
    int last_paused;
    int seek_req;
    int seek_flags;
    int64_t seek_pos;
    int64_t seek_rel;
    int read_pause_return;
    AVFormatContext *ic;

    int audio_stream;

    int av_sync_type;
    double external_clock; /* external clock base */
    int64_t external_clock_time;

    double audio_clock;
    double audio_diff_cum; /* used for AV difference average computation */
    double audio_diff_avg_coef;
    double audio_diff_threshold;
    int audio_diff_avg_count;
    AVStream *audio_st;
    PacketQueue audioq;
    int audio_hw_buf_size;
    /* samples output by the codec. we reserve more space for avsync
       compensation */
    DECLARE_ALIGNED(16,uint8_t,audio_buf1)[(AVCODEC_MAX_AUDIO_FRAME_SIZE * 3) / 2];
    DECLARE_ALIGNED(16,uint8_t,audio_buf2)[(AVCODEC_MAX_AUDIO_FRAME_SIZE * 3) / 2];
    uint8_t *audio_buf;
    unsigned int audio_buf_size; /* in bytes */
    int audio_buf_index; /* in bytes */
    AVPacket audio_pkt_temp;
    AVPacket audio_pkt;
    enum AVSampleFormat audio_src_fmt;
    AVAudioConvert *reformat_ctx;

    enum ShowMode {
        SHOW_MODE_NONE = -1, SHOW_MODE_VIDEO = 0, SHOW_MODE_WAVES, SHOW_MODE_RDFT, SHOW_MODE_NB
    } show_mode;
    int16_t sample_array[SAMPLE_ARRAY_SIZE];
    int sample_array_index;
    int last_i_start;
    RDFTContext *rdft;
    int rdft_bits;
    FFTSample *rdft_data;
    int xpos;

    SDL_Thread *subtitle_tid;
    int subtitle_stream;
    int subtitle_stream_changed;
    AVStream *subtitle_st;
    PacketQueue subtitleq;
    SubPicture subpq[SUBPICTURE_QUEUE_SIZE];
    int subpq_size, subpq_rindex, subpq_windex;
    SDL_mutex *subpq_mutex;
    SDL_cond *subpq_cond;

    double frame_timer;
    double frame_last_pts;
    double frame_last_delay;
    double video_clock;                          ///<pts of last decoded frame / predicted pts of next decoded frame
    int video_stream;
    AVStream *video_st;
    PacketQueue videoq;
    double video_current_pts;                    ///<current displayed pts (different from video_clock if frame fifos are used)
    double video_current_pts_drift;              ///<video_current_pts - time (av_gettime) at which we updated video_current_pts - used to have running video pts
    int64_t video_current_pos;                   ///<current displayed file pos
    VideoPicture pictq[VIDEO_PICTURE_QUEUE_SIZE];
    int pictq_size, pictq_rindex, pictq_windex;
    SDL_mutex *pictq_mutex;
    SDL_cond *pictq_cond;
#if !CONFIG_AVFILTER
    struct SwsContext *img_convert_ctx;
#endif

    char filename[1024];
    int width, height, xleft, ytop;

#if CONFIG_AVFILTER
    AVFilterContext *out_video_filter;          ///<the last filter in the video chain
#endif

    float skip_frames;
    float skip_frames_index;
    int refresh;
} VideoState;

static int opt_help(const char *opt, const char *arg);

/* options specified by the user */
static AVInputFormat *file_iformat;
static const char *input_filename;
static const char *window_title;
static int fs_screen_width;
static int fs_screen_height;
static int screen_width = 0;
static int screen_height = 0;
static int frame_width = 0;
static int frame_height = 0;
static enum PixelFormat frame_pix_fmt = PIX_FMT_NONE;
static int audio_disable;
static int video_disable;
static int wanted_stream[AVMEDIA_TYPE_NB]={
    [AVMEDIA_TYPE_AUDIO]=-1,
    [AVMEDIA_TYPE_VIDEO]=-1,
    [AVMEDIA_TYPE_SUBTITLE]=-1,
};
static int seek_by_bytes=-1;
static int display_disable;
static int show_status = 1;
static int av_sync_type = AV_SYNC_AUDIO_MASTER;
static int64_t start_time = AV_NOPTS_VALUE;
static int64_t duration = AV_NOPTS_VALUE;
static int step = 0;
static int thread_count = 1;
static int workaround_bugs = 1;
static int fast = 0;
static int genpts = 0;
static int lowres = 0;
static int idct = FF_IDCT_AUTO;
static enum AVDiscard skip_frame= AVDISCARD_DEFAULT;
static enum AVDiscard skip_idct= AVDISCARD_DEFAULT;
static enum AVDiscard skip_loop_filter= AVDISCARD_DEFAULT;
static int error_recognition = FF_ER_CAREFUL;
static int error_concealment = 3;
static int decoder_reorder_pts= -1;
static int autoexit;
static int exit_on_keydown;
static int exit_on_mousedown;
static int loop=1;
static int framedrop=-1;
static enum ShowMode show_mode = SHOW_MODE_NONE;

static int rdftspeed=20;
#if CONFIG_AVFILTER
static char *vfilters = NULL;
#endif

/* current context */
static int is_full_screen;
static VideoState *cur_stream;
static int64_t audio_callback_time;

static AVPacket flush_pkt;

#define FF_ALLOC_EVENT   (SDL_USEREVENT)
#define FF_REFRESH_EVENT (SDL_USEREVENT + 1)
#define FF_QUIT_EVENT    (SDL_USEREVENT + 2)

static SDL_Surface *screen;

//========================================= PacketQueue ====================================================
static int packet_queue_put(PacketQueue *q, AVPacket *pkt)
{
    AVPacketList *pkt1;

    /* duplicate the packet */
    if (pkt!=&flush_pkt && av_dup_packet(pkt) < 0)
        return -1;

    pkt1 = av_malloc(sizeof(AVPacketList));
    if (!pkt1)
        return -1;
    pkt1->pkt = *pkt;
    pkt1->next = NULL;


    SDL_LockMutex(q->mutex);

    if (!q->last_pkt)

        q->first_pkt = pkt1;
    else
        q->last_pkt->next = pkt1;
    q->last_pkt = pkt1;
    q->nb_packets++;
    q->size += pkt1->pkt.size + sizeof(*pkt1);
    /* XXX: should duplicate packet data in DV case */
    SDL_CondSignal(q->cond);

    SDL_UnlockMutex(q->mutex);
    return 0;
}

/* packet queue handling */
static void packet_queue_init(PacketQueue *q)
{
    memset(q, 0, sizeof(PacketQueue));
    q->mutex = SDL_CreateMutex();
    q->cond = SDL_CreateCond();
    packet_queue_put(q, &flush_pkt);
}

static void packet_queue_flush(PacketQueue *q)
{
    AVPacketList *pkt, *pkt1;

    SDL_LockMutex(q->mutex);
    for(pkt = q->first_pkt; pkt != NULL; pkt = pkt1) {
        pkt1 = pkt->next;
        av_free_packet(&pkt->pkt);
        av_freep(&pkt);
    }
    q->last_pkt = NULL;
    q->first_pkt = NULL;
    q->nb_packets = 0;
    q->size = 0;
    SDL_UnlockMutex(q->mutex);
}

static void packet_queue_end(PacketQueue *q)
{
    packet_queue_flush(q);
    SDL_DestroyMutex(q->mutex);
    SDL_DestroyCond(q->cond);
}

static void packet_queue_abort(PacketQueue *q)
{
    SDL_LockMutex(q->mutex);

    q->abort_request = 1;

    SDL_CondSignal(q->cond);

    SDL_UnlockMutex(q->mutex);
}

/* return < 0 if aborted, 0 if no packet and > 0 if packet.  */
static int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block)
{
    AVPacketList *pkt1;
    int ret;

    SDL_LockMutex(q->mutex);

    for(;;) {
        if (q->abort_request) {
            ret = -1;
            break;
        }

        pkt1 = q->first_pkt;
        if (pkt1) {
            q->first_pkt = pkt1->next;
            if (!q->first_pkt)
                q->last_pkt = NULL;
            q->nb_packets--;
            q->size -= pkt1->pkt.size + sizeof(*pkt1);
            *pkt = pkt1->pkt;
            av_free(pkt1);
            ret = 1;
            break;
        } else if (!block) {
            ret = 0;
            break;
        } else {
            SDL_CondWait(q->cond, q->mutex);
        }
    }
    SDL_UnlockMutex(q->mutex);
    return ret;
}
//========================================================================================================



static inline void fill_rectangle(SDL_Surface *screen,
                                  int x, int y, int w, int h, int color)
{
    SDL_Rect rect;
    rect.x = x;
    rect.y = y;
    rect.w = w;
    rect.h = h;
    SDL_FillRect(screen, &rect, color);
}

#define ALPHA_BLEND(a, oldp, newp, s)\
((((oldp << s) * (255 - (a))) + (newp * (a))) / (255 << s))

#define RGBA_IN(r, g, b, a, s)\
{\
    unsigned int v = ((const uint32_t *)(s))[0];\
    a = (v >> 24) & 0xff;\
    r = (v >> 16) & 0xff;\
    g = (v >> 8) & 0xff;\
    b = v & 0xff;\
}

#define YUVA_IN(y, u, v, a, s, pal)\
{\
    unsigned int val = ((const uint32_t *)(pal))[*(const uint8_t*)(s)];\
    a = (val >> 24) & 0xff;\
    y = (val >> 16) & 0xff;\
    u = (val >> 8) & 0xff;\
    v = val & 0xff;\
}

#define YUVA_OUT(d, y, u, v, a)\
{\
    ((uint32_t *)(d))[0] = (a << 24) | (y << 16) | (u << 8) | v;\
}


#define BPP 1

static void blend_subrect(AVPicture *dst, const AVSubtitleRect *rect, int imgw, int imgh)
{
    int wrap, wrap3, width2, skip2;
    int y, u, v, a, u1, v1, a1, w, h;
    uint8_t *lum, *cb, *cr;
    const uint8_t *p;
    const uint32_t *pal;
    int dstx, dsty, dstw, dsth;

    dstw = av_clip(rect->w, 0, imgw);
    dsth = av_clip(rect->h, 0, imgh);
    dstx = av_clip(rect->x, 0, imgw - dstw);
    dsty = av_clip(rect->y, 0, imgh - dsth);
    lum = dst->data[0] + dsty * dst->linesize[0];
    cb = dst->data[1] + (dsty >> 1) * dst->linesize[1];
    cr = dst->data[2] + (dsty >> 1) * dst->linesize[2];

    width2 = ((dstw + 1) >> 1) + (dstx & ~dstw & 1);
    skip2 = dstx >> 1;
    wrap = dst->linesize[0];
    wrap3 = rect->pict.linesize[0];
    p = rect->pict.data[0];
    pal = (const uint32_t *)rect->pict.data[1];  /* Now in YCrCb! */

    if (dsty & 1) {
        lum += dstx;
        cb += skip2;
        cr += skip2;

        if (dstx & 1) {
            YUVA_IN(y, u, v, a, p, pal);
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);
            cb[0] = ALPHA_BLEND(a >> 2, cb[0], u, 0);
            cr[0] = ALPHA_BLEND(a >> 2, cr[0], v, 0);
            cb++;
            cr++;
            lum++;
            p += BPP;
        }
        for(w = dstw - (dstx & 1); w >= 2; w -= 2) {
            YUVA_IN(y, u, v, a, p, pal);
            u1 = u;
            v1 = v;
            a1 = a;
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);

            YUVA_IN(y, u, v, a, p + BPP, pal);
            u1 += u;
            v1 += v;
            a1 += a;
            lum[1] = ALPHA_BLEND(a, lum[1], y, 0);
            cb[0] = ALPHA_BLEND(a1 >> 2, cb[0], u1, 1);
            cr[0] = ALPHA_BLEND(a1 >> 2, cr[0], v1, 1);
            cb++;
            cr++;
            p += 2 * BPP;
            lum += 2;
        }
        if (w) {
            YUVA_IN(y, u, v, a, p, pal);
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);
            cb[0] = ALPHA_BLEND(a >> 2, cb[0], u, 0);
            cr[0] = ALPHA_BLEND(a >> 2, cr[0], v, 0);
            p++;
            lum++;
        }
        p += wrap3 - dstw * BPP;
        lum += wrap - dstw - dstx;
        cb += dst->linesize[1] - width2 - skip2;
        cr += dst->linesize[2] - width2 - skip2;
    }
    for(h = dsth - (dsty & 1); h >= 2; h -= 2) {
        lum += dstx;
        cb += skip2;
        cr += skip2;

        if (dstx & 1) {
            YUVA_IN(y, u, v, a, p, pal);
            u1 = u;
            v1 = v;
            a1 = a;
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);
            p += wrap3;
            lum += wrap;
            YUVA_IN(y, u, v, a, p, pal);
            u1 += u;
            v1 += v;
            a1 += a;
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);
            cb[0] = ALPHA_BLEND(a1 >> 2, cb[0], u1, 1);
            cr[0] = ALPHA_BLEND(a1 >> 2, cr[0], v1, 1);
            cb++;
            cr++;
            p += -wrap3 + BPP;
            lum += -wrap + 1;
        }
        for(w = dstw - (dstx & 1); w >= 2; w -= 2) {
            YUVA_IN(y, u, v, a, p, pal);
            u1 = u;
            v1 = v;
            a1 = a;
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);

            YUVA_IN(y, u, v, a, p + BPP, pal);
            u1 += u;
            v1 += v;
            a1 += a;
            lum[1] = ALPHA_BLEND(a, lum[1], y, 0);
            p += wrap3;
            lum += wrap;

            YUVA_IN(y, u, v, a, p, pal);
            u1 += u;
            v1 += v;
            a1 += a;
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);

            YUVA_IN(y, u, v, a, p + BPP, pal);
            u1 += u;
            v1 += v;
            a1 += a;
            lum[1] = ALPHA_BLEND(a, lum[1], y, 0);

            cb[0] = ALPHA_BLEND(a1 >> 2, cb[0], u1, 2);
            cr[0] = ALPHA_BLEND(a1 >> 2, cr[0], v1, 2);

            cb++;
            cr++;
            p += -wrap3 + 2 * BPP;
            lum += -wrap + 2;
        }
        if (w) {
            YUVA_IN(y, u, v, a, p, pal);
            u1 = u;
            v1 = v;
            a1 = a;
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);
            p += wrap3;
            lum += wrap;
            YUVA_IN(y, u, v, a, p, pal);
            u1 += u;
            v1 += v;
            a1 += a;
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);
            cb[0] = ALPHA_BLEND(a1 >> 2, cb[0], u1, 1);
            cr[0] = ALPHA_BLEND(a1 >> 2, cr[0], v1, 1);
            cb++;
            cr++;
            p += -wrap3 + BPP;
            lum += -wrap + 1;
        }
        p += wrap3 + (wrap3 - dstw * BPP);
        lum += wrap + (wrap - dstw - dstx);
        cb += dst->linesize[1] - width2 - skip2;
        cr += dst->linesize[2] - width2 - skip2;
    }
    /* handle odd height */
    if (h) {
        lum += dstx;
        cb += skip2;
        cr += skip2;

        if (dstx & 1) {
            YUVA_IN(y, u, v, a, p, pal);
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);
            cb[0] = ALPHA_BLEND(a >> 2, cb[0], u, 0);
            cr[0] = ALPHA_BLEND(a >> 2, cr[0], v, 0);
            cb++;
            cr++;
            lum++;
            p += BPP;
        }
        for(w = dstw - (dstx & 1); w >= 2; w -= 2) {
            YUVA_IN(y, u, v, a, p, pal);
            u1 = u;
            v1 = v;
            a1 = a;
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);

            YUVA_IN(y, u, v, a, p + BPP, pal);
            u1 += u;
            v1 += v;
            a1 += a;
            lum[1] = ALPHA_BLEND(a, lum[1], y, 0);
            cb[0] = ALPHA_BLEND(a1 >> 2, cb[0], u, 1);
            cr[0] = ALPHA_BLEND(a1 >> 2, cr[0], v, 1);
            cb++;
            cr++;
            p += 2 * BPP;
            lum += 2;
        }
        if (w) {
            YUVA_IN(y, u, v, a, p, pal);
            lum[0] = ALPHA_BLEND(a, lum[0], y, 0);
            cb[0] = ALPHA_BLEND(a >> 2, cb[0], u, 0);
            cr[0] = ALPHA_BLEND(a >> 2, cr[0], v, 0);
        }
    }
}

static void free_subpicture(SubPicture *sp)
{
    avsubtitle_free(&sp->sub);
}

/**



**/
static void video_image_display(VideoState *is)
{
    VideoPicture *vp;
    SubPicture *sp;
    AVPicture pict;
    float aspect_ratio;
    int width, height, x, y;
    SDL_Rect rect;
    int i;

    vp = &is->pictq[is->pictq_rindex];
    if (vp->bmp) {
#if CONFIG_AVFILTER
         if (vp->picref->video->sample_aspect_ratio.num == 0)
             aspect_ratio = 0;
         else
             aspect_ratio = av_q2d(vp->picref->video->sample_aspect_ratio);
#else

        /* XXX: use variable in the frame */
        if (is->video_st->sample_aspect_ratio.num)
            aspect_ratio = av_q2d(is->video_st->sample_aspect_ratio);
        else if (is->video_st->codec->sample_aspect_ratio.num)
            aspect_ratio = av_q2d(is->video_st->codec->sample_aspect_ratio);
        else
            aspect_ratio = 0;
#endif
        if (aspect_ratio <= 0.0)
            aspect_ratio = 1.0;
        aspect_ratio *= (float)vp->width / (float)vp->height;

        if (is->subtitle_st) {
            if (is->subpq_size > 0) {
                sp = &is->subpq[is->subpq_rindex];

                if (vp->pts >= sp->pts + ((float) sp->sub.start_display_time / 1000)) {
                    SDL_LockYUVOverlay (vp->bmp);

                    pict.data[0] = vp->bmp->pixels[0];
                    pict.data[1] = vp->bmp->pixels[2];
                    pict.data[2] = vp->bmp->pixels[1];

                    pict.linesize[0] = vp->bmp->pitches[0];
                    pict.linesize[1] = vp->bmp->pitches[2];
                    pict.linesize[2] = vp->bmp->pitches[1];

                    for (i = 0; i < sp->sub.num_rects; i++)
                        blend_subrect(&pict, sp->sub.rects[i],
                                      vp->bmp->w, vp->bmp->h);

                    SDL_UnlockYUVOverlay (vp->bmp);
                }
            }
        }


        /* XXX: we suppose the screen has a 1.0 pixel ratio */
        height = is->height;
        width = ((int)rint(height * aspect_ratio)) & ~1;
        if (width > is->width) {
            width = is->width;
            height = ((int)rint(width / aspect_ratio)) & ~1;
        }
        x = (is->width - width) / 2;
        y = (is->height - height) / 2;
        is->no_background = 0;
        rect.x = is->xleft + x;
        rect.y = is->ytop  + y;
        rect.w = FFMAX(width,  1);
        rect.h = FFMAX(height, 1);
        SDL_DisplayYUVOverlay(vp->bmp, &rect);
    }
}

/* get the current audio output buffer size, in samples. With SDL, we
   cannot have a precise information */
static int audio_write_get_buf_size(VideoState *is)
{
    return is->audio_buf_size - is->audio_buf_index;
}

static inline int compute_mod(int a, int b)
{
    return a < 0 ? a%b + b : a%b;
}

/**
    输出音视频：
    
    

**/
static void video_audio_display(VideoState *s)
{
    int i, i_start, x, y1, y, ys, delay, n, nb_display_channels;
    int ch, channels, h, h2, bgcolor, fgcolor;
    int16_t time_diff;
    int rdft_bits, nb_freq;

    for(rdft_bits=1; (1<<rdft_bits)<2*s->height; rdft_bits++)
        ;
    nb_freq= 1<<(rdft_bits-1);

    /* compute display index : center on currently output samples */
    channels = s->audio_st->codec->channels;
    nb_display_channels = channels;
    if (!s->paused) {
        int data_used= s->show_mode == SHOW_MODE_WAVES ? s->width : (2*nb_freq);
        n = 2 * channels;
        delay = audio_write_get_buf_size(s);
        delay /= n;

        /* to be more precise, we take into account the time spent since
           the last buffer computation */
        if (audio_callback_time) {
            time_diff = av_gettime() - audio_callback_time;
            delay -= (time_diff * s->audio_st->codec->sample_rate) / 1000000;
        }

        delay += 2*data_used;
        if (delay < data_used)
            delay = data_used;

        i_start= x = compute_mod(s->sample_array_index - delay * channels, SAMPLE_ARRAY_SIZE);
        if (s->show_mode == SHOW_MODE_WAVES) {
            h= INT_MIN;
            for(i=0; i<1000; i+=channels){
                int idx= (SAMPLE_ARRAY_SIZE + x - i) % SAMPLE_ARRAY_SIZE;
                int a= s->sample_array[idx];
                int b= s->sample_array[(idx + 4*channels)%SAMPLE_ARRAY_SIZE];
                int c= s->sample_array[(idx + 5*channels)%SAMPLE_ARRAY_SIZE];
                int d= s->sample_array[(idx + 9*channels)%SAMPLE_ARRAY_SIZE];
                int score= a-d;
                if(h<score && (b^c)<0){
                    h= score;
                    i_start= idx;
                }
            }
        }

        s->last_i_start = i_start;
    } else {
        i_start = s->last_i_start;
    }

    bgcolor = SDL_MapRGB(screen->format, 0x00, 0x00, 0x00);
    if (s->show_mode == SHOW_MODE_WAVES) {
        fill_rectangle(screen,
                       s->xleft, s->ytop, s->width, s->height,
                       bgcolor);

        fgcolor = SDL_MapRGB(screen->format, 0xff, 0xff, 0xff);

        /* total height for one channel */
        h = s->height / nb_display_channels;
        /* graph height / 2 */
        h2 = (h * 9) / 20;
        for(ch = 0;ch < nb_display_channels; ch++) {
            i = i_start + ch;
            y1 = s->ytop + ch * h + (h / 2); /* position of center line */
            for(x = 0; x < s->width; x++) {
                y = (s->sample_array[i] * h2) >> 15;
                if (y < 0) {
                    y = -y;
                    ys = y1 - y;
                } else {
                    ys = y1;
                }
                fill_rectangle(screen,
                               s->xleft + x, ys, 1, y,
                               fgcolor);
                i += channels;
                if (i >= SAMPLE_ARRAY_SIZE)
                    i -= SAMPLE_ARRAY_SIZE;
            }
        }

        fgcolor = SDL_MapRGB(screen->format, 0x00, 0x00, 0xff);

        for(ch = 1;ch < nb_display_channels; ch++) {
            y = s->ytop + ch * h;
            fill_rectangle(screen,
                           s->xleft, y, s->width, 1,
                           fgcolor);
        }
        SDL_UpdateRect(screen, s->xleft, s->ytop, s->width, s->height);
    }else{
        nb_display_channels= FFMIN(nb_display_channels, 2);
        if(rdft_bits != s->rdft_bits){
            av_rdft_end(s->rdft);
            av_free(s->rdft_data);
            s->rdft = av_rdft_init(rdft_bits, DFT_R2C);
            s->rdft_bits= rdft_bits;
            s->rdft_data= av_malloc(4*nb_freq*sizeof(*s->rdft_data));
        }
        {
            FFTSample *data[2];
            for(ch = 0;ch < nb_display_channels; ch++) {
                data[ch] = s->rdft_data + 2*nb_freq*ch;
                i = i_start + ch;
                for(x = 0; x < 2*nb_freq; x++) {
                    double w= (x-nb_freq)*(1.0/nb_freq);
                    data[ch][x]= s->sample_array[i]*(1.0-w*w);
                    i += channels;
                    if (i >= SAMPLE_ARRAY_SIZE)
                        i -= SAMPLE_ARRAY_SIZE;
                }
                av_rdft_calc(s->rdft, data[ch]);
            }
            //least efficient way to do this, we should of course directly access it but its more than fast enough
            for(y=0; y<s->height; y++){
                double w= 1/sqrt(nb_freq);
                int a= sqrt(w*sqrt(data[0][2*y+0]*data[0][2*y+0] + data[0][2*y+1]*data[0][2*y+1]));
                int b= (nb_display_channels == 2 ) ? sqrt(w*sqrt(data[1][2*y+0]*data[1][2*y+0]
                       + data[1][2*y+1]*data[1][2*y+1])) : a;
                a= FFMIN(a,255);
                b= FFMIN(b,255);
                fgcolor = SDL_MapRGB(screen->format, a, b, (a+b)/2);

                fill_rectangle(screen,
                            s->xpos, s->height-y, 1, 1,
                            fgcolor);
            }
        }
        SDL_UpdateRect(screen, s->xpos, s->ytop, 1, s->height);
        s->xpos++;
        if(s->xpos >= s->width)
            s->xpos= s->xleft;
    }
}

static void stream_close(VideoState *is)
{
    VideoPicture *vp;
    int i;
    /* XXX: use a special url_shutdown call to abort parse cleanly */
    is->abort_request = 1;
    SDL_WaitThread(is->read_tid, NULL);
    SDL_WaitThread(is->refresh_tid, NULL);

    /* free all pictures */
    for(i=0;i<VIDEO_PICTURE_QUEUE_SIZE; i++) {
        vp = &is->pictq[i];
#if CONFIG_AVFILTER
        if (vp->picref) {
            avfilter_unref_buffer(vp->picref);
            vp->picref = NULL;
        }
#endif
        if (vp->bmp) {
            SDL_FreeYUVOverlay(vp->bmp);
            vp->bmp = NULL;
        }
    }
    SDL_DestroyMutex(is->pictq_mutex);
    SDL_DestroyCond(is->pictq_cond);
    SDL_DestroyMutex(is->subpq_mutex);
    SDL_DestroyCond(is->subpq_cond);
#if !CONFIG_AVFILTER
    if (is->img_convert_ctx)
        sws_freeContext(is->img_convert_ctx);
#endif
    av_free(is);
}

static void do_exit(void)
{
    if (cur_stream) {
        stream_close(cur_stream);
        cur_stream = NULL;
    }
    uninit_opts();
#if CONFIG_AVFILTER
    avfilter_uninit();
#endif
    if (show_status)
        printf("\n");
    SDL_Quit();
    av_log(NULL, AV_LOG_QUIET, "%s", "");
    exit(0);
}

/**
    首先,确定窗口大小，用来确定分配显示内存；
    然后,用SDL_SetVideoMode()来创建一个窗口，还创建了一块用来显示图像的内存区域叫"screen buffer"，由这块区域负责显示画面到屏幕；
    最后,用SDL_WM_SetCaption()来设置窗口的标题；
**/
static int video_open(VideoState *is){
    int flags = SDL_HWSURFACE|SDL_ASYNCBLIT|SDL_HWACCEL;
    int w,h;

    if(is_full_screen) 
        flags |= SDL_FULLSCREEN;
    else              
        flags |= SDL_RESIZABLE;

    if (is_full_screen && fs_screen_width) {
        w = fs_screen_width;
        h = fs_screen_height;
    } else if(!is_full_screen && screen_width){
        w = screen_width;
        h = screen_height;
#if CONFIG_AVFILTER
    }else if (is->out_video_filter && is->out_video_filter->inputs[0]){
        w = is->out_video_filter->inputs[0]->w;
        h = is->out_video_filter->inputs[0]->h;
#else
    }else if (is->video_st && is->video_st->codec->width){
        w = is->video_st->codec->width;
        h = is->video_st->codec->height;
#endif
    } else {
        w = 640;
        h = 480;
    }
    if(screen && is->width == screen->w && screen->w == w && is->height== screen->h && screen->h == h)
        return 0;

#ifndef __APPLE__
    screen = SDL_SetVideoMode(w, h, 0, flags);
#else
    /* setting bits_per_pixel = 0 or 32 causes blank video on OS X */
    screen = SDL_SetVideoMode(w, h, 24, flags);
#endif
    if (!screen) {
        fprintf(stderr, "SDL: could not set video mode - exiting\n");
        do_exit();
    }
    if (!window_title)
        window_title = input_filename;
    SDL_WM_SetCaption(window_title, window_title);

    is->width = screen->w;
    is->height = screen->h;

    return 0;
}

/* display the current picture, if any */
static void video_display(VideoState *is)
{
    if(!screen)
        video_open(cur_stream);
    if (is->audio_st && is->show_mode != SHOW_MODE_VIDEO)
        video_audio_display(is);
    else if (is->video_st)
        video_image_display(is);
}

/**
    刷新线程：
        创建SDL_Event消息事件，将FF_REFRESH_EVENT刷新消息事件发送到SDL消息队列；
        默认是间隔20ms检测一次消息事件；
**/
static int refresh_thread(void *opaque)
{
    VideoState *is= opaque;
    while(!is->abort_request){
        SDL_Event event;
        event.type = FF_REFRESH_EVENT;
        event.user.data1 = opaque;
        if(!is->refresh){
            is->refresh=1;
            SDL_PushEvent(&event);
        }
        //FIXME ideally we should wait the correct time but SDLs event passing is so slow it would be silly
        usleep(is->audio_st && is->show_mode != SHOW_MODE_VIDEO ? rdftspeed*1000 : 5000);
    }
    return 0;
}

/* get the current audio clock value */
static double get_audio_clock(VideoState *is)
{
    double pts;
    int hw_buf_size, bytes_per_sec;
    pts = is->audio_clock;
    hw_buf_size = audio_write_get_buf_size(is);
    bytes_per_sec = 0;
    if (is->audio_st) {
        bytes_per_sec = is->audio_st->codec->sample_rate *
            2 * is->audio_st->codec->channels;
    }
    if (bytes_per_sec)
        pts -= (double)hw_buf_size / bytes_per_sec;
    return pts;
}

/* get the current video clock value */
static double get_video_clock(VideoState *is)
{
    if (is->paused) {
        return is->video_current_pts;
    } else {
        return is->video_current_pts_drift + av_gettime() / 1000000.0;
    }
}

/* get the current external clock value */
static double get_external_clock(VideoState *is)
{
    int64_t ti;
    ti = av_gettime();
    return is->external_clock + ((ti - is->external_clock_time) * 1e-6);
}

/* get the current master clock value */
static double get_master_clock(VideoState *is)
{
    double val;

    if (is->av_sync_type == AV_SYNC_VIDEO_MASTER) {
        if (is->video_st)
            val = get_video_clock(is);
        else
            val = get_audio_clock(is);
    } else if (is->av_sync_type == AV_SYNC_AUDIO_MASTER) {
        if (is->audio_st)
            val = get_audio_clock(is);
        else
            val = get_video_clock(is);
    } else {
        val = get_external_clock(is);
    }
    return val;
}

/* seek in the stream */
static void stream_seek(VideoState *is, int64_t pos, int64_t rel, int seek_by_bytes)
{
    if (!is->seek_req) {
        is->seek_pos = pos;
        is->seek_rel = rel;
        is->seek_flags &= ~AVSEEK_FLAG_BYTE;
        if (seek_by_bytes)
            is->seek_flags |= AVSEEK_FLAG_BYTE;
        is->seek_req = 1;
    }
}

/* pause or resume the video */
static void stream_toggle_pause(VideoState *is)
{
    if (is->paused) {
        is->frame_timer += av_gettime() / 1000000.0 + is->video_current_pts_drift - is->video_current_pts;
        if(is->read_pause_return != AVERROR(ENOSYS)){
            is->video_current_pts = is->video_current_pts_drift + av_gettime() / 1000000.0;
        }
        is->video_current_pts_drift = is->video_current_pts - av_gettime() / 1000000.0;
    }
    is->paused = !is->paused;
}

static double compute_target_time(double frame_current_pts, VideoState *is)
{
    double delay, sync_threshold, diff;

    /* compute nominal delay */
    delay = frame_current_pts - is->frame_last_pts;
    if (delay <= 0 || delay >= 10.0) {
        /* if incorrect delay, use previous one */
        delay = is->frame_last_delay;
    } else {
        is->frame_last_delay = delay;
    }
    is->frame_last_pts = frame_current_pts;

    /* update delay to follow master synchronisation source */
    if (((is->av_sync_type == AV_SYNC_AUDIO_MASTER && is->audio_st) ||
         is->av_sync_type == AV_SYNC_EXTERNAL_CLOCK)) {
        /* if video is slave, we try to correct big delays by
           duplicating or deleting a frame */
        diff = get_video_clock(is) - get_master_clock(is);

        /* skip or repeat frame. We take into account the
           delay to compute the threshold. I still don't know
           if it is the best guess */
        sync_threshold = FFMAX(AV_SYNC_THRESHOLD, delay);
        if (fabs(diff) < AV_NOSYNC_THRESHOLD) {
            if (diff <= -sync_threshold)
                delay = 0;
            else if (diff >= sync_threshold)
                delay = 2 * delay;
        }
    }
    is->frame_timer += delay;

    av_dlog(NULL, "video: delay=%0.3f pts=%0.3f A-V=%f\n",
            delay, frame_current_pts, -diff);

    return is->frame_timer;
}

/* called to display each frame */
/**
    显示图片到屏幕：
    由refresh_thread产生的消息事件触发，
**/
static void video_refresh(void *opaque)
{
    VideoState *is = opaque;
    VideoPicture *vp;

    SubPicture *sp, *sp2;

    if (is->video_st) {
retry:
        if (is->pictq_size == 0) {
            //nothing to do, no picture to display in the que
        } else {
            double time= av_gettime()/1000000.0;
            double next_target;
            /* dequeue the picture */
            vp = &is->pictq[is->pictq_rindex];

            if(time < vp->target_clock)
                return;
            /* update current video pts */
            is->video_current_pts = vp->pts;
            is->video_current_pts_drift = is->video_current_pts - time;
            is->video_current_pos = vp->pos;
            if(is->pictq_size > 1){
                VideoPicture *nextvp= &is->pictq[(is->pictq_rindex+1)%VIDEO_PICTURE_QUEUE_SIZE];
                assert(nextvp->target_clock >= vp->target_clock);
                next_target= nextvp->target_clock;
            }else{
                next_target= vp->target_clock + is->video_clock - vp->pts; //FIXME pass durations cleanly
            }
            if((framedrop>0 || (framedrop && is->audio_st)) && time > next_target){
                is->skip_frames *= 1.0 + FRAME_SKIP_FACTOR;
                if(is->pictq_size > 1 || time > next_target + 0.5){
                    /* update queue size and signal for next picture */
                    if (++is->pictq_rindex == VIDEO_PICTURE_QUEUE_SIZE)
                        is->pictq_rindex = 0;

                    SDL_LockMutex(is->pictq_mutex);
                    is->pictq_size--;
                    SDL_CondSignal(is->pictq_cond);
                    SDL_UnlockMutex(is->pictq_mutex);
                    goto retry;
                }
            }

            if(is->subtitle_st) {
                if (is->subtitle_stream_changed) {
                    SDL_LockMutex(is->subpq_mutex);

                    while (is->subpq_size) {
                        free_subpicture(&is->subpq[is->subpq_rindex]);

                        /* update queue size and signal for next picture */
                        if (++is->subpq_rindex == SUBPICTURE_QUEUE_SIZE)
                            is->subpq_rindex = 0;

                        is->subpq_size--;
                    }
                    is->subtitle_stream_changed = 0;

                    SDL_CondSignal(is->subpq_cond);
                    SDL_UnlockMutex(is->subpq_mutex);
                } else {
                    if (is->subpq_size > 0) {
                        sp = &is->subpq[is->subpq_rindex];

                        if (is->subpq_size > 1)
                            sp2 = &is->subpq[(is->subpq_rindex + 1) % SUBPICTURE_QUEUE_SIZE];
                        else
                            sp2 = NULL;

                        if ((is->video_current_pts > (sp->pts + ((float) sp->sub.end_display_time / 1000)))
                                || (sp2 && is->video_current_pts > (sp2->pts + ((float) sp2->sub.start_display_time / 1000))))
                        {
                            free_subpicture(sp);

                            /* update queue size and signal for next picture */
                            if (++is->subpq_rindex == SUBPICTURE_QUEUE_SIZE)
                                is->subpq_rindex = 0;

                            SDL_LockMutex(is->subpq_mutex);
                            is->subpq_size--;
                            SDL_CondSignal(is->subpq_cond);
                            SDL_UnlockMutex(is->subpq_mutex);
                        }
                    }
                }
            }

            /* display picture */
            if (!display_disable)
                video_display(is);

            /* update queue size and signal for next picture */
            if (++is->pictq_rindex == VIDEO_PICTURE_QUEUE_SIZE)
                is->pictq_rindex = 0;

            SDL_LockMutex(is->pictq_mutex);
            is->pictq_size--;
            SDL_CondSignal(is->pictq_cond);
            SDL_UnlockMutex(is->pictq_mutex);
        }
    } else if (is->audio_st) {
        /* draw the next audio frame */

        /* if only audio stream, then display the audio bars (better
           than nothing, just to test the implementation */

        /* display picture */
        if (!display_disable)
            video_display(is);
    }
    if (show_status) {
        static int64_t last_time;
        int64_t cur_time;
        int aqsize, vqsize, sqsize;
        double av_diff;

        cur_time = av_gettime();
        if (!last_time || (cur_time - last_time) >= 30000) {
            aqsize = 0;
            vqsize = 0;
            sqsize = 0;
            if (is->audio_st)
                aqsize = is->audioq.size;
            if (is->video_st)
                vqsize = is->videoq.size;
            if (is->subtitle_st)
                sqsize = is->subtitleq.size;
            av_diff = 0;
            if (is->audio_st && is->video_st)
                av_diff = get_audio_clock(is) - get_video_clock(is);
            printf("%7.2f A-V:%7.3f s:%3.1f aq=%5dKB vq=%5dKB sq=%5dB f=%"PRId64"/%"PRId64"   \r",
                   get_master_clock(is),
                   av_diff,
                   FFMAX(is->skip_frames-1, 0),
                   aqsize / 1024,
                   vqsize / 1024,
                   sqsize,
                   is->video_st ? is->video_st->codec->pts_correction_num_faulty_dts : 0,
                   is->video_st ? is->video_st->codec->pts_correction_num_faulty_pts : 0);
            fflush(stdout);
            last_time = cur_time;
        }
    }
}

/* allocate a picture (needs to do that in main thread to avoid
   potential locking problems */
static void alloc_picture(void *opaque)
{
    VideoState *is = opaque;
    VideoPicture *vp;

    vp = &is->pictq[is->pictq_windex];

    if (vp->bmp)
        SDL_FreeYUVOverlay(vp->bmp);

#if CONFIG_AVFILTER
    if (vp->picref)
        avfilter_unref_buffer(vp->picref);
    vp->picref = NULL;

    vp->width   = is->out_video_filter->inputs[0]->w;
    vp->height  = is->out_video_filter->inputs[0]->h;
    vp->pix_fmt = is->out_video_filter->inputs[0]->format;
#else
    vp->width   = is->video_st->codec->width;
    vp->height  = is->video_st->codec->height;
    vp->pix_fmt = is->video_st->codec->pix_fmt;
#endif

    vp->bmp = SDL_CreateYUVOverlay(vp->width, vp->height,
                                   SDL_YV12_OVERLAY,
                                   screen);
    if (!vp->bmp || vp->bmp->pitches[0] < vp->width) {
        /* SDL allocates a buffer smaller than requested if the video
         * overlay hardware is unable to support the requested size. */
        fprintf(stderr, "Error: the video system does not support an image\n"
                        "size of %dx%d pixels. Try using -lowres or -vf \"scale=w:h\"\n"
                        "to reduce the image size.\n", vp->width, vp->height );
        do_exit();
    }

    SDL_LockMutex(is->pictq_mutex);
    vp->allocated = 1;
    SDL_CondSignal(is->pictq_cond);
    SDL_UnlockMutex(is->pictq_mutex);
}

/**
    首先,等待 通过FF_ALLOC_EVENT消息来完成对硬件图片缓冲的分配或者调整大小 ；
    然后,由sws_getCachedContext()获取渲染的句柄，；
    最后,通过sws_scale()来进行图像缩放和格式转换，将输入的AVFrame格式的帧渲染到AVPicture格式的pict对应的图层中；
**/
static int queue_picture(VideoState *is, AVFrame *src_frame, double pts1, int64_t pos)
{
    VideoPicture *vp;
    double frame_delay, pts = pts1;

    /* compute the exact PTS for the picture if it is omitted in the stream
     * pts1 is the dts of the pkt / pts of the frame */
    if (pts != 0) {
        /* update video clock with pts, if present */
        is->video_clock = pts;
    } else {
        pts = is->video_clock;
    }
    /* update video clock for next frame */
    frame_delay = av_q2d(is->video_st->codec->time_base);
    /* for MPEG2, the frame can be repeated, so we update the
       clock accordingly */
    frame_delay += src_frame->repeat_pict * (frame_delay * 0.5);
    is->video_clock += frame_delay;

#if defined(DEBUG_SYNC) && 0
    printf("frame_type=%c clock=%0.3f pts=%0.3f\n", av_get_picture_type_char(src_frame->pict_type), pts, pts1);
#endif

    /* wait until we have space to put a new picture */
    SDL_LockMutex(is->pictq_mutex);

    if(is->pictq_size>=VIDEO_PICTURE_QUEUE_SIZE && !is->refresh)
        is->skip_frames= FFMAX(1.0 - FRAME_SKIP_FACTOR, is->skip_frames * (1.0-FRAME_SKIP_FACTOR));

    while (is->pictq_size >= VIDEO_PICTURE_QUEUE_SIZE && !is->videoq.abort_request) {
        SDL_CondWait(is->pictq_cond, is->pictq_mutex);
    }
    SDL_UnlockMutex(is->pictq_mutex);

    if (is->videoq.abort_request)
        return -1;

    vp = &is->pictq[is->pictq_windex];

    /* alloc or resize hardware picture buffer */
    if (!vp->bmp ||
#if CONFIG_AVFILTER
        vp->width  != is->out_video_filter->inputs[0]->w ||
        vp->height != is->out_video_filter->inputs[0]->h) {
#else
        vp->width != is->video_st->codec->width ||
        vp->height != is->video_st->codec->height) {
#endif
        SDL_Event event;

        vp->allocated = 0;

        /* the allocation must be done in the main thread to avoid
           locking problems */
        event.type = FF_ALLOC_EVENT;
        event.user.data1 = is;
        SDL_PushEvent(&event);

        /* wait until the picture is allocated */
        SDL_LockMutex(is->pictq_mutex);
        while (!vp->allocated && !is->videoq.abort_request) {
            SDL_CondWait(is->pictq_cond, is->pictq_mutex);
        }
        SDL_UnlockMutex(is->pictq_mutex);

        if (is->videoq.abort_request)
            return -1;
    }

    /* if the frame is not skipped, then display it */
    if (vp->bmp) {
        AVPicture pict;
#if CONFIG_AVFILTER
        if(vp->picref)
            avfilter_unref_buffer(vp->picref);
        vp->picref = src_frame->opaque;
#endif

        /* get a pointer on the bitmap */
        SDL_LockYUVOverlay (vp->bmp);

        memset(&pict,0,sizeof(AVPicture));
        pict.data[0] = vp->bmp->pixels[0];
        pict.data[1] = vp->bmp->pixels[2];
        pict.data[2] = vp->bmp->pixels[1];

        pict.linesize[0] = vp->bmp->pitches[0];
        pict.linesize[1] = vp->bmp->pitches[2];
        pict.linesize[2] = vp->bmp->pitches[1];

#if CONFIG_AVFILTER
        //FIXME use direct rendering
        av_picture_copy(&pict, (AVPicture *)src_frame, vp->pix_fmt, vp->width, vp->height);
#else
        sws_flags = av_get_int(sws_opts, "sws_flags", NULL);
        is->img_convert_ctx = sws_getCachedContext(is->img_convert_ctx, vp->width, vp->height, 
                            vp->pix_fmt, vp->width, vp->height, PIX_FMT_YUV420P, sws_flags, NULL, NULL, NULL);
        if (is->img_convert_ctx == NULL) {
            fprintf(stderr, "Cannot initialize the conversion context\n");
            exit(1);
        }
        sws_scale(is->img_convert_ctx, src_frame->data, src_frame->linesize, 0, vp->height, pict.data, pict.linesize);
#endif
        /* update the bitmap content */
        SDL_UnlockYUVOverlay(vp->bmp);

        vp->pts = pts;
        vp->pos = pos;

        /* now we can update the picture count */
        if (++is->pictq_windex == VIDEO_PICTURE_QUEUE_SIZE)
            is->pictq_windex = 0;
        SDL_LockMutex(is->pictq_mutex);
        vp->target_clock= compute_target_time(vp->pts, is);

        is->pictq_size++;
        SDL_UnlockMutex(is->pictq_mutex);
    }
    return 0;
}

/**
    首先,从videoq视频队列中取出一个包packet，
    然后,avcodec_decode_video2 来解码视频，把包转换成帧；
    最后,将解码出的视频帧frame加入时间戳信息；
**/
static int get_video_frame(VideoState *is, AVFrame *frame, int64_t *pts, AVPacket *pkt)
{
    int len1 av_unused, got_picture, i;

    if (packet_queue_get(&is->videoq, pkt, 1) < 0)
        return -1;

    if (pkt->data == flush_pkt.data) {
        avcodec_flush_buffers(is->video_st->codec);

        SDL_LockMutex(is->pictq_mutex);
        //Make sure there are no long delay timers (ideally we should just flush the que but thats harder)
        for (i = 0; i < VIDEO_PICTURE_QUEUE_SIZE; i++) {
            is->pictq[i].target_clock= 0;
        }
        while (is->pictq_size && !is->videoq.abort_request) {
            SDL_CondWait(is->pictq_cond, is->pictq_mutex);
        }
        is->video_current_pos = -1;
        SDL_UnlockMutex(is->pictq_mutex);

        is->frame_last_pts = AV_NOPTS_VALUE;
        is->frame_last_delay = 0;
        is->frame_timer = (double)av_gettime() / 1000000.0;
        is->skip_frames = 1;
        is->skip_frames_index = 0;
        return 0;
    }

    /**
        函数avcodec_decode_video2 来解码视频，单独解码线程，解码后放入队列，能够减少绘制时延时；
        把包转换为帧;
    **/
    len1 = avcodec_decode_video2(is->video_st->codec, frame, &got_picture, pkt);

    if (got_picture) {
        if (decoder_reorder_pts == -1) {
            *pts = frame->best_effort_timestamp;
        } else if (decoder_reorder_pts) {
            *pts = frame->pkt_pts;
        } else {
            *pts = frame->pkt_dts;
        }

        if (*pts == AV_NOPTS_VALUE) {
            *pts = 0;
        }

        is->skip_frames_index += 1;
        if(is->skip_frames_index >= is->skip_frames){
            is->skip_frames_index -= FFMAX(is->skip_frames, 1.0);
            return 1;
        }

    }
    return 0;
}

#if CONFIG_AVFILTER
typedef struct {
    VideoState *is;
    AVFrame *frame;
    int use_dr1;
} FilterPriv;

static int input_get_buffer(AVCodecContext *codec, AVFrame *pic)
{
    AVFilterContext *ctx = codec->opaque;
    AVFilterBufferRef  *ref;
    int perms = AV_PERM_WRITE;
    int i, w, h, stride[4];
    unsigned edge;
    int pixel_size;

    av_assert0(codec->flags & CODEC_FLAG_EMU_EDGE);

    if (codec->codec->capabilities & CODEC_CAP_NEG_LINESIZES)
        perms |= AV_PERM_NEG_LINESIZES;

    if(pic->buffer_hints & FF_BUFFER_HINTS_VALID) {
        if(pic->buffer_hints & FF_BUFFER_HINTS_READABLE) perms |= AV_PERM_READ;
        if(pic->buffer_hints & FF_BUFFER_HINTS_PRESERVE) perms |= AV_PERM_PRESERVE;
        if(pic->buffer_hints & FF_BUFFER_HINTS_REUSABLE) perms |= AV_PERM_REUSE2;
    }
    if(pic->reference) perms |= AV_PERM_READ | AV_PERM_PRESERVE;

    w = codec->width;
    h = codec->height;

    if(av_image_check_size(w, h, 0, codec))
        return -1;

    avcodec_align_dimensions2(codec, &w, &h, stride);
    edge = codec->flags & CODEC_FLAG_EMU_EDGE ? 0 : avcodec_get_edge_width();
    w += edge << 1;
    h += edge << 1;

    if(!(ref = avfilter_get_video_buffer(ctx->outputs[0], perms, w, h)))
        return -1;

    pixel_size = av_pix_fmt_descriptors[ref->format].comp[0].step_minus1+1;
    ref->video->w = codec->width;
    ref->video->h = codec->height;
    for(i = 0; i < 4; i ++) {
        unsigned hshift = (i == 1 || i == 2) ? av_pix_fmt_descriptors[ref->format].log2_chroma_w : 0;
        unsigned vshift = (i == 1 || i == 2) ? av_pix_fmt_descriptors[ref->format].log2_chroma_h : 0;

        if (ref->data[i]) {
            ref->data[i]    += ((edge * pixel_size) >> hshift) + ((edge * ref->linesize[i]) >> vshift);
        }
        pic->data[i]     = ref->data[i];
        pic->linesize[i] = ref->linesize[i];
    }
    pic->opaque = ref;
    pic->age    = INT_MAX;
    pic->type   = FF_BUFFER_TYPE_USER;
    pic->reordered_opaque = codec->reordered_opaque;
    if(codec->pkt) pic->pkt_pts = codec->pkt->pts;
    else           pic->pkt_pts = AV_NOPTS_VALUE;
    return 0;
}

static void input_release_buffer(AVCodecContext *codec, AVFrame *pic)
{
    memset(pic->data, 0, sizeof(pic->data));
    avfilter_unref_buffer(pic->opaque);
}

static int input_reget_buffer(AVCodecContext *codec, AVFrame *pic)
{
    AVFilterBufferRef *ref = pic->opaque;

    if (pic->data[0] == NULL) {
        pic->buffer_hints |= FF_BUFFER_HINTS_READABLE;
        return codec->get_buffer(codec, pic);
    }

    if ((codec->width != ref->video->w) || (codec->height != ref->video->h) ||
        (codec->pix_fmt != ref->format)) {
        av_log(codec, AV_LOG_ERROR, "Picture properties changed.\n");
        return -1;
    }

    pic->reordered_opaque = codec->reordered_opaque;
    if(codec->pkt) pic->pkt_pts = codec->pkt->pts;
    else           pic->pkt_pts = AV_NOPTS_VALUE;
    return 0;
}

static int input_init(AVFilterContext *ctx, const char *args, void *opaque)
{
    FilterPriv *priv = ctx->priv;
    AVCodecContext *codec;
    if(!opaque) return -1;

    priv->is = opaque;
    codec    = priv->is->video_st->codec;
    codec->opaque = ctx;
    if((codec->codec->capabilities & CODEC_CAP_DR1)
    ) {
        av_assert0(codec->flags & CODEC_FLAG_EMU_EDGE);
        priv->use_dr1 = 1;
        codec->get_buffer     = input_get_buffer;
        codec->release_buffer = input_release_buffer;
        codec->reget_buffer   = input_reget_buffer;
        codec->thread_safe_callbacks = 1;
    }

    priv->frame = avcodec_alloc_frame();

    return 0;
}

static void input_uninit(AVFilterContext *ctx)
{
    FilterPriv *priv = ctx->priv;
    av_free(priv->frame);
}

static int input_request_frame(AVFilterLink *link)
{
    FilterPriv *priv = link->src->priv;
    AVFilterBufferRef *picref;
    int64_t pts = 0;
    AVPacket pkt;
    int ret;

    while (!(ret = get_video_frame(priv->is, priv->frame, &pts, &pkt)))
        av_free_packet(&pkt);
    if (ret < 0)
        return -1;

    if(priv->use_dr1 && priv->frame->opaque) {
        picref = avfilter_ref_buffer(priv->frame->opaque, ~0);
    } else {
        picref = avfilter_get_video_buffer(link, AV_PERM_WRITE, link->w, link->h);
        av_image_copy(picref->data, picref->linesize,
                      priv->frame->data, priv->frame->linesize,
                      picref->format, link->w, link->h);
    }
    av_free_packet(&pkt);

    avfilter_copy_frame_props(picref, priv->frame);
    picref->pts = pts;

    avfilter_start_frame(link, picref);
    avfilter_draw_slice(link, 0, link->h, 1);
    avfilter_end_frame(link);

    return 0;
}

static int input_query_formats(AVFilterContext *ctx)
{
    FilterPriv *priv = ctx->priv;
    enum PixelFormat pix_fmts[] = {
        priv->is->video_st->codec->pix_fmt, PIX_FMT_NONE
    };

    avfilter_set_common_pixel_formats(ctx, avfilter_make_format_list(pix_fmts));
    return 0;
}

static int input_config_props(AVFilterLink *link)
{
    FilterPriv *priv  = link->src->priv;
    AVCodecContext *c = priv->is->video_st->codec;

    link->w = c->width;
    link->h = c->height;
    link->time_base = priv->is->video_st->time_base;

    return 0;
}

/*
    定义AVFilter结构体默认直接初始化了；
*/
static AVFilter input_filter =
{
    .name      = "ffplay_input",

    .priv_size = sizeof(FilterPriv),

    .init      = input_init,
    .uninit    = input_uninit,

    .query_formats = input_query_formats,

    .inputs    = (AVFilterPad[]) {{ .name = NULL }},
    .outputs   = (AVFilterPad[]) {{ .name = "default",
                                    .type = AVMEDIA_TYPE_VIDEO,
                                    .request_frame = input_request_frame,
                                    .config_props  = input_config_props, },
                                  { .name = NULL }},
};

static int configure_video_filters(AVFilterGraph *graph, VideoState *is, const char *vfilters)
{
    char sws_flags_str[128];
    int ret;
    enum PixelFormat pix_fmts[] = { PIX_FMT_YUV420P, PIX_FMT_NONE };
    AVFilterContext *filt_src = NULL, *filt_out = NULL;
    snprintf(sws_flags_str, sizeof(sws_flags_str), "flags=%d", sws_flags);
    graph->scale_sws_opts = av_strdup(sws_flags_str);

    if ((ret = avfilter_graph_create_filter(&filt_src, &input_filter, "src",
                                            NULL, is, graph)) < 0)
        goto the_end;
    if ((ret = avfilter_graph_create_filter(&filt_out, avfilter_get_by_name("buffersink"), "out",
                                            NULL, pix_fmts, graph)) < 0)
        goto the_end;

    if(vfilters) {
        AVFilterInOut *outputs = avfilter_inout_alloc();
        AVFilterInOut *inputs  = avfilter_inout_alloc();

        outputs->name    = av_strdup("in");
        outputs->filter_ctx = filt_src;
        outputs->pad_idx = 0;
        outputs->next    = NULL;

        inputs->name    = av_strdup("out");
        inputs->filter_ctx = filt_out;
        inputs->pad_idx = 0;
        inputs->next    = NULL;

        if ((ret = avfilter_graph_parse(graph, vfilters, &inputs, &outputs, NULL)) < 0)
            goto the_end;
        av_freep(&vfilters);
    } else {
        if ((ret = avfilter_link(filt_src, 0, filt_out, 0)) < 0)
            goto the_end;
    }

    if ((ret = avfilter_graph_config(graph, NULL)) < 0)
        goto the_end;

    is->out_video_filter = filt_out;
the_end:
    return ret;
}

#endif  /* CONFIG_AVFILTER */


/**
    首先,通过get_video_frame()获取并解码视频帧；
    然后,使用queue_picture()将视频帧渲染到对应的图层上，等待显示到屏幕上；
**/
static int video_thread(void *arg)
{
    VideoState *is = arg;
    AVFrame *frame= avcodec_alloc_frame();
    int64_t pts_int = AV_NOPTS_VALUE, pos = -1;
    double pts;
    int ret;

#if CONFIG_AVFILTER
    AVFilterGraph *graph = avfilter_graph_alloc();
    AVFilterContext *filt_out = NULL;

    if ((ret = configure_video_filters(graph, is, vfilters)) < 0)
        goto the_end;
    filt_out = is->out_video_filter;
#endif

    for(;;) {
#if !CONFIG_AVFILTER
        AVPacket pkt;
#else
        AVFilterBufferRef *picref;
        AVRational tb = filt_out->inputs[0]->time_base;
#endif
        while (is->paused && !is->videoq.abort_request)
            SDL_Delay(10);
#if CONFIG_AVFILTER
        ret = av_vsink_buffer_get_video_buffer_ref(filt_out, &picref, 0);
        if (picref) {
            avfilter_fill_frame_from_video_buffer_ref(frame, picref);
            pts_int = picref->pts;
            pos     = picref->pos;
            frame->opaque = picref;
        }

        if (av_cmp_q(tb, is->video_st->time_base)) {
            av_unused int64_t pts1 = pts_int;
            pts_int = av_rescale_q(pts_int, tb, is->video_st->time_base);
            av_dlog(NULL, "video_thread(): tb:%d/%d pts:%"PRId64" -> tb:%d/%d pts:%"PRId64"\n",
                    tb.num, tb.den, pts1, is->video_st->time_base.num, is->video_st->time_base.den, pts_int);
        }
#else
        ret = get_video_frame(is, frame, &pts_int, &pkt);
        pos = pkt.pos;
        av_free_packet(&pkt);
#endif

        if (ret < 0) goto the_end;

#if CONFIG_AVFILTER
        if (!picref)
            continue;
#endif

        pts = pts_int*av_q2d(is->video_st->time_base);

        ret = queue_picture(is, frame, pts, pos);

        if (ret < 0)
            goto the_end;

        if (step)
            if (cur_stream)
                stream_toggle_pause(cur_stream);
    }
 the_end:
#if CONFIG_AVFILTER
    avfilter_graph_free(&graph);
#endif
    av_free(frame);
    return 0;
}

/**
    subtitle的解码线程；
    首先,packet_queue_get()从队列中获取包packet；
    然后,avcodec_decode_subtitle2()解码;
    最后,将rgba格式转换成yuva格式输出；
**/
static int subtitle_thread(void *arg)
{
    VideoState *is = arg;
    SubPicture *sp;
    AVPacket pkt1, *pkt = &pkt1;
    int len1 av_unused, got_subtitle;
    double pts;
    int i, j;
    int r, g, b, y, u, v, a;

    for(;;) {
        while (is->paused && !is->subtitleq.abort_request) {
            SDL_Delay(10);
        }
        if (packet_queue_get(&is->subtitleq, pkt, 1) < 0)
            break;

        if(pkt->data == flush_pkt.data){
            avcodec_flush_buffers(is->subtitle_st->codec);
            continue;
        }
        SDL_LockMutex(is->subpq_mutex);
        while (is->subpq_size >= SUBPICTURE_QUEUE_SIZE && !is->subtitleq.abort_request) {
            SDL_CondWait(is->subpq_cond, is->subpq_mutex);
        }
        SDL_UnlockMutex(is->subpq_mutex);

        if (is->subtitleq.abort_request)
            goto the_end;

        sp = &is->subpq[is->subpq_windex];

       /* NOTE: ipts is the PTS of the _first_ picture beginning in this packet, if any */
        pts = 0;
        if (pkt->pts != AV_NOPTS_VALUE)
            pts = av_q2d(is->subtitle_st->time_base)*pkt->pts;

        len1 = avcodec_decode_subtitle2(is->subtitle_st->codec, &sp->sub, &got_subtitle, pkt);
        if (got_subtitle && sp->sub.format == 0) {
            sp->pts = pts;

            for (i = 0; i < sp->sub.num_rects; i++)
            {
                for (j = 0; j < sp->sub.rects[i]->nb_colors; j++)
                {
                    RGBA_IN(r, g, b, a, (uint32_t*)sp->sub.rects[i]->pict.data[1] + j);
                    y = RGB_TO_Y_CCIR(r, g, b);
                    u = RGB_TO_U_CCIR(r, g, b, 0);
                    v = RGB_TO_V_CCIR(r, g, b, 0);
                    YUVA_OUT((uint32_t*)sp->sub.rects[i]->pict.data[1] + j, y, u, v, a);
                }
            }

            /* now we can update the picture count */
            if (++is->subpq_windex == SUBPICTURE_QUEUE_SIZE)
                is->subpq_windex = 0;
            SDL_LockMutex(is->subpq_mutex);
            is->subpq_size++;
            SDL_UnlockMutex(is->subpq_mutex);
        }
        av_free_packet(pkt);
    }
 the_end:
    return 0;
}

/* copy samples for viewing in editor window */
static void update_sample_display(VideoState *is, short *samples, int samples_size)
{
    int size, len;

    size = samples_size / sizeof(short);
    while (size > 0) {
        len = SAMPLE_ARRAY_SIZE - is->sample_array_index;
        if (len > size)
            len = size;
        memcpy(is->sample_array + is->sample_array_index, samples, len * sizeof(short));
        samples += len;
        is->sample_array_index += len;
        if (is->sample_array_index >= SAMPLE_ARRAY_SIZE)
            is->sample_array_index = 0;
        size -= len;
    }
}

/* return the new audio buffer size (samples can be added or deleted
   to get better sync if video or external master clock) */
static int synchronize_audio(VideoState *is, short *samples,
                             int samples_size1, double pts)
{
    int n, samples_size;
    double ref_clock;

    n = 2 * is->audio_st->codec->channels;
    samples_size = samples_size1;

    /* if not master, then we try to remove or add samples to correct the clock */
    if (((is->av_sync_type == AV_SYNC_VIDEO_MASTER && is->video_st) ||
         is->av_sync_type == AV_SYNC_EXTERNAL_CLOCK)) {
        double diff, avg_diff;
        int wanted_size, min_size, max_size, nb_samples;

        ref_clock = get_master_clock(is);
        diff = get_audio_clock(is) - ref_clock;

        if (diff < AV_NOSYNC_THRESHOLD) {
            is->audio_diff_cum = diff + is->audio_diff_avg_coef * is->audio_diff_cum;
            if (is->audio_diff_avg_count < AUDIO_DIFF_AVG_NB) {
                /* not enough measures to have a correct estimate */
                is->audio_diff_avg_count++;
            } else {
                /* estimate the A-V difference */
                avg_diff = is->audio_diff_cum * (1.0 - is->audio_diff_avg_coef);

                if (fabs(avg_diff) >= is->audio_diff_threshold) {
                    wanted_size = samples_size + ((int)(diff * is->audio_st->codec->sample_rate) * n);
                    nb_samples = samples_size / n;

                    min_size = ((nb_samples * (100 - SAMPLE_CORRECTION_PERCENT_MAX)) / 100) * n;
                    max_size = ((nb_samples * (100 + SAMPLE_CORRECTION_PERCENT_MAX)) / 100) * n;
                    if (wanted_size < min_size)
                        wanted_size = min_size;
                    else if (wanted_size > max_size)
                        wanted_size = max_size;

                    /* add or remove samples to correction the synchro */
                    if (wanted_size < samples_size) {
                        /* remove samples */
                        samples_size = wanted_size;
                    } else if (wanted_size > samples_size) {
                        uint8_t *samples_end, *q;
                        int nb;

                        /* add samples */
                        nb = (samples_size - wanted_size);
                        samples_end = (uint8_t *)samples + samples_size - n;
                        q = samples_end + n;
                        while (nb > 0) {
                            memcpy(q, samples_end, n);
                            q += n;
                            nb -= n;
                        }
                        samples_size = wanted_size;
                    }
                }
#if 0
                printf("diff=%f adiff=%f sample_diff=%d apts=%0.3f vpts=%0.3f %f\n",
                       diff, avg_diff, samples_size - samples_size1,
                       is->audio_clock, is->video_clock, is->audio_diff_threshold);
#endif
            }
        } else {
            /* too big difference : may be initial PTS errors, so
               reset A-V filter */
            is->audio_diff_avg_count = 0;
            is->audio_diff_cum = 0;
        }
    }

    return samples_size;
}

/* decode one audio frame and returns its uncompressed size */
/**
    audio_decode_frame()音频解码线程，
    the audio packet can contain several frames。
**/
static int audio_decode_frame(VideoState *is, double *pts_ptr)
{
    AVPacket *pkt_temp = &is->audio_pkt_temp;
    AVPacket *pkt = &is->audio_pkt;
    AVCodecContext *dec= is->audio_st->codec;
    int n, len1, data_size;
    double pts;

    for(;;) {
        /* NOTE: the audio packet can contain several frames */
        while (pkt_temp->size > 0) {
            data_size = sizeof(is->audio_buf1);
            len1 = avcodec_decode_audio3(dec,
                                        (int16_t *)is->audio_buf1, &data_size,
                                        pkt_temp);
            /* 解码错误，跳过整个包 */
            if (len1 < 0) {
                /* if error, we skip the frame */
                pkt_temp->size = 0;
                break;
            }

            pkt_temp->data += len1;
            pkt_temp->size -= len1;
            if (data_size <= 0)
                continue;

            if (dec->sample_fmt != is->audio_src_fmt) {
                if (is->reformat_ctx)
                    av_audio_convert_free(is->reformat_ctx);
                is->reformat_ctx= av_audio_convert_alloc(AV_SAMPLE_FMT_S16, 1,
                                                         dec->sample_fmt, 1, NULL, 0);
                if (!is->reformat_ctx) {
                    fprintf(stderr, "Cannot convert %s sample format to %s sample format\n",
                        av_get_sample_fmt_name(dec->sample_fmt),
                        av_get_sample_fmt_name(AV_SAMPLE_FMT_S16));
                        break;
                }
                is->audio_src_fmt= dec->sample_fmt;
            }

            if (is->reformat_ctx) {
                const void *ibuf[6]= {is->audio_buf1};
                void *obuf[6]= {is->audio_buf2};
                int istride[6]= {av_get_bytes_per_sample(dec->sample_fmt)};
                int ostride[6]= {2};
                int len= data_size/istride[0];
                if (av_audio_convert(is->reformat_ctx, obuf, ostride, ibuf, istride, len)<0) {
                    printf("av_audio_convert() failed\n");
                    break;
                }
                is->audio_buf= is->audio_buf2;
                /* FIXME: existing code assume that data_size equals framesize*channels*2
                          remove this legacy cruft */
                data_size= len*2;
            }else{
                is->audio_buf= is->audio_buf1;
            }

            /* if no pts, then compute it */
            pts = is->audio_clock;
            *pts_ptr = pts;
            n = 2 * dec->channels;
            is->audio_clock += (double)data_size /
                (double)(n * dec->sample_rate);
#ifdef DEBUG
            {
                static double last_clock;
                printf("audio: delay=%0.3f clock=%0.3f pts=%0.3f\n",
                       is->audio_clock - last_clock,
                       is->audio_clock, pts);
                last_clock = is->audio_clock;
            }
#endif
            return data_size;
        }

        /* free the current packet */
        if (pkt->data)
            av_free_packet(pkt);

        if (is->paused || is->audioq.abort_request) {
            return -1;
        }

        /* read next packet */
        if (packet_queue_get(&is->audioq, pkt, 1) < 0)
            return -1;
        if(pkt->data == flush_pkt.data){
            avcodec_flush_buffers(dec);
            continue;
        }

        pkt_temp->data = pkt->data;
        pkt_temp->size = pkt->size;

        /* if update the audio clock with the pts */
        if (pkt->pts != AV_NOPTS_VALUE) {
            is->audio_clock = av_q2d(is->audio_st->time_base)*pkt->pts;
        }
    }
}

/* prepare a new audio buffer */
/**
设置SDL的音频参数 —-> 打开声音设备，播放静音 —-> ffmpeg读取音频流中数据放入队列 —-> SDL调用用户设置的函数来获取音频数据 —-> 播放音频

SDL内部维护了一个buffer来存放解码后的数据，这个buffer中的数据来源是我们注册的回调函数（audio_callback），audio_callback调用audio_decode_frame来做具体的音频解码工作，
需要引起注意的是：
    从流中读取出的一个音频包(avpacket)可能含有多个音频桢(avframe)，所以需要多次调用avcodec_decode_audio4来完成整个包的解码，解码出来的数据存放在我们自己的缓冲中（audio_buf2）。
    SDL每一次回调都会引起数据从audio_buf2拷贝到SDL内部缓冲区，当audio_buf2中的数据大于SDL的缓冲区大小时，需要分多次拷贝。
**/
static void sdl_audio_callback(void *opaque, Uint8 *stream, int len)
{
    VideoState *is = opaque;
    int audio_size, len1;
    double pts;

    audio_callback_time = av_gettime();

    /* len是由SDL传入的SDL缓冲区的大小，如果这个缓冲未满，我们就一直往里填充数据 */
    while (len > 0) {
        /*  audio_buf_index 和 audio_buf_size 标示我们自己用来放置解码出来的数据的缓冲区，这些数据待copy到SDL缓冲区， 
            当 audio_buf_index >= audio_buf_size 的时候意味着我们的音频缓冲为空，当前没有数据可供copy，这时候需要调用
            audio_decode_frame来解码出更多的桢数据 */
        if (is->audio_buf_index >= is->audio_buf_size) {
           audio_size = audio_decode_frame(is, &pts);
           /* audio_data_size < 0 标示没能解码出数据，我们默认播放静音 */
           if (audio_size < 0) {
                /* if error, just output silence */
               is->audio_buf = is->audio_buf1;
               is->audio_buf_size = 1024;
               /* 清零，静音 */
               memset(is->audio_buf, 0, is->audio_buf_size);
           } else {
               if (is->show_mode != SHOW_MODE_VIDEO)
                   update_sample_display(is, (int16_t *)is->audio_buf, audio_size);
               audio_size = synchronize_audio(is, (int16_t *)is->audio_buf, audio_size, pts);
               is->audio_buf_size = audio_size;
           }
           is->audio_buf_index = 0;
        }
        /*  查看stream可用空间，决定一次copy多少数据，剩下的下次继续copy */
        len1 = is->audio_buf_size - is->audio_buf_index;
        if (len1 > len)
            len1 = len;
        memcpy(stream, (uint8_t *)is->audio_buf + is->audio_buf_index, len1);
        len -= len1;
        stream += len1;
        is->audio_buf_index += len1;
    }
}

/* open a given stream. Return 0 if OK */
/**
    首先,avcodec_find_decoder()来通过AVCodecContext结构体中的code ID查找一个已经注册的音视频解码器；
    接着,初始化AVCodecContext结构体；
    最后,根据传入的参数创建 video_thread和subtitle_thread，音频直接由SDL输出；
**/
static int stream_component_open(VideoState *is, int stream_index)
{
    AVFormatContext *ic = is->ic;
    AVCodecContext *avctx;
    AVCodec *codec;
    /* 在用SDL_OpenAudio()打开音频设备的时候需要这两个参数*/
	/* wanted_spec是我们期望设置的属性，spec是系统最终接受的参数 */
	/* 我们需要检查系统接受的参数是否正确 */
    SDL_AudioSpec wanted_spec, spec;

    if (stream_index < 0 || stream_index >= ic->nb_streams)
        return -1;
    avctx = ic->streams[stream_index]->codec;

    /* prepare audio output */
    if (avctx->codec_type == AVMEDIA_TYPE_AUDIO) {
        if (avctx->channels > 0) {
            avctx->request_channels = FFMIN(2, avctx->channels);
        } else {
            avctx->request_channels = 2;
        }
    }
    /**
    avcodec_find_decoder()来通过code ID查找一个已经注册的音视频解码器；
        查找解码器之前,必须先调用av_register_all注册所有支持的解码器；
        查找成功返回解码器指针,否则返回NULL；   
        音视频解码器保存在一个链表中,查找过程中,函数从头到尾遍历链表,通过比较解码器的ID来查找；
    **/
    codec = avcodec_find_decoder(avctx->codec_id);
    if (!codec)
        return -1;

    avctx->workaround_bugs = workaround_bugs;
    avctx->lowres = lowres;
    if(avctx->lowres > codec->max_lowres){
        av_log(avctx, AV_LOG_WARNING, "The maximum value for lowres supported by the decoder is %d\n",
                codec->max_lowres);
        avctx->lowres= codec->max_lowres;
    }
    if(avctx->lowres) avctx->flags |= CODEC_FLAG_EMU_EDGE;
    avctx->idct_algo= idct;
    if(fast) avctx->flags2 |= CODEC_FLAG2_FAST;
    avctx->skip_frame= skip_frame;
    avctx->skip_idct= skip_idct;
    avctx->skip_loop_filter= skip_loop_filter;
    avctx->error_recognition= error_recognition;
    avctx->error_concealment= error_concealment;
    avctx->thread_count= thread_count;

    set_context_opts(avctx, avcodec_opts[avctx->codec_type], 0, codec);

    if(codec->capabilities & CODEC_CAP_DR1)
        avctx->flags |= CODEC_FLAG_EMU_EDGE;

    if (avcodec_open(avctx, codec) < 0)
        return -1;

    /* prepare audio output */
    if (avctx->codec_type == AVMEDIA_TYPE_AUDIO) {
        if(avctx->sample_rate <= 0 || avctx->channels <= 0){
            fprintf(stderr, "Invalid sample rate or channel count\n");
            return -1;
        }
        wanted_spec.freq = avctx->sample_rate;
        wanted_spec.format = AUDIO_S16SYS;          // 具体含义请查看“SDL宏定义”部分
        wanted_spec.channels = avctx->channels;
        wanted_spec.silence = 0;             // 0指示静音
        wanted_spec.samples = SDL_AUDIO_BUFFER_SIZE;        // 自定义SDL缓冲区大小
        
        /**
        ///设置音频输出回调函数
        callback是用户自定义的用于处理音频的函数，当有音频数据需要处理时就调用该函数。
            userdata是callback的第一个参数，在SDL_AudioSpec变量初始化时赋值，一般传递的是AVCodecContext变量。
            注解中指出，size是音频缓冲的大小，当SDL_OpenAudio()调用时计算得到。跟踪发现，size的值与audio_callback函数的第三个参数len的值相同。
        
        网上资料：
            通过SDL库对audio_callback的不断调用，不断解码数据，然后放到stream的末尾，SDL库认为stream中数据
            够播放一帧音频了，就播放它,第三个参数len是向stream中写数据的内存分配尺度，是分配给audio_callback函数写入缓存大小。
        **/
        wanted_spec.callback = sdl_audio_callback;          // 音频解码的关键回调函数
        wanted_spec.userdata = is;           // 传给上面回调函数的外带数据
        
        /*  SDL打开音频设备 */
        if (SDL_OpenAudio(&wanted_spec, &spec) < 0) {
            fprintf(stderr, "SDL_OpenAudio: %s\n", SDL_GetError());
            return -1;
        }
        /* 把设置好的参数保存到大结构中 */
        is->audio_hw_buf_size = spec.size;
        is->audio_src_fmt= AV_SAMPLE_FMT_S16;
    }

    ic->streams[stream_index]->discard = AVDISCARD_DEFAULT;
    switch(avctx->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
        is->audio_stream = stream_index;
        is->audio_st = ic->streams[stream_index];
        is->audio_buf_size = 0;
        is->audio_buf_index = 0;

        /* init averaging filter */
        is->audio_diff_avg_coef = exp(log(0.01) / AUDIO_DIFF_AVG_NB);
        is->audio_diff_avg_count = 0;
        /* since we do not have a precise anough audio fifo fullness,
           we correct audio sync only if larger than this threshold */
        is->audio_diff_threshold = 2.0 * SDL_AUDIO_BUFFER_SIZE / avctx->sample_rate;

        memset(&is->audio_pkt, 0, sizeof(is->audio_pkt));
        packet_queue_init(&is->audioq);
        SDL_PauseAudio(0);      // 开始播放静音
        break;
    case AVMEDIA_TYPE_VIDEO:
        is->video_stream = stream_index;
        is->video_st = ic->streams[stream_index];

        packet_queue_init(&is->videoq);
        /*   会使用函数avcodec_decode_video2 来解码视频，单独解码线程，解码后放入队列，能够减少绘制时延时 */
        is->video_tid = SDL_CreateThread(video_thread, is);
        break;
    case AVMEDIA_TYPE_SUBTITLE:
        is->subtitle_stream = stream_index;
        is->subtitle_st = ic->streams[stream_index];
        packet_queue_init(&is->subtitleq);

        is->subtitle_tid = SDL_CreateThread(subtitle_thread, is);
        break;
    default:
        break;
    }
    return 0;
}

static void stream_component_close(VideoState *is, int stream_index)
{
    AVFormatContext *ic = is->ic;
    AVCodecContext *avctx;

    if (stream_index < 0 || stream_index >= ic->nb_streams)
        return;
    avctx = ic->streams[stream_index]->codec;

    switch(avctx->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
        packet_queue_abort(&is->audioq);

        SDL_CloseAudio();

        packet_queue_end(&is->audioq);
        if (is->reformat_ctx)
            av_audio_convert_free(is->reformat_ctx);
        is->reformat_ctx = NULL;
        break;
    case AVMEDIA_TYPE_VIDEO:
        packet_queue_abort(&is->videoq);

        /* note: we also signal this mutex to make sure we deblock the
           video thread in all cases */
        SDL_LockMutex(is->pictq_mutex);
        SDL_CondSignal(is->pictq_cond);  //发送条件信号,方便等待数据的地方唤醒
        SDL_UnlockMutex(is->pictq_mutex);

        SDL_WaitThread(is->video_tid, NULL); //等待某一个线程退出

        packet_queue_end(&is->videoq);
        break;
    case AVMEDIA_TYPE_SUBTITLE:
        packet_queue_abort(&is->subtitleq);

        /* note: we also signal this mutex to make sure we deblock the
           video thread in all cases */
        SDL_LockMutex(is->subpq_mutex);
        is->subtitle_stream_changed = 1;

        SDL_CondSignal(is->subpq_cond);
        SDL_UnlockMutex(is->subpq_mutex);

        SDL_WaitThread(is->subtitle_tid, NULL);

        packet_queue_end(&is->subtitleq);
        break;
    default:
        break;
    }

    ic->streams[stream_index]->discard = AVDISCARD_ALL;
    avcodec_close(avctx);
    switch(avctx->codec_type) {
    case AVMEDIA_TYPE_AUDIO:
        is->audio_st = NULL;
        is->audio_stream = -1;
        break;
    case AVMEDIA_TYPE_VIDEO:
        is->video_st = NULL;
        is->video_stream = -1;
        break;
    case AVMEDIA_TYPE_SUBTITLE:
        is->subtitle_st = NULL;
        is->subtitle_stream = -1;
        break;
    default:
        break;
    }
}

/* since we have only one decoding thread, we can use a global variable instead of a thread local variable */
static VideoState *global_video_state;

static int decode_interrupt_cb(void)
{
    return (global_video_state && global_video_state->abort_request);
}

/* this thread gets the stream from the disk or the network */
/**
    负责将音视频流->Packets->Frame->push到各自的decode队列
    
    首先,使用avformat_open_input()以指定的媒体格式fmt或自动分析媒体文件头（解码器和解码器参数）得到的媒体文件指定的格式打开媒体文件filename，主要是结构体AVFormatContext中的AVIOContext结构的初始化；
    接着,av_find_stream_info()来获取必要的编解码器参数，设置到ic→streams[i]→codec中（也就是填充AVStream结构）；
    然后,调用stream_component_open()函数创建三个解码线程，音频、视频、subtitle；
    再然后,创建视频的刷新线程refresh_thread()，定时发送FF_REFRESH_EVENT消息消息队列，使用SDL消息机制刷新视频画面；
    最后,陷入死循环，作为分发各种格式帧的主线程循环，通过av_read_frame()读取音视频帧，放到各自的解码队列中；
**/
static int read_thread(void *arg)
{
    VideoState *is = arg;
    AVFormatContext *ic = NULL;
    int err, i, ret;
    int st_index[AVMEDIA_TYPE_NB];
    AVPacket pkt1, *pkt = &pkt1;
    int eof=0;
    int pkt_in_play_range = 0;
    AVDictionaryEntry *t;

    memset(st_index, -1, sizeof(st_index));
    is->video_stream = -1;
    is->audio_stream = -1;
    is->subtitle_stream = -1;

    global_video_state = is;
    avio_set_interrupt_cb(decode_interrupt_cb);

	/**
    在该函数中，FFMPEG根据文件的头信息及媒体流内的头部信息完成初始化AVFormatContext结构：
        1、输入输出结构体AVIOContext的初始化；
        2、输入数据的协议（例如RTMP，或者file）的识别（通过一套评分机制）: 
                1-判断文件名的后缀 
                2-读取文件头的数据进行比对；
        3、使用获得最高分的文件协议对应的URLProtocol，通过函数指针的方式，与FFMPEG连接（非专业用词）；\
            每个具体的输入协议都有自己对应的URLProtocol。
        4、调用该URLProtocol的url_open,url_read等这些函数完成各种具体输入协议的open,read等操作了;
      
    int avformat_open_input(AVFormatContext **ps, const char *filename, AVInputFormat *fmt, AVDictionary **options)      
    // ps 为成功打开媒体文件后的上下文。filename为要打开的文件名。
    // fmt为强制指定以何种格式打开文件，如果fmt为null，ffmpeg将自动分析媒体文件头，以媒体文件指定的格式（解码器和解码器参数）打开。
    // 该函数只分析解码器、解码器参数，并形成上下文，但不打开解码器。
    **/
    err = avformat_open_input(&ic, is->filename, is->iformat, &format_opts);
    if (err < 0) {
        print_error(is->filename, err);
        ret = -1;
        goto fail;
    }
    if ((t = av_dict_get(format_opts, "", NULL, AV_DICT_IGNORE_SUFFIX))) {
        av_log(NULL, AV_LOG_ERROR, "Option %s not found.\n", t->key);
        ret = AVERROR_OPTION_NOT_FOUND;
        goto fail;
    }
    is->ic = ic;

    if(genpts)
        ic->flags |= AVFMT_FLAG_GENPTS;
    
    /**
    该函数从文件中提取流信息；
        主要用于获取必要的编解码器参数，设置到ic→streams[i]→codec中。
        1、AVStream结构的大部分成员的值可以由avformat_open_input函数根据文件头的信息确定，缺少的信息    \
            需要通过调用av_find_stream_info函数读帧及软解码进一步获取；
        2、多媒体应用可以从AVFormatContext对象中拿到媒体文件的时间信息：
            主要是总时间长度和开始时间，
            此外还有与时间信息相关的比特率和文件大小。
        其中时间信息的单位是AV_TIME_BASE：微秒
    **/
    err = av_find_stream_info(ic);
    if (err < 0) {
        fprintf(stderr, "%s: could not find codec parameters\n", is->filename);
        ret = -1;
        goto fail;
    }
    if(ic->pb)
        ic->pb->eof_reached= 0; //FIXME hack, ffplay maybe should not use url_feof() to test for the end

    if(seek_by_bytes<0)
        seek_by_bytes= !!(ic->iformat->flags & AVFMT_TS_DISCONT);

    /* if seeking requested, we execute it */
    if (start_time != AV_NOPTS_VALUE) {
        int64_t timestamp;

        timestamp = start_time;
        /* add the stream start time */
        if (ic->start_time != AV_NOPTS_VALUE)
            timestamp += ic->start_time;
        /**
            使用ffmpeg的seek功能，基本上就是av_seek_frame和avformat_seek_file这两个API了;
            当外界发起seek请求后，将执行以下操作。
                1、调用avformat_seek_file()，完成文件的seek定位；
                2、清空解码前packet队列（音频、视频、字幕）；
                3、调用avcodec_flush_buffers()，清空解码buffer和相关状态；
            该函数会调用av_seek_frame()实现seek；
        **/
        ret = avformat_seek_file(ic, -1, INT64_MIN, timestamp, INT64_MAX, 0);
        if (ret < 0) {
            fprintf(stderr, "%s: could not seek to position %0.3f\n",
                    is->filename, (double)timestamp / AV_TIME_BASE);
        }
    }

    for (i = 0; i < ic->nb_streams; i++)
        ic->streams[i]->discard = AVDISCARD_ALL;
    /**
        在函数av_find_best_stream()里,通过find_program_from_stream()从流中查找出program，
    **/
    if (!video_disable)
        st_index[AVMEDIA_TYPE_VIDEO] =
            av_find_best_stream(ic, AVMEDIA_TYPE_VIDEO,
                                wanted_stream[AVMEDIA_TYPE_VIDEO], 
                                -1, NULL, 0);
    if (!audio_disable)
        st_index[AVMEDIA_TYPE_AUDIO] =
            av_find_best_stream(ic, AVMEDIA_TYPE_AUDIO,
                                wanted_stream[AVMEDIA_TYPE_AUDIO],
                                st_index[AVMEDIA_TYPE_VIDEO],
                                NULL, 0);
    if (!video_disable)
        st_index[AVMEDIA_TYPE_SUBTITLE] =
            av_find_best_stream(ic, AVMEDIA_TYPE_SUBTITLE,
                                wanted_stream[AVMEDIA_TYPE_SUBTITLE],
                                (st_index[AVMEDIA_TYPE_AUDIO] >= 0 ? st_index[AVMEDIA_TYPE_AUDIO] : st_index[AVMEDIA_TYPE_VIDEO]),
                                NULL, 0);
    if (show_status) {
        ///av_dump_format()为帮助函数，用来输出文件格式的信息，也就是我们在使用ffmpeg时能看到的文件详细信息；
        av_dump_format(ic, 0, is->filename, 0);
    }

    is->show_mode = show_mode;

    /* open the streams */
    /**
    stream_component_open()函数;
        1、依照编解码上下文的codec_id，遍历编解码器链表，通过avcodec_find_decoder找到相应的解码器；
        2、由avcodec_open打开编解码器，初始化具体编解码器的运行环；
        3、初始化音频、视频或subtitle队列；
        4、启动广义的音频解码、视频解码或subtitle解码线程；
    **/
    if (st_index[AVMEDIA_TYPE_AUDIO] >= 0) {
        stream_component_open(is, st_index[AVMEDIA_TYPE_AUDIO]);
    }

    ret=-1;
    if (st_index[AVMEDIA_TYPE_VIDEO] >= 0) {
        ret= stream_component_open(is, st_index[AVMEDIA_TYPE_VIDEO]);
    }
    /**
        视频的刷新线程refresh_thread()，使用SDL消息机制刷新视频画面；
        1、至于音频为何不需要这样的刷新线程，应该主要是因为音频是直接给硬件进行输出的，并且是严格按照
            媒体文件中的时间戳信息输出，因此是不需要刷新的；
        2、而视频则是有缓冲的，这就需要设置一个定时器来定时触发一个事件来把一帧图像从图像队列中显示到屏幕上；
    **/
    is->refresh_tid = SDL_CreateThread(refresh_thread, is);
    if (is->show_mode == SHOW_MODE_NONE)
        is->show_mode = ret >= 0 ? SHOW_MODE_VIDEO : SHOW_MODE_RDFT;

    if (st_index[AVMEDIA_TYPE_SUBTITLE] >= 0) {
        stream_component_open(is, st_index[AVMEDIA_TYPE_SUBTITLE]);
    }

    if (is->video_stream < 0 && is->audio_stream < 0) {
        fprintf(stderr, "%s: could not open codecs\n", is->filename);
        ret = -1;
        goto fail;
    }

    for(;;) {
        if (is->abort_request)
            break;
        if (is->paused != is->last_paused) {
            is->last_paused = is->paused;
            if (is->paused)
                is->read_pause_return= av_read_pause(ic);
            else
                av_read_play(ic);
        }
#if CONFIG_RTSP_DEMUXER
        if (is->paused && !strcmp(ic->iformat->name, "rtsp")) {
            /* wait 10 ms to avoid trying to get another packet */
            /* XXX: horrible */
            SDL_Delay(10);
            continue;
        }
#endif
        if (is->seek_req) {
            int64_t seek_target= is->seek_pos;
            int64_t seek_min= is->seek_rel > 0 ? seek_target - is->seek_rel + 2: INT64_MIN;
            int64_t seek_max= is->seek_rel < 0 ? seek_target - is->seek_rel - 2: INT64_MAX;
//FIXME the +-2 is due to rounding being not done in the correct direction in generation
//      of the seek_pos/seek_rel variables

            ret = avformat_seek_file(is->ic, -1, seek_min, seek_target, seek_max, is->seek_flags);
            if (ret < 0) {
                fprintf(stderr, "%s: error while seeking\n", is->ic->filename);
            }else{
                if (is->audio_stream >= 0) {
                    packet_queue_flush(&is->audioq);
                    packet_queue_put(&is->audioq, &flush_pkt);
                }
                if (is->subtitle_stream >= 0) {
                    packet_queue_flush(&is->subtitleq);
                    packet_queue_put(&is->subtitleq, &flush_pkt);
                }
                if (is->video_stream >= 0) {
                    packet_queue_flush(&is->videoq);
                    packet_queue_put(&is->videoq, &flush_pkt);
                }
            }
            is->seek_req = 0;
            eof= 0;
        }

        /* if the queue are full, no need to read more */
        if (   is->audioq.size + is->videoq.size + is->subtitleq.size > MAX_QUEUE_SIZE
            || (   (is->audioq.size  > MIN_AUDIOQ_SIZE || is->audio_stream<0)
                && (is->videoq.nb_packets > MIN_FRAMES || is->video_stream<0)
                && (is->subtitleq.nb_packets > MIN_FRAMES || is->subtitle_stream<0))) {
            /* wait 10 ms */
            SDL_Delay(10);
            continue;
        }
        if(eof) {
            if(is->video_stream >= 0){
                /**
                    初始化AVPacket结构
                **/
                av_init_packet(pkt);
                pkt->data=NULL;
                pkt->size=0;
                pkt->stream_index= is->video_stream;
                packet_queue_put(&is->videoq, pkt);
            }
            SDL_Delay(10);
            if(is->audioq.size + is->videoq.size + is->subtitleq.size ==0){
                ///到播放文件结尾后，判断是否是循环播放
                if(loop!=1 && (!loop || --loop)){
                    stream_seek(cur_stream, start_time != AV_NOPTS_VALUE ? start_time : 0, 0, 0);
                }else if(autoexit){
                    ret=AVERROR_EOF;
                    goto fail;
                }
            }
            eof=0;
            continue;
        }
        /**
            函数av_read_frame 作用是读取帧，读取码流中的若干帧音频或者一帧视频，推测单独拿出一个线程是因为接受网络数据时，怕因堵塞丢包;
            解码视频的时候，每解码一个视频帧，需要先调用 av_read_frame()获得一帧视频的压缩数据，然后才能对该数据进行解码;
        
        新版本的ffmpeg用的是av_read_frame，而老版本的是av_read_packet；区别是:
            av_read_packet读出的是包，它可能是半帧或多帧，不保证帧的完整性。  
            av_read_frame对av_read_packet进行了封装，使读出的数据总是完整的帧
            
        通过 av_read_packet()，读取一个包，需要说明的是此函数必须是包含整数帧的，不存在半帧的情况;
        以ts流为例：
        首先是读取一个完整的PES包（一个完整pes包包含若干视频或音频es包），读取完毕后，通过       \
            av_parser_parse2()分析出视频一帧（或音频若干帧），返回，下次进入循环的时候，如果     \
            上次的数据没有完全取完，则st = s->cur_st;不会是NULL，即再次进入av_parser_parse2()    \
            流程，而不是下面的av_read_packet（）流程。
        这样就保证了，如果读取一次包含了N帧视频数据（以视频为例），则调用av_read_frame（）N次    \
            都不会去读数据，而是返回第一次读取的数据，直到全部解析完毕。
        **/
        ret = av_read_frame(ic, pkt);
        if (ret < 0) {
            if (ret == AVERROR_EOF || url_feof(ic->pb))
                eof=1;
            if (ic->pb && ic->pb->error)
                break;
            SDL_Delay(100); /* wait for user event */
            continue;
        }
        /* check if packet is in play range specified by user, then queue, otherwise discard */
        /** 检查 packet包 是不是在用户识别的播放范围内（只是识别：audio_stream、video_stream、subtitle_stream这三种类型），如果在该范围内，则加入解码队列，否则丢弃**/
        pkt_in_play_range = (duration == AV_NOPTS_VALUE) ||
                ((pkt->pts - ic->streams[pkt->stream_index]->start_time) 
                * av_q2d(ic->streams[pkt->stream_index]->time_base) 
                - (double)(start_time != AV_NOPTS_VALUE ? start_time : 0)/1000000)
                <= ((double)duration/1000000);
        if (pkt->stream_index == is->audio_stream && pkt_in_play_range) {
            packet_queue_put(&is->audioq, pkt);
        } else if (pkt->stream_index == is->video_stream && pkt_in_play_range) {
            packet_queue_put(&is->videoq, pkt);
        } else if (pkt->stream_index == is->subtitle_stream && pkt_in_play_range) {
            packet_queue_put(&is->subtitleq, pkt);
        } else {
            av_free_packet(pkt);
        }
    }
    /* wait until the end */
    while (!is->abort_request) {
        SDL_Delay(100);
    }

    ret = 0;
 fail:
    /* disable interrupting */
    global_video_state = NULL;

    /* close each stream */
    if (is->audio_stream >= 0)
        stream_component_close(is, is->audio_stream);
    if (is->video_stream >= 0)
        stream_component_close(is, is->video_stream);
    if (is->subtitle_stream >= 0)
        stream_component_close(is, is->subtitle_stream);
    if (is->ic) {
        av_close_input_file(is->ic);
        is->ic = NULL; /* safety */
    }
    avio_set_interrupt_cb(NULL);

    /**
        push 退出消息FF_QUIT_EVENT事件
    **/
    if (ret != 0) {
        SDL_Event event;

        event.type = FF_QUIT_EVENT;
        event.user.data1 = is;
        SDL_PushEvent(&event);
    }
    return 0;
}

/** 
    首先创建 VideoState结构体：
        主要初始化成员filename和read_tid；以后所有的操作都基于这个结构体，并且一直在这个结构体中；
        至于为何使用一个大的结构体VideoState，并且几乎将所有的数据都放到这个结构体里面，应该是为       \
        了方便各个函数对数据进行的操作，更是方便数据的携带；
    
    然后创建read_thread线程；
**/
static VideoState *stream_open(const char *filename, AVInputFormat *iformat)
{
    VideoState *is;
    
    is = av_mallocz(sizeof(VideoState));
    if (!is)
        return NULL;
    av_strlcpy(is->filename, filename, sizeof(is->filename));
    is->iformat = iformat;
    is->ytop = 0;
    is->xleft = 0;

    /* start video display */
    is->pictq_mutex = SDL_CreateMutex();
    is->pictq_cond = SDL_CreateCond();

    is->subpq_mutex = SDL_CreateMutex();
    is->subpq_cond = SDL_CreateCond();

    is->av_sync_type = av_sync_type;
    is->read_tid = SDL_CreateThread(read_thread, is);
    if (!is->read_tid) {
        av_free(is);
        return NULL;
    }
    return is;
}

static void stream_cycle_channel(VideoState *is, int codec_type)
{
    AVFormatContext *ic = is->ic;
    int start_index, stream_index;
    AVStream *st;

    if (codec_type == AVMEDIA_TYPE_VIDEO)
        start_index = is->video_stream;
    else if (codec_type == AVMEDIA_TYPE_AUDIO)
        start_index = is->audio_stream;
    else
        start_index = is->subtitle_stream;
    if (start_index < (codec_type == AVMEDIA_TYPE_SUBTITLE ? -1 : 0))
        return;
    stream_index = start_index;
    for(;;) {
        if (++stream_index >= is->ic->nb_streams)
        {
            if (codec_type == AVMEDIA_TYPE_SUBTITLE)
            {
                stream_index = -1;
                goto the_end;
            } else
                stream_index = 0;
        }
        if (stream_index == start_index)
            return;
        st = ic->streams[stream_index];
        if (st->codec->codec_type == codec_type) {
            /* check that parameters are OK */
            switch(codec_type) {
            case AVMEDIA_TYPE_AUDIO:
                if (st->codec->sample_rate != 0 &&
                    st->codec->channels != 0)
                    goto the_end;
                break;
            case AVMEDIA_TYPE_VIDEO:
            case AVMEDIA_TYPE_SUBTITLE:
                goto the_end;
            default:
                break;
            }
        }
    }
 the_end:
    stream_component_close(is, start_index);
    stream_component_open(is, stream_index);
}


static void toggle_full_screen(void)
{
    is_full_screen = !is_full_screen;
    video_open(cur_stream);
}

static void toggle_pause(void)
{
    if (cur_stream)
        stream_toggle_pause(cur_stream);
    step = 0;
}

static void step_to_next_frame(void)
{
    if (cur_stream) {
        /* if the stream is paused unpause it, then step */
        if (cur_stream->paused)
            stream_toggle_pause(cur_stream);
    }
    step = 1;
}

static void toggle_audio_display(void)
{
    if (cur_stream) {
        int bgcolor = SDL_MapRGB(screen->format, 0x00, 0x00, 0x00);
        cur_stream->show_mode = (cur_stream->show_mode + 1) % SHOW_MODE_NB;
        fill_rectangle(screen,
                    cur_stream->xleft, cur_stream->ytop, cur_stream->width, cur_stream->height,
                    bgcolor);
        SDL_UpdateRect(screen, cur_stream->xleft, cur_stream->ytop, cur_stream->width, cur_stream->height);
    }
}

/* handle an event sent by the GUI */
/**
    消息事件循环：
    
**/
static void event_loop(void)
{
    SDL_Event event;
    double incr, pos, frac;

    for(;;) {
        double x;
        SDL_WaitEvent(&event);
        switch(event.type) {
        case SDL_KEYDOWN:
            if (exit_on_keydown) {
                do_exit();
                break;
            }
            switch(event.key.keysym.sym) {
            case SDLK_ESCAPE:
            case SDLK_q:
                do_exit();
                break;
            case SDLK_f:
                toggle_full_screen();
                break;
            case SDLK_p:
            case SDLK_SPACE:
                toggle_pause();
                break;
            case SDLK_s: //S: Step to next frame
                step_to_next_frame();
                break;
            case SDLK_a:
                if (cur_stream)
                    stream_cycle_channel(cur_stream, AVMEDIA_TYPE_AUDIO);
                break;
            case SDLK_v:
                if (cur_stream)
                    stream_cycle_channel(cur_stream, AVMEDIA_TYPE_VIDEO);
                break;
            case SDLK_t:
                if (cur_stream)
                    stream_cycle_channel(cur_stream, AVMEDIA_TYPE_SUBTITLE);
                break;
            case SDLK_w:
                toggle_audio_display();
                break;
            case SDLK_LEFT:
                incr = -10.0;
                goto do_seek;
            case SDLK_RIGHT:
                incr = 10.0;
                goto do_seek;
            case SDLK_UP:
                incr = 60.0;
                goto do_seek;
            case SDLK_DOWN:
                incr = -60.0;
            do_seek:
                if (cur_stream) {
                    if (seek_by_bytes) {
                        if (cur_stream->video_stream >= 0 && cur_stream->video_current_pos>=0){
                            pos= cur_stream->video_current_pos;
                        }else if(cur_stream->audio_stream >= 0 && cur_stream->audio_pkt.pos>=0){
                            pos= cur_stream->audio_pkt.pos;
                        }else
                            pos = avio_tell(cur_stream->ic->pb);
                        if (cur_stream->ic->bit_rate)
                            incr *= cur_stream->ic->bit_rate / 8.0;
                        else
                            incr *= 180000.0;
                        pos += incr;
                        stream_seek(cur_stream, pos, incr, 1);
                    } else {
                        pos = get_master_clock(cur_stream);
                        pos += incr;
                        stream_seek(cur_stream, (int64_t)(pos * AV_TIME_BASE), (int64_t)(incr * AV_TIME_BASE), 0);
                    }
                }
                break;
            default:
                break;
            }
            break;
        case SDL_MOUSEBUTTONDOWN:
            if (exit_on_mousedown) {
                do_exit();
                break;
            }
        case SDL_MOUSEMOTION:
            if(event.type ==SDL_MOUSEBUTTONDOWN){
                x= event.button.x;
            }else{
                if(event.motion.state != SDL_PRESSED)
                    break;
                x= event.motion.x;
            }
            if (cur_stream) {
                if(seek_by_bytes || cur_stream->ic->duration<=0){
                    uint64_t size=  avio_size(cur_stream->ic->pb);
                    stream_seek(cur_stream, size*x/cur_stream->width, 0, 1);
                }else{
                    int64_t ts;
                    int ns, hh, mm, ss;
                    int tns, thh, tmm, tss;
                    tns = cur_stream->ic->duration/1000000LL;
                    thh = tns/3600;
                    tmm = (tns%3600)/60;
                    tss = (tns%60);
                    frac = x/cur_stream->width;
                    ns = frac*tns;
                    hh = ns/3600;
                    mm = (ns%3600)/60;
                    ss = (ns%60);
                    fprintf(stderr, "Seek to %2.0f%% (%2d:%02d:%02d) of total duration (%2d:%02d:%02d)       \n", frac*100,
                            hh, mm, ss, thh, tmm, tss);
                    ts = frac*cur_stream->ic->duration;
                    if (cur_stream->ic->start_time != AV_NOPTS_VALUE)
                        ts += cur_stream->ic->start_time;
                    stream_seek(cur_stream, ts, 0, 0);
                }
            }
            break;
        case SDL_VIDEORESIZE:
            if (cur_stream) {
                screen = SDL_SetVideoMode(event.resize.w, event.resize.h, 0,
                                          SDL_HWSURFACE|SDL_RESIZABLE|SDL_ASYNCBLIT|SDL_HWACCEL);
                screen_width = cur_stream->width = event.resize.w;
                screen_height= cur_stream->height= event.resize.h;
            }
            break;
        case SDL_QUIT:
        case FF_QUIT_EVENT:
            do_exit();
            break;
        case FF_ALLOC_EVENT:
            video_open(event.user.data1);
            alloc_picture(event.user.data1);
            break;
        case FF_REFRESH_EVENT:
            video_refresh(event.user.data1);
            cur_stream->refresh=0;
            break;
        default:
            break;
        }
    }
}

static int opt_frame_size(const char *opt, const char *arg)
{
    if (av_parse_video_size(&frame_width, &frame_height, arg) < 0) {
        fprintf(stderr, "Incorrect frame size\n");
        return AVERROR(EINVAL);
    }
    if ((frame_width % 2) != 0 || (frame_height % 2) != 0) {
        fprintf(stderr, "Frame size must be a multiple of 2\n");
        return AVERROR(EINVAL);
    }
    return 0;
}

static int opt_width(const char *opt, const char *arg)
{
    screen_width = parse_number_or_die(opt, arg, OPT_INT64, 1, INT_MAX);
    return 0;
}

static int opt_height(const char *opt, const char *arg)
{
    screen_height = parse_number_or_die(opt, arg, OPT_INT64, 1, INT_MAX);
    return 0;
}

static int opt_format(const char *opt, const char *arg)
{
    file_iformat = av_find_input_format(arg);
    if (!file_iformat) {
        fprintf(stderr, "Unknown input format: %s\n", arg);
        return AVERROR(EINVAL);
    }
    return 0;
}

static int opt_frame_pix_fmt(const char *opt, const char *arg)
{
    frame_pix_fmt = av_get_pix_fmt(arg);
    return 0;
}

static int opt_sync(const char *opt, const char *arg)
{
    if (!strcmp(arg, "audio"))
        av_sync_type = AV_SYNC_AUDIO_MASTER;
    else if (!strcmp(arg, "video"))
        av_sync_type = AV_SYNC_VIDEO_MASTER;
    else if (!strcmp(arg, "ext"))
        av_sync_type = AV_SYNC_EXTERNAL_CLOCK;
    else {
        fprintf(stderr, "Unknown value for %s: %s\n", opt, arg);
        exit(1);
    }
    return 0;
}

static int opt_seek(const char *opt, const char *arg)
{
    start_time = parse_time_or_die(opt, arg, 1);
    return 0;
}

static int opt_duration(const char *opt, const char *arg)
{
    duration = parse_time_or_die(opt, arg, 1);
    return 0;
}

static int opt_thread_count(const char *opt, const char *arg)
{
    thread_count= parse_number_or_die(opt, arg, OPT_INT64, 0, INT_MAX);
#if !HAVE_THREADS
    fprintf(stderr, "Warning: not compiled with thread support, using thread emulation\n");
#endif
    return 0;
}

static int opt_show_mode(const char *opt, const char *arg)
{
    show_mode = !strcmp(arg, "video") ? SHOW_MODE_VIDEO :
                !strcmp(arg, "waves") ? SHOW_MODE_WAVES :
                !strcmp(arg, "rdft" ) ? SHOW_MODE_RDFT  :
                parse_number_or_die(opt, arg, OPT_INT, 0, SHOW_MODE_NB-1);
    return 0;
}

static int opt_input_file(const char *opt, const char *filename)
{
    if (input_filename) {
        fprintf(stderr, "Argument '%s' provided as input filename, but '%s' was already specified.\n",
                filename, input_filename);
        exit(1);
    }
    if (!strcmp(filename, "-"))
        filename = "pipe:";
    input_filename = filename;
    return 0;
}

static const OptionDef options[] = {
#include "cmdutils_common_opts.h"
    { "x", HAS_ARG, {(void*)opt_width}, "force displayed width", "width" },
    { "y", HAS_ARG, {(void*)opt_height}, "force displayed height", "height" },
    { "s", HAS_ARG | OPT_VIDEO, {(void*)opt_frame_size}, "set frame size (WxH or abbreviation)", "size" },
    { "fs", OPT_BOOL, {(void*)&is_full_screen}, "force full screen" },
    { "an", OPT_BOOL, {(void*)&audio_disable}, "disable audio" },
    { "vn", OPT_BOOL, {(void*)&video_disable}, "disable video" },
    { "ast", OPT_INT | HAS_ARG | OPT_EXPERT, {(void*)&wanted_stream[AVMEDIA_TYPE_AUDIO]}, "select desired audio stream", "stream_number" },
    { "vst", OPT_INT | HAS_ARG | OPT_EXPERT, {(void*)&wanted_stream[AVMEDIA_TYPE_VIDEO]}, "select desired video stream", "stream_number" },
    { "sst", OPT_INT | HAS_ARG | OPT_EXPERT, {(void*)&wanted_stream[AVMEDIA_TYPE_SUBTITLE]}, "select desired subtitle stream", "stream_number" },
    { "ss", HAS_ARG, {(void*)&opt_seek}, "seek to a given position in seconds", "pos" },
    { "t", HAS_ARG, {(void*)&opt_duration}, "play  \"duration\" seconds of audio/video", "duration" },
    { "bytes", OPT_INT | HAS_ARG, {(void*)&seek_by_bytes}, "seek by bytes 0=off 1=on -1=auto", "val" },
    { "nodisp", OPT_BOOL, {(void*)&display_disable}, "disable graphical display" },
    { "f", HAS_ARG, {(void*)opt_format}, "force format", "fmt" },
    { "pix_fmt", HAS_ARG | OPT_EXPERT | OPT_VIDEO, {(void*)opt_frame_pix_fmt}, "set pixel format", "format" },
    { "stats", OPT_BOOL | OPT_EXPERT, {(void*)&show_status}, "show status", "" },
    { "bug", OPT_INT | HAS_ARG | OPT_EXPERT, {(void*)&workaround_bugs}, "workaround bugs", "" },
    { "fast", OPT_BOOL | OPT_EXPERT, {(void*)&fast}, "non spec compliant optimizations", "" },
    { "genpts", OPT_BOOL | OPT_EXPERT, {(void*)&genpts}, "generate pts", "" },
    { "drp", OPT_INT | HAS_ARG | OPT_EXPERT, {(void*)&decoder_reorder_pts}, "let decoder reorder pts 0=off 1=on -1=auto", ""},
    { "lowres", OPT_INT | HAS_ARG | OPT_EXPERT, {(void*)&lowres}, "", "" },
    { "skiploop", OPT_INT | HAS_ARG | OPT_EXPERT, {(void*)&skip_loop_filter}, "", "" },
    { "skipframe", OPT_INT | HAS_ARG | OPT_EXPERT, {(void*)&skip_frame}, "", "" },
    { "skipidct", OPT_INT | HAS_ARG | OPT_EXPERT, {(void*)&skip_idct}, "", "" },
    { "idct", OPT_INT | HAS_ARG | OPT_EXPERT, {(void*)&idct}, "set idct algo",  "algo" },
    { "er", OPT_INT | HAS_ARG | OPT_EXPERT, {(void*)&error_recognition}, "set error detection threshold (0-4)",  "threshold" },
    { "ec", OPT_INT | HAS_ARG | OPT_EXPERT, {(void*)&error_concealment}, "set error concealment options",  "bit_mask" },
    { "sync", HAS_ARG | OPT_EXPERT, {(void*)opt_sync}, "set audio-video sync. type (type=audio/video/ext)", "type" },
    { "threads", HAS_ARG | OPT_EXPERT, {(void*)opt_thread_count}, "thread count", "count" },
    { "autoexit", OPT_BOOL | OPT_EXPERT, {(void*)&autoexit}, "exit at the end", "" },
    { "exitonkeydown", OPT_BOOL | OPT_EXPERT, {(void*)&exit_on_keydown}, "exit on key down", "" },
    { "exitonmousedown", OPT_BOOL | OPT_EXPERT, {(void*)&exit_on_mousedown}, "exit on mouse down", "" },
    { "loop", OPT_INT | HAS_ARG | OPT_EXPERT, {(void*)&loop}, "set number of times the playback shall be looped", "loop count" },
    { "framedrop", OPT_BOOL | OPT_EXPERT, {(void*)&framedrop}, "drop frames when cpu is too slow", "" },
    { "window_title", OPT_STRING | HAS_ARG, {(void*)&window_title}, "set window title", "window title" },
#if CONFIG_AVFILTER
    { "vf", OPT_STRING | HAS_ARG, {(void*)&vfilters}, "video filters", "filter list" },
#endif
    { "rdftspeed", OPT_INT | HAS_ARG| OPT_AUDIO | OPT_EXPERT, {(void*)&rdftspeed}, "rdft speed", "msecs" },
    { "showmode", HAS_ARG, {(void*)opt_show_mode}, "select show mode (0 = video, 1 = waves, 2 = RDFT)", "mode" },
    { "default", HAS_ARG | OPT_AUDIO | OPT_VIDEO | OPT_EXPERT, {(void*)opt_default}, "generic catch all option", "" },
    { "i", HAS_ARG, {(void *)opt_input_file}, "read specified file", "input_file"},
    { NULL, },
};

static void show_usage(void)
{
    printf("Simple media player\n");
    printf("usage: ffplay [options] input_file\n");
    printf("\n");
}

static int opt_help(const char *opt, const char *arg)
{
    av_log_set_callback(log_callback_help);
    show_usage();
    show_help_options(options, "Main options:\n",
                      OPT_EXPERT, 0);
    show_help_options(options, "\nAdvanced options:\n",
                      OPT_EXPERT, OPT_EXPERT);
    printf("\n");
    av_opt_show2(avcodec_opts[0], NULL,
                 AV_OPT_FLAG_DECODING_PARAM, 0);
    printf("\n");
    av_opt_show2(avformat_opts, NULL,
                 AV_OPT_FLAG_DECODING_PARAM, 0);
#if !CONFIG_AVFILTER
    printf("\n");
    av_opt_show2(sws_opts, NULL,
                 AV_OPT_FLAG_ENCODING_PARAM, 0);
#endif
    printf("\nWhile playing:\n"
           "q, ESC              quit\n"
           "f                   toggle full screen\n"
           "p, SPC              pause\n"
           "a                   cycle audio channel\n"
           "v                   cycle video channel\n"
           "t                   cycle subtitle channel\n"
           "w                   show audio waves\n"
           "s                   activate frame-step mode\n"
           "left/right          seek backward/forward 10 seconds\n"
           "down/up             seek backward/forward 1 minute\n"
           "mouse click         seek to percentage in file corresponding to fraction of width\n"
           );
    return 0;
}


/* Called from the main */
/**
    首先,注册编解码器、复用-解复用器、协议等;
    接着,按照flag标志初始化SDL各模块，并屏蔽一些SDL监听的系统消息事件；
    然后,调用stream_open()；
    最后,陷入event_loop()函数的消息事件循环，监听SDL消息；
**/
int main(int argc, char **argv)
{
    int flags;

    av_log_set_flags(AV_LOG_SKIP_REPEATED);

    /** 
        register all codecs, demux and protocols 
    */
    avcodec_register_all();
#if CONFIG_AVDEVICE
    avdevice_register_all();
#endif
#if CONFIG_AVFILTER
    avfilter_register_all();
#endif
    av_register_all();

	/** 
    for av-structs alloc memory and initialize
        AVCodecContext结构，这是个描述编解码器上下文的数据结构，包含了众多编解码器需要的参数      \
                信息。若是单纯使用libavcodec，这部分信息需要调用者进行初始化；
        AVFormatContext结构，描述了一个媒体文件或媒体流的构成和基本信息。是FFMPEG解封装（flv，    \
                mp4，avi等）功能的结构体，也是其他所有结构的根，是一个多媒体文件或流的根本抽象。
                如果调用者希望自己创建该结构，则需要显式为该结构的一些成员置缺省值——如果没有      \
                缺省值，会导致之后的动作产生异常。
        SwsContext结构，是用来变换图像格式的。
    init_opts()在退出时调用 uninit_opts();
    **/
    init_opts();

	/** output version-info onto stderr **/
    show_banner();  

	/** parse paramter-options which from command line, and get input_filename  **/
    parse_options(argc, argv, options, opt_input_file);
    if (!input_filename) {
        show_usage();
        fprintf(stderr, "An input file must be specified\n");
        fprintf(stderr, "Use -h to get full help or, even better, run 'man ffplay'\n");
        exit(1);
    }

    if (display_disable) {
        video_disable = 1;
    }

	/** set SDL initialization-flags to use SDL output **/
    flags = SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER;
    if (audio_disable)
        flags &= ~SDL_INIT_AUDIO;
#if !defined(__MINGW32__) && !defined(__APPLE__)
    flags |= SDL_INIT_EVENTTHREAD; /* Not supported on Windows or Mac OS X */
#endif
    if (SDL_Init (flags)) {
        fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
        exit(1);
    }

	/** now get width && heiight, current screen_width && screen_height should be 0 **/
    if (!display_disable) {
#if HAVE_SDL_VIDEO_SIZE
        const SDL_VideoInfo *vi = SDL_GetVideoInfo();
        fs_screen_width = vi->current_w;
        fs_screen_height = vi->current_h;
#endif
    }

	/** 
		ignore SDL event:
		SDL_ACTIVEEVENT //窗口焦点、输入焦点及鼠标焦点的失去和得到事件
		SDL_SYSWMEVENT  //平台相关的系统事件
		SDL_USEREVENT   //用户自定义事件
	**/
    SDL_EventState(SDL_ACTIVEEVENT, SDL_IGNORE);
    SDL_EventState(SDL_SYSWMEVENT, SDL_IGNORE);
    SDL_EventState(SDL_USEREVENT, SDL_IGNORE);

	/** 
        initialize Packets ( an AVPacket-struct);
    先来区分下一些概念，视频文件有很多基本的组成部分。
        首先，文件本身被称为容器Container，容器的类型决定了信息被存放在文件中的位置。        \
            AVI格式就是一种容器的例子。
        接着，从容器中会得到一组数据流，通常的是一个音频流和一个视频流。（                   \
            一个流只是一种想像出来的词语，用来表示一连串的通过时间来串连的数据元素）。
            在流中的数据结构元素被称为帧Frame。 
            每个流是由不同的编码器来编码生成的。编解码器描述了实际的数据是如何被编码Coded    \
            和解码DECoded的，因此它的名字叫做CODEC。Divx和 MP3就是编解码器的例子。
        接着从流中被读出来的叫做包Packets。包是一段数据，它包含了一段可以被解码成方便我们    \
            最后在应用程序中操作的原始帧的数据。 每个包包含了完整的帧或半帧 或者对于音频     \
            来说是许多格式的完整帧。包Packets不保证帧的完整性；这里是指 AVPacket结构；
                    
        AVPacket结构，定义在avcodec.h中，是存储压缩编码数据相关信息的结构体；ffmpeg 使用     \
                AVPacket来暂存解复用之后、解码之前的媒体数据（一个音/视频帧、一个字幕包等）  \
                及附加信息（解码时间戳、显示时间戳、时长等）
    **/
    av_init_packet(&flush_pkt);
    flush_pkt.data= "FLUSH";

    /** 
        基本上来说，处理视频和音频流一般如下：
            1 从video.avi文件中打开视频流video_stream；
            2 从视频流中读取包Packets到帧Frame中；
            3 如果这个帧Frame还不完整，跳到2；
            4 对这个帧Frame进行一些操作；
            5 跳回到2；
    在退出时需调用stream_close()释放资源；
    **/ 
    cur_stream = stream_open(input_filename, file_iformat);

    event_loop();

    /* never returns */
    return 0;
}


/**

ffplay基本框架分析：
1.      初始化化工作:注册解码器，编码器，demux.mux,网络等 
    avcodec_register_all();
#if CONFIG_AVDEVICE
    avdevice_register_all();
#endif
#if CONFIG_AVFILTER
    avfilter_register_all();
#endif
    av_register_all();
    avformat_network_init();
av_register_all();
包含编解码器资源，demux资源，拉流协议资源
这里注册了所有的文件格式和编解码器的库，所以它们将被自动的使用在被打开的合适格式的文件上。注意你只需要调用 av_register_all()一次，因此我们在主函数main()中来调用它。如果你喜欢，也可以只注册特定的格式和编解码器，但是通常你没有必要这样做。
注意av_register_all();包含 avcodec_register_all(); 所以在ffplay中注册avcodec_register_all();是多余的。不过没有关系，每次注册都要检测是否已经注册了，如果已经注册了，就不会第二次注册了

2.      解析命令行，打显示banner信息
     show_banner(argc, argv, options);
     parse_options(NULL, argc, argv, options,opt_input_file);

3.      初始化SDL（因为视频显示是基于SDL媒体框架，所以必须出示SDL）
SDL_Init（）

4.      初始化化AVPacket;（改结构体描述音视频的属性信息，如pts,dts,pos,duration等）
av_init_packet(&flush_pkt);

5.      打开流对象，进行解码播放
   stream_open(input_filename,file_iformat);
   ffplay核心就在这个函数，这个函数创建了一个read_thread,线程完成了拉流（读文件），demux(解析，解复用)，decoder（解码），display（显示）动作，即完成读文件模块(source filter)，解复用模块(Demux filter)，视/音频解码模块(Decode filter)，颜色空间转换模块(Color Space converter filter)，视频/音频播放模块(Renderfilter)三个模块的过程。

6.      事件处理event_loop(is);
event_loop(is);就是一个死循环，不断接受外部事件的动作，完成对播放的控制，如暂停，快进，快退，resume，窗口缩放等。这些事件来自于SDL从GUI中获取，触发源主要是鼠标和键盘的动作。
这样5，6动作就是实现了主线程完成播放控制，子线程完成解码播放的动作。主线程在循环中，由于某种外部事件触发，改变全局的播放控制变量，而子线程在每解码一帧前都要判断该控制变量，进而决定trick的动作或窗口的大小。


stream_open(input_filename, file_iformat)函数的简要分析：
1.      为VideoState分配内存
2.        av_strlcpy(is->filename, filename,sizeof(is->filename));将文件名赋给VideoState 初始化VideoState其他成员。
3.         创建read_thread 线程

read_thread 的分析：
avformat_open_input（）完成各种协议拉流（读文件）动作
av_find_best_stream（）找到因视频流，字幕流
stream_component_open()打开音视频流并音打开视频解码器并创建
video_thread()，subtitle_thread（）进行解码
refresh_thread（）开启屏幕视频显示刷新线程
av_read_frame（）将音视频读取，解析并写入解码器





        
1. 注册所有容器格式和CODEC:av_register_all()
2. 打开文件:av_open_input_file()
3. 从文件中提取流信息:av_find_stream_info()
4. 穷举所有的流，查找其中种类为CODEC_TYPE_VIDEO
5. 查找对应的解码器:avcodec_find_decoder()
6. 打开编解码器:avcodec_open()
7. 为解码帧分配内存:avcodec_alloc_frame()
8. 不停地从码流中提取出帧数据:av_read_frame()
9. 判断帧的类型，对于视频帧调用:avcodec_decode_video()
10. 解码完后，释放解码器:avcodec_close()
11. 关闭输入文件:av_close_input_file()

首先第一件事情就是开一个视频文件并从中得到流。我们要做的第一件事情就是使用av_register_all();来初始化libavformat/libavcodec: 
    这一步注册库中含有的所有可用的文件格式和编码器，这样当打开一个文件时，它们才能够自动选择相应的文件格式和编码器。
    av_register_all()只需调用一次，所以，要放在初始化代码中。也可以仅仅注册个人的文件格式和编码。

下一步，打开文件：
        AVFormatContext *pFormatCtx;
        const char      *filename="myvideo.mpg";
        av_open_input_file(&pFormatCtx, filename, NULL, 0, NULL)；   // 打开视频文件
    最后三个参数描述了文件格式，缓冲区大小（size）和格式参数；
    我们通过简单地指明NULL或0告诉 libavformat 去自动探测文件格式并且使用默认的缓冲区大小。
    这里的格式参数指的是视频输出参数，比如宽高的坐标。

再下一步，我们需要取出包含在文件中的流信息：
        av_find_stream_info(pFormatCtx)；    // 取出流信息  
        AVFormatContext 结构体  
        dump_format(pFormatCtx, 0, filename, false);   //我们可以使用这个函数把获取到得参数全部输出。  
        
        for(i=0; i<pFormatCtx->nb_streams; i++)    //区分视频流和音频流  
         if(pFormatCtx->streams->codec.codec_type==CODEC_TYPE_VIDEO) //找到视频流，这里也可以换成音频  
            {  
                videoStream=i;  
                break;  
            }  

接下来就需要寻找解码器
        AVCodec *pCodec;  
        pCodec=avcodec_find_decoder(pCodecCtx->codec_id);  
          
        avcodec_open(pCodecCtx, pCodec)；    // 打开解码器  

然后再给视频帧分配空间以便存储解码后的图片：
        AVFrame *pFrame;  
        pFrame=avcodec_alloc_frame();  


/////////////开始解码/////////////

第一步当然是读数据：
    我们将要做的是通过读取包来读取整个视频流，然后把它解码成帧，最后转换格式并且保存。
    while(av_read_frame(pFormatCtx, &packet)>=0) {     //读数据  
        if(packet.stream_index==videoStream){          //判断是否视频流  
         avcodec_decode_video(pCodecCtx,pFrame, &frameFinished, packet.data, packet.size);     //解码  

        if(frameFinished) {  
            img_convert((AVPicture *)pFrameRGB, PIX_FMT_RGB24,(AVPicture*)pFrame,
                            pCodecCtx->pix_fmt, pCodecCtx->width,pCodecCtx->height);     //转换   
        }     
  
        SaveFrame(pFrameRGB, pCodecCtx->width,pCodecCtx->height, i); //保存数据  
  
        av_free_packet(&packet);      //释放  


    av_read_frame()读取一个包并且把它保存到AVPacket结构体中。这些数据可以在后面通过av_free_packet()来释放。
    函数avcodec_decode_video()把包转换为帧。
    然而当解码一个包的时候，我们可能没有得到我们需要的关于帧的信息。
    因此，当我们得到下一帧的时候，avcodec_decode_video()为我们设置了帧结束标志frameFinished。
    
最后，我们使用 img_convert()函数来把帧从原始格式（pCodecCtx->pix_fmt）转换成为RGB格式。
要记住，你可以把一个 AVFrame结构体的指针转换为AVPicture结构体的指针。


最后，我们把帧和高度宽度信息传递给我们的SaveFrame函数。



======================================================================================
使用ffmpeg步骤：

av_register_all();//初始化ffmpeg库，如果系统里面的ffmpeg没配置好这里会出错
 if (isNetwork) {
     //需要播放网络视频
     avformat_network_init();
 }
 avformat_open_input();//打开视频文件
 avformat_find_stream_info();//查找文件的流信息
 av_dump_format();//dump只是个调试函数，输出文件的音、视频流的基本信息了，帧率、分辨率、音频采样等等
 for(...);//遍历文件的各个流，找到第一个视频流，并记录该流的编码信息
 sws_getContext();//根据编码信息设置渲染格式
 avcodec_find_decoder();//在库里面查找支持该格式的解码器
 avcodec_open2();//打开解码器
 pFrame=avcodec_alloc_frame();//分配一个帧指针，指向解码后的原始帧
 pFrameRGB=avcodec_alloc_frame();//分配一个帧指针，指向存放转换成RGB后的帧
 avpicture_fill(pFrameRGB);//给pFrameRGB帧加上分配的内存;
 while(true)
 {
     av_read_frame();//读取一个帧（到最后帧则break）
     avcodec_decode_video2();//解码该帧
     sws_getCachedContext()sws_scale（）;//把该帧转换（渲染）成RGB
     SaveFrame();//对前5帧保存成ppm图形文件(这个是自定义函数，非API)
     av_free_packet();//释放本次读取的帧内存
 }




**/




/*
源码分析：
    首先，熟悉基本的大体上的流程；
        音视频解码流程：
        
    接着，了解所使用的数据结构，以及数据结构在哪个阶段使用；
    
    然后，从main函数开始按照流程向下依次捋顺；
        主要是 把每个函数作为过滤器，根据处理流程，先看每个函数的输入数据结构和输出数据结构；
















*/
