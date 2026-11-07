package dev.mc.legacy

import android.app.Activity
import android.opengl.GLSurfaceView
import android.os.Bundle
import android.util.Log
import android.view.WindowManager
import javax.microedition.khronos.egl.EGL10
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.egl.EGLDisplay
import javax.microedition.khronos.egl.EGLContext
import javax.microedition.khronos.opengles.GL10

class MainActivity : Activity(), GLSurfaceView.Renderer {
    private lateinit var surface: GLSurfaceView
    private var nativeReady = false

    private external fun nativeInit(): Boolean
    private external fun nativeResize(width: Int, height: Int)
    private external fun nativeRender()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        if (!nativeLibraryLoaded) {
            Log.e(TAG, "Native renderer is unavailable; showing a safe fallback surface")
        }

        surface = GLSurfaceView(this).apply {
            // Request GLES 3 and choose an ES3-capable config with fallbacks for
            // drivers that do not expose an RGBA8888 + depth16 combination.
            setEGLContextClientVersion(3)
            setEGLConfigChooser(GLES3ConfigChooser())
            preserveEGLContextOnPause = false
            setRenderer(this@MainActivity)
            renderMode = GLSurfaceView.RENDERMODE_CONTINUOUSLY
        }
        setContentView(surface)
        Log.i(TAG, "Activity created; requesting OpenGL ES 3.0")
    }

    override fun onSurfaceCreated(gl: GL10?, config: EGLConfig?) {
        nativeReady = false
        Log.i(TAG, "OpenGL renderer: ${gl?.glGetString(GL10.GL_RENDERER) ?: "unavailable"}")
        Log.i(TAG, "OpenGL version: ${gl?.glGetString(GL10.GL_VERSION) ?: "unavailable"}")

        if (!nativeLibraryLoaded) return

        nativeReady = try {
            nativeInit()
        } catch (error: UnsatisfiedLinkError) {
            Log.e(TAG, "JNI entry point is unavailable", error)
            false
        } catch (error: RuntimeException) {
            Log.e(TAG, "Native renderer initialization failed", error)
            false
        }

        if (!nativeReady) {
            Log.e(TAG, "Native GLES3 initialization did not complete")
        }
    }

    override fun onSurfaceChanged(gl: GL10?, width: Int, height: Int) {
        if (!nativeReady || width <= 0 || height <= 0) return
        try {
            nativeResize(width, height)
        } catch (error: UnsatisfiedLinkError) {
            nativeReady = false
            Log.e(TAG, "nativeResize JNI call failed", error)
        }
    }

    override fun onDrawFrame(gl: GL10?) {
        if (!nativeReady) {
            // Keep the UI thread and GLSurfaceView alive even if native setup
            // could not initialize on this device.
            gl?.glClearColor(0.025f, 0.035f, 0.055f, 1.0f)
            gl?.glClear(GL10.GL_COLOR_BUFFER_BIT or GL10.GL_DEPTH_BUFFER_BIT)
            return
        }

        try {
            nativeRender()
        } catch (error: UnsatisfiedLinkError) {
            nativeReady = false
            Log.e(TAG, "nativeRender JNI call failed", error)
        }
    }

    override fun onPause() {
        if (::surface.isInitialized) surface.onPause()
        super.onPause()
    }

    override fun onResume() {
        super.onResume()
        if (::surface.isInitialized) surface.onResume()
    }

    companion object {
        private const val TAG = "MCLG-Android"

        private val nativeLibraryLoaded: Boolean = try {
            System.loadLibrary("mclg_android")
            Log.i(TAG, "Loaded libmclg_android.so")
            true
        } catch (error: UnsatisfiedLinkError) {
            Log.e(TAG, "Could not load libmclg_android.so", error)
            false
        }
    }
}

/**
 * Select only EGL configs that can create an OpenGL ES 3 context.
 * Some mobile drivers omit alpha or depth formats from their preferred
 * window configs, so retry progressively less restrictive pixel formats.
 */
private class GLES3ConfigChooser : GLSurfaceView.EGLConfigChooser {
    override fun chooseConfig(egl: EGL10, display: EGLDisplay): EGLConfig {
        val candidates = arrayOf(
            configSpec(8, 8, 8, 8, 16),
            configSpec(8, 8, 8, 0, 16),
            configSpec(5, 6, 5, 0, 16),
            configSpec(5, 6, 5, 0, 0)
        )

        for ((index, spec) in candidates.withIndex()) {
            val configs = arrayOfNulls<EGLConfig>(1)
            val count = IntArray(1)
            val found = egl.eglChooseConfig(display, spec, configs, 1, count)
            val config = configs.firstOrNull()

            if (found && count[0] > 0 && config != null) {
                if (index > 0) {
                    Log.w("MCLG-Android", "Using EGL fallback config #$index")
                }
                return config
            }
        }

        throw IllegalArgumentException(
            "This device has no EGL window config capable of OpenGL ES 3"
        )
    }

    private fun configSpec(
        red: Int,
        green: Int,
        blue: Int,
        alpha: Int,
        depth: Int
    ): IntArray = intArrayOf(
        EGL10.EGL_SURFACE_TYPE, EGL10.EGL_WINDOW_BIT,
        EGL10.EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
        EGL10.EGL_RED_SIZE, red,
        EGL10.EGL_GREEN_SIZE, green,
        EGL10.EGL_BLUE_SIZE, blue,
        EGL10.EGL_ALPHA_SIZE, alpha,
        EGL10.EGL_DEPTH_SIZE, depth,
        EGL10.EGL_STENCIL_SIZE, 0,
        EGL10.EGL_NONE
    )

    companion object {
        // EGL_OPENGL_ES3_BIT_KHR from EGL_KHR_create_context.
        private const val EGL_OPENGL_ES3_BIT_KHR = 0x0040
    }
}
