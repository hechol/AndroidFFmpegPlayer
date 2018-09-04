#include <jni.h>
#include <string>

extern "C" {
#include "BasicPlayer.h"
}

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
    return 0;
}

extern "C" JNIEXPORT jint JNICALL Java_com_example_maner_dvideoplayer_PlayVideo_openMovie(JNIEnv *env, jobject thiz, jstring filePath, jobject surface)
{
    const char *str;
    int result;

    str = env->GetStringUTFChars(filePath, NULL);

    ANativeWindow* nativeWindow = ANativeWindow_fromSurface(env, surface);
    result = openMovie(nativeWindow, str);

    env->ReleaseStringUTFChars(filePath, str);

    return result;
}

extern "C" JNIEXPORT jint JNICALL Java_com_example_maner_dvideoplayer_PlayVideo_renderFrame(JNIEnv *env, jobject thiz, jobject bitmap)
{
    void *pixels;
    int result = 0;

    if ((result = AndroidBitmap_lockPixels(env, bitmap, &pixels)) < 0)
        return result;

    //decodeFrame();
    copyPixels((uint8_t*)pixels);

    AndroidBitmap_unlockPixels(env, bitmap);

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

extern "C" JNIEXPORT void JNICALL Java_com_example_maner_dvideoplayer_PlayVideo_closeMovie(JNIEnv *env, jobject thiz)
{
    closeMovie();
}

extern "C" JNIEXPORT void JNICALL Java_com_example_maner_dvideoplayer_PlayVideo_StreamSeek(JNIEnv *env, jobject thiz, jdouble incr)
{
    stream_seek( incr);
}