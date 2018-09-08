/*
 * Main functions of BasicPlayer
 * 2011-2011 Jaebong Lee (novaever@gmail.com)
 *
 * BasicPlayer is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */


#include "BasicPlayer.h"
#include <unistd.h>

size_t outputBufferSize;

AVPacket packet;
int audioStream;
SwrContext *swr;
AVFrame *audioFrame;

// engine interfaces
static SLObjectItf engineObject = NULL;
static SLEngineItf engineEngine;

// output mix interfaces
static SLObjectItf outputMixObject = NULL;
static SLEnvironmentalReverbItf outputMixEnvironmentalReverb = NULL;

// buffer queue player interfaces
static SLObjectItf bqPlayerObject = NULL;
static SLPlayItf bqPlayerPlay;
static SLAndroidSimpleBufferQueueItf bqPlayerBufferQueue;
static SLEffectSendItf bqPlayerEffectSend;
static SLMuteSoloItf bqPlayerMuteSolo;
static SLVolumeItf bqPlayerVolume;

// aux effect on the output mix, used by the buffer queue player
static const SLEnvironmentalReverbSettings reverbSettings =
        SL_I3DL2_ENVIRONMENT_PRESET_STONECORRIDOR;

static void *buffer;
static size_t bufferSize;

//AVFormatContext *gFormatCtx = NULL;

AVCodecContext *gVideoCodecCtx = NULL;
AVCodecContext *gAudioCodecCtx = NULL;
AVCodec *gVideoCodec = NULL;
AVCodec *gAudioCodec = NULL;
int gVideoStreamIdx = -1;
int gAudioStreamIdx = -1;

AVFrame *gFrame = NULL;
AVFrame *gFrameRGB = NULL;

struct SwsContext *gImgConvertCtx = NULL;

int gPictureSize = 0;
uint8_t *gVideoBuffer = NULL;

uint8_t *audioOutputBuffer = NULL;
int audio_data_size = 0;



VideoState      *is;


void createEngine()
{
    is = av_mallocz(sizeof(VideoState));

    SLresult result;

    // create engine
    result = slCreateEngine(&engineObject, 0, NULL, 0, NULL, NULL);

    // realize the engine
    result = (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);

    // get the engine interface, which is needed in order to create other objects
    result = (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE, &engineEngine);

    // create output mix, with environmental reverb specified as a non-required interface
    const SLInterfaceID ids[1] = {SL_IID_ENVIRONMENTALREVERB};
    const SLboolean req[1] = {SL_BOOLEAN_FALSE};
    result = (*engineEngine)->CreateOutputMix(engineEngine, &outputMixObject, 0, 0, 0);

    // realize the output mix
    result = (*outputMixObject)->Realize(outputMixObject, SL_BOOLEAN_FALSE);

    // get the environmental reverb interface
    // this could fail if the environmental reverb effect is not available,
    // either because the feature is not present, excessive CPU load, or
    // the required MODIFY_AUDIO_SETTINGS permission was not requested and granted
    result = (*outputMixObject)->GetInterface(outputMixObject, SL_IID_ENVIRONMENTALREVERB,
                                              &outputMixEnvironmentalReverb);
    if (SL_RESULT_SUCCESS == result) {
        result = (*outputMixEnvironmentalReverb)->SetEnvironmentalReverbProperties(
                outputMixEnvironmentalReverb, &reverbSettings);
    }
}

int openMovie(ANativeWindow* nativeWindow, const char filePath[])
{
    createEngine();

    AVFormatContext *ic = avformat_alloc_context();

    if (avformat_open_input(&ic, filePath, NULL, NULL) != 0)
        return -2;

    is->ic = ic;

    if (avformat_find_stream_info(ic, 0) < 0)
        return -3;

    int i;
    for (i = 0; i < ic->nb_streams; i++) {
        AVCodecContext *enc = ic->streams[i]->codec;
        //ic->streams[i]->discard = AVDISCARD_ALL;

        if (enc->codec_type == AVMEDIA_TYPE_VIDEO) {
            gVideoStreamIdx = i;
        }
        if (enc->codec_type == AVMEDIA_TYPE_AUDIO) {
            gAudioStreamIdx = i;
        }
    }
    if (gVideoStreamIdx == -1)
        return -4;

    gVideoCodecCtx = ic->streams[gVideoStreamIdx]->codec;
    gAudioCodecCtx = ic->streams[gAudioStreamIdx]->codec;

    is->video_st = ic->streams[gVideoStreamIdx];

    gVideoCodec = avcodec_find_decoder(gVideoCodecCtx->codec_id);
    if (gVideoCodec == NULL)
        return -5;
    gAudioCodec = avcodec_find_decoder(gAudioCodecCtx->codec_id);
    if (gAudioCodec == NULL)
        return -5;

    if (avcodec_open2(gVideoCodecCtx, gVideoCodec, NULL) < 0)
        return -6;
    if (avcodec_open2(gAudioCodecCtx, gAudioCodec, NULL) < 0)
        return -6;

    gFrame = avcodec_alloc_frame();
    if (gFrame == NULL)
        return -7;

    gFrameRGB = avcodec_alloc_frame();
    if (gFrameRGB == NULL)
        return -8;

    // video init

    gPictureSize = avpicture_get_size(PIX_FMT_RGBA, gVideoCodecCtx->width, gVideoCodecCtx->height);
    gVideoBuffer = (uint8_t*)(malloc(sizeof(uint8_t) * gPictureSize));
    avpicture_fill((AVPicture*)gFrameRGB, gVideoBuffer, PIX_FMT_RGBA, gVideoCodecCtx->width, gVideoCodecCtx->height);

    // audio init
    swr = swr_alloc_set_opts(NULL,
                             gAudioCodecCtx->channel_layout, AV_SAMPLE_FMT_S16, gAudioCodecCtx->sample_rate,
                             gAudioCodecCtx->channel_layout, gAudioCodecCtx->sample_fmt, gAudioCodecCtx->sample_rate,
                             0, NULL);
    swr_init(swr);

    outputBufferSize = 8196;
    audioOutputBuffer = (uint8_t *) malloc(sizeof(uint8_t) * outputBufferSize);

    int rate = gAudioCodecCtx->sample_rate;
    int channel = gAudioCodecCtx->channels;

    audioFrame = avcodec_alloc_frame();

    createBufferQueueAudioPlayer(rate, channel, SL_PCMSAMPLEFORMAT_FIXED_16);
    //tbqPlayerCallback(bqPlayerBufferQueue, NULL);

    ANativeWindow_setBuffersGeometry(nativeWindow,  gVideoCodecCtx->width, gVideoCodecCtx->height, WINDOW_FORMAT_RGBA_8888);
    ANativeWindow_Buffer windowBuffer;

    for(;;){
        decodeFrame(nativeWindow);
    }

    return 0;
}

int decodeFrame(ANativeWindow* nativeWindow)
{
    int frameFinished = 0;
    AVPacket packet;

    static AVFrame audioFrame;
    ANativeWindow_Buffer windowBuffer;

    for(;;){

        if(is->seek_req) {
            int stream_index= -1;
            int64_t seek_target = is->seek_pos;

            if     (gVideoStreamIdx >= 0) stream_index = gVideoStreamIdx;
            else if(gAudioStreamIdx >= 0) stream_index = gAudioStreamIdx;

            if(stream_index>=0){
                seek_target= av_rescale_q(seek_target, AV_TIME_BASE_Q,
                                          is->ic->streams[stream_index]->time_base);
            }
            if(av_seek_frame(is->ic, stream_index,
                             seek_target, is->seek_flags) < 0) {
                fprintf(stderr, "%s: error while seeking\n",
                        is->ic->filename);
            }
            is->seek_req = 0;
        }

        if(av_read_frame(is->ic, &packet) >= 0) {
            if (packet.stream_index == gVideoStreamIdx) {
                avcodec_decode_video2(gVideoCodecCtx, gFrame, &frameFinished, &packet);

                double pts;
                if ((pts = av_frame_get_best_effort_timestamp(gFrame)) == AV_NOPTS_VALUE) {
                    pts = 0;
                }
                pts *= av_q2d(is->video_st->time_base);
                is->video_current_pts = pts;
                is->video_current_pts_time = av_gettime();

                if (frameFinished) {
                    gImgConvertCtx = sws_getCachedContext(gImgConvertCtx,
                                                          gVideoCodecCtx->width, gVideoCodecCtx->height, gVideoCodecCtx->pix_fmt,
                                                          gVideoCodecCtx->width, gVideoCodecCtx->height, PIX_FMT_RGBA, SWS_BICUBIC, NULL, NULL, NULL);

                    sws_scale(gImgConvertCtx, gFrame->data, gFrame->linesize, 0, gVideoCodecCtx->height, gFrameRGB->data, gFrameRGB->linesize);

                    ANativeWindow_lock(nativeWindow, &windowBuffer, 0);

                    uint8_t * dst = windowBuffer.bits;
                    int dstStride = windowBuffer.stride * 4;
                    uint8_t * src = (uint8_t*) (gFrameRGB->data[0]);
                    int srcStride = gFrameRGB->linesize[0];

                    int h;
                    for (h = 0; h < gVideoCodecCtx->height; h++) {
                        memcpy(dst + h * dstStride, src + h * srcStride, srcStride);
                    }

                    ANativeWindow_unlockAndPost(nativeWindow);

                    av_free_packet(&packet);

                    usleep(11000);

                    return 0;
                }
            }else if(packet.stream_index == gAudioStreamIdx){
                avcodec_decode_audio4(gAudioCodecCtx, &audioFrame, &frameFinished, &packet);
                if (frameFinished) {
                    audio_data_size = av_samples_get_buffer_size(
                            audioFrame.linesize, gAudioCodecCtx->channels,
                            audioFrame.nb_samples, gAudioCodecCtx->sample_fmt, 1);

                    if (audio_data_size > outputBufferSize) {
                        audioOutputBuffer = (uint8_t *) realloc(audioOutputBuffer,
                                                           sizeof(uint8_t) * outputBufferSize);
                    }

                    swr_convert(swr, &audioOutputBuffer, audioFrame.nb_samples,
                                (uint8_t const **) (audioFrame.extended_data),
                                audioFrame.nb_samples);

                    if (NULL != audioOutputBuffer && 0 != audio_data_size) {

                        SLresult result;
                        // enqueue another buffer
                        result = (*bqPlayerBufferQueue)->Enqueue(bqPlayerBufferQueue, audioOutputBuffer,
                                                                 audio_data_size);
                        // the most likely other result is SL_RESULT_BUFFER_INSUFFICIENT,
                        // which for this code example would indicate a programming error
                        (void)result;
                    }

                    av_free_packet(&packet);
                    return 0;
                }else{
                    av_free_packet(&packet);
                }
            }
        }
    }

    return -1;
}

void copyPixels(uint8_t *pixels)
{
    memcpy(pixels, gFrameRGB->data[0], gPictureSize);
    return;
}

int getWidth()
{
    return gVideoCodecCtx->width;
}

int getHeight()
{
    return gVideoCodecCtx->height;
}

void closeMovie()
{
    if (gVideoBuffer != NULL) {
        free(gVideoBuffer);
        gVideoBuffer = NULL;
    }

    if (gFrame != NULL)
        av_freep(gFrame);
    if (gFrameRGB != NULL)
        av_freep(gFrameRGB);

    if (gVideoCodecCtx != NULL) {
        avcodec_close(gVideoCodecCtx);
        gVideoCodecCtx = NULL;
    }

    if (is->ic != NULL) {
        //av_close_input_file(gFormatCtx);
        is->ic = NULL;
    }
    return;
}

AVPacket audioPacket;

void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context){
    return;
}


// this callback handler is called every time a buffer finishes playing
void tbqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context)
{
    //assert(NULL == context);

    int frameFinished = 0;

    while (av_read_frame(is->ic, &audioPacket) >= 0) {
        if(audioPacket.stream_index == gAudioStreamIdx){
            avcodec_decode_audio4(gAudioCodecCtx, audioFrame, &frameFinished, &audioPacket);
            if (frameFinished) {
                audio_data_size = av_samples_get_buffer_size(
                        audioFrame->linesize, gAudioCodecCtx->channels,
                        audioFrame->nb_samples, gAudioCodecCtx->sample_fmt, 1);

                if (audio_data_size > outputBufferSize) {
                    audioOutputBuffer = (uint8_t *) realloc(audioOutputBuffer,
                                                            sizeof(uint8_t) * outputBufferSize);
                }

                swr_convert(swr, &audioOutputBuffer, audioFrame->nb_samples,
                            (uint8_t const **) (audioFrame->extended_data),
                            audioFrame->nb_samples);

                // for streaming playback, replace this test by logic to find and fill the next buffer
                if (NULL != audioOutputBuffer && 0 != audio_data_size) {



                    SLresult result;
                    // enqueue another buffer
                    result = (*bqPlayerBufferQueue)->Enqueue(bqPlayerBufferQueue, audioOutputBuffer,
                                                             audio_data_size);
                    // the most likely other result is SL_RESULT_BUFFER_INSUFFICIENT,
                    // which for this code example would indicate a programming error
                    (void)result;

                    av_free_packet(&audioPacket);


                }else{
                    return;
                }
            }
        }
    }
    return;
}

// this callback handler is called every time a buffer finishes playing
void bbqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context)
{
    //assert(NULL == context);

    int frameFinished = 0;

    while (av_read_frame(is->ic, &audioPacket) >= 0) {
        if(audioPacket.stream_index == gAudioStreamIdx){
            avcodec_decode_audio4(gAudioCodecCtx, audioFrame, &frameFinished, &audioPacket);
            if (frameFinished) {
                audio_data_size = av_samples_get_buffer_size(
                        audioFrame->linesize, gAudioCodecCtx->channels,
                        audioFrame->nb_samples, gAudioCodecCtx->sample_fmt, 1);

                if (audio_data_size > outputBufferSize) {
                    audioOutputBuffer = (uint8_t *) realloc(audioOutputBuffer,
                                                            sizeof(uint8_t) * outputBufferSize);
                }

                swr_convert(swr, &audioOutputBuffer, audioFrame->nb_samples,
                            (uint8_t const **) (audioFrame->extended_data),
                            audioFrame->nb_samples);
                break;
            }
        }

        av_free_packet(&audioPacket);
    }

    // for streaming playback, replace this test by logic to find and fill the next buffer
    if (NULL != audioOutputBuffer && 0 != audio_data_size) {
        SLresult result;
        // enqueue another buffer
        result = (*bqPlayerBufferQueue)->Enqueue(bqPlayerBufferQueue, audioOutputBuffer,
                                                 audio_data_size);
        // the most likely other result is SL_RESULT_BUFFER_INSUFFICIENT,
        // which for this code example would indicate a programming error
        (void)result;
    }
    return;
}


// create buffer queue audio player
void createBufferQueueAudioPlayer(int rate, int channel, int bitsPerSample)
{
    SLresult result;

    // configure audio source
    SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2};
    SLDataFormat_PCM format_pcm;
    format_pcm.formatType = SL_DATAFORMAT_PCM;
    format_pcm.numChannels = channel;
    format_pcm.samplesPerSec = rate * 1000;
    format_pcm.bitsPerSample = bitsPerSample;
    format_pcm.containerSize = 16;
    if (channel == 2)
        format_pcm.channelMask = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
    else
        format_pcm.channelMask = SL_SPEAKER_FRONT_CENTER;
    format_pcm.endianness = SL_BYTEORDER_LITTLEENDIAN;
    SLDataSource audioSrc = {&loc_bufq, &format_pcm};

    // configure audio sink
    SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, outputMixObject};
    SLDataSink audioSnk = {&loc_outmix, NULL};

    // create audio player
    const SLInterfaceID ids[3] = {SL_IID_BUFFERQUEUE, SL_IID_EFFECTSEND,
            /*SL_IID_MUTESOLO,*/ SL_IID_VOLUME};
    const SLboolean req[3] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE,
            /*SL_BOOLEAN_TRUE,*/ SL_BOOLEAN_TRUE};
    result = (*engineEngine)->CreateAudioPlayer(engineEngine, &bqPlayerObject, &audioSrc, &audioSnk,
                                                3, ids, req);

    (void)result;

    // realize the player
    result = (*bqPlayerObject)->Realize(bqPlayerObject, SL_BOOLEAN_FALSE);

    (void)result;

    // get the play interface
    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_PLAY, &bqPlayerPlay);

    (void)result;

    // get the buffer queue interface
    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_BUFFERQUEUE,
                                             &bqPlayerBufferQueue);

    (void)result;

    // register callback on the buffer queue
    result = (*bqPlayerBufferQueue)->RegisterCallback(bqPlayerBufferQueue, bqPlayerCallback, NULL);

    (void)result;

    // get the effect send interface
    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_EFFECTSEND,
                                             &bqPlayerEffectSend);

    (void)result;

    // get the volume interface
    result = (*bqPlayerObject)->GetInterface(bqPlayerObject, SL_IID_VOLUME, &bqPlayerVolume);

    (void)result;

    // set the player's state to playing
    result = (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_PLAYING);

    (void)result;

    return;
}

double get_video_clock(VideoState *is) {
    double delta;

    delta = (av_gettime() - is->video_current_pts_time) / 1000000.0;
    return is->video_current_pts + delta;
}

double get_master_clock(VideoState *is) {
     return get_video_clock(is);
}

void stream_seek(double rel) {
    double pos = get_master_clock(is);
    pos += rel;
    pos = (int64_t)(pos * AV_TIME_BASE);

    if(!is->seek_req) {
        is->seek_pos = pos;
        is->seek_flags = rel < 0 ? AVSEEK_FLAG_BACKWARD : 0;
        is->seek_req = 1;
    }
}


