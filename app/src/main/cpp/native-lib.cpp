
#include <string>

#include "BasicPlayer.h"

extern VideoState *is;
extern JavaVM *g_VM;
extern JNIEnv *g_env;

extern "C" JNIEXPORT jstring JNICALL Java_com_example_maner_dvideoplayer_PlayVideo_stringFromJNI(
        JNIEnv *env,
        jobject /* this */) {
    std::string hello = "Hello from C++";

    //ANativeWindow_Buffer buffer;
   // ANativeWindow * window;

    //ANativeWindow_setBuffersGeometry(NULL, 0, 0, 0);

    return env->NewStringUTF(hello.c_str());
}

extern "C" JNIEXPORT jint JNICALL Java_com_example_maner_dvideoplayer_PlayVideo_initBasicPlayer(JNIEnv *env, jobject thiz)
{
    /*
    if (android_getCpuFamily() == ANDROID_CPU_FAMILY_ARM && (android_getCpuFeatures() & ANDROID_CPU_ARM_FEATURE_NEON) != 0) {
        av_register_all();

        return 0;
    }
    else
        return -1;
        */

    av_register_all();
    createEngine();

    JNIEnv *g_env = env;
    env->GetJavaVM(&g_VM);
    return 0;
}

extern "C" JNIEXPORT void JNICALL Java_com_example_maner_dvideoplayer_PlayVideo_setWindow(JNIEnv *env, jobject thiz, jobject surface)
{
    ANativeWindow* nativeWindow = ANativeWindow_fromSurface(env, surface);
    setWindow(nativeWindow);
}

extern "C" JNIEXPORT jint JNICALL Java_com_example_maner_dvideoplayer_PlayVideo_openMovie(JNIEnv *env, jobject thiz, jstring filePath)
{
    const char *str;
    int result;

    str = env->GetStringUTFChars(filePath, NULL);

    result = openMovie(str);

    env->ReleaseStringUTFChars(filePath, str);

    return result;
}

extern "C" JNIEXPORT jint JNICALL Java_com_example_maner_dvideoplayer_PlayVideo_getMovieWidth(JNIEnv *env, jobject thiz)
{
    return getWidth();
}

extern "C" JNIEXPORT jint JNICALL Java_com_example_maner_dvideoplayer_PlayVideo_getMovieHeight(JNIEnv *env, jobject thiz)
{
    return getHeight();
}

extern "C" JNIEXPORT void JNICALL Java_com_example_maner_dvideoplayer_PlayVideo_close(JNIEnv *env, jobject thiz)
{
    closePlayer();
}

extern "C" JNIEXPORT void JNICALL Java_com_example_maner_dvideoplayer_PlayVideo_streamSeek(JNIEnv *env, jobject thiz, jdouble incr)
{
    stream_seek(incr);
}

extern "C" JNIEXPORT void JNICALL Java_com_example_maner_dvideoplayer_PlayVideo_seekTo(JNIEnv *env, jobject thiz, jdouble seekPos)
{
    stream_seek_to(seekPos);
}

extern "C" JNIEXPORT void JNICALL Java_com_example_maner_dvideoplayer_PlayVideo_changeAutoRepeatState(JNIEnv *env, jobject thiz, jint state){
    changeAutoRepeatState(state);
    return;
}

extern "C" JNIEXPORT jdouble JNICALL Java_com_example_maner_dvideoplayer_PlayVideo_getAutoRepeatStartPosition(JNIEnv *env, jobject thiz){
    return getAutoRepeatStartPts();
}

extern "C" JNIEXPORT jdouble JNICALL Java_com_example_maner_dvideoplayer_PlayVideo_getCurrentPosition(JNIEnv *env, jobject thiz){
    return get_master_clock(is);
}

extern "C" JNIEXPORT void JNICALL Java_com_example_maner_dvideoplayer_PlayVideo_clickPause(JNIEnv *env, jobject thiz){
    stream_pause(is);
}