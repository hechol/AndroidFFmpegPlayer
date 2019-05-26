
#include <string>

#include "BasicPlayer.h"

extern VideoState *is;
extern JavaVM *g_VM;
extern JNIEnv *g_env;

extern jobject javaObject_PlayCallback;
extern jclass javaClass_PlayCallback;
extern jmethodID javaMethod_updateClock;
extern jmethodID javaMethod_seekEnd;

void makeGlobalRef(JNIEnv* pEnv, jobject* pRef) {
    if (*pRef != NULL) {
        jobject lGlobalRef = pEnv->NewGlobalRef(*pRef);
        // No need for a local reference any more.
        pEnv->DeleteLocalRef(*pRef);
        // Here, lGlobalRef may be null.
        *pRef = lGlobalRef;
    }
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

extern "C" JNIEXPORT jint JNICALL Java_com_example_maner_dvideoplayer_PlayVideo_initBasicPlayer(JNIEnv *env, jobject thiz, jobject playerCall)
{
    /*
    if (android_getCpuFamily() == ANDROID_CPU_FAMILY_ARM && (android_getCpuFeatures() & ANDROID_CPU_ARM_FEATURE_NEON) != 0) {
        av_register_all();

        return 0;
    }
    else
        return -1;
        */

    javaObject_PlayCallback = playerCall;
    makeGlobalRef(env, &javaObject_PlayCallback);

    javaClass_PlayCallback = env->FindClass("com/example/maner/dvideoplayer/PlayerCallback");
    makeGlobalRef(env, (jobject*)&javaClass_PlayCallback);

    javaMethod_updateClock = env->GetMethodID(javaClass_PlayCallback, "updateClock", "(D)V");
    javaMethod_seekEnd = env->GetMethodID(javaClass_PlayCallback, "seekEnd", "()V");

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

extern "C" JNIEXPORT jlong JNICALL Java_com_example_maner_dvideoplayer_PlayVideo_getDuration(JNIEnv *env, jobject thiz){
    return is->ic->duration;
}

extern "C" JNIEXPORT void JNICALL Java_com_example_maner_dvideoplayer_PlayVideo_clickPause(JNIEnv *env, jobject thiz){
    stream_pause(is);
}

extern "C" JNIEXPORT void JNICALL Java_com_example_maner_dvideoplayer_PlayVideo_update(JNIEnv *env, jobject thiz){

}