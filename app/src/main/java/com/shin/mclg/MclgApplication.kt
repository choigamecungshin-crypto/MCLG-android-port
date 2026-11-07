package dev.mc.legacy

import android.app.Application
import android.content.Context
import android.os.Process
import android.util.Log
import java.io.File
import java.io.PrintWriter
import java.io.StringWriter
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * Installs the uncaught-exception recorder before MainActivity is created.
 * This captures Java exceptions thrown during Activity/class initialization
 * that the Activity-level handler could miss.
 */
class MclgApplication : Application() {
    override fun attachBaseContext(base: Context) {
        super.attachBaseContext(base)
        val previous = Thread.getDefaultUncaughtExceptionHandler()
        Thread.setDefaultUncaughtExceptionHandler { thread, error ->
            try {
                val writer = StringWriter()
                error.printStackTrace(PrintWriter(writer))
                val timestamp = SimpleDateFormat("yyyy-MM-dd HH:mm:ss.SSS z", Locale.US)
                    .format(Date())
                val report = buildString {
                    appendLine("Minecraft Legacy Android fatal Java exception")
                    appendLine("Timestamp: $timestamp")
                    appendLine("Package: $packageName")
                    appendLine("PID: ${Process.myPid()}")
                    appendLine("Thread: ${thread.name} (id=${thread.id})")
                    appendLine("Exception: ${error.javaClass.name}: ${error.message}")
                    appendLine()
                    appendLine(writer.toString())
                }
                File(filesDir, "mclg-crash.txt").appendText("\n\n$report")
                Log.e("MCLG-Android", "Fatal Java exception saved to filesDir/mclg-crash.txt", error)
            } catch (writeError: Throwable) {
                Log.e("MCLG-Android", "Could not persist fatal Java exception", writeError)
            }

            if (previous != null) {
                previous.uncaughtException(thread, error)
            } else {
                Process.killProcess(Process.myPid())
            }
        }
    }
}
