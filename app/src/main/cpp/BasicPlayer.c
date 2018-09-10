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
#include "linkedqueue.h"
#include <unistd.h>

pthread_t refresh_tid;
pthread_mutex_t refresh_mutex;
LinkedQueue* refreshTimeQueue;

size_t outputBufferSize;

AVPacket packet;
int audioStream;
SwrContext *swr;
AVFrame *audioFrame;

AVPacket flush_pkt;

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

//AVCodecContext *gVideoCodecCtx = NULL;
//AVCodecContext *gAudioCodecCtx = NULL;
//AVCodec *gVideoCodec = NULL;
//AVCodec *gAudioCodec = NULL;
//int gVideoStreamIdx = -1;
//int gAudioStreamIdx = -1;

AVFrame *gFrame = NULL;
AVFrame *gFrameRGB = NULL;

struct SwsContext *gImgConvertCtx = NULL;

int gPictureSize = 0;
uint8_t *gVideoBuffer = NULL;

uint8_t *audioOutputBuffer = NULL;
int audio_data_size = 0;

VideoState      *is;


/* packet queue handling */
void packet_queue_init(PacketQueue *q)
{
    memset(q, 0, sizeof(PacketQueue));
    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->cond, NULL);
}

void packet_queue_flush(PacketQueue *q)
{
    AVPacketList *pkt, *pkt1;

    pthread_mutex_lock(&q->mutex);
    for(pkt = q->first_pkt; pkt != NULL; pkt = pkt1) {
        pkt1 = pkt->next;
        av_free_packet(&pkt->pkt);
        av_freep(&pkt);
    }
    q->last_pkt = NULL;
    q->first_pkt = NULL;
    q->nb_packets = 0;
    q->size = 0;
    pthread_mutex_unlock(&q->mutex);
}

int packet_queue_put(PacketQueue *q, AVPacket *pkt)
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

    pthread_mutex_lock(&q->mutex);

    if (!q->last_pkt)

        q->first_pkt = pkt1;
    else
        q->last_pkt->next = pkt1;
    q->last_pkt = pkt1;
    q->nb_packets++;
    q->size += pkt1->pkt.size + sizeof(*pkt1);
    /* XXX: should duplicate packet data in DV case */
    pthread_cond_signal(&q->cond);

    pthread_mutex_unlock(&q->mutex);
    return 0;
}

/* return < 0 if aborted, 0 if no packet and > 0 if packet.  */
int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block)
{
    AVPacketList *pkt1;
    int ret;

    pthread_mutex_lock(&q->mutex);

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
            pthread_cond_wait(&q->cond, &q->mutex);
        }
    }
    pthread_mutex_unlock(&q->mutex);
    return ret;
}

void createEngine() {

    is = av_mallocz(sizeof(VideoState));
    is->pictq_rindex = 0;
    is->frame_last_pts = 0;
    is->pictq_size = 0;

    av_init_packet(&flush_pkt);
    flush_pkt.data= "FLUSH";

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

    int video_index, audio_index;

    AVFormatContext *ic = avformat_alloc_context();

    if (avformat_open_input(&ic, filePath, NULL, NULL) != 0)
        return -2;

    is->nativeWindow = nativeWindow;
    is->ic = ic;

    if (avformat_find_stream_info(ic, 0) < 0)
        return -3;

    int i;
    for (i = 0; i < ic->nb_streams; i++) {
        AVCodecContext *enc = ic->streams[i]->codec;

        if (enc->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_index = i;
        }
        if (enc->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_index = i;
        }
    }

    if (audio_index >= 0) {
        stream_component_open(is, audio_index, NULL);
    }

    if (video_index >= 0) {
        stream_component_open(is, video_index, nativeWindow);
    }

    refreshTimeQueue = createLinkedQueue();

    //pthread_create(&refresh_tid, NULL, refresh_thread, NULL);

    for(;;){
        decodeFrame(nativeWindow);
    }

    return 0;
}

void schedule_refresh(int delay)
{
    QueueNode node;
    node.data = delay;
    enqueueLQ(refreshTimeQueue, node);
}

double compute_frame_delay(double frame_current_pts, VideoState *is)
{
    double actual_delay, delay, sync_threshold, ref_clock, diff;

    /* compute nominal delay */
    delay = frame_current_pts - is->frame_last_pts;
    if (delay <= 0 || delay >= 10.0) {
        /* if incorrect delay, use previous one */
        delay = is->frame_last_delay;
    } else {
        is->frame_last_delay = delay;
    }
    is->frame_last_pts = frame_current_pts;

    is->frame_timer += delay;
    /* compute the REAL delay (we need to do that to avoid
       long term errors */
    actual_delay = is->frame_timer - (av_gettime() / 1000000.0);
    if (actual_delay < 0.010) {
        /* XXX: should skip picture */
        actual_delay = 0.010;
    }

    return actual_delay;
}

void video_refresh_timer()
{
    VideoPicture *vp;

    if (is->pictq_size == 0) {
        /* if no picture, need to wait */
        //schedule_refresh(1);
    }else{
        vp = &is->pictq[is->pictq_rindex];

        /* update current video pts */
        is->video_current_pts = vp->pts;
        is->video_current_pts_time = av_gettime();

        schedule_refresh((int)(compute_frame_delay(vp->pts, is) * 1000 + 0.5));

        pthread_mutex_lock(&is->pictq_mutex);
        is->pictq_size--;
        pthread_cond_signal(&is->pictq_cond);
        pthread_mutex_unlock(&is->pictq_mutex);
    }
}

int refresh_thread(void *arg)
{
    for(;;){
        if (isLinkedQueueEmpty(refreshTimeQueue) == FALSE) {
            QueueNode* pNode = dequeueLQ(refreshTimeQueue);

            usleep(pNode->data);
            video_refresh_timer();
        }
    }

    return NULL;
}

int queue_picture(AVFrame *src_frame, double pts){

    /*
    pthread_mutex_lock(&is->pictq_mutex);

    while (is->pictq_size >= VIDEO_PICTURE_QUEUE_SIZE){
        pthread_cond_wait(&is->pictq_cond, &is->pictq_mutex);
    }

    pthread_mutex_unlock(&is->pictq_mutex);
      */

    ANativeWindow_Buffer windowBuffer;

    gImgConvertCtx = sws_getCachedContext(gImgConvertCtx,
                                          is->video_st->codec->width,
                                          is->video_st->codec->height,
                                          is->video_st->codec->pix_fmt,
                                          is->video_st->codec->width,
                                          is->video_st->codec->height, PIX_FMT_RGBA,
                                          SWS_BICUBIC, NULL, NULL, NULL);

    sws_scale(gImgConvertCtx, src_frame->data, src_frame->linesize, 0,
              is->video_st->codec->height, gFrameRGB->data, gFrameRGB->linesize);

    ANativeWindow_lock(is->nativeWindow, &windowBuffer, 0);

    uint8_t *dst = windowBuffer.bits;
    int dstStride = windowBuffer.stride * 4;
    uint8_t *src = (uint8_t *) (gFrameRGB->data[0]);
    int srcStride = gFrameRGB->linesize[0];

    int h;
    for (h = 0; h < is->video_st->codec->height; h++) {
        memcpy(dst + h * dstStride, src + h * srcStride, srcStride);
    }

    ANativeWindow_unlockAndPost(is->nativeWindow);

    /*
    pthread_mutex_lock(&is->pictq_mutex);
    is->pictq_size++;
    pthread_mutex_unlock(&is->pictq_mutex);
     */
}

int video_thread(void *arg)
{
    AVPacket *pkt;
    int got_picture = 0;

    AVFrame *frame= avcodec_alloc_frame();

    for(;;) {

        if (packet_queue_get(&is->videoq, pkt, 1) < 0)
            break;

        avcodec_decode_video2(is->video_st->codec, frame, &got_picture, pkt);

        double pts;
        if(pkt->dts != AV_NOPTS_VALUE)
            pts= pkt->dts;
        else
            pts= 0;
        pts *= av_q2d(is->video_st->time_base);

        ANativeWindow_Buffer windowBuffer;

        if (got_picture) {

            //queue_picture(frame, pts);

            gImgConvertCtx = sws_getCachedContext(gImgConvertCtx,
                                                  is->video_st->codec->width,
                                                  is->video_st->codec->height,
                                                  is->video_st->codec->pix_fmt,
                                                  is->video_st->codec->width,
                                                  is->video_st->codec->height, PIX_FMT_RGBA,
                                                  SWS_BICUBIC, NULL, NULL, NULL);

            sws_scale(gImgConvertCtx, frame->data, frame->linesize, 0,
                      is->video_st->codec->height, gFrameRGB->data, gFrameRGB->linesize);

            ANativeWindow_lock(is->nativeWindow, &windowBuffer, 0);

            uint8_t *dst = windowBuffer.bits;
            int dstStride = windowBuffer.stride * 4;
            uint8_t *src = (uint8_t *) (gFrameRGB->data[0]);
            int srcStride = gFrameRGB->linesize[0];

            int h;
            for (h = 0; h < is->video_st->codec->height; h++) {
                memcpy(dst + h * dstStride, src + h * srcStride, srcStride);
            }

            ANativeWindow_unlockAndPost(is->nativeWindow);


            av_free_packet(pkt);

            //usleep(21000);
        }
    }
}

void* audio_thread(void *t){
    bqPlayerCallback(bqPlayerBufferQueue, NULL);
    return NULL;
}

int stream_component_open(VideoState *is, int stream_index, ANativeWindow* nativeWindow)
{
    AVFormatContext *ic = is->ic;
    AVCodecContext *enc;
    AVCodec *codec;

    enc = ic->streams[stream_index]->codec;
    codec = avcodec_find_decoder(enc->codec_id);
    if (avcodec_open2(enc, codec, NULL) < 0)
        return -1;

    switch(enc->codec_type) {
        case AVMEDIA_TYPE_VIDEO:
            is->frame_last_delay = 40e-3;
            is->frame_timer = (double)av_gettime() / 1000000.0;
            is->video_current_pts_time = av_gettime();

            is->video_stream = stream_index;
            is->video_st = ic->streams[stream_index];

            packet_queue_init(&is->videoq);

            gFrame = avcodec_alloc_frame();
            if (gFrame == NULL)
                return -7;

            gFrameRGB = avcodec_alloc_frame();
            if (gFrameRGB == NULL)
                return -8;

            // video init

            gPictureSize = avpicture_get_size(PIX_FMT_RGBA, enc->width, enc->height);
            gVideoBuffer = (uint8_t*)(malloc(sizeof(uint8_t) * gPictureSize));
            avpicture_fill((AVPicture*)gFrameRGB, gVideoBuffer, PIX_FMT_RGBA, enc->width, enc->height);

            ANativeWindow_setBuffersGeometry(nativeWindow,  enc->width, enc->height, WINDOW_FORMAT_RGBA_8888);
            ANativeWindow_Buffer windowBuffer;

            pthread_create(&is->video_tid, NULL, video_thread, NULL);
            break;
        case AVMEDIA_TYPE_AUDIO:
            is->audio_stream = stream_index;
            is->audio_st = ic->streams[stream_index];
            packet_queue_init(&is->audioq);

            // audio init
            swr = swr_alloc_set_opts(NULL,
                                     enc->channel_layout, AV_SAMPLE_FMT_S16, enc->sample_rate,
                                     enc->channel_layout, enc->sample_fmt, enc->sample_rate,
                                     0, NULL);
            swr_init(swr);

            outputBufferSize = 8196;
            audioOutputBuffer = (uint8_t *) malloc(sizeof(uint8_t) * outputBufferSize);

            int rate = enc->sample_rate;
            int channel = enc->channels;

            audioFrame = avcodec_alloc_frame();

            createBufferQueueAudioPlayer(rate, channel, SL_PCMSAMPLEFORMAT_FIXED_16);

            pthread_create(&is->audio_tid, NULL, audio_thread, NULL);
            //tbqPlayerCallback(bqPlayerBufferQueue, NULL);
            break;
        default:
            break;
    }
}

int decodeFrame(ANativeWindow* nativeWindow)
{
    int frameFinished = 0;
    AVPacket packet;

    static AVFrame audioFrame;
    ANativeWindow_Buffer windowBuffer;

    if(is->seek_req) {
        int stream_index= -1;
        int64_t seek_target = is->seek_pos;

        stream_index = is->video_stream;

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
        if (packet.stream_index == is->video_stream) {

            packet_queue_put(&is->videoq, &packet);

            return 0;

        }else if(packet.stream_index == is->audio_stream){

            packet_queue_put(&is->audioq, &packet);

        }
    }else{
        printf("abc");
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
    return is->video_st->codec->width;
}

int getHeight()
{
    return is->video_st->codec->height;
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

    if (is->video_st->codec != NULL) {
        avcodec_close(is->video_st->codec);
        is->video_st->codec = NULL;
    }

    if (is->ic != NULL) {
        //av_close_input_file(gFormatCtx);
        is->ic = NULL;
    }
    return;
}

AVPacket audioPacket;

void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context){

    for (;;)
    {
        if (packet_queue_get(&is->audioq, &audioPacket, 1) < 0)
            return;

        AVCodecContext *dec = is->audio_st->codec;
        int frameFinished = 0;

        avcodec_decode_audio4(dec, audioFrame, &frameFinished, &audioPacket);
        if (frameFinished) {
            audio_data_size = av_samples_get_buffer_size(
                    audioFrame->linesize, dec->channels,
                    audioFrame->nb_samples, dec->sample_fmt, 1);

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
                (void) result;

                av_free_packet(&audioPacket);
            }
            return;
        }
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


