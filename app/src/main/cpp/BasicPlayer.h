/*
 * Main functions of BasicPlayer
 * 2011-2011 Jaebong Lee (novaever@gmail.com)
 *
 * BasicPlayer is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#ifndef BASICPLAYER_H__INCED__110326
#define BASICPLAYER_H__INCED__110326

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <android/bitmap.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/log.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include <jni.h>

extern "C" {
    #include "libavutil/time.h"
    #include "libavcodec/avcodec.h"
    #include "libavformat/avformat.h"
    #include "libswscale/swscale.h"
    #include "libswresample/swresample.h"
    #include "libavutil/avstring.h"
    #include "linkedqueue.h"
}

//#include "testh.h"

// return: == 0 - success
//          < 0 - error code

#define VIDEO_PICTURE_QUEUE_SIZE 1

typedef struct VideoPicture {
    double pts;
    bool isEnd;
} VideoPicture;

typedef struct PacketQueue {

    AVPacketList *first_pkt, *last_pkt;
    int nb_packets;
    int size;
    int abort_request;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} PacketQueue;

typedef struct VideoState {

    int ready;
    int paused;
    int abort_request;

    VideoPicture pictq[VIDEO_PICTURE_QUEUE_SIZE];
    int pictq_rindex;
    int pictq_windex;
    int pictq_size;

    pthread_t video_tid;
    pthread_t audio_tid;

    pthread_mutex_t pause_mutex;

    pthread_mutex_t pictq_mutex;
    pthread_cond_t pictq_cond;

    PacketQueue videoq;
    PacketQueue audioq;

    AVFormatContext *ic;
    int video_stream;
    int audio_stream;

    double frame_last_pts;
    double frame_skip_last_pts;
    double frame_last_delay;
    double frame_timer;
    double frame_skip_timer;
    double          video_current_pts;
    int64_t         video_current_pts_time;

    AVStream        *video_st;
    AVStream        *audio_st;

    int             seek_req;
    int             seek_flags;
    int64_t         seek_pos;

    double audio_clock;
} VideoState;

// thread
void* decode_thread(void* arge);
void* video_thread(void *arg);
void* refresh_thread(void *arg);

int openMovie(const char filePath[]);
int startMovie();
void copyPixels(uint8_t *pixels);
int getWidth();
int getHeight();
void clearMovie(void);
void do_exit(void);
void closePlayer();
void stream_close(VideoState *is);
void stream_component_close(VideoState *is, int stream_index);
void closeAudio();
void createEngine();
void createBufferQueueAudioPlayer(int rate, int channel, int bitsPerSample);
void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context);
void tbqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context);
double get_master_clock(VideoState *is);
void stream_seek(double rel);
void stream_seek_to(double seekPos);
void changeAutoRepeatState(int state);
double getAutoRepeatStartPts();
double getAutoRepeatEndPts();
void stream_pause(VideoState *is);
void setWindow(ANativeWindow* nativeWindow);
int stream_component_open(VideoState *is, int stream_index, ANativeWindow* nativeWindow);
void render(ANativeWindow* nativeWindow);

double get_video_clock(VideoState *is);

#endif
