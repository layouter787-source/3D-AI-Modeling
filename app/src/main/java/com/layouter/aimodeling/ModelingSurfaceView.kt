package com.layouter.aimodeling

import android.content.Context
import android.opengl.GLSurfaceView
import android.view.MotionEvent
import android.view.ScaleGestureDetector

/**
 * Viewport 3D com câmera orbital:
 * - 1 dedo: rodar
 * - 2 dedos: deslocar (pan)
 * - pinça: aproximar/afastar
 */
class ModelingSurfaceView(context: Context) : GLSurfaceView(context) {

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
            MotionEvent.ACTION_DOWN,
            MotionEvent.ACTION_POINTER_DOWN,
            MotionEvent.ACTION_POINTER_UP -> {
                lastX = cx
                lastY = cy
            }
            MotionEvent.ACTION_MOVE -> {
                val dx = cx - lastX
                val dy = cy - lastY
                if (event.pointerCount == 1) {
                    queueEvent { NativeBridge.onOrbit(dx, dy) }
                } else if (event.pointerCount >= 2) {
                    queueEvent { NativeBridge.onPan(dx, dy) }
                }
                lastX = cx
                lastY = cy
            }
        }
        return true
    }
}
