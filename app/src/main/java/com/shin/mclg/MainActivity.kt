package com.shin.mclg

import android.app.Activity
import android.opengl.GLSurfaceView
import android.os.Bundle
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
            setEGLContextClientVersion(3)
            preserveEGLContextOnPause = true
            setRenderer(this@MainActivity)
            renderMode = GLSurfaceView.RENDERMODE_CONTINUOUSLY
        }
        setContentView(surface)
    }

    override fun onSurfaceCreated(gl: GL10?, config: EGLConfig?) = nativeInit()
    override fun onSurfaceChanged(gl: GL10?, width: Int, height: Int) = nativeResize(width, height)
    override fun onDrawFrame(gl: GL10?) = nativeRender()

    override fun onPause() {
        super.onPause()
        if (::surface.isInitialized) surface.onPause()
    }

    override fun onResume() {
        super.onResume()
        if (::surface.isInitialized) surface.onResume()
    }

    companion object {
        init { System.loadLibrary("mclg_android") }
    }
}
