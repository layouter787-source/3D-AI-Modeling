package com.layouter.aimodeling

import android.content.Context
import android.opengl.GLSurfaceView
import android.view.MotionEvent
import android.view.ScaleGestureDetector
import android.view.ViewConfiguration
import android.widget.Toast
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

    @Volatile private var vertexPaintEnabled = false
    @Volatile private var vertexBrushRadius = 0.22f
    @Volatile private var vertexBrushOpacity = 0.85f
    @Volatile private var vertexBrushR = 0.20f
    @Volatile private var vertexBrushG = 0.75f
    @Volatile private var vertexBrushB = 0.38f
    private var painting = false
    private var paintObjectId = -1
    private var lastPaintX = -1f
    private var lastPaintY = -1f

    fun setVertexPaintMode(
        enabled: Boolean,
        radius: Float = vertexBrushRadius,
        opacity: Float = vertexBrushOpacity,
        red: Float = vertexBrushR,
        green: Float = vertexBrushG,
        blue: Float = vertexBrushB
    ) {
        vertexPaintEnabled = enabled
        vertexBrushRadius = radius.coerceIn(0.03f, 2.0f)
        vertexBrushOpacity = opacity.coerceIn(0f, 1f)
        vertexBrushR = red.coerceIn(0f, 1f)
        vertexBrushG = green.coerceIn(0f, 1f)
        vertexBrushB = blue.coerceIn(0f, 1f)
        if (!enabled && painting) {
            painting = false
            paintObjectId = -1
            queueEvent { NativeBridge.endVertexPaintStroke() }
            onSceneChanged?.invoke()
        }
    }

    private fun paintAt(x: Float, y: Float) {
        val id = paintObjectId
        val radius = vertexBrushRadius
        val red = vertexBrushR
        val green = vertexBrushG
        val blue = vertexBrushB
        val opacity = vertexBrushOpacity
        queueEvent {
            NativeBridge.paintVertexAt(id, x, y, radius, red, green, blue, opacity)
        }
    }

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
        if (vertexPaintEnabled) {
            when (event.actionMasked) {
                MotionEvent.ACTION_DOWN -> {
                    val selectedId = NativeBridge.getSelected()
                    if (selectedId < 0) {
                        Toast.makeText(context, "Seleciona um objecto antes de pintar", Toast.LENGTH_SHORT).show()
                        return true
                    }
                    paintObjectId = selectedId
                    painting = true
                    lastPaintX = event.x
                    lastPaintY = event.y
                    val id = paintObjectId
                    val x = event.x
                    val y = event.y
                    val radius = vertexBrushRadius
                    val red = vertexBrushR
                    val green = vertexBrushG
                    val blue = vertexBrushB
                    val opacity = vertexBrushOpacity
                    queueEvent {
                        if (NativeBridge.beginVertexPaintStroke(id)) {
                            NativeBridge.paintVertexAt(id, x, y, radius, red, green, blue, opacity)
                        }
                    }
                    return true
                }
                MotionEvent.ACTION_MOVE -> {
                    if (painting && event.pointerCount == 1) {
                        val dx = event.x - lastPaintX
                        val dy = event.y - lastPaintY
                        if (hypot(dx, dy) >= 3.5f) {
                            lastPaintX = event.x
                            lastPaintY = event.y
                            paintAt(event.x, event.y)
                        }
                    }
                    return true
                }
                MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL, MotionEvent.ACTION_POINTER_DOWN -> {
                    if (painting) {
                        painting = false
                        paintObjectId = -1
                        queueEvent { NativeBridge.endVertexPaintStroke() }
                        onSceneChanged?.invoke()
                    }
                    return true
                }
                MotionEvent.ACTION_POINTER_UP -> return true
            }
            return true
        }

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
