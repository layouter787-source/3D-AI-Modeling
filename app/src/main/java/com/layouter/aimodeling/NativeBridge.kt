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
    /** PBR: base color + metallic + roughness */
    external fun setPbr(id: Int, r: Float, g: Float, b: Float, metallic: Float, roughness: Float): Boolean

    external fun setDisplayMode(mode: String): Boolean

    external fun meshOp(id: Int, op: String, amount: Float, segments: Int, faceIndices: IntArray?): Boolean
    external fun combineObjects(idA: Int, idB: Int): Int
    external fun separateFaces(id: Int, faceIndices: IntArray?): Int
    external fun setSelectMode(mode: String): Boolean
    external fun setSelectedFaces(faceIndices: IntArray?): Boolean

    /** mode: vertex|edge|face|object — returns JSON pick result */
    external fun pickAtComponent(x: Float, y: Float, mode: String, objectId: Int): String

    external fun exportGlb(id: Int, path: String): Boolean
    external fun importGlb(path: String): Int

    external fun undo(): Boolean
    external fun redo(): Boolean
    external fun canUndo(): Boolean
    external fun canRedo(): Boolean

    external fun select(id: Int)
    external fun getSelected(): Int
    external fun pickAt(x: Float, y: Float): Int
    external fun describeScene(): String
}
