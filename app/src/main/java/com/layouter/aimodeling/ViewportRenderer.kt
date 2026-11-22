package com.layouter.aimodeling

import android.opengl.GLSurfaceView
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10

class ViewportRenderer : GLSurfaceView.Renderer {
    override fun onSurfaceCreated(gl: GL10?, config: EGLConfig?) {
        NativeBridge.onSurfaceCreated()
    }

    override fun onSurfaceChanged(gl: GL10?, width: Int, height: Int) {
        NativeBridge.onSurfaceChanged(width, height)
    }

    override fun onDrawFrame(gl: GL10?) {
        NativeBridge.onDrawFrame()
    }
}
