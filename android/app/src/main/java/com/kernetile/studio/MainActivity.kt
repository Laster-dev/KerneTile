package com.kernetile.studio

import android.app.Activity
import android.content.Context
import android.graphics.Color
import android.os.Bundle
import android.text.InputType
import android.view.Choreographer
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.inputmethod.BaseInputConnection
import android.view.inputmethod.EditorInfo
import android.view.inputmethod.InputConnection
import android.view.inputmethod.InputMethodManager

class MainActivity : Activity(), SurfaceHolder.Callback, Choreographer.FrameCallback {
    private lateinit var surfaceView: KerneTileSurfaceView
    private var choreographerRunning = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        window.statusBarColor = Color.parseColor("#202020")
        window.navigationBarColor = Color.parseColor("#202020")

        KerneTileBridge.attachActivity(this)
        KerneTileBridge.nativeInit()

        surfaceView = KerneTileSurfaceView(this).apply {
            holder.addCallback(this@MainActivity)
            isFocusable = true
            isFocusableInTouchMode = true
        }
        setContentView(surfaceView)
    }

    override fun onResume() {
        super.onResume()
        startFrameLoop()
    }

    override fun onPause() {
        stopFrameLoop()
        super.onPause()
    }

    override fun onDestroy() {
        stopFrameLoop()
        KerneTileBridge.detachActivity(this)
        if (isFinishing) {
            KerneTileBridge.nativeShutdown()
        }
        super.onDestroy()
    }

    private fun startFrameLoop() {
        if (!choreographerRunning) {
            choreographerRunning = true
            Choreographer.getInstance().postFrameCallback(this)
        }
    }

    private fun stopFrameLoop() {
        if (choreographerRunning) {
            choreographerRunning = false
            Choreographer.getInstance().removeFrameCallback(this)
        }
    }

    override fun doFrame(frameTimeNanos: Long) {
        if (!choreographerRunning) return
        KerneTileBridge.nativeStepFrame()
        Choreographer.getInstance().postFrameCallback(this)
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        val rect = holder.surfaceFrame
        val density = resources.displayMetrics.density
        KerneTileBridge.nativeSurfaceChanged(holder.surface, rect.width(), rect.height(), density)
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        val density = resources.displayMetrics.density
        KerneTileBridge.nativeSurfaceChanged(holder.surface, width, height, density)
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        KerneTileBridge.nativeSurfaceDestroyed()
    }

    fun setSoftKeyboardVisible(visible: Boolean) {
        val imm = getSystemService(Context.INPUT_METHOD_SERVICE) as? InputMethodManager ?: return
        if (visible) {
            surfaceView.post {
                surfaceView.isFocusable = true
                surfaceView.isFocusableInTouchMode = true
                surfaceView.requestFocus()
                imm.restartInput(surfaceView)
                imm.showSoftInput(surfaceView, InputMethodManager.SHOW_IMPLICIT)
            }
        } else {
            surfaceView.post {
                imm.hideSoftInputFromWindow(surfaceView.windowToken, 0)
            }
        }
    }
}

private class KerneTileSurfaceView(context: Context) : SurfaceView(context) {
    override fun onTouchEvent(event: MotionEvent): Boolean {
        val action = event.actionMasked
        if (action == MotionEvent.ACTION_DOWN ||
            action == MotionEvent.ACTION_MOVE ||
            action == MotionEvent.ACTION_UP ||
            action == MotionEvent.ACTION_CANCEL
        ) {
            KerneTileBridge.nativeOnTouch(action, event.x, event.y)
            return true
        }
        return super.onTouchEvent(event)
    }

    override fun onKeyDown(keyCode: Int, event: KeyEvent): Boolean {
        KerneTileBridge.nativeOnKey(
            0, keyCode, event.isShiftPressed, event.isCtrlPressed, event.isAltPressed
        )
        return true
    }

    override fun onKeyUp(keyCode: Int, event: KeyEvent): Boolean {
        KerneTileBridge.nativeOnKey(
            1, keyCode, event.isShiftPressed, event.isCtrlPressed, event.isAltPressed
        )
        return true
    }

    override fun onCheckIsTextEditor(): Boolean = true

    override fun onCreateInputConnection(outAttrs: EditorInfo): InputConnection {
        outAttrs.inputType = InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_VARIATION_NORMAL
        outAttrs.imeOptions = EditorInfo.IME_FLAG_NO_EXTRACT_UI or EditorInfo.IME_ACTION_DONE
        return object : BaseInputConnection(this, false) {
            override fun setComposingText(text: CharSequence?, newCursorPosition: Int): Boolean {
                val s = text?.toString() ?: ""
                KerneTileBridge.nativeOnImeComposition(s, s.toByteArray(Charsets.UTF_8).size)
                return true
            }

            override fun finishComposingText(): Boolean {
                KerneTileBridge.nativeOnImeComposition("", 0)
                return true
            }

            override fun commitText(text: CharSequence?, newCursorPosition: Int): Boolean {
                val s = text?.toString() ?: ""
                if (s.isNotEmpty()) {
                    KerneTileBridge.nativeOnImeCommit(s)
                }
                KerneTileBridge.nativeOnImeComposition("", 0)
                return true
            }

            override fun performEditorAction(editorAction: Int): Boolean {
                KerneTileBridge.nativeOnKey(0, KeyEvent.KEYCODE_ENTER, false, false, false)
                KerneTileBridge.nativeOnKey(1, KeyEvent.KEYCODE_ENTER, false, false, false)
                return true
            }

            override fun deleteSurroundingText(beforeLength: Int, afterLength: Int): Boolean {
                repeat(beforeLength.coerceAtLeast(0)) {
                    KerneTileBridge.nativeOnKey(0, KeyEvent.KEYCODE_DEL, false, false, false)
                    KerneTileBridge.nativeOnKey(1, KeyEvent.KEYCODE_DEL, false, false, false)
                }
                return true
            }

            override fun sendKeyEvent(event: KeyEvent): Boolean {
                val act = if (event.action == KeyEvent.ACTION_DOWN) 0 else 1
                KerneTileBridge.nativeOnKey(
                    act, event.keyCode, event.isShiftPressed, event.isCtrlPressed, event.isAltPressed
                )
                return true
            }
        }
    }
}
