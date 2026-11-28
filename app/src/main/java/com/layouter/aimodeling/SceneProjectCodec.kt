package com.layouter.aimodeling

import org.json.JSONArray
import org.json.JSONObject

/**
 * Serialização de projetos editáveis. O ficheiro guarda os parâmetros que geram as malhas
 * nativas; não é uma exportação de geometria triangulada como OBJ.
 */
object SceneProjectCodec {
    const val MAX_PROJECT_BYTES = 2 * 1024 * 1024
    private const val FORMAT = "3d-ai-modeling-project"
    private const val VERSION = 1
    private const val MAX_OBJECTS = 500

    private data class StoredObject(
        val sourceId: Int,
        val type: PrimitiveType,
        val size: Float,
        val detail: Int,
        val smoothAngle: Float,
        val position: FloatArray,
        val rotation: FloatArray,
        val scale: FloatArray,
        val color: FloatArray,
        val metallic: Float,
        val roughness: Float,
        val clearcoat: Float,
        val emission: Float
    )

    private data class StoredEnvironment(
        val sunDirection: FloatArray,
        val sunColor: FloatArray,
        val sunIntensity: Float,
        val ambientStrength: Float,
        val skyColor: FloatArray
    )

    fun encode(): String {
        val scene = JSONObject(NativeBridge.describeScene())
        return JSONObject()
            .put("format", FORMAT)
            .put("version", VERSION)
            .put("savedAtEpochMs", System.currentTimeMillis())
            .put("objects", scene.getJSONArray("objects"))
            .put("environment", scene.optJSONObject("environment"))
            .put("selected", scene.optInt("selected", -1))
            .toString(2)
    }

    /**
     * Valida todo o ficheiro antes de alterar a cena. Se a reconstrução nativa falhar,
     * limpa a cena parcial em vez de deixar um projeto incompleto no editor.
     */
    fun load(contents: String): Int {
        require(contents.toByteArray(Charsets.UTF_8).size <= MAX_PROJECT_BYTES) {
            "Projeto demasiado grande (máximo 2 MB)"
        }

        val root = JSONObject(contents)
        require(root.optString("format") == FORMAT) { "Formato de projeto não reconhecido" }
        require(root.optInt("version", -1) == VERSION) { "Versão de projeto não suportada" }

        val array = root.optJSONArray("objects")
            ?: throw IllegalArgumentException("A lista de objetos está em falta")
        require(array.length() <= MAX_OBJECTS) { "O projeto excede o limite de 500 objetos" }

        val parsed = ArrayList<StoredObject>(array.length())
        val seenIds = HashSet<Int>()
        for (index in 0 until array.length()) {
            val item = array.getJSONObject(index)
            val sourceId = item.getInt("id")
            require(sourceId > 0 && seenIds.add(sourceId)) { "ID de objeto inválido ou repetido" }

            val type = PrimitiveType.fromApiName(item.getString("type"))
                ?: throw IllegalArgumentException("Tipo de objeto não suportado")
            val size = boundedNumber(item, "size", 0.01, 100.0)
            val detail = item.optInt("detail", 8).also { require(it in 1..64) { "Resolução de malha inválida" } }
            val defaultSmoothAngle = when (type) {
                PrimitiveType.SPHERE, PrimitiveType.TORUS, PrimitiveType.CAPSULE, PrimitiveType.ICOSPHERE -> 180.0
                else -> 35.0
            }
            val smoothAngle = item.optDouble("smooth_angle", defaultSmoothAngle).also {
                require(it.isFinite() && it in 0.0..180.0) { "Ângulo de normais inválido" }
            }.toFloat()
            val position = vector(item, "position", -10000.0, 10000.0)
            val rotation = vector(item, "rotation", -100000.0, 100000.0)
            val scale = vector(item, "scale", 0.001, 1000.0)
            val color = vector(item, "color", 0.0, 1.0)
            val material = item.optJSONObject("material")
            val metallic = material?.let { boundedNumber(it, "metallic", 0.0, 1.0) } ?: 0f
            val roughness = material?.let { boundedNumber(it, "roughness", 0.04, 1.0) } ?: 0.48f
            val clearcoat = material?.let { boundedNumber(it, "clearcoat", 0.0, 1.0) } ?: 0f
            val emission = material?.let { boundedNumber(it, "emission", 0.0, 8.0) } ?: 0f

            parsed.add(
                StoredObject(sourceId, type, size, detail, smoothAngle, position, rotation, scale, color, metallic, roughness, clearcoat, emission)
            )
        }

        val selectedSourceId = root.optInt("selected", -1)
        val environment = root.optJSONObject("environment")?.let { item ->
            StoredEnvironment(
                vector(item, "sun_direction", -1.0, 1.0),
                vector(item, "sun_color", 0.0, 1.0),
                boundedNumber(item, "sun_intensity", 0.0, 8.0),
                boundedNumber(item, "ambient_strength", 0.0, 1.0),
                vector(item, "sky_color", 0.0, 1.0)
            )
        }
        NativeBridge.clearScene()
        return try {
            val remappedIds = HashMap<Int, Int>(parsed.size)
            for (item in parsed) {
                val id = NativeBridge.addPrimitiveDetailed(
                    item.type.id,
                    item.size,
                    item.position[0],
                    item.position[1],
                    item.position[2],
                    item.detail
                )
                check(id >= 0) { "O motor recusou um objeto do projeto" }
                check(NativeBridge.meshOperation(id, "normal_angle", intArrayOf(), 1, item.smoothAngle)) {
                    "Não foi possível restaurar o modo de normais"
                }

                check(
                    NativeBridge.setTransform(
                        id,
                        item.position[0], item.position[1], item.position[2],
                        item.rotation[0], item.rotation[1], item.rotation[2],
                        item.scale[0], item.scale[1], item.scale[2]
                    )
                ) { "Não foi possível restaurar a transformação do objeto" }

                check(NativeBridge.setColor(id, item.color[0], item.color[1], item.color[2])) {
                    "Não foi possível restaurar a cor do objeto"
                }
                check(NativeBridge.setMaterial(id, item.metallic, item.roughness, item.clearcoat, item.emission)) {
                    "Não foi possível restaurar o material do objeto"
                }
                remappedIds[item.sourceId] = id
            }

            environment?.let {
                NativeBridge.setEnvironment(
                    it.sunDirection[0], it.sunDirection[1], it.sunDirection[2],
                    it.sunColor[0], it.sunColor[1], it.sunColor[2],
                    it.sunIntensity, it.ambientStrength,
                    it.skyColor[0], it.skyColor[1], it.skyColor[2]
                )
            }

            NativeBridge.select(remappedIds[selectedSourceId] ?: -1)
            NativeBridge.clearHistory()
            parsed.size
        } catch (error: Exception) {
            NativeBridge.clearScene()
            throw IllegalStateException(
                "Não foi possível reconstruir o projeto: " + (error.message ?: "erro interno"),
                error
            )
        }
    }

    private fun boundedNumber(item: JSONObject, key: String, min: Double, max: Double): Float {
        val value = item.getDouble(key)
        require(value.isFinite() && value in min..max) {
            "Valor inválido em " + key
        }
        return value.toFloat()
    }

    private fun vector(item: JSONObject, key: String, min: Double, max: Double): FloatArray {
        val values: JSONArray = item.getJSONArray(key)
        require(values.length() == 3) { key + " deve conter exatamente 3 valores" }
        return FloatArray(3) { index ->
            val value = values.getDouble(index)
            require(value.isFinite() && value in min..max) {
                "Vetor inválido em " + key
            }
            value.toFloat()
        }
    }
}
