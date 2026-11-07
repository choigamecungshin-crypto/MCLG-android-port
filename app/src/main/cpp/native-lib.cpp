#include <jni.h>
#include <GLES3/gl3.h>
#include <android/log.h>

#define LOG_TAG "MCLG-Android"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// All renderer callbacks are invoked by GLSurfaceView on its GL thread.
// This flag is therefore only read and written on that thread.
static bool g_glesReady = false;

extern "C" JNIEXPORT jboolean JNICALL
Java_dev_mc_legacy_MainActivity_nativeInit(JNIEnv*, jobject) {
    g_glesReady = false;

    // JNI methods must never issue GL commands before EGL has made a context
    // current on this thread (including after an EGL context recreation).
    const GLubyte* version = glGetString(GL_VERSION);
    const GLubyte* renderer = glGetString(GL_RENDERER);
    if (version == nullptr || renderer == nullptr) {
        LOGE("nativeInit called without a current OpenGL ES context");
        return JNI_FALSE;
    }

    GLint majorVersion = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &majorVersion);
    const GLenum queryError = glGetError();

    if (queryError != GL_NO_ERROR || majorVersion < 3) {
        LOGE(
            "OpenGL ES 3 required; reported major version=%d, error=0x%x, version=%s",
            majorVersion,
            queryError,
            reinterpret_cast<const char*>(version)
        );
        return JNI_FALSE;
    }

    glDisable(GL_DITHER);
    glClearColor(0.025f, 0.035f, 0.055f, 1.0f);
    glClearDepthf(1.0f);

    const GLenum initError = glGetError();
    if (initError != GL_NO_ERROR) {
        LOGE("GLES initialization failed with error 0x%x", initError);
        return JNI_FALSE;
    }

    g_glesReady = true;
    LOGI(
        "Native GLES3 renderer initialized: %s (%s)",
        reinterpret_cast<const char*>(renderer),
        reinterpret_cast<const char*>(version)
    );
    return JNI_TRUE;
}

extern "C" JNIEXPORT void JNICALL
Java_dev_mc_legacy_MainActivity_nativeResize(
    JNIEnv*,
    jobject,
    jint width,
    jint height
) {
    if (!g_glesReady || width <= 0 || height <= 0) {
        return;
    }

    glViewport(0, 0, width, height);
}

extern "C" JNIEXPORT void JNICALL
Java_dev_mc_legacy_MainActivity_nativeRender(JNIEnv*, jobject) {
    if (!g_glesReady) {
        return;
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const GLenum renderError = glGetError();
    if (renderError != GL_NO_ERROR) {
        LOGE("GLES render failed with error 0x%x; disabling native rendering", renderError);
        g_glesReady = false;
    }
}
