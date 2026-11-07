package dev.mc.legacy

import android.app.Activity
import android.opengl.GLSurfaceView
import android.os.Bundle
import android.util.Log
import android.view.WindowManager
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10

class MainActivity : Activity(), GLSurfaceView.Renderer {
    private lateinit var surface: GLSurfaceView

    private external fun nativeInit()
    private external fun nativeResize(width: Int, height: Int)
    private external fun nativeRender()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        surface = GLSurfaceView(this).apply {
            // Use the device's native OpenGL ES 3.0 driver (Adreno 610 supports ES 3.2).
            setEGLContextClientVersion(3)
            setEGLConfigChooser(8, 8, 8, 8, 16, 0)
            preserveEGLContextOnPause = false
            setRenderer(this@MainActivity)
            renderMode = GLSurfaceView.RENDERMODE_CONTINUOUSLY
        }
        setContentView(surface)
        Log.i(TAG, "Activity created; requesting OpenGL ES 3.0")
    }

    override fun onSurfaceCreated(gl: GL10?, config: EGLConfig?) {
        Log.i(TAG, "OpenGL renderer: ${gl?.glGetString(GL10.GL_RENDERER)}")
        Log.i(TAG, "OpenGL version: ${gl?.glGetString(GL10.GL_VERSION)}")
        nativeInit()
    }

    override fun onSurfaceChanged(gl: GL10?, width: Int, height: Int) {
        nativeResize(width, height)
    }

    override fun onDrawFrame(gl: GL10?) {
        nativeRender()
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

        init {
            System.loadLibrary("mclg_android")
        }
    }
}
