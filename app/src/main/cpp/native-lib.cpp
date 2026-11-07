#include <jni.h>
#include <GLES3/gl3.h>
#include <android/log.h>

#define LOG_TAG "MCLG-Android"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

extern "C" JNIEXPORT void JNICALL
Java_dev_mc_legacy_MainActivity_nativeInit(JNIEnv*, jobject) {
    glDisable(GL_DITHER);
    glClearColor(0.025f, 0.035f, 0.055f, 1.0f);
    LOGI("Native GLES3 platform bootstrap initialized");
}

extern "C" JNIEXPORT void JNICALL
Java_dev_mc_legacy_MainActivity_nativeResize(JNIEnv*, jobject, jint width, jint height) {
    glViewport(0, 0, width, height);
}

extern "C" JNIEXPORT void JNICALL
Java_dev_mc_legacy_MainActivity_nativeRender(JNIEnv*, jobject) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}
