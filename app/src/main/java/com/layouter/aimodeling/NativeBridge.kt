package com.layouter.aimodeling

object NativeBridge {
    init {
        System.loadLibrary("aimodeling")
    }

    external fun onSurfaceCreated()
    external fun onSurfaceChanged(width: Int, height: Int)
    external fun onDrawFrame()

    // Chamadas sempre na thread do GL (via queueEvent).
    external fun onOrbit(dx: Float, dy: Float)
    external fun onPan(dx: Float, dy: Float)
    external fun onZoom(factor: Float)
}
