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
#include <android/bitmap.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"

// return: == 0 - success
//          < 0 - error code

typedef struct VideoState {
    double          video_current_pts;
    int64_t         video_current_pts_time;

    AVStream        *video_st;

    int             seek_req;
    int             seek_flags;
    int64_t         seek_pos;
} VideoState;

int openMovie(ANativeWindow* nativeWindow, const char filePath[]);
int decodeFrame(ANativeWindow* nativeWindow);
void copyPixels(uint8_t *pixels);
int getWidth();
int getHeight();
void closeMovie();

void createEngine();
void createBufferQueueAudioPlayer(int rate, int channel, int bitsPerSample);
void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context);
void tbqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context);
double get_master_clock(VideoState *is);
void stream_seek(double rel);

#endif
