
#include "BasicPlayer.h"

#include <unistd.h>

ANativeWindow* gNativeWindow;

JavaVM *g_VM;
JNIEnv *refreashJniEnv;
JNIEnv *videoJniEnv;
JNIEnv *decodeJniEnv;
jobject javaObject_PlayCallback;
jclass javaClass_PlayCallback;
jmethodID javaMethod_updateClock;
jmethodID javaMethod_seekEnd;
jmethodID javaMethod_movieEnd;

pthread_t parse_tid;
pthread_t refresh_tid;
pthread_t audio_tid;
pthread_mutex_t video_queue_mutex;
pthread_mutex_t refresh_mutex;
pthread_mutex_t refresh_queue_mutex;
LinkedQueue* refreshTimeQueue;

size_t outputBufferSize;

AVPacket packet;
int audioStream;
SwrContext *swr;
AVFrame *audioFrame;

AVPacket flush_pkt;
AVPacket end_pkt;

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

AVFrame *gFrameRGB = NULL;

struct SwsContext *gImgConvertCtx = NULL;

int gPictureSize = 0;
uint8_t *gVideoBuffer = NULL;

uint8_t *audioOutputBuffer = NULL;
int audio_data_size = 0;

VideoState      *is;

int auto_repeat_off= 0;
int auto_repeat_select_A = 1;
int auto_repeat_on = 2;
int auto_repeat_on_wait = 3;
int auto_repeat_on_working = 4;

int auto_repeat_state = 0;

double autoRepeatStartPts = 0;
double autoRepeatEndPts= 10;

int autoRepeatState = 0;

bool end_read_frame = false;
int skip_frame_count = 0;
int skip_level = 0;
AVCodecContext *videoCodecContext;

void* audio_thread(void *arg)
{
    bqPlayerCallback(bqPlayerBufferQueue, NULL);

    return NULL;
}

double get_video_pts(double pts, VideoState *is){
    if(pts != AV_NOPTS_VALUE){

    }
    else{
        pts= 0;
    }

    pts *= av_q2d(is->video_st->time_base);
    return pts;
}

double get_audio_pts(double pts, VideoState *is){
    if(pts != AV_NOPTS_VALUE){

    }
    else{
        pts= 0;
    }

    pts *= av_q2d(is->audio_st->time_base);
    return pts;
}

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
        av_packet_unref(&pkt->pkt);
        av_freep(&pkt);
    }
    q->last_pkt = NULL;
    q->first_pkt = NULL;
    q->nb_packets = 0;
    q->size = 0;
    pthread_mutex_unlock(&q->mutex);
}

PacketQueue *test __attribute__((aligned(8)));
int my_counter __attribute__((aligned(8)));
int my_counter2 __attribute__((aligned(4)));

int packet_queue_put(PacketQueue *q, AVPacket *pkt)
{

    my_counter = 1;
    my_counter2 = 1;

    test = q;
    AVPacketList *pkt1;

    pthread_mutex_lock(&q->mutex);

    pkt1 = (AVPacketList *)av_malloc(sizeof(AVPacketList));
    if (!pkt1)
        return -1;
    pkt1->pkt = *pkt;
    pkt1->next = NULL;

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

void packet_queue_abort(PacketQueue *q)
{
    pthread_mutex_lock(&q->mutex);

    q->abort_request = 1;

    pthread_cond_signal(&q->cond);

    pthread_mutex_unlock(&q->mutex);
}

void packet_queue_end(PacketQueue *q)
{
    packet_queue_flush(q);
    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->cond);
}

void createEngine() {

    //int t = getTest(2);
    //char *org_name = (char *)malloc(sizeof(char));
    //free(org_name);
    //char *org_name2 = (char *)malloc(sizeof(char)*25);
    //free(org_name2);
    //uint8_t *gVideoBuffer2 = (uint8_t*)(malloc(sizeof(uint8_t) * 5));
    //free(gVideoBuffer2);

    avformat_network_init();

    av_init_packet(&flush_pkt);
    flush_pkt.data= (uint8_t *)"FLUSH";

    av_init_packet(&end_pkt);
    end_pkt.data= (uint8_t *)"END";

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

void setWindow(ANativeWindow* nativeWindow){


    gNativeWindow = nativeWindow;

    /*
    if(is != NULL) {
        if(is->ic->nb_streams > 0) {
            int i;
            int video_index = 0;
            AVCodecContext *enc;

            for (i = 0; i < is->ic->nb_streams; i++) {
                AVCodecContext *enc = is->ic->streams[i]->codec;

                if (enc->codec_type == AVMEDIA_TYPE_VIDEO) {
                    video_index = i;
                }
            }
            enc = is->ic->streams[video_index]->codec;

            ANativeWindow_setBuffersGeometry(ntiveWindow, enc->width, enc->height,
                                             WINDOW_FORMAT_RGBA_8888);

            render(ntiveWindow);
        }

    }
    */

}

void render(ANativeWindow* nativeWindow){

    ANativeWindow_Buffer windowBuffer;
    ARect rect;
    rect.left = 0;
    rect.right = 500;
    rect.top = 0;
    rect.bottom = 500;
    ANativeWindow_lock(nativeWindow, &windowBuffer, NULL);

    uint8_t *dst = (uint8_t *)windowBuffer.bits;
    int dstStride = windowBuffer.stride * 4;
    uint8_t *src = (uint8_t *) (gFrameRGB->data[0]);
    int srcStride = gFrameRGB->linesize[0];

    int h;
    for (h = 0; h < is->video_st->codec->height; h++) {
        memcpy(dst + h * dstStride, src + h * srcStride, srcStride);
    }

    ANativeWindow_unlockAndPost(nativeWindow);
}

char gFilePath[1024];

int openMovie(const char filePath[])
{
    AVFormatContext *ic = avformat_alloc_context();

    if (avformat_open_input(&ic, filePath, NULL, NULL) != 0){
        avformat_close_input(&ic);
        return -1;
    }

    if (avformat_find_stream_info(ic, 0) < 0){
        avformat_close_input(&ic);
        return -2;
    }

    if(ic->duration < 0){
        return - 3;
    }

    is = (VideoState*)av_mallocz(sizeof(VideoState));

    is->ready = 0;
    is->abort_request = 0;
    is->paused = 0;

    is->pictq_rindex = 0;
    is->pictq_windex = 0;
    is->frame_last_pts = 0;
    is->frame_skip_last_pts = 0;
    is->pictq_size = 0;

    end_read_frame = false;
    skip_level = 0;
    skip_frame_count = 0;

    for(int i = 0; i < VIDEO_PICTURE_QUEUE_SIZE; i++){
        is->pictq[i].isEnd = false;
    }

    is->ic = ic;

    return 0;
}

int startMovie()
{
    int video_index, audio_index;

    int i;
    for (i = 0; i < is->ic->nb_streams; i++) {
        AVCodecContext *enc = is->ic->streams[i]->codec;

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
        stream_component_open(is, video_index, gNativeWindow);
    }

    refreshTimeQueue = createLinkedQueue();

    pthread_create(&refresh_tid, NULL, refresh_thread, NULL);

    pthread_create(&parse_tid, NULL, decode_thread, gNativeWindow);

    return 0;
}

void schedule_refresh(int delay)
{
    QueueNode node;
    node.data = delay;

    pthread_mutex_lock(&refresh_queue_mutex);

    enqueueLQ(refreshTimeQueue, node);

    pthread_mutex_unlock(&refresh_queue_mutex);
}


double compute_frame_delay(double frame_current_pts, VideoState *is)
{
    double actual_delay, delay, sync_threshold, ref_clock, diff;

    /* compute nominal delay */
    delay = frame_current_pts - is->frame_last_pts;
    //if (delay <= 0 || delay >= 10.0) {
        /* if incorrect delay, use previous one */

    if (delay <= 0) {
        delay = 0;
    } else {
        is->frame_last_delay = delay;
    }
    is->frame_last_pts = frame_current_pts;

    /* update delay to follow master synchronisation source */

    /* if video is slave, we try to correct big delays by
       duplicating or deleting a frame */
    ref_clock = get_master_clock(is);
    diff = frame_current_pts - ref_clock;

    /* skip or repeat frame. We take into account the
       delay to compute the threshold. I still don't know
       if it is the best guess */
    sync_threshold = delay;
    if (diff <= -sync_threshold) {
        if(delay != 0){
            delay = 0.0f;
            __android_log_print(ANDROID_LOG_DEBUG, "compute_frame_delay", "diff <= -sync_threshold");
        }
    }
    else if (diff >= sync_threshold) {
        delay *= 1.5f;
        __android_log_print(ANDROID_LOG_DEBUG, "compute_frame_delay", "diff >= sync_threshold");
    }
    else{
        __android_log_print(ANDROID_LOG_DEBUG, "compute_frame_delay", "diff : -sync_threshold");
    }

    is->frame_timer += delay;
    /* compute the REAL delay (we need to do that to avoid
       long term errors */
    actual_delay = is->frame_timer - (av_gettime() / 1000000.0);

    double store_actual_delay = actual_delay;
    __android_log_print(ANDROID_LOG_VERBOSE, "compute_frame_delay", "actual_delay: %f", store_actual_delay);

    if (actual_delay < 0.010) {
        /* XXX: should skip picture */
        actual_delay = 0.010;
    }else{
        actual_delay = actual_delay;
    }

    return actual_delay;
}

bool test_skip_frame(double frame_current_pts, VideoState *is)
{
    double actual_delay, delay;

    /* compute nominal delay */
    delay = frame_current_pts - is->frame_last_pts;
    actual_delay = (is->frame_timer + delay) - (av_gettime() / 1000000.0);

    //__android_log_print(ANDROID_LOG_VERBOSE, "CHK", "actual_delay: %f", actual_delay);
    if(delay <= 0){
        //__android_log_print(ANDROID_LOG_DEBUG, "test_skip_frame", "skip_frame no: pts:%f, actual_delay:%f", frame_current_pts, actual_delay);
        return false;
    }else if (actual_delay < 0.0) {
        //__android_log_print(ANDROID_LOG_DEBUG, "test_skip_frame", "skip_frame yes: pts:%f, actual_delay:%f", frame_current_pts, actual_delay);
        return true;
    }else{
       //__android_log_print(ANDROID_LOG_DEBUG, "test_skip_frame", "skip_frame no: pts:%f, actual_delay:%f", frame_current_pts, actual_delay);
        return false;
    }
}

bool test_skip_frame2(double frame_current_pts, VideoState *is)
{
    if((frame_current_pts) < get_master_clock(is)){
        __android_log_print(ANDROID_LOG_DEBUG, "skip", "skip_frame yes: pts:%f, last_pts:%f", frame_current_pts, is->frame_last_pts);
        return true;
    }else{
        __android_log_print(ANDROID_LOG_DEBUG, "skip", "skip_frame no: pts:%f, last_pts:%f", frame_current_pts, is->frame_last_pts);
        return false;
    }
}

void video_refresh_timer()
{
    __android_log_print(ANDROID_LOG_VERBOSE, "video_refresh_timer", "start");

    if(is->abort_request){
        return;
    }

    VideoPicture *vp;

    if (is->pictq_size == 0) {
        /* if no picture, need to wait */
        __android_log_print(ANDROID_LOG_VERBOSE, "video_refresh_timer", "is->pictq_size == 0");
        schedule_refresh(10);
    }else{

        vp = &is->pictq[is->pictq_rindex];

        if(vp->isEnd){
            vp->isEnd = false;

            if (++is->pictq_rindex == VIDEO_PICTURE_QUEUE_SIZE)
                is->pictq_rindex = 0;

            pthread_mutex_lock(&is->pictq_mutex);
            is->pictq_size--;
            pthread_mutex_unlock(&is->pictq_mutex);

            /*
            stream_seek_to(0);
            schedule_refresh(1);
             */

            //openMovie(gFilePath);
            refreashJniEnv->CallVoidMethod(javaObject_PlayCallback, javaMethod_movieEnd);

            __android_log_print(ANDROID_LOG_VERBOSE, "video_refresh_timer", "vp->isEnd)");

        }else {

            /* update current video pts */
            is->video_current_pts = vp->pts;
            is->video_current_pts_time = av_gettime();

            int delay = (compute_frame_delay(vp->pts, is) * 1000 + 0.5);

            __android_log_print(ANDROID_LOG_VERBOSE, "video_refresh_timer", "delay: %d", delay);
            schedule_refresh(delay);

            /* update queue size and signal for next picture */
            if (++is->pictq_rindex == VIDEO_PICTURE_QUEUE_SIZE)
                is->pictq_rindex = 0;

            pthread_mutex_lock(&is->pictq_mutex);
            is->pictq_size--;
            pthread_cond_signal(&is->pictq_cond);
            pthread_mutex_unlock(&is->pictq_mutex);

            __android_log_print(ANDROID_LOG_DEBUG, "video_refresh_timer", "pictq_size: %d", is->pictq_size);

            pthread_mutex_lock(&is->pause_mutex);

            if (!is->paused) { // background로 전환될 때 crash를 막기 위한 부분

                render(gNativeWindow);

                refreashJniEnv->CallVoidMethod(javaObject_PlayCallback, javaMethod_updateClock,
                                               get_video_clock(is));

                __android_log_print(ANDROID_LOG_DEBUG, "video_refresh_timer", "render video_pts: %f", is->video_current_pts);
            }

            pthread_mutex_unlock(&is->pause_mutex);
        }
    }

}

void* refresh_thread(void *arg)
{
    g_VM->AttachCurrentThread(&refreashJniEnv, NULL);

    for(;;){
        __android_log_print(ANDROID_LOG_VERBOSE, "refresh_thread", "before lock");
        pthread_mutex_lock(&refresh_mutex);
        __android_log_print(ANDROID_LOG_VERBOSE, "refresh_thread", "after lock");

        if(is->abort_request){
            deleteLinkedQueue(refreshTimeQueue);
            pthread_mutex_unlock(&refresh_mutex);

            g_VM->DetachCurrentThread();

            __android_log_print(ANDROID_LOG_VERBOSE, "refresh_thread", "return");
            return NULL;
        }

        if (isLinkedQueueEmpty(refreshTimeQueue) == FALSE) {
            __android_log_print(ANDROID_LOG_VERBOSE, "refresh_thread", "refresh_queue_mutex before lock");
            pthread_mutex_lock(&refresh_queue_mutex);
            __android_log_print(ANDROID_LOG_VERBOSE, "refresh_thread", "refresh_queue_mutex after lock");
            QueueNode* pNode = dequeueLQ(refreshTimeQueue);
            pthread_mutex_unlock(&refresh_queue_mutex);

            __android_log_print(ANDROID_LOG_VERBOSE, "refresh_thread", "usleep: %d", pNode->data * 1000);
            usleep(pNode->data * 1000);

            __android_log_print(ANDROID_LOG_VERBOSE, "refresh_thread", "before video_refresh_timer");
            video_refresh_timer();
            __android_log_print(ANDROID_LOG_VERBOSE, "refresh_thread", "after video_refresh_timer");


        }else{
            usleep(1);
            __android_log_print(ANDROID_LOG_VERBOSE, "refresh_thread", "usleep");
        }

        pthread_mutex_unlock(&refresh_mutex);

    }
    g_VM->DetachCurrentThread();

    return NULL;
}

int frameTest = 10;

int queue_picture(AVFrame *src_frame, double pts) {

    pthread_mutex_lock(&is->pictq_mutex);
    while (is->pictq_size >= VIDEO_PICTURE_QUEUE_SIZE &&
           !is->videoq.abort_request) {
        __android_log_print(ANDROID_LOG_VERBOSE, "queue_picture", "queue_picture wait start");
        pthread_cond_wait(&is->pictq_cond, &is->pictq_mutex);
    }
    pthread_mutex_unlock(&is->pictq_mutex);

    __android_log_print(ANDROID_LOG_VERBOSE, "queue_picture", "queue_picture wait end");

    if (is->videoq.abort_request)
        return -1;

    VideoPicture *vp;
    vp = &is->pictq[is->pictq_windex];
    vp->pts = pts;

    frameTest--;
    if(frameTest != 0){
        //return 0;
    }else{
        frameTest = 10;
    }

    gImgConvertCtx = sws_getCachedContext(gImgConvertCtx,
                                          is->video_st->codec->width,
                                          is->video_st->codec->height,
                                          is->video_st->codec->pix_fmt,
                                          is->video_st->codec->width,
                                          is->video_st->codec->height, AV_PIX_FMT_RGBA,
                                          SWS_BICUBIC, NULL, NULL, NULL);

    sws_scale(gImgConvertCtx, src_frame->data, src_frame->linesize, 0,
              is->video_st->codec->height, gFrameRGB->data, gFrameRGB->linesize);

    /*
    pthread_mutex_lock(&is->pause_mutex);

    if (!is->paused) { // background로 전환될 때 crash를 막기 위한 부분
        render(is->nativeWindow);
    }

    pthread_mutex_unlock(&is->pause_mutex);
     */

    if (++is->pictq_windex == VIDEO_PICTURE_QUEUE_SIZE)
        is->pictq_windex = 0;

    pthread_mutex_lock(&is->pictq_mutex);

    is->pictq_size++;

    pthread_mutex_unlock(&is->pictq_mutex);

    __android_log_print(ANDROID_LOG_DEBUG, "queue_picture", "pts: %f", pts);

    if ((is->ready == 0) && (is->pictq_size >= VIDEO_PICTURE_QUEUE_SIZE)) {
        is->ready = 1;
        schedule_refresh(1);
        bqPlayerCallback(bqPlayerBufferQueue, NULL);
    }

    return 0;
}

bool waitAfterSeek =false;

void* video_thread(void *arg)
{
    g_VM->AttachCurrentThread(&videoJniEnv, NULL);

    AVPacket pkt1, *pkt = &pkt1;
    int got_picture = 0;

    //schedule_refresh(1);

    AVFrame *frame= av_frame_alloc();

    for(;;) {
        __android_log_print(ANDROID_LOG_DEBUG, "video_thread", "start");

        __android_log_print(ANDROID_LOG_DEBUG, "video_thread", "video_queue_mutex before lock");
        //pthread_mutex_lock(&video_queue_mutex);
        __android_log_print(ANDROID_LOG_DEBUG, "video_thread", "video_queue_mutex after lock");

        if(is->abort_request){
            __android_log_print(ANDROID_LOG_DEBUG, "video_thread", "is->abort_request");
            //pthread_mutex_unlock(&video_queue_mutex);
            break;
        }

        if(is->paused ) {
            __android_log_print(ANDROID_LOG_DEBUG, "video_thread", "is->paused");
            usleep(1);
            //pthread_mutex_unlock(&video_queue_mutex);
            continue;
        }

        pthread_mutex_lock(&video_queue_mutex);

        __android_log_print(ANDROID_LOG_DEBUG, "video_thread", "packet_queue_get before");
        if (packet_queue_get(&is->videoq, pkt, 1) < 0){

            pthread_mutex_unlock(&video_queue_mutex);
            __android_log_print(ANDROID_LOG_DEBUG, "video_thread", "packet_queue_get error");
            break;
        }else{
            __android_log_print(ANDROID_LOG_DEBUG, "video_thread", "packet_queue_get video_stream - pts: %f, count: %d", get_video_pts(pkt->dts, is), is->videoq.nb_packets);
        }
        __android_log_print(ANDROID_LOG_DEBUG, "video_thread", "packet_queue_get after");

        double pts;
        if(pkt->dts != AV_NOPTS_VALUE)
            pts= pkt->dts;
        else
            pts= 0;
        pts *= av_q2d(is->video_st->time_base);

        if(pkt->data == end_pkt.data){

            __android_log_print(ANDROID_LOG_DEBUG, "video_thread", "end_pkt");

            VideoPicture *vp;
            vp = &is->pictq[is->pictq_windex];
            vp->isEnd = true;

            if (++is->pictq_windex == VIDEO_PICTURE_QUEUE_SIZE)
                is->pictq_windex = 0;

            pthread_mutex_lock(&is->pictq_mutex);
            is->pictq_size++;
            pthread_mutex_unlock(&is->pictq_mutex);

            pthread_mutex_unlock(&video_queue_mutex);
            continue;
        }

        if(waitAfterSeek){
            waitAfterSeek = false;

            // seek 후 처리될 부분들
            is->frame_last_pts = pts;
            is->frame_skip_last_pts = pts;
            is->frame_timer = av_gettime() / 1000000.0;
            is->frame_skip_timer = av_gettime() / 1000000.0;

            __android_log_print(ANDROID_LOG_DEBUG, "video_thread", "after seek : pts: %f", pts);
        }

        if(pkt->data == flush_pkt.data){

            __android_log_print(ANDROID_LOG_DEBUG, "video_thread", "flush_pkt");
            avcodec_flush_buffers(is->video_st->codec);

            waitAfterSeek = true;

            pthread_mutex_unlock(&video_queue_mutex);
            continue;
        }

        int ret = 0;

        //avcodec_decode_video2(is->video_st->codec, frame, &got_picture, pkt);
        avcodec_send_packet(is->video_st->codec, pkt);
        ret = avcodec_receive_frame(is->video_st->codec, frame);

        if(test_skip_frame(pts, is)){
            skip_frame_count++;
            pthread_mutex_unlock(&video_queue_mutex);
            av_packet_unref(pkt);

            __android_log_print(ANDROID_LOG_DEBUG, "skip_frame", "frame skip");

            if(skip_level < 2){
                if(skip_frame_count > 90){
                    skip_frame_count = 0;
                    skip_level++;

                    __android_log_print(ANDROID_LOG_DEBUG, "skip_frame", "skip_frame : %d", videoCodecContext->skip_frame);

                    if(skip_level == 1){
                        videoCodecContext->skip_frame = AVDISCARD_BIDIR;
                        __android_log_print(ANDROID_LOG_DEBUG, "skip_frame", "skip_frame = AVDISCARD_BIDIR");
                    }else if(skip_level == 2){
                        videoCodecContext->skip_frame = AVDISCARD_NONKEY;
                        __android_log_print(ANDROID_LOG_DEBUG, "skip_frame", "skip_frame = AVDISCARD_NONKEY");
                    }
                }
            }

            if(skip_frame_count > 10){
                continue;
            }
        }else{
            skip_frame_count = 0;
        }

        if(ret == AVERROR(EINVAL)){
            pthread_mutex_unlock(&video_queue_mutex);
            av_packet_unref(pkt);
            continue;
        }

        if(ret >= 0){
            got_picture = 1;
        }

        if (got_picture) {
            queue_picture(frame, pts);
            __android_log_print(ANDROID_LOG_DEBUG, "video_thread", "got_picture");
            av_packet_unref(pkt);
        }

        pthread_mutex_unlock(&video_queue_mutex);
    }

    av_free(frame);

    g_VM->DetachCurrentThread();
    return 0;
}

int stream_component_open(VideoState *is, int stream_index, ANativeWindow* nativeWindow)
{
    AVFormatContext *ic = is->ic;
    AVCodecContext *enc;
    AVCodec *codec;

    enc = ic->streams[stream_index]->codec;
    codec = avcodec_find_decoder(enc->codec_id);
    if (avcodec_open2(enc, codec, NULL) < 0)
    //if (avcodec_open2(NULL, NULL, NULL) < 0)
        return -1;

    switch(enc->codec_type) {
        case AVMEDIA_TYPE_VIDEO:
            videoCodecContext = enc;

            is->frame_last_delay = 40e-3;
            is->frame_timer = (double)av_gettime() / 1000000.0;
            is->frame_skip_timer = (double)av_gettime() / 1000000.0;
            is->video_current_pts_time = av_gettime();

            is->video_stream = stream_index;
            is->video_st = ic->streams[stream_index];

            packet_queue_init(&is->videoq);

            gFrameRGB = av_frame_alloc();
            if (gFrameRGB == NULL)
                return -8;

            // video init
            gPictureSize = avpicture_get_size(AV_PIX_FMT_RGBA, enc->width, enc->height);
            gVideoBuffer = (uint8_t*)(malloc(sizeof(uint8_t) * gPictureSize));
            avpicture_fill((AVPicture*)gFrameRGB, gVideoBuffer, AV_PIX_FMT_RGBA, enc->width, enc->height);

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

            audioFrame = av_frame_alloc();

            createBufferQueueAudioPlayer(rate, channel, SL_PCMSAMPLEFORMAT_FIXED_16);
            break;
    }
    return 0;
}



void* decode_thread(void* arge)
{
    g_VM->AttachCurrentThread(&decodeJniEnv, NULL);

    ANativeWindow* nativeWindow = gNativeWindow;
    int frameFinished = 0;
    AVPacket packet;

    static AVFrame audioFrame;
    ANativeWindow_Buffer windowBuffer;

    for(;;){

        //__android_log_print(ANDROID_LOG_DEBUG, "decode_thread", "start");

        if (is->abort_request) {
            __android_log_print(ANDROID_LOG_DEBUG, "decode_thread", "is->abort_request");
            break;
        }

        if (is->paused){
            __android_log_print(ANDROID_LOG_DEBUG, "decode_thread", "is->paused");
            continue;
        }

        if(autoRepeatState == auto_repeat_on_wait){

            if((get_master_clock(is) - getAutoRepeatEndPts()) > 0){
                autoRepeatState = auto_repeat_on_working;
                double gap =  getAutoRepeatEndPts() - getAutoRepeatStartPts();
                __android_log_print(ANDROID_LOG_VERBOSE, "auto_repeat", "autoRepeatState == auto_repeat_on_working");
                __android_log_print(ANDROID_LOG_VERBOSE, "auto_repeat", "getAutoRepeatEndPts: %f", getAutoRepeatEndPts());
                __android_log_print(ANDROID_LOG_VERBOSE, "auto_repeat", "getAutoRepeatStartPts: %f", getAutoRepeatStartPts());
                stream_seek(-gap);
            }else{

            }
        }else if(autoRepeatState == auto_repeat_on_working){
            if((getAutoRepeatEndPts() - get_master_clock(is)) > 0){
                autoRepeatState = auto_repeat_on_wait;
                __android_log_print(ANDROID_LOG_VERBOSE, "auto_repeat", "autoRepeatState == auto_repeat_on_wait");
                __android_log_print(ANDROID_LOG_VERBOSE, "auto_repeat", "getAutoRepeatEndPts: %f", getAutoRepeatEndPts());
                __android_log_print(ANDROID_LOG_VERBOSE, "auto_repeat", "get_master_clock: %f", get_master_clock(is));
            }
        }

        if(is->seek_req) {

            pthread_mutex_lock(&video_queue_mutex);
            pthread_mutex_lock(&refresh_mutex);
            __android_log_print(ANDROID_LOG_DEBUG, "decode_thread", "if(is->seek_req) start");

            clearLQ(refreshTimeQueue);

            pthread_mutex_lock(&is->pictq_mutex);
            __android_log_print(ANDROID_LOG_DEBUG, "decode_thread", "lock pictq_mutex");
            is->pictq_rindex = 0;
            is->pictq_windex = 0;
            is->pictq_size = 0;
            pthread_mutex_unlock(&is->pictq_mutex);

            int stream_index= -1;
            int64_t seek_target = is->seek_pos;

            stream_index = is->video_stream;

            if(stream_index>=0){
                seek_target= av_rescale_q(seek_target, AV_TIME_BASE_Q,
                                          is->ic->streams[stream_index]->time_base);
            }
            /*
            if(av_seek_frame(is->ic, stream_index,
                             seek_target, is->seek_flags) < 0) {
                fprintf(stderr, "%s: error while seeking\n",
                        is->ic->filename);
            }
             */

            if(avformat_seek_file(is->ic, stream_index,
                                  0, seek_target, seek_target, is->seek_flags) < 0)
            {
                fprintf(stderr, "%s: error while seeking\n",
                        is->ic->filename);
            }

            if (is->audio_stream >= 0) {
                packet_queue_flush(&is->audioq);
                packet_queue_put(&is->audioq, &flush_pkt);
            }
            if (is->video_stream >= 0) {
                packet_queue_flush(&is->videoq);
                packet_queue_put(&is->videoq, &flush_pkt);
            }

            is->seek_req = 0;

            __android_log_print(ANDROID_LOG_DEBUG, "put", "seek end");
            __android_log_print(ANDROID_LOG_DEBUG, "get", "seek end");

            __android_log_print(ANDROID_LOG_DEBUG, "decode_thread", "if(is->seek_req) end");

            pthread_mutex_unlock(&refresh_mutex);
            pthread_mutex_unlock(&video_queue_mutex);

            skip_level = 0;
            skip_frame_count = 0;
            videoCodecContext->skip_frame = AVDISCARD_DEFAULT;

            schedule_refresh(1);

            decodeJniEnv->CallVoidMethod(javaObject_PlayCallback, javaMethod_seekEnd);

            end_read_frame = false;
        }

        if(!end_read_frame)
        {
              if(is->videoq.nb_packets > 2000){
                  continue;
              }

            int ret =  av_read_frame(is->ic, &packet);

            if(ret >= 0 ){
                if (packet.stream_index == is->video_stream) {

                    packet_queue_put(&is->videoq, &packet);
                    __android_log_print(ANDROID_LOG_DEBUG, "decode_thread", "packet_queue_put video_stream - pts: %f, count: %d", get_video_pts(packet.dts, is), is->videoq.nb_packets);

                }else if(packet.stream_index == is->audio_stream){
                    packet_queue_put(&is->audioq, &packet);
                    __android_log_print(ANDROID_LOG_DEBUG, "decode_thread", "packet_queue_put audio_stream - pts: %f, count: %d", get_audio_pts(packet.dts, is), is->audioq.nb_packets);

                }else {
                    av_packet_unref(&packet);
                    __android_log_print(ANDROID_LOG_DEBUG, "decode_thread", "av_read_frame error ");
                }
            }else{
                if(ret == AVERROR_EOF){
                    if (packet.stream_index == is->video_stream) {
                        end_read_frame = true;
                        packet_queue_put(&is->videoq, &end_pkt);
                        __android_log_print(ANDROID_LOG_DEBUG, "decode_thread", "av_read_frame AVERROR_EOF ");
                    }
                }
            }
        }

    }

    g_VM->DetachCurrentThread();

    return NULL;
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

void closePlayer()
{
    do_exit();

    if (gVideoBuffer != NULL) {
        // char *org_name = (char *)av_mallocz(sizeof(char));
        //av_free(org_name);
        //char *org_name2 = (char *)av_mallocz(sizeof(char)*25);
        // av_free(org_name2);

        //uint8_t *gVideoBuffer2 = (uint8_t*)(malloc(sizeof(uint8_t) * 5));
        //free(gVideoBuffer2);
        free(gVideoBuffer);
        gVideoBuffer = NULL;
    }

    if (gFrameRGB != NULL)
        av_free(gFrameRGB);

    return;
}

AVPacket audioPacket;

void bqPlayerCallback(SLAndroidSimpleBufferQueueItf bq, void *context){

    __android_log_print(ANDROID_LOG_VERBOSE, "bqPlayerCallback", "bqPlayerCallback: %d", is->audioq.nb_packets);

    for (;;)
    {
        if (is->audioq.abort_request) {
            return;
        }
        if (packet_queue_get(&is->audioq, &audioPacket, 1) < 0){
            return;
        }

        __android_log_print(ANDROID_LOG_DEBUG, "pts", "packet_queue_get audio_stream - pts: %f, count: %d", get_audio_pts(audioPacket.pts, is), is->audioq.nb_packets);

        AVCodecContext *dec = is->audio_st->codec;

        if(audioPacket.data == flush_pkt.data){
            avcodec_flush_buffers(dec);
            continue;
        }

        /* if update the audio clock with the pts */
        if (audioPacket.pts != AV_NOPTS_VALUE) {
            is->audio_clock = av_q2d(is->audio_st->time_base)*audioPacket.pts;
        }

        //__android_log_print(ANDROID_LOG_DEBUG, "skip", "skip audio pts: %f, video pts: %f", is->audio_clock, get_video_clock(is));

        if(is->audio_clock < (get_video_clock(is) - 0.5f)){

            __android_log_print(ANDROID_LOG_DEBUG, "bqPlayerCallback", "skip audio pts: %f, video last pts: %f", is->audio_clock, is->frame_last_pts);
            continue;
        }

        int frameFinished = 0;

        //avcodec_decode_audio4(dec, audioFrame, &frameFinished, &audioPacket);
        avcodec_send_packet(is->audio_st->codec, &audioPacket);
        int ret = avcodec_receive_frame(is->audio_st->codec, audioFrame);

        if(ret >= 0){
            frameFinished = 1;
        }

        if (frameFinished) {

            audio_data_size = av_samples_get_buffer_size(
                    audioFrame->linesize, dec->channels,
                    audioFrame->nb_samples, AV_SAMPLE_FMT_S16, 1);

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

                av_packet_unref(&audioPacket);
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

    if (is->paused) {
        delta = 0;
    } else {
        delta = (av_gettime() - is->video_current_pts_time) / 1000000.0;
    }
    return is->video_current_pts + delta;
}

double get_audio_clock(VideoState *is)
{
    double pts;
    int hw_buf_size, bytes_per_sec;
    pts = is->audio_clock;

    /*
    hw_buf_size = audio_write_get_buf_size(is);
    bytes_per_sec = 0;
    if (is->audio_st) {
        bytes_per_sec = is->audio_st->codec->sample_rate *
                        2 * is->audio_st->codec->channels;
    }
    if (bytes_per_sec)
        pts -= (double)hw_buf_size / bytes_per_sec;
        */

    return pts;
}

double get_master_clock(VideoState *is) {
    return get_audio_clock(is);
}

void stream_seek(double rel) {
    double pos = get_master_clock(is);
    pos += rel;
    pos = (int64_t)(pos * AV_TIME_BASE);

    is->seek_flags |= AVSEEK_FLAG_ANY;

    if(!is->seek_req) {
        is->seek_pos = pos;

        is->seek_flags &= ~AVSEEK_FLAG_BACKWARD;
        if(rel < 0){
            is->seek_flags |= AVSEEK_FLAG_BACKWARD;
        }
        is->seek_req = 1;
    }
}

void stream_seek_to(double seekPos) {
    double coefficient = 100.0f;
    double coefficient2 = (is->ic->duration / coefficient);
    seekPos = seekPos * coefficient2;

    is->seek_flags |= AVSEEK_FLAG_ANY;

    is->seek_flags &= ~AVSEEK_FLAG_BACKWARD;

    if(!is->seek_req) {
        is->seek_pos = seekPos;
        is->seek_req = 1;
    }
}

void clearMovie(void)
{
    is->abort_request = 1;

    pthread_join(refresh_tid, NULL);

    /* close each stream */
    if (is->audio_stream >= 0){
        stream_component_close(is, is->audio_stream);
    }

    if (is->video_stream >= 0){
        stream_component_close(is, is->video_stream);
    }

    if (is->ic) {
        avformat_close_input(&is->ic);
        is->ic = NULL; /* safety */
    }

    (*bqPlayerBufferQueue)->Clear(bqPlayerBufferQueue);

    if (is) {
        stream_close(is);
        is = NULL;
    }
}

void do_exit(void)
{
    is->abort_request = 1;

    pthread_join(refresh_tid, NULL);

    /* close each stream */
    if (is->audio_stream >= 0){
        stream_component_close(is, is->audio_stream);
    }

    if (is->video_stream >= 0){
        stream_component_close(is, is->video_stream);
    }

    closeAudio();

    if (is->ic) {
        avformat_close_input(&is->ic);
        is->ic = NULL; /* safety */
    }

    free(audioOutputBuffer);
    //av_free(audioOutputBuffer);

    av_free(audioFrame);

    if (is) {
        stream_close(is);
        is = NULL;
    }

    exit(0);
}

void stream_close(VideoState *is)
{
    int i;
    /* XXX: use a special url_shutdown call to abort parse cleanly */



    pthread_join(parse_tid, NULL);

    pthread_mutex_destroy(&is->pictq_mutex);
    pthread_cond_destroy(&is->pictq_cond);

    pthread_mutex_destroy(&is->pause_mutex);

    av_free(is);
}

void stream_component_close(VideoState *is, int stream_index)
{
    AVFormatContext *ic = is->ic;
    AVCodecContext *enc;

    if (stream_index < 0 || stream_index >= ic->nb_streams)
        return;
    enc = ic->streams[stream_index]->codec;

    switch(enc->codec_type) {
        case AVMEDIA_TYPE_AUDIO:
            packet_queue_abort(&is->audioq);

            packet_queue_end(&is->audioq);
            break;
        case AVMEDIA_TYPE_VIDEO:
            packet_queue_abort(&is->videoq);

            /* note: we also signal this mutex to make sure we deblock the
               video thread in all cases */
            pthread_mutex_lock(&is->pictq_mutex);
            pthread_cond_signal(&is->pictq_cond);
            pthread_mutex_unlock(&is->pictq_mutex);

            pthread_join(is->video_tid, NULL);

            packet_queue_end(&is->videoq);
            break;
        default:
            break;
    }

    ic->streams[stream_index]->discard = AVDISCARD_ALL;
    avcodec_close(enc);
    switch(enc->codec_type) {
        case AVMEDIA_TYPE_AUDIO:
            is->audio_st = NULL;
            is->audio_stream = -1;
            break;
        case AVMEDIA_TYPE_VIDEO:
            is->video_st = NULL;
            is->video_stream = -1;
            break;
        default:
            break;
    }
}

void closeAudio(){
    // destroy buffer queue audio player object, and invalidate all associated interfaces
    if (bqPlayerObject != NULL) {
        (*bqPlayerObject)->Destroy(bqPlayerObject);
        bqPlayerObject = NULL;
        bqPlayerPlay = NULL;
        bqPlayerBufferQueue = NULL;
        bqPlayerEffectSend = NULL;
        bqPlayerMuteSolo = NULL;
        bqPlayerVolume = NULL;
    }

    // destroy output mix object, and invalidate all associated interfaces
    if (outputMixObject != NULL) {
        (*outputMixObject)->Destroy(outputMixObject);
        outputMixObject = NULL;
        outputMixEnvironmentalReverb = NULL;
    }

    // destroy engine object, and invalidate all associated interfaces
    if (engineObject != NULL) {
        (*engineObject)->Destroy(engineObject);
        engineObject = NULL;
        engineEngine = NULL;
    }
}

void changeAutoRepeatState(int state){
    autoRepeatState = state;

    if(autoRepeatState == auto_repeat_off){
        autoRepeatStartPts = 0;
        autoRepeatEndPts = 10;
    }else if(autoRepeatState == auto_repeat_select_A){
        autoRepeatStartPts = get_master_clock(is);
    }else if(autoRepeatState == auto_repeat_on){
        autoRepeatState = auto_repeat_on_wait;
        autoRepeatEndPts = get_master_clock(is);
    }
}

double getAutoRepeatStartPts(){
    return autoRepeatStartPts;
}

double getAutoRepeatEndPts(){
    return autoRepeatEndPts;
}

/* pause or resume the video */
void stream_pause(VideoState *is)
{
    if(is == NULL){
        return;
    }

    if(bqPlayerPlay == NULL){
        return;
    }

    pthread_mutex_lock(&is->pause_mutex);

    is->paused = !is->paused;

    if (!is->paused) {
        is->video_current_pts = get_video_clock(is);
        is->frame_timer += (av_gettime() - is->video_current_pts_time) / 1000000.0;
        is->frame_skip_timer += (av_gettime() - is->video_current_pts_time) / 1000000.0;

        (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_PLAYING);
    }else{
        (*bqPlayerPlay)->SetPlayState(bqPlayerPlay, SL_PLAYSTATE_PAUSED);
    }

    pthread_mutex_unlock(&is->pause_mutex);
}