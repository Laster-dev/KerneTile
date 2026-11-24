package com.kernetile.studio

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Rect
import android.graphics.Typeface
import android.view.Surface
import java.lang.ref.WeakReference
import java.nio.ByteBuffer
import kotlin.math.ceil
import kotlin.math.floor

object KerneTileBridge {
    init {
        System.loadLibrary("ktui_gallery")
    }

    private var activityRef: WeakReference<MainActivity>? = null

    private val measurePaint = Paint(Paint.ANTI_ALIAS_FLAG or Paint.SUBPIXEL_TEXT_FLAG).apply {
        typeface = Typeface.DEFAULT
        color = Color.WHITE
    }

    private val boundsRect = Rect()

    fun attachActivity(activity: MainActivity) {
        activityRef = WeakReference(activity)
    }

    fun detachActivity(activity: MainActivity) {
        if (activityRef?.get() === activity) {
            activityRef = null
        }
    }

    @JvmStatic
    fun measureText(text: String, fontSize: Float): FloatArray {
        synchronized(measurePaint) {
            measurePaint.textSize = fontSize
            val fm = measurePaint.fontMetrics
            val width = measurePaint.measureText(text)
            val ascent = -fm.ascent
            val descent = fm.descent
            return floatArrayOf(width, ascent, descent)
        }
    }

    @JvmStatic
    fun rasterizeGlyph(text: String, fontSize: Float, outBox: IntArray): ByteArray? {
        if (text.isEmpty() || outBox.size < 4) return null
        synchronized(measurePaint) {
            measurePaint.textSize = fontSize
            measurePaint.getTextBounds(text, 0, text.length, boundsRect)
            if (boundsRect.isEmpty) {
                return null
            }
            val pad = 1
            val left = boundsRect.left - pad
            val top = boundsRect.top - pad
            val width = (boundsRect.width() + pad * 2).coerceIn(1, 2048)
            val height = (boundsRect.height() + pad * 2).coerceIn(1, 2048)

            val bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ALPHA_8)
            val canvas = Canvas(bitmap)
            canvas.drawColor(Color.TRANSPARENT)
            canvas.drawText(text, (-left).toFloat(), (-top).toFloat(), measurePaint)

            val pixels = ByteArray(width * height)
            bitmap.copyPixelsToBuffer(ByteBuffer.wrap(pixels))
            bitmap.recycle()

            outBox[0] = width
            outBox[1] = height
            outBox[2] = floor(left.toDouble()).toInt()
            outBox[3] = ceil((-top).toDouble()).toInt()
            return pixels
        }
    }

    @JvmStatic
    fun setImeEnabled(enabled: Boolean) {
        val activity = activityRef?.get() ?: return
        activity.runOnUiThread {
            activity.setSoftKeyboardVisible(enabled)
        }
    }

    @JvmStatic
    fun getClipboardText(): String {
        val activity = activityRef?.get() ?: return ""
        val cm = activity.getSystemService(Context.CLIPBOARD_SERVICE) as? ClipboardManager ?: return ""
        val clip = cm.primaryClip ?: return ""
        if (clip.itemCount <= 0) return ""
        return clip.getItemAt(0).coerceToText(activity)?.toString() ?: ""
    }

    @JvmStatic
    fun setClipboardText(text: String) {
        val activity = activityRef?.get() ?: return
        activity.runOnUiThread {
            val cm = activity.getSystemService(Context.CLIPBOARD_SERVICE) as? ClipboardManager ?: return@runOnUiThread
            cm.setPrimaryClip(ClipData.newPlainText("KerneTile", text))
        }
    }

    @JvmStatic external fun nativeInit()
    @JvmStatic external fun nativeSurfaceChanged(surface: Surface?, width: Int, height: Int, density: Float)
    @JvmStatic external fun nativeSurfaceDestroyed()
    @JvmStatic external fun nativeStepFrame()
    @JvmStatic external fun nativeOnTouch(action: Int, x: Float, y: Float)
    @JvmStatic external fun nativeOnKey(action: Int, keyCode: Int, shift: Boolean, ctrl: Boolean, alt: Boolean)
    @JvmStatic external fun nativeOnImeComposition(text: String, cursorByte: Int)
    @JvmStatic external fun nativeOnImeCommit(text: String)
    @JvmStatic external fun nativeShutdown()
}
