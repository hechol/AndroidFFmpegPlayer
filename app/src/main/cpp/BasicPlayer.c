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

AVFormatContext *gFormatCtx = NULL;

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

int openMovie(ANativeWindow* nativeWindow, const char filePath[])
{
    int i;

    if (gFormatCtx != NULL)
        return -1;

    gFormatCtx = avformat_alloc_context();

    if (avformat_open_input(&gFormatCtx, filePath, NULL, NULL) != 0)
        return -2;

    if (avformat_find_stream_info(gFormatCtx, 0) < 0)
        return -3;

    for (i = 0; i < gFormatCtx->nb_streams; i++) {
        if (gFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
            gVideoStreamIdx = i;
        }
        if (gFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
            gAudioStreamIdx = i;
        }
    }
    if (gVideoStreamIdx == -1)
        return -4;

    gVideoCodecCtx = gFormatCtx->streams[gVideoStreamIdx]->codec;
    gAudioCodecCtx = gFormatCtx->streams[gAudioStreamIdx]->codec;

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
    createEngine();
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

    while (av_read_frame(gFormatCtx, &packet) >= 0) {
        if (packet.stream_index == gVideoStreamIdx) {
            avcodec_decode_video2(gVideoCodecCtx, gFrame, &frameFinished, &packet);

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

    if (gFormatCtx != NULL) {
        //av_close_input_file(gFormatCtx);
        gFormatCtx = NULL;
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

    while (av_read_frame(gFormatCtx, &audioPacket) >= 0) {
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

    while (av_read_frame(gFormatCtx, &audioPacket) >= 0) {
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

void createEngine()
{
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


