package com.layouter.aimodeling

object NativeBridge {
    init {
        System.loadLibrary("aimodeling")
    }

    external fun onSurfaceCreated()
    external fun onSurfaceChanged(width: Int, height: Int)
    external fun onDrawFrame()

    external fun onOrbit(dx: Float, dy: Float)
    external fun onPan(dx: Float, dy: Float)
    external fun onZoom(factor: Float)

    external fun addPrimitive(type: Int, size: Float, x: Float, y: Float, z: Float): Int
    external fun cloneObject(id: Int): Int
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

    /** mode: shaded | wireframe | wireframe_on_shaded */
    external fun setDisplayMode(mode: String): Boolean

    /**
     * Mesh op: soft_normals, hard_normals, divide, smooth, weld, mirror,
     * extrude, bevel. amount/segments depend on op. faceIndicesCsv = "0,1,2" or empty.
     */
    external fun meshOperation(
        id: Int,
        op: String,
        amount: Float,
        segments: Int,
        faceIndicesCsv: String
    ): Boolean

    external fun combineObjects(idA: Int, idB: Int): Int
    external fun separateFaces(id: Int, faceIndicesCsv: String): Int

    external fun undo(): Boolean
    external fun redo(): Boolean
    external fun canUndo(): Boolean
    external fun canRedo(): Boolean

    external fun select(id: Int)
    external fun getSelected(): Int
    external fun pickAt(x: Float, y: Float): Int
    external fun describeScene(): String
}
