package com.layouter.aimodeling

import android.content.Context
import android.opengl.GLSurfaceView
import android.view.MotionEvent
import android.view.ScaleGestureDetector
import android.view.ViewConfiguration
import kotlin.math.hypot

/**
 * Viewport 3D:
 * - toque rápido: seleciona o objeto (ou limpa a seleção)
 * - 1 dedo a arrastar: roda a câmera
 * - 2 dedos: desloca (pan)
 * - pinça: aproxima/afasta
 */
class ModelingSurfaceView(context: Context) : GLSurfaceView(context) {

    /** Chamado na thread da UI quando a seleção muda por toque. */
    var onSceneChanged: (() -> Unit)? = null

    private val touchSlop = ViewConfiguration.get(context).scaledTouchSlop

    private val scaleDetector = ScaleGestureDetector(
        context,
        object : ScaleGestureDetector.SimpleOnScaleGestureListener() {
            override fun onScale(detector: ScaleGestureDetector): Boolean {
                val factor = detector.scaleFactor
                queueEvent { NativeBridge.onZoom(factor) }
                return true
            }
        }
    )

    private var lastX = 0f
    private var lastY = 0f
    private var downX = 0f
    private var downY = 0f
    private var downTime = 0L
    private var moved = false
    private var multiTouch = false

    init {
        setEGLContextClientVersion(3)
        setRenderer(ViewportRenderer())
        renderMode = RENDERMODE_CONTINUOUSLY
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        scaleDetector.onTouchEvent(event)

        // Centro dos dedos, sem contar o dedo que acabou de levantar.
        val skip = if (event.actionMasked == MotionEvent.ACTION_POINTER_UP) event.actionIndex else -1
        var cx = 0f
        var cy = 0f
        var n = 0
        for (i in 0 until event.pointerCount) {
            if (i == skip) continue
            cx += event.getX(i)
            cy += event.getY(i)
            n++
        }
        if (n > 0) {
            cx /= n
            cy /= n
        }

        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                downX = event.x
                downY = event.y
                downTime = event.eventTime
                moved = false
                multiTouch = false
                lastX = cx
                lastY = cy
            }
            MotionEvent.ACTION_POINTER_DOWN -> {
                multiTouch = true
                lastX = cx
                lastY = cy
            }
            MotionEvent.ACTION_POINTER_UP -> {
                lastX = cx
                lastY = cy
            }
            MotionEvent.ACTION_MOVE -> {
                if (hypot(event.x - downX, event.y - downY) > touchSlop) moved = true
                val dx = cx - lastX
                val dy = cy - lastY
                if (event.pointerCount == 1) {
                    queueEvent { NativeBridge.onOrbit(dx, dy) }
                } else {
                    queueEvent { NativeBridge.onPan(dx, dy) }
                }
                lastX = cx
                lastY = cy
            }
            MotionEvent.ACTION_UP -> {
                if (!multiTouch && !moved && event.eventTime - downTime < 300) {
                    NativeBridge.pickAt(event.x, event.y)
                    onSceneChanged?.invoke()
                }
            }
        }
        return true
    }
}
