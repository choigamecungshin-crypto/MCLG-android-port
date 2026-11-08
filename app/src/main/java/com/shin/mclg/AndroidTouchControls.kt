package dev.mc.legacy

import android.content.Context
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Typeface
import android.view.MotionEvent
import android.view.View
import kotlin.math.min
import kotlin.math.sqrt

/**
 * Lightweight touch overlay for the Legacy controller model.
 * Movement, look and button states are passed to the native C4JInput adapter.
 */
class AndroidTouchControls(
    context: Context,
    private val button: (Int, Boolean) -> Unit,
    private val move: (Float, Float) -> Unit,
    private val look: (Float, Float) -> Unit
) : View(context) {
    private enum class Role { STICK, LOOK, BUTTON }

    private data class Pointer(
        var role: Role,
        var lastX: Float,
        var lastY: Float,
        var mask: Int = 0
    )

    private val pointers = HashMap<Int, Pointer>()
    private val density = resources.displayMetrics.density
    private val paint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        typeface = Typeface.create(Typeface.DEFAULT, Typeface.BOLD)
        textAlign = Paint.Align.CENTER
    }

    private val buttonJump = 0x00000001
    private val buttonSneak = 0x00000100
    private val buttonInventory = 0x00000008
    private val buttonStart = 0x00000010
    private val buttonAttack = 0x00400000
    private val buttonUse = 0x00800000

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)
        val shortSide = min(width, height).toFloat()
        if (shortSide <= 0f) return

        val stickX = width * 0.16f
        val stickY = height * 0.76f
        val stickRadius = shortSide * 0.115f
        drawControl(canvas, stickX, stickY, stickRadius, "MOVE", false)

        drawControl(canvas, width * 0.90f, height * 0.62f, shortSide * 0.075f, "JUMP", false)
        drawControl(canvas, width * 0.77f, height * 0.82f, shortSide * 0.065f, "HIT", false)
        drawControl(canvas, width * 0.90f, height * 0.84f, shortSide * 0.060f, "USE", false)
        drawControl(canvas, width * 0.65f, height * 0.88f, shortSide * 0.052f, "SNEAK", false)
        drawControl(canvas, width * 0.72f, height * 0.65f, shortSide * 0.047f, "INV", false)
        drawControl(canvas, width * 0.95f, height * 0.13f, shortSide * 0.038f, "Ⅱ", true)

        paint.color = 0x66FFFFFF
        paint.textSize = 10f * density
        canvas.drawText("DRAG TO LOOK", width * 0.76f, height * 0.98f, paint)
    }

    private fun drawControl(
        canvas: Canvas,
        x: Float,
        y: Float,
        radius: Float,
        label: String,
        small: Boolean
    ) {
        paint.style = Paint.Style.FILL
        paint.color = 0x44000000
        canvas.drawCircle(x, y, radius, paint)
        paint.style = Paint.Style.STROKE
        paint.strokeWidth = (if (small) 1.2f else 1.8f) * density
        paint.color = 0x88FFFFFF.toInt()
        canvas.drawCircle(x, y, radius, paint)
        paint.style = Paint.Style.FILL
        paint.color = 0xCCFFFFFF.toInt()
        paint.textSize = (if (small) 13f else 10f) * density
        canvas.drawText(label, x, y + paint.textSize * 0.35f, paint)
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> {
                val index = event.actionIndex
                beginPointer(event.getPointerId(index), event.getX(index), event.getY(index))
            }
            MotionEvent.ACTION_MOVE -> {
                for (index in 0 until event.pointerCount) {
                    movePointer(event.getPointerId(index), event.getX(index), event.getY(index))
                }
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP -> {
                val index = event.actionIndex
                endPointer(event.getPointerId(index))
            }
            MotionEvent.ACTION_CANCEL -> {
                val active = pointers.keys.toList()
                active.forEach(::endPointer)
            }
        }
        invalidate()
        return true
    }

    private fun beginPointer(id: Int, x: Float, y: Float) {
        val shortSide = min(width, height).toFloat()
        val stickX = width * 0.16f
        val stickY = height * 0.76f
        val stickRadius = shortSide * 0.16f

        if (distance(x, y, stickX, stickY) <= stickRadius) {
            pointers[id] = Pointer(Role.STICK, x, y)
            updateStick(x, y)
            return
        }

        val control = nearestButton(x, y, shortSide)
        if (control != null) {
            pointers[id] = Pointer(Role.BUTTON, x, y, control.first)
            button(control.first, true)
            return
        }

        if (x >= width * 0.43f) {
            pointers[id] = Pointer(Role.LOOK, x, y)
        } else {
            pointers[id] = Pointer(Role.LOOK, x, y)
        }
    }

    private fun movePointer(id: Int, x: Float, y: Float) {
        val pointer = pointers[id] ?: return
        when (pointer.role) {
            Role.STICK -> updateStick(x, y)
            Role.LOOK -> {
                val dx = (x - pointer.lastX) / min(width, height).toFloat() * 5.0f
                val dy = (y - pointer.lastY) / min(width, height).toFloat() * 5.0f
                if (dx != 0f || dy != 0f) look(dx, dy)
            }
            Role.BUTTON -> Unit
        }
        pointer.lastX = x
        pointer.lastY = y
    }

    private fun endPointer(id: Int) {
        val pointer = pointers.remove(id) ?: return
        when (pointer.role) {
            Role.STICK -> move(0f, 0f)
            Role.LOOK -> look(0f, 0f)
            Role.BUTTON -> button(pointer.mask, false)
        }
    }

    private fun updateStick(x: Float, y: Float) {
        val radius = min(width, height).toFloat() * 0.115f
        val dx = (x - width * 0.16f) / radius
        val dy = (y - height * 0.76f) / radius
        val length = sqrt(dx * dx + dy * dy)
        val scale = if (length > 1f) 1f / length else 1f
        move((dx * scale).coerceIn(-1f, 1f), (dy * scale).coerceIn(-1f, 1f))
    }

    private fun nearestButton(x: Float, y: Float, shortSide: Float): Pair<Int, Float>? {
        val controls = listOf(
            Triple(buttonJump, width * 0.90f, height * 0.62f) to shortSide * 0.075f,
            Triple(buttonAttack, width * 0.77f, height * 0.82f) to shortSide * 0.065f,
            Triple(buttonUse, width * 0.90f, height * 0.84f) to shortSide * 0.060f,
            Triple(buttonSneak, width * 0.65f, height * 0.88f) to shortSide * 0.052f,
            Triple(buttonInventory, width * 0.72f, height * 0.65f) to shortSide * 0.047f,
            Triple(buttonStart, width * 0.95f, height * 0.13f) to shortSide * 0.055f
        )
        var selected: Pair<Int, Float>? = null
        for ((control, radius) in controls) {
            val (mask, cx, cy) = control
            val d = distance(x, y, cx, cy)
            if (d <= radius && (selected == null || d < selected.second)) {
                selected = mask to d
            }
        }
        return selected
    }

    private fun distance(x: Float, y: Float, cx: Float, cy: Float): Float {
        val dx = x - cx
        val dy = y - cy
        return sqrt(dx * dx + dy * dy)
    }
}
