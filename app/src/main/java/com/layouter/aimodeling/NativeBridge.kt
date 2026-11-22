package com.layouter.aimodeling

object NativeBridge {
    init {
        System.loadLibrary("aimodeling")
    }

    external fun onSurfaceCreated()
    external fun onSurfaceChanged(width: Int, height: Int)
    external fun onDrawFrame()
}
