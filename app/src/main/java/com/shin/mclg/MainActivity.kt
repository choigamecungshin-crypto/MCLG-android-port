package dev.mc.legacy

import android.app.Activity
import android.app.ActivityManager
import android.app.ApplicationExitInfo
import android.os.Build
import android.app.AlertDialog
import android.content.Intent
import android.opengl.GLSurfaceView
import android.os.Bundle
import android.util.Log
import android.view.WindowManager
import android.view.ViewGroup
import android.widget.FrameLayout
import android.widget.TextView
import java.io.File
import java.io.PrintWriter
import java.io.StringWriter
import javax.microedition.khronos.egl.EGL10
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.egl.EGLDisplay
import javax.microedition.khronos.opengles.GL10

class MainActivity : Activity(), GLSurfaceView.Renderer {
    private lateinit var surface: GLSurfaceView
    @Volatile private var nativeReady = false

    private external fun nativeInit(storagePath: String): Boolean
    private external fun nativeResize(width: Int, height: Int)
    private external fun nativeRender()
    private external fun nativeInputButton(mask: Int, pressed: Boolean)
    private external fun nativeInputMove(x: Float, y: Float)
    private external fun nativeInputLook(dx: Float, dy: Float)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        collectPreviousExitInfo()
        showPreviousCrashIfPresent()
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        try {
            installGameResources()
        } catch (error: Throwable) {
            recordCrash("Legacy resource installation", error)
            showStartupError(error)
            return
        }

        if (!nativeLibraryLoaded) {
            Log.e(TAG, "Native renderer is unavailable; showing a safe fallback surface")
        }

        try {
            surface = GLSurfaceView(this).apply {
                setEGLContextClientVersion(3)
                setEGLConfigChooser(GLES3ConfigChooser())
                preserveEGLContextOnPause = false
                setRenderer(this@MainActivity)
                renderMode = GLSurfaceView.RENDERMODE_CONTINUOUSLY
            }
            val root = FrameLayout(this).apply {
                addView(
                    surface,
                    FrameLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT,
                        ViewGroup.LayoutParams.MATCH_PARENT
                    )
                )
                addView(
                    AndroidTouchControls(
                        this@MainActivity,
                        { mask, pressed -> if (nativeReady) nativeInputButton(mask, pressed) },
                        { x, y -> if (nativeReady) nativeInputMove(x, y) },
                        { dx, dy -> if (nativeReady) nativeInputLook(dx, dy) }
                    ),
                    FrameLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT,
                        ViewGroup.LayoutParams.MATCH_PARENT
                    )
                )
            }
            setContentView(root)
            Log.i(TAG, "Activity created; requesting OpenGL ES 3.0 with touch controls")
        } catch (error: Throwable) {
            recordCrash("Activity/EGL setup", error)
            showStartupError(error)
        }

    }


    private fun installGameResources() {
        val destination = File(filesDir, "Common/res/TitleUpdate/res")
        val marker = File(filesDir, ".mclg-base-resources")
        val resourceVersion = "legacy-base-resources-1"
        if (destination.isDirectory && marker.isFile && marker.readText() == resourceVersion) {
            return
        }

        destination.deleteRecursively()
        copyAssetTree("Common/res/TitleUpdate/res", destination)
        marker.writeText(resourceVersion)
        Log.i(TAG, "Installed Legacy base resources into ${destination.absolutePath}")
    }

    private fun copyAssetTree(assetPath: String, destination: File) {
        val children = assets.list(assetPath) ?: emptyArray()
        if (children.isEmpty()) {
            destination.parentFile?.mkdirs()
            assets.open(assetPath).use { input ->
                destination.outputStream().use { output -> input.copyTo(output) }
            }
            return
        }

        destination.mkdirs()
        for (child in children) {
            copyAssetTree("$assetPath/$child", File(destination, child))
        }
    }

    // Native SIGSEGVs cannot be caught by a Java uncaught-exception handler.
    // Android records the previous process exit; collect its tombstone/trace
    // on the next launch so we can diagnose crashes that happen before UI.
    private fun collectPreviousExitInfo() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) return

        try {
            val manager = getSystemService(ACTIVITY_SERVICE) as ActivityManager
            val prefs = getSharedPreferences("mclg-crash", MODE_PRIVATE)
            val lastHandled = prefs.getLong("last_handled_exit_timestamp", 0L)

            val exit = manager.getHistoricalProcessExitReasons(packageName, 0, 10)
                .firstOrNull { info ->
                    info.timestamp > lastHandled && (
                        info.reason == ApplicationExitInfo.REASON_CRASH ||
                        info.reason == ApplicationExitInfo.REASON_CRASH_NATIVE ||
                        info.reason == ApplicationExitInfo.REASON_ANR ||
                        info.reason == ApplicationExitInfo.REASON_INITIALIZATION_FAILURE
                    )
                } ?: return

            val reason = when (exit.reason) {
                ApplicationExitInfo.REASON_CRASH -> "JAVA_CRASH"
                ApplicationExitInfo.REASON_CRASH_NATIVE -> "NATIVE_CRASH"
                ApplicationExitInfo.REASON_ANR -> "ANR"
                ApplicationExitInfo.REASON_INITIALIZATION_FAILURE -> "INITIALIZATION_FAILURE"
                else -> "UNKNOWN(${exit.reason})"
            }

            val trace = try {
                exit.traceInputStream?.bufferedReader()?.use { it.readText() }
                    ?: "(Android did not provide a trace)"
            } catch (error: Throwable) {
                "(Could not read Android trace: ${error.message})"
            }

            val report = buildString {
                appendLine("Previous process exited abnormally")
                appendLine("Package: $packageName")
                appendLine("Reason: $reason")
                appendLine("Timestamp: ${exit.timestamp}")
                appendLine("Status: ${exit.status}")
                appendLine("Importance: ${exit.importance}")
                appendLine("Description: ${exit.description}")
                appendLine()
                appendLine("Android trace / tombstone:")
                appendLine(trace)
            }

            val reportFile = File(filesDir, CRASH_FILE)
            val oldReport = if (reportFile.exists()) {
                try { reportFile.readText() } catch (_: Throwable) { "" }
            } else ""
            reportFile.writeText(if (oldReport.isBlank()) report else "$oldReport\n\n$report")

            prefs.edit().putLong("last_handled_exit_timestamp", exit.timestamp).apply()
            Log.e(TAG, "Recovered previous process exit: $reason; report saved to ${reportFile.absolutePath}")
        } catch (error: Throwable) {
            Log.w(TAG, "Could not inspect previous process exit information", error)
        }
    }

    private fun recordCrash(stage: String, error: Throwable) {
        try {
            val writer = StringWriter()
            error.printStackTrace(PrintWriter(writer))
            File(filesDir, CRASH_FILE).writeText(
                "Stage: $stage\nThread: ${Thread.currentThread().name}\n\n$writer"
            )
            Log.e(TAG, "Crash recorded at $stage", error)
        } catch (writeError: Throwable) {
            Log.e(TAG, "Could not save crash report", writeError)
        }
    }

    private fun showStartupError(error: Throwable) {
        val message = error.stackTraceToString()
        setContentView(TextView(this).apply {
            text = "Minecraft Legacy could not start.\n\n$message"
            textSize = 12f
            setPadding(24, 24, 24, 24)
        })
    }

    private fun showPreviousCrashIfPresent() {
        val report = File(filesDir, CRASH_FILE)
        if (!report.exists()) return
        val content = try { report.readText() } catch (_: Throwable) { return }
        AlertDialog.Builder(this)
            .setTitle("Previous startup crash")
            .setMessage("A crash report was saved. Share it so the exact cause can be fixed.")
            .setPositiveButton("Share report") { _, _ ->
                val send = Intent(Intent.ACTION_SEND).apply {
                    type = "text/plain"
                    putExtra(Intent.EXTRA_SUBJECT, "Minecraft Legacy Android crash")
                    putExtra(Intent.EXTRA_TEXT, content)
                }
                startActivity(Intent.createChooser(send, "Send crash report"))
            }
            .setNegativeButton("Close", null)
            .show()
    }

    override fun onSurfaceCreated(gl: GL10?, config: EGLConfig?) {
        nativeReady = false
        Log.i(TAG, "OpenGL renderer: ${gl?.glGetString(GL10.GL_RENDERER) ?: "unavailable"}")
        Log.i(TAG, "OpenGL version: ${gl?.glGetString(GL10.GL_VERSION) ?: "unavailable"}")
        if (!nativeLibraryLoaded) return

        nativeReady = try {
            nativeInit(filesDir.absolutePath)
        } catch (error: Throwable) {
            recordCrash("JNI nativeInit", error)
            false
        }
        if (!nativeReady) Log.e(TAG, "Native GLES3 initialization did not complete")
    }

    override fun onSurfaceChanged(gl: GL10?, width: Int, height: Int) {
        if (!nativeReady || width <= 0 || height <= 0) return
        try {
            nativeResize(width, height)
        } catch (error: Throwable) {
            recordCrash("JNI nativeResize", error)
            nativeReady = false
        }
    }

    override fun onDrawFrame(gl: GL10?) {
        if (!nativeReady) {
            gl?.glClearColor(0.025f, 0.035f, 0.055f, 1.0f)
            gl?.glClear(GL10.GL_COLOR_BUFFER_BIT or GL10.GL_DEPTH_BUFFER_BIT)
            return
        }
        try {
            nativeRender()
        } catch (error: Throwable) {
            recordCrash("JNI nativeRender", error)
            nativeReady = false
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
        private const val CRASH_FILE = "mclg-crash.txt"

        private val nativeLibraryLoaded: Boolean = try {
            System.loadLibrary("mclg_android")
            Log.i(TAG, "Loaded libmclg_android.so")
            true
        } catch (error: Throwable) {
            Log.e(TAG, "Could not load libmclg_android.so", error)
            false
        }
    }
}

private class GLES3ConfigChooser : GLSurfaceView.EGLConfigChooser {
    override fun chooseConfig(egl: EGL10, display: EGLDisplay): EGLConfig {
        val candidates = arrayOf(
            configSpec(8, 8, 8, 8, 16, EGL_OPENGL_ES3_BIT_KHR),
            configSpec(8, 8, 8, 0, 16, EGL_OPENGL_ES3_BIT_KHR),
            configSpec(5, 6, 5, 0, 16, EGL_OPENGL_ES3_BIT_KHR),
            configSpec(5, 6, 5, 0, 0, EGL_OPENGL_ES3_BIT_KHR),
            // Some EGL drivers advertise ES3 contexts on configs carrying the ES2 bit.
            configSpec(8, 8, 8, 0, 16, EGL_OPENGL_ES2_BIT)
        )

        for ((index, spec) in candidates.withIndex()) {
            val configs = arrayOfNulls<EGLConfig>(1)
            val count = IntArray(1)
            val found = egl.eglChooseConfig(display, spec, configs, 1, count)
            val config = configs.firstOrNull()
            if (found && count[0] > 0 && config != null) {
                Log.i("MCLG-Android", "Selected EGL config candidate #$index")
                return config
            }
            Log.w("MCLG-Android", "EGL config candidate #$index unavailable; eglError=0x${egl.eglGetError().toString(16)}")
        }

        throw IllegalArgumentException("No compatible EGL window config found for GLES3")
    }

    private fun configSpec(
        red: Int, green: Int, blue: Int, alpha: Int, depth: Int, renderable: Int
    ): IntArray = intArrayOf(
        EGL10.EGL_SURFACE_TYPE, EGL10.EGL_WINDOW_BIT,
        EGL10.EGL_RENDERABLE_TYPE, renderable,
        EGL10.EGL_RED_SIZE, red,
        EGL10.EGL_GREEN_SIZE, green,
        EGL10.EGL_BLUE_SIZE, blue,
        EGL10.EGL_ALPHA_SIZE, alpha,
        EGL10.EGL_DEPTH_SIZE, depth,
        EGL10.EGL_STENCIL_SIZE, 0,
        EGL10.EGL_NONE
    )

    companion object {
        private const val EGL_OPENGL_ES2_BIT = 0x0004
        private const val EGL_OPENGL_ES3_BIT_KHR = 0x0040
    }
}
