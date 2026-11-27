package com.layouter.aimodeling

object NativeBridge {
    init {
        System.loadLibrary("aimodeling")
    }

    // Ciclo de vida do GL (thread do GL)
    external fun onSurfaceCreated()
    external fun onSurfaceChanged(width: Int, height: Int)
    external fun onDrawFrame()

    // Câmera
    external fun onOrbit(dx: Float, dy: Float)
    external fun onPan(dx: Float, dy: Float)
    external fun onZoom(factor: Float)

    // Ferramentas da IA: definições (JSON array) e execução. Seguras em qualquer thread.
    external fun toolDefinitions(): String
    external fun callTool(name: String, argsJson: String): String

    // Interface (botões e toques). Rotações em graus.
    external fun addPrimitive(type: Int, size: Float, x: Float, y: Float, z: Float): Int
    external fun deleteObject(id: Int): Boolean
    external fun setTransform(
        id: Int,
        px: Float, py: Float, pz: Float,
        rx: Float, ry: Float, rz: Float,
        sx: Float, sy: Float, sz: Float
    ): Boolean
    external fun moveBy(id: Int, dx: Float, dy: Float, dz: Float): Boolean
    external fun rotateBy(id: Int, rx: Float, ry: Float, rz: Float): Boolean
    external fun scaleBy(id: Int, fx: Float, fy: Float, fz: Float): Boolean
    external fun setColor(id: Int, r: Float, g: Float, b: Float): Boolean

    // Histórico
    external fun undo(): Boolean
    external fun redo(): Boolean
    external fun canUndo(): Boolean
    external fun canRedo(): Boolean

    // Seleção e consulta
    external fun select(id: Int)
    external fun getSelected(): Int
    external fun pickAt(x: Float, y: Float): Int
    external fun describeScene(): String
}
