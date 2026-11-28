package com.layouter.aimodeling

object NativeBridge {
    init {
        System.loadLibrary("aimodeling")
    }

    // Ciclo de vida do GL (thread do GL)
    external fun onSurfaceCreated()
    external fun onSurfaceChanged(width: Int, height: Int)
    external fun onDrawFrame()
    external fun setViewportTheme(dark: Boolean)
    // Ambiente do viewport: direção do sol, cor, intensidade, ambiente e céu.
    external fun setEnvironment(
        sunX: Float, sunY: Float, sunZ: Float,
        sunR: Float, sunG: Float, sunB: Float,
        sunIntensity: Float, ambientStrength: Float,
        skyR: Float, skyG: Float, skyB: Float
    )

    // Câmera
    external fun onOrbit(dx: Float, dy: Float)
    external fun onPan(dx: Float, dy: Float)
    external fun onZoom(factor: Float)

    // Edição da cena (seguras em qualquer thread). Rotações em graus.
    external fun addPrimitive(type: Int, size: Float, x: Float, y: Float, z: Float): Int
    external fun addPrimitiveDetailed(type: Int, size: Float, x: Float, y: Float, z: Float, detail: Int): Int
    external fun createLoftMesh(profiles: FloatArray, segments: Int, x: Float, y: Float, z: Float): Int
    external fun duplicateObject(id: Int, dx: Float, dy: Float, dz: Float): Int
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
    external fun getVertexColors(id: Int): FloatArray
    external fun setVertexColors(id: Int, colors: FloatArray): Boolean
    external fun paintVertexRegion(id: Int, cx: Float, cy: Float, cz: Float, radius: Float, r: Float, g: Float, b: Float, opacity: Float): Boolean
    external fun setMaterial(id: Int, metallic: Float, roughness: Float, clearcoat: Float, emission: Float): Boolean
    external fun sculptMesh(id: Int, centerX: Float, centerY: Float, centerZ: Float, radius: Float, offsetX: Float, offsetY: Float, offsetZ: Float): Boolean
    external fun beginSculptStroke(id: Int): Boolean
    external fun endSculptStroke()
    external fun sculptMeshAtScreen(id: Int, screenX: Float, screenY: Float, radius: Float, brush: Int, strength: Float, strokeX: Float, strokeY: Float): Boolean
    external fun meshOperation(id: Int, operation: String, faceIds: IntArray, parameter: Int, value: Float, otherObjectId: Int = -1): Boolean
    external fun detachFaces(id: Int, faceIds: IntArray): Int
    external fun combineObjects(id: Int, otherObjectId: Int): Boolean
    external fun getFaceShell(id: Int, startFace: Int): IntArray
    external fun getEdgeLoopFaces(id: Int, vertexA: Int, vertexB: Int): IntArray
    external fun beginVertexPaintStroke(id: Int): Boolean
    external fun paintVertexAt(id: Int, x: Float, y: Float, radius: Float, r: Float, g: Float, b: Float, opacity: Float): Boolean
    external fun endVertexPaintStroke()
    external fun getMeshTopology(id: Int, offset: Int, limit: Int): String
    external fun snapToGround(id: Int): Boolean
    external fun snapToGrid(id: Int, step: Float): Boolean

    // Histórico
    external fun undo(): Boolean
    external fun redo(): Boolean
    external fun canUndo(): Boolean
    external fun canRedo(): Boolean
    external fun clearScene()
    external fun clearHistory()

    // Seleção e consulta
    external fun select(id: Int)
    external fun getSelected(): Int
    external fun pickAt(x: Float, y: Float): Int
    external fun describeScene(): String

    external fun exportGlb(id: Int, path: String): Boolean
    external fun importGlb(path: String): Int
    external fun pickAtComponent(x: Float, y: Float, mode: String, objectId: Int): String
    external fun setDisplayMode(mode: String): Boolean

    private var selectionMode: String = "object"
    fun setSelectMode(mode: String): Boolean {
        if (mode !in setOf("object", "vertex", "edge", "face")) return false
        selectionMode = mode
        return true
    }
    fun getSelectMode(): String = selectionMode

    fun cloneObject(id: Int): Int = duplicateObject(id, 0.15f, 0.0f, 0.0f)
    fun meshOp(id: Int, operation: String, amount: Float, segments: Int, faceIndices: IntArray?): Boolean =
        meshOperation(id, operation, faceIndices ?: intArrayOf(), segments, amount, -1)
    fun separateFaces(id: Int, faceIndices: IntArray?): Int = detachFaces(id, faceIndices ?: intArrayOf())
    fun setPbr(id: Int, r: Float, g: Float, b: Float, metallic: Float, roughness: Float): Boolean =
        setColor(id, r, g, b) && setMaterial(id, metallic, roughness, 0.0f, 0.0f)

    external fun exportObj(): String
    external fun exportMtl(): String
}
