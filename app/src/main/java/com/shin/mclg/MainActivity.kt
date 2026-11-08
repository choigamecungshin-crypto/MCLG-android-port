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
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.InputDevice
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
    private val heldKeys = HashSet<Int>()

    private external fun nativeInit(storagePath: String): Boolean
    private external fun nativeResize(width: Int, height: Int)
    private external fun nativeRender()
    private external fun nativeInputButton(mask: Int, pressed: Boolean)
    private external fun nativeInputMove(x: Float, y: Float)
    private external fun nativeInputLook(dx: Float, dy: Float)
    private external fun nativeInputTouch(x: Float, y: Float, pressed: Boolean)
    private external fun nativeInputMouse(x: Float, y: Float, button: Int, pressed: Boolean)
    private external fun nativeInputKey(keyCode: Int, unicode: Int, pressed: Boolean)
    private external fun nativeInputTrigger(right: Boolean, value: Int)

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
            surface.isFocusable = true
            surface.isFocusableInTouchMode = true
            surface.setOnTouchListener { _, event ->
                if (event.actionMasked == MotionEvent.ACTION_DOWN) surface.requestFocus()
                if (nativeReady && surface.width > 0 && surface.height > 0) {
                    when (event.actionMasked) {
                        MotionEvent.ACTION_DOWN -> nativeInputTouch(
                            event.x / surface.width, event.y / surface.height, true
                        )
                        MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> nativeInputTouch(
                            event.x / surface.width, event.y / surface.height, false
                        )
                    }
                }
                true
            }
            setContentView(surface)
            Log.i(TAG, "Activity created; hardware keyboard, mouse and gamepad input enabled (no touch overlay)")

            } catch (error: Throwable) {
            recordCrash("Activity/EGL setup", error)
            showStartupError(error)
        }

    }



    private fun buttonMask(keyCode: Int): Int? = when (keyCode) {
        KeyEvent.KEYCODE_SPACE, KeyEvent.KEYCODE_BUTTON_A -> 0x00000001
        KeyEvent.KEYCODE_Q, KeyEvent.KEYCODE_BUTTON_B -> 0x00000002
        KeyEvent.KEYCODE_F, KeyEvent.KEYCODE_BUTTON_X -> 0x00000004
        KeyEvent.KEYCODE_E, KeyEvent.KEYCODE_BUTTON_Y -> 0x00000008
        KeyEvent.KEYCODE_ESCAPE, KeyEvent.KEYCODE_BUTTON_START -> 0x00000010
        KeyEvent.KEYCODE_TAB, KeyEvent.KEYCODE_BUTTON_SELECT -> 0x00000020
        KeyEvent.KEYCODE_BUTTON_R1 -> 0x00000040
        KeyEvent.KEYCODE_BUTTON_L1 -> 0x00000080
        KeyEvent.KEYCODE_SHIFT_LEFT, KeyEvent.KEYCODE_SHIFT_RIGHT,
        KeyEvent.KEYCODE_BUTTON_THUMBR -> 0x00000100
        KeyEvent.KEYCODE_F5, KeyEvent.KEYCODE_BUTTON_THUMBL -> 0x00000200
        KeyEvent.KEYCODE_DPAD_UP -> 0x00000400
        KeyEvent.KEYCODE_DPAD_DOWN -> 0x00000800
        KeyEvent.KEYCODE_DPAD_LEFT -> 0x00001000
        KeyEvent.KEYCODE_DPAD_RIGHT -> 0x00002000
        KeyEvent.KEYCODE_BUTTON_R2 -> 0x00400000
        KeyEvent.KEYCODE_BUTTON_L2 -> 0x00800000
        else -> null
    }

    private fun updateKeyboardMovement() {
        if (!nativeReady) return
        val x = (if (heldKeys.contains(KeyEvent.KEYCODE_D)) 1f else 0f) -
                (if (heldKeys.contains(KeyEvent.KEYCODE_A)) 1f else 0f)
        val y = (if (heldKeys.contains(KeyEvent.KEYCODE_S)) 1f else 0f) -
                (if (heldKeys.contains(KeyEvent.KEYCODE_W)) 1f else 0f)
        nativeInputMove(x, y)
    }

    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        val down = event.action == KeyEvent.ACTION_DOWN
        val up = event.action == KeyEvent.ACTION_UP
        if (!down && !up) return super.dispatchKeyEvent(event)

        val code = event.keyCode
        if (code == KeyEvent.KEYCODE_W || code == KeyEvent.KEYCODE_A ||
            code == KeyEvent.KEYCODE_S || code == KeyEvent.KEYCODE_D) {
            if (down) heldKeys.add(code) else heldKeys.remove(code)
            updateKeyboardMovement()
            return true
        }

        val mask = buttonMask(code)
        if (nativeReady) {
            nativeInputKey(code, event.unicodeChar, down)
            if (mask != null || code == KeyEvent.KEYCODE_ESCAPE ||
                code == KeyEvent.KEYCODE_BACK || event.unicodeChar != 0 ||
                code == KeyEvent.KEYCODE_ENTER || code == KeyEvent.KEYCODE_TAB) {
                return true
            }
        }
        return super.dispatchKeyEvent(event)
    }


    private fun handleMouseButtons(event: MotionEvent): Boolean {
        if (event.actionMasked != MotionEvent.ACTION_BUTTON_PRESS &&
            event.actionMasked != MotionEvent.ACTION_BUTTON_RELEASE) return false
        if (!nativeReady || surface.width <= 0 || surface.height <= 0) return true
        val pressed = event.actionMasked == MotionEvent.ACTION_BUTTON_PRESS
        val button = when (event.actionButton) {
            MotionEvent.BUTTON_PRIMARY -> 0
            MotionEvent.BUTTON_SECONDARY -> 1
            MotionEvent.BUTTON_TERTIARY -> 2
            else -> -1
        }
        if (button >= 0) nativeInputMouse(
            event.x / surface.width, event.y / surface.height, button, pressed
        )
        return true
    }

    private fun clearHardwareInput() {
        if (nativeReady) {
            for (mask in intArrayOf(
                0x00000001, 0x00000002, 0x00000004, 0x00000008,
                0x00000010, 0x00000020, 0x00000040, 0x00000080,
                0x00000100, 0x00000200, 0x00000400, 0x00000800,
                0x00001000, 0x00002000, 0x00400000, 0x00800000
            )) nativeInputButton(mask, false)
            nativeInputMove(0f, 0f)
            nativeInputLook(0f, 0f)
            nativeInputTrigger(false, 0)
            nativeInputTrigger(true, 0)
        }
        heldKeys.clear()
    }

    private var lastMouseX = Float.NaN
    private var lastMouseY = Float.NaN

    override fun onGenericMotionEvent(event: MotionEvent): Boolean {
        if (handleMouseButtons(event)) return true
        val source = event.source
        if ((source and InputDevice.SOURCE_MOUSE) == InputDevice.SOURCE_MOUSE) {
            if ((event.action == MotionEvent.ACTION_HOVER_MOVE ||
                 event.action == MotionEvent.ACTION_MOVE) && nativeReady) {
                if (!lastMouseX.isNaN() && !lastMouseY.isNaN()) {
                    nativeInputLook(
                        (event.x - lastMouseX) * 0.012f,
                        (event.y - lastMouseY) * 0.012f
                    )
                }
                lastMouseX = event.x
                lastMouseY = event.y
                return true
            }
        }
        if ((source and InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK ||
            (source and InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD) {
            if (event.action == MotionEvent.ACTION_MOVE && nativeReady) {
                nativeInputMove(
                    event.getAxisValue(MotionEvent.AXIS_X),
                    event.getAxisValue(MotionEvent.AXIS_Y)
                )
                nativeInputLook(
                    event.getAxisValue(MotionEvent.AXIS_RX),
                    event.getAxisValue(MotionEvent.AXIS_RY)
                )
                val leftTrigger = (event.getAxisValue(MotionEvent.AXIS_LTRIGGER) * 255f)
                    .toInt().coerceIn(0, 255)
                val rightTrigger = (event.getAxisValue(MotionEvent.AXIS_RTRIGGER) * 255f)
                    .toInt().coerceIn(0, 255)
                nativeInputTrigger(false, leftTrigger)
                nativeInputTrigger(true, rightTrigger)
                return true
            }
        }
        return super.onGenericMotionEvent(event)
    }

    private fun installGameResources() {
        val resourceRoot = File(filesDir, "Common/res")
        val mediaRoot = File(filesDir, "Common/Media")
        val marker = File(filesDir, ".mclg-base-resources")
        val resourceVersion = "legacy-base-resources-2"
        if (resourceRoot.isDirectory && mediaRoot.isDirectory &&
            marker.isFile && marker.readText() == resourceVersion) {
            return
        }

        resourceRoot.deleteRecursively()
        mediaRoot.deleteRecursively()
        copyAssetTree("Common/res", resourceRoot)
        copyAssetTree("Common/Media", mediaRoot)
        marker.writeText(resourceVersion)
        Log.i(TAG, "Installed complete Legacy resources into ${filesDir.absolutePath}")
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
        clearHardwareInput()
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
