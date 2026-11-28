package com.layouter.aimodeling

import android.content.Intent
import android.net.Uri
import android.widget.Toast
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.systemBarsPadding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.Add
import androidx.compose.material.icons.outlined.ArrowBack
import androidx.compose.material.icons.outlined.ArrowDownward
import androidx.compose.material.icons.outlined.ArrowForward
import androidx.compose.material.icons.outlined.ArrowUpward
import androidx.compose.material.icons.outlined.AutoAwesome
import androidx.compose.material.icons.outlined.DeleteOutline
import androidx.compose.material.icons.outlined.DarkMode
import androidx.compose.material.icons.outlined.LightMode
import androidx.compose.material.icons.outlined.ContentCopy
import androidx.compose.material.icons.outlined.FileDownload
import androidx.compose.material.icons.outlined.FolderOpen
import androidx.compose.material.icons.outlined.Save
import androidx.compose.material.icons.outlined.Palette
import androidx.compose.material.icons.outlined.Redo
import androidx.compose.material.icons.outlined.RotateLeft
import androidx.compose.material.icons.outlined.RotateRight
import androidx.compose.material.icons.outlined.SmartToy
import androidx.compose.material.icons.outlined.Undo
import androidx.compose.material.icons.outlined.ZoomIn
import androidx.compose.material.icons.outlined.ZoomOut
import androidx.compose.material.icons.outlined.Build
import androidx.compose.material.icons.outlined.Extension
import androidx.compose.material.icons.outlined.WbSunny
import androidx.compose.material.icons.outlined.Tune
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.TextButton
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.Text
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Slider
import androidx.compose.runtime.Composable
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import java.io.ByteArrayOutputStream
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

private const val MOVE_STEP = 0.25f

private val palette = listOf(
    Triple(0.72f, 0.78f, 0.82f),
    Triple(0.78f, 0.56f, 0.42f),
    Triple(0.48f, 0.70f, 0.62f),
    Triple(0.70f, 0.62f, 0.78f),
    Triple(0.92f, 0.90f, 0.84f),
)

@Composable
fun EditorScreen(isDarkTheme: Boolean = true, onToggleTheme: () -> Unit = {}) {
    val revision = remember { mutableIntStateOf(0) }
    val rev = revision.intValue
    val refresh: () -> Unit = { revision.intValue++ }
    var showAi by remember { mutableStateOf(false) }
    var showMeshTools by remember { mutableStateOf(false) }
    var showExtensions by remember { mutableStateOf(false) }
    var showLighting by remember { mutableStateOf(false) }
    var showPrimitiveDialog by remember { mutableStateOf(false) }
    var showMaterialDialog by remember { mutableStateOf(false) }
    var vertexPaintMode by remember { mutableStateOf(false) }
    var vertexBrushSize by remember { mutableStateOf(0.22f) }
    var vertexBrushOpacity by remember { mutableStateOf(0.85f) }
    var vertexPaintColorIndex by remember { mutableIntStateOf(2) }
    var faceIndex by remember { mutableStateOf("0") }
    var objExport by remember { mutableStateOf("") }
    var projectExport by remember { mutableStateOf("") }
    var mtlExport by remember { mutableStateOf("") }
    val context = androidx.compose.ui.platform.LocalContext.current
    val scope = androidx.compose.runtime.rememberCoroutineScope()
    val mtlExportLauncher = rememberLauncherForActivityResult(
        ActivityResultContracts.CreateDocument("text/plain")
    ) { uri ->
        if (uri != null && mtlExport.isNotBlank()) {
            scope.launch {
                val saved = withContext(Dispatchers.IO) {
                    runCatching {
                        context.contentResolver.openOutputStream(uri)?.bufferedWriter()?.use {
                            it.write(mtlExport)
                        } ?: error("Could not open MTL file")
                    }
                }
                Toast.makeText(
                    context,
                    if (saved.isSuccess) "Materiais MTL exportados" else "Falha ao exportar MTL",
                    Toast.LENGTH_LONG
                ).show()
            }
        }
    }

    val exportLauncher = rememberLauncherForActivityResult(
        ActivityResultContracts.CreateDocument("text/plain")
    ) { uri ->
        if (uri != null && objExport.isNotBlank()) {
            scope.launch {
                val saved = withContext(Dispatchers.IO) {
                    runCatching {
                        context.contentResolver.openOutputStream(uri)?.bufferedWriter()?.use { it.write(objExport) }
                            ?: error("Could not open file")
                    }
                }
                if (saved.isSuccess) {
                    mtlExport = NativeBridge.exportMtl()
                    if (mtlExport.isNotBlank()) {
                        Toast.makeText(
                            context,
                            "OBJ guardado. Guarda o MTL na mesma pasta para manter as cores.",
                            Toast.LENGTH_LONG
                        ).show()
                        mtlExportLauncher.launch("3d-model.mtl")
                    } else {
                        Toast.makeText(context, "OBJ guardado; não foi possível gerar MTL", Toast.LENGTH_LONG).show()
                    }
                } else {
                    Toast.makeText(context, "OBJ export failed", Toast.LENGTH_LONG).show()
                }
            }
        }
    }

    val projectSaveLauncher = rememberLauncherForActivityResult(
        ActivityResultContracts.CreateDocument("application/json")
    ) { uri ->
        if (uri != null && projectExport.isNotBlank()) {
            scope.launch {
                val saved = withContext(Dispatchers.IO) {
                    runCatching {
                        context.contentResolver.openOutputStream(uri)?.bufferedWriter(Charsets.UTF_8)?.use {
                            it.write(projectExport)
                        } ?: error("Could not open project file")
                    }
                }
                Toast.makeText(
                    context,
                    if (saved.isSuccess) "Projeto guardado" else "Falha ao guardar o projeto",
                    Toast.LENGTH_LONG
                ).show()
            }
        }
    }

    val projectOpenLauncher = rememberLauncherForActivityResult(
        ActivityResultContracts.OpenDocument()
    ) { uri ->
        if (uri != null) {
            scope.launch {
                val opened = try {
                    val projectText = withContext(Dispatchers.IO) {
                        context.contentResolver.openInputStream(uri)?.use { input ->
                            val output = ByteArrayOutputStream()
                            val buffer = ByteArray(8192)
                            var total = 0
                            while (true) {
                                val count = input.read(buffer)
                                if (count < 0) break
                                total += count
                                require(total <= SceneProjectCodec.MAX_PROJECT_BYTES) {
                                    "O ficheiro excede o limite de 2 MB"
                                }
                                output.write(buffer, 0, count)
                            }
                            String(output.toByteArray(), Charsets.UTF_8)
                        } ?: error("Não foi possível abrir o ficheiro")
                    }
                    withContext(Dispatchers.Default) { SceneProjectCodec.load(projectText) }
                } catch (error: Exception) {
                    Toast.makeText(
                        context,
                        "Falha ao abrir: " + (error.message ?: "ficheiro inválido"),
                        Toast.LENGTH_LONG
                    ).show()
                    null
                }

                if (opened != null) {
                    refresh()
                    Toast.makeText(context, "Projeto aberto: " + opened + " objetos", Toast.LENGTH_LONG).show()
                }
            }
        }
    }

    val selected = remember(rev) { NativeBridge.getSelected() }
    val canUndo = remember(rev) { NativeBridge.canUndo() }
    val canRedo = remember(rev) { NativeBridge.canRedo() }

    Box(Modifier.fillMaxSize()) {
        AndroidView(
            modifier = Modifier.fillMaxSize(),
            factory = { viewContext ->
                ModelingSurfaceView(viewContext).apply {
                    onSceneChanged = refresh
                    val initialColor = palette[vertexPaintColorIndex]
                    setVertexPaintMode(vertexPaintMode, vertexBrushSize, vertexBrushOpacity, initialColor.first, initialColor.second, initialColor.third)
                }
            },
            update = { view ->
                view.onSceneChanged = refresh
                val selectedColor = palette.getOrElse(vertexPaintColorIndex) { palette[2] }
                view.setVertexPaintMode(vertexPaintMode, vertexBrushSize, vertexBrushOpacity, selectedColor.first, selectedColor.second, selectedColor.third)
            }
        )

        Row(
            modifier = Modifier.align(Alignment.TopCenter).fillMaxWidth().horizontalScroll(rememberScrollState()).systemBarsPadding().padding(8.dp)
                .background(if (isDarkTheme) Color(0xE9000000) else Color(0xE9FFFFFF), androidx.compose.foundation.shape.RoundedCornerShape(20.dp))
                .padding(horizontal = 8.dp, vertical = 4.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(2.dp)
        ) {
            Text("STUDIO", color = if (isDarkTheme) Color(0xFF60A5FA) else Color(0xFF2563EB), style = MaterialTheme.typography.labelMedium, modifier = Modifier.padding(start = 8.dp, end = 4.dp))
            Text("/ 3D AI", color = MaterialTheme.colorScheme.onSurfaceVariant, style = MaterialTheme.typography.labelSmall, modifier = Modifier.padding(end = 5.dp))
            IconButton(onClick = onToggleTheme) { Icon(if (isDarkTheme) Icons.Outlined.LightMode else Icons.Outlined.DarkMode, if (isDarkTheme) "Tema claro" else "Tema escuro", tint = if (isDarkTheme) Color(0xFF60A5FA) else Color(0xFF2563EB)) }
            IconButton(onClick = { NativeBridge.undo(); refresh() }, enabled = canUndo) {
                Icon(Icons.Outlined.Undo, "Desfazer", tint = if (canUndo) MaterialTheme.colorScheme.onSurface else MaterialTheme.colorScheme.onSurfaceVariant)
            }
            IconButton(onClick = { NativeBridge.redo(); refresh() }, enabled = canRedo) {
                Icon(Icons.Outlined.Redo, "Refazer", tint = if (canRedo) MaterialTheme.colorScheme.onSurface else MaterialTheme.colorScheme.onSurfaceVariant)
            }
            Box(Modifier.size(1.dp, 25.dp).background(if (isDarkTheme) Color(0xFF454B54) else Color(0xFFD1D5DB)))
            IconButton(onClick = {
                projectExport = SceneProjectCodec.encode()
                projectSaveLauncher.launch("untitled.3dproj")
            }) {
                Icon(Icons.Outlined.Save, "Guardar projeto", tint = MaterialTheme.colorScheme.onSurfaceVariant)
            }
            IconButton(onClick = {
                projectOpenLauncher.launch(arrayOf("*/*"))
            }) {
                Icon(Icons.Outlined.FolderOpen, "Abrir projeto", tint = Color(0xFFBFC7D1))
            }
            IconButton(onClick = {
                objExport = NativeBridge.exportObj()
                if (objExport.isBlank()) Toast.makeText(context, "Scene is empty", Toast.LENGTH_SHORT).show()
                else exportLauncher.launch("3d-model.obj")
            }) {
                Icon(Icons.Outlined.FileDownload, "Export OBJ", tint = Color(0xFFBFC7D1))
            }
            IconButton(onClick = { showAi = !showAi }) {
                Icon(Icons.Outlined.AutoAwesome, "Assistente IA", tint = MaterialTheme.colorScheme.primary)
            }
            IconButton(onClick = { showMeshTools = true }) {
                Icon(Icons.Outlined.Tune, "Ferramentas de malha", tint = MaterialTheme.colorScheme.primary)
            }
            IconButton(onClick = { showLighting = true }) {
                Icon(Icons.Outlined.WbSunny, "Luz, sol e céu", tint = Color(0xFFFFC857))
            }
            IconButton(onClick = { showExtensions = true }) {
                Icon(Icons.Outlined.Extension, "Extensões", tint = MaterialTheme.colorScheme.onSurface)
            }
        }

        if (showMeshTools) MeshWorkbenchDialog(selected, faceIndex, { faceIndex = it }, { showMeshTools = false }, refresh)
        if (showLighting) LightingDialog { showLighting = false }
        if (showExtensions) ExtensionLibraryDialog { showExtensions = false }
        if (showMaterialDialog) MaterialWorkbenchDialog(selected, onClose = { showMaterialDialog = false }, refresh = refresh)
        if (showPrimitiveDialog) AdvancedPrimitiveDialog(onClose = { showPrimitiveDialog = false }) { type, size, detail ->
            val y = if (type == PrimitiveType.PLANE) 0f else size / 2f
            val id = NativeBridge.addPrimitiveDetailed(type.id, size, 0f, y, 0f, detail)
            if (id >= 0) { NativeBridge.select(id); refresh() }
            else Toast.makeText(context, "Não foi possível criar esta malha com a resolução escolhida", Toast.LENGTH_LONG).show()
            showPrimitiveDialog = false
        }
        AiPanel(
            onClose = { showAi = false },
            onSceneChanged = refresh,
            modifier = Modifier.align(Alignment.Center).fillMaxSize(),
            visible = showAi
        )
        if (!showAi) {
            Column(
                modifier = Modifier.align(Alignment.BottomCenter).fillMaxWidth(0.96f)
                    .background(Color(0xE91B1E23), androidx.compose.foundation.shape.RoundedCornerShape(22.dp)).systemBarsPadding().padding(10.dp),
                verticalArrangement = Arrangement.spacedBy(7.dp)
            ) {
                if (vertexPaintMode && selected >= 0) {
                    VertexPaintControls(
                        brushSize = vertexBrushSize,
                        onBrushSize = { vertexBrushSize = it },
                        opacity = vertexBrushOpacity,
                        onOpacity = { vertexBrushOpacity = it },
                        selectedColorIndex = vertexPaintColorIndex,
                        onSelectColor = { vertexPaintColorIndex = it },
                        onDone = { vertexPaintMode = false }
                    )
                } else if (selected >= 0) {
                    SelectionTools(selected, refresh, onMaterial = { showMaterialDialog = true }, onVertexPaint = { vertexPaintMode = true })
                }
                PrimitiveBar(refresh) { showPrimitiveDialog = true }
            }
        }
    }
}


@Composable
private fun LightingDialog(onClose: () -> Unit) {
    var elevation by remember { mutableStateOf(48f) }
    var azimuth by remember { mutableStateOf(-35f) }
    var intensity by remember { mutableStateOf(1.2f) }
    var ambient by remember { mutableStateOf(0.18f) }
    var sky by remember { mutableStateOf(0.34f) }

    fun apply() {
        val yaw = Math.toRadians(azimuth.toDouble())
        val pitch = Math.toRadians(elevation.toDouble())
        val direction = floatArrayOf(
            (Math.cos(pitch) * Math.sin(yaw)).toFloat(),
            Math.sin(pitch).toFloat(),
            (Math.cos(pitch) * Math.cos(yaw)).toFloat()
        )
        NativeBridge.setEnvironment(
            direction[0], direction[1], direction[2],
            1.0f, 0.93f, 0.78f, intensity, ambient,
            0.12f + sky * 0.18f, 0.16f + sky * 0.22f, 0.24f + sky * 0.35f
        )
    }

    AlertDialog(onDismissRequest = onClose, title = {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Icon(Icons.Outlined.WbSunny, null, tint = Color(0xFFFFC857), modifier = Modifier.padding(end = 8.dp))
            Text("Luz e ambiente")
        }
    }, text = {
        Column(Modifier.fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(5.dp)) {
            Text("Ajusta a luz principal e o céu do viewport. A IA também pode controlar este ambiente.", style = MaterialTheme.typography.bodySmall)
            Text("Elevação do sol · ${elevation.toInt()}°", style = MaterialTheme.typography.labelMedium)
            Slider(value = elevation, onValueChange = { elevation = it; apply() }, valueRange = 5f..85f)
            Text("Azimute · ${azimuth.toInt()}°", style = MaterialTheme.typography.labelMedium)
            Slider(value = azimuth, onValueChange = { azimuth = it; apply() }, valueRange = -180f..180f)
            Text("Intensidade · ${"%.2f".format(intensity)}", style = MaterialTheme.typography.labelMedium)
            Slider(value = intensity, onValueChange = { intensity = it; apply() }, valueRange = 0f..4f)
            Text("Luz ambiente · ${"%.2f".format(ambient)}", style = MaterialTheme.typography.labelMedium)
            Slider(value = ambient, onValueChange = { ambient = it; apply() }, valueRange = 0f..0.8f)
            Text("Atmosfera do céu · ${"%.2f".format(sky)}", style = MaterialTheme.typography.labelMedium)
            Slider(value = sky, onValueChange = { sky = it; apply() }, valueRange = 0f..1f)
        }
    }, confirmButton = { TextButton(onClick = onClose) { Text("Concluído") } })
}

@Composable
private fun MaterialWorkbenchDialog(id: Int, onClose: () -> Unit, refresh: () -> Unit) {
    var metallic by remember { mutableStateOf(0.0f) }
    var roughness by remember { mutableStateOf(0.55f) }
    var clearcoat by remember { mutableStateOf(0.0f) }
    var emission by remember { mutableStateOf(0.0f) }
    val context = androidx.compose.ui.platform.LocalContext.current

    AlertDialog(
        onDismissRequest = onClose,
        title = { Text("Material · propriedades PBR") },
        text = {
            Column(Modifier.fillMaxWidth().verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text("Material não é uma textura UV: estes controlos alteram as propriedades do shader existente.")
                Text("Metallic · ${(metallic * 100).toInt()}%")
                Slider(value = metallic, onValueChange = { metallic = it }, valueRange = 0f..1f)
                Text("Roughness · ${(roughness * 100).toInt()}%")
                Slider(value = roughness, onValueChange = { roughness = it }, valueRange = 0.02f..1f)
                Text("Clearcoat · ${(clearcoat * 100).toInt()}%")
                Slider(value = clearcoat, onValueChange = { clearcoat = it }, valueRange = 0f..1f)
                Text("Emission · ${"%.2f".format(emission)}")
                Slider(value = emission, onValueChange = { emission = it }, valueRange = 0f..8f)
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    TextButton(onClick = { metallic = 0f; roughness = 0.85f; clearcoat = 0f; emission = 0f }) { Text("Matte") }
                    TextButton(onClick = { metallic = 0.9f; roughness = 0.22f; clearcoat = 0.35f; emission = 0f }) { Text("Metal") }
                    TextButton(onClick = { metallic = 0f; roughness = 0.18f; clearcoat = 0.8f; emission = 0f }) { Text("Glossy") }
                }
            }
        },
        confirmButton = {
            Button(onClick = {
                val ok = NativeBridge.setMaterial(id, metallic, roughness, clearcoat, emission)
                Toast.makeText(context, if (ok) "Propriedades do material aplicadas" else "Falha ao aplicar material", Toast.LENGTH_SHORT).show()
                if (ok) refresh()
                onClose()
            }) { Text("Aplicar") }
        },
        dismissButton = { TextButton(onClick = onClose) { Text("Cancelar") } }
    )
}

@Composable
private fun MeshWorkbenchDialog(selected: Int, faceIndex: String, onFaceIndex: (String) -> Unit, onClose: () -> Unit, onChanged: () -> Unit) {
    val context = androidx.compose.ui.platform.LocalContext.current
    var edgeA by remember { mutableStateOf("0") }
    var edgeB by remember { mutableStateOf("1") }
    var booleanTarget by remember { mutableStateOf("") }
    var vertexIds by remember { mutableStateOf("0,1,2") }
    var topologySummary by remember { mutableStateOf("") }
    var topologyOffset by remember { mutableIntStateOf(0) }
    var totalFaceCount by remember { mutableIntStateOf(0) }
    var topologyFaces by remember { mutableStateOf<List<Pair<Int, String>>>(emptyList()) }
    var selectedFaceIds by remember { mutableStateOf(setOf<Int>()) }

    fun inspectTopology(offset: Int = topologyOffset) {
        if (selected < 0) return
        runCatching {
            val data = org.json.JSONObject(NativeBridge.getMeshTopology(selected, offset.coerceAtLeast(0), 80))
            val faces = data.optJSONArray("faces") ?: org.json.JSONArray()
            topologyOffset = offset.coerceAtLeast(0)
            totalFaceCount = data.optInt("face_count")
            topologyFaces = (0 until faces.length()).map { index ->
                val face = faces.getJSONArray(index)
                (topologyOffset + index) to (0 until face.length()).joinToString(",") { face.getInt(it).toString() }
            }
            topologySummary = "${data.optInt("vertex_count")} vértices · ${totalFaceCount} faces · IDs ${topologyOffset}–${(topologyOffset + topologyFaces.size - 1).coerceAtLeast(topologyOffset)}"
            selectedFaceIds = emptySet()
        }.onFailure {
            topologySummary = "Não foi possível ler a topologia desta malha"
            topologyFaces = emptyList()
        }
    }

    fun parseVertexIds(): IntArray = vertexIds.split(',', ';', ' ')
        .mapNotNull { it.trim().toIntOrNull() }.distinct().toIntArray()

    fun selectedFaceArray(): IntArray =
        selectedFaceIds.toIntArray().takeIf { it.isNotEmpty() } ?: intArrayOf(faceIndex.toIntOrNull() ?: 0)

    fun apply(operation: String, parameter: Int = 1, value: Float = 0.15f, faces: IntArray = intArrayOf(), otherObjectId: Int = -1) {
        if (selected < 0) { Toast.makeText(context, "Seleciona um objeto primeiro", Toast.LENGTH_SHORT).show(); return }
        val ok = NativeBridge.meshOperation(selected, operation, faces, parameter, value, otherObjectId)
        Toast.makeText(context, if (ok) "Malha alterada · Undo disponível" else "Operação falhou: verifica a topologia", Toast.LENGTH_SHORT).show()
        if (ok) onChanged()
    }
    AlertDialog(onDismissRequest = onClose, title = { Text("Mesh Workbench") }, text = {
        Column(Modifier.fillMaxWidth().verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(7.dp)) {
            Text("Ferramentas de geometria nativa. As alterações entram no Undo/Redo.")
            Text("Topologia da malha", style = MaterialTheme.typography.titleSmall)
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(7.dp)) {
                Button(onClick = { inspectTopology() }, enabled = selected >= 0) { Text("Ler faces e vértices") }
                Text(topologySummary, style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
            if (topologyFaces.isNotEmpty()) {
                Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(4.dp)) {
                    TextButton(onClick = { inspectTopology((topologyOffset - 80).coerceAtLeast(0)) }, enabled = topologyOffset > 0) { Text("Anterior") }
                    TextButton(onClick = { inspectTopology(topologyOffset + 80) }, enabled = topologyOffset + topologyFaces.size < totalFaceCount) { Text("Próxima") }
                }
                Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                    TextButton(onClick = { selectedFaceIds = topologyFaces.map { it.first }.toSet() }) { Text("Selecionar página") }
                    TextButton(onClick = { selectedFaceIds = emptySet() }) { Text("Limpar seleção") }
                }
                topologyFaces.chunked(2).forEach { pair ->
                    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                        pair.forEach { (id, vertices) ->
                            val active = id in selectedFaceIds
                            Button(
                                onClick = {
                                    selectedFaceIds = if (active) selectedFaceIds - id else selectedFaceIds + id
                                },
                                modifier = Modifier.weight(1f),
                                contentPadding = PaddingValues(horizontal = 7.dp, vertical = 7.dp),
                                colors = ButtonDefaults.buttonColors(
                                    containerColor = if (active) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.surfaceVariant,
                                    contentColor = if (active) MaterialTheme.colorScheme.onPrimary else MaterialTheme.colorScheme.onSurface
                                )
                            ) {
                                Text("F$id  [$vertices]", style = MaterialTheme.typography.labelSmall, maxLines = 1)
                            }
                        }
                        if (pair.size == 1) androidx.compose.foundation.layout.Spacer(Modifier.weight(1f))
                    }
                }
            }
            Text("Seleção por ID (também podes digitar diretamente)")
            OutlinedTextField(faceIndex, onFaceIndex, label = { Text("Índice da face") }, singleLine = true)
            Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                Button(onClick = {
                    selectedFaceIds = NativeBridge.getFaceShell(selected, faceIndex.toIntOrNull() ?: 0).toSet()
                    if (selectedFaceIds.isEmpty()) Toast.makeText(context, "Não foi encontrada uma concha conectada", Toast.LENGTH_SHORT).show()
                }, enabled = selected >= 0) { Text("Select Shell") }
                Button(onClick = { selectedFaceIds = selectedFaceIds + (faceIndex.toIntOrNull() ?: 0) }) { Text("Adicionar face") }
            }

            Text("Comandos de faces", style = MaterialTheme.typography.titleSmall, color = MaterialTheme.colorScheme.primary)
            Row(horizontalArrangement = Arrangement.spacedBy(5.dp)) {
                Button(onClick = {
                    val newId = NativeBridge.detachFaces(selected, selectedFaceArray())
                    Toast.makeText(context, if (newId >= 0) "Faces separadas no objeto $newId" else "Não foi possível separar as faces", Toast.LENGTH_SHORT).show()
                    if (newId >= 0) onChanged()
                }, enabled = selected >= 0) { Text("Detach") }
                Button(onClick = { apply("clone_faces", value = 0.12f, faces = selectedFaceArray()) }) { Text("Clone faces") }
                Button(onClick = { apply("delete_faces", faces = selectedFaceArray()) }) { Text("Delete faces") }
            }
            Text("Vértices para desenhar uma face ou unir vários pontos", style = MaterialTheme.typography.labelMedium)
            OutlinedTextField(
                value = vertexIds,
                onValueChange = { vertexIds = it.filter { ch -> ch.isDigit() || ch == ',' || ch == ';' || ch.isWhitespace() } },
                label = { Text("IDs de vértices · ex.: 0,1,2,3") },
                singleLine = true,
                modifier = Modifier.fillMaxWidth()
            )
            Row(horizontalArrangement = Arrangement.spacedBy(5.dp)) {
                Button(onClick = { apply("draw_face", faces = parseVertexIds()) }) { Text("Draw Face") }
                Button(onClick = { apply("merge_vertices", faces = parseVertexIds()) }) { Text("Merge") }
                Button(onClick = { apply("target_merge", faces = intArrayOf(edgeA.toIntOrNull() ?: 0, edgeB.toIntOrNull() ?: 1)) }) { Text("Target Merge") }
            }
            Row(horizontalArrangement = Arrangement.spacedBy(5.dp)) {
                Button(onClick = {
                    val ok = NativeBridge.combineObjects(selected, booleanTarget.toIntOrNull() ?: -1)
                    Toast.makeText(context, if (ok) "Objectos combinados" else "Combinação recusada: verifica os IDs", Toast.LENGTH_SHORT).show()
                    if (ok) onChanged()
                }, enabled = selected >= 0) { Text("Combine objects") }
                Button(onClick = { apply("soft_normals") }) { Text("Soft Normals") }
                Button(onClick = { apply("hard_normals") }) { Text("Hard Normals") }
            }
            Text("Cortes e ligações · IDs dos dois vértices da aresta", style = MaterialTheme.typography.labelMedium)
            Row(horizontalArrangement = Arrangement.spacedBy(7.dp)) {
                OutlinedTextField(edgeA, { edgeA = it.filter(Char::isDigit) }, label = { Text("Vértice A") }, modifier = Modifier.weight(1f), singleLine = true)
                OutlinedTextField(edgeB, { edgeB = it.filter(Char::isDigit) }, label = { Text("Vértice B") }, modifier = Modifier.weight(1f), singleLine = true)
            }
            Row(horizontalArrangement = Arrangement.spacedBy(5.dp)) {
                Button(onClick = { apply("loop_cut", parameter = 1, faces = intArrayOf(edgeA.toIntOrNull() ?: 0, edgeB.toIntOrNull() ?: 1)) }) { Text("Loop Cut") }
                Button(onClick = { apply("connect", parameter = 1, faces = intArrayOf(edgeA.toIntOrNull() ?: 0, edgeB.toIntOrNull() ?: 1)) }) { Text("Connect") }
            }
            Text("Boolean · ID do segundo objeto", style = MaterialTheme.typography.labelMedium)
            OutlinedTextField(booleanTarget, { booleanTarget = it.filter(Char::isDigit) }, label = { Text("ID do objeto operand") }, singleLine = true)
            Row(horizontalArrangement = Arrangement.spacedBy(5.dp)) {
                Button(onClick = { apply("boolean_union", otherObjectId = booleanTarget.toIntOrNull() ?: -1) }) { Text("Union") }
                Button(onClick = { apply("boolean_difference", otherObjectId = booleanTarget.toIntOrNull() ?: -1) }) { Text("Difference") }
                Button(onClick = { apply("boolean_intersection", otherObjectId = booleanTarget.toIntOrNull() ?: -1) }) { Text("Intersect") }
            }
            Row(horizontalArrangement = Arrangement.spacedBy(5.dp)) {
                Button(onClick = { apply("extrude", value = 0.2f, faces = selectedFaceArray()) }) { Text("Extrude") }
                Button(onClick = { apply("inset", value = 0.08f, faces = selectedFaceIds.toIntArray().takeIf { it.isNotEmpty() } ?: intArrayOf(faceIndex.toIntOrNull() ?: 0)) }) { Text("Inset") }
                Button(onClick = { apply("bevel", value = 0.08f, faces = selectedFaceIds.toIntArray().takeIf { it.isNotEmpty() } ?: intArrayOf(faceIndex.toIntOrNull() ?: 0)) }) { Text("Bevel") }
            }
            Row(horizontalArrangement = Arrangement.spacedBy(5.dp)) {
                Button(onClick = { apply("subdivide") }) { Text("Subdividir") }
                Button(onClick = { apply("smooth", parameter = 3, value = 0.35f) }) { Text("Suavizar") }
            }
            Row(horizontalArrangement = Arrangement.spacedBy(5.dp)) {
                Button(onClick = { apply("solidify", value = 0.08f) }) { Text("Espessura") }
                Button(onClick = { apply("weld", value = 0.0001f) }) { Text("Weld") }
            }
            Row(horizontalArrangement = Arrangement.spacedBy(5.dp)) {
                Button(onClick = { apply("triangulate") }) { Text("Triangular") }
                Button(onClick = { apply("divide") }) { Text("Divide") }
                Button(onClick = { apply("flip_normals") }) { Text("Inverter normais") }
            }
            Row(horizontalArrangement = Arrangement.spacedBy(5.dp)) {
                Button(onClick = { apply("crease_by_angle", value = 30f) }) { Text("Crease 30°") }
                Button(onClick = { apply("mirror", parameter = 0) }) { Text("Mirror X") }
            }
            Row(horizontalArrangement = Arrangement.spacedBy(5.dp)) {
                Button(onClick = { apply("mirror", parameter = 1) }) { Text("Mirror Y") }
                Button(onClick = { apply("mirror", parameter = 2) }) { Text("Mirror Z") }
            }
            Text("As operações de face, vértice, concha e normal usam a topologia nativa; desfazer/refazer continua disponível.")
        }
    }, confirmButton = { TextButton(onClick = onClose) { Text("Fechar") } })
}

@Composable
private fun ExtensionLibraryDialog(onClose: () -> Unit) {
    val context = androidx.compose.ui.platform.LocalContext.current
    val items = listOf(
        "Terrain3D" to ("Terreno com sculpt, heightmaps, textura e LOD/clipmaps." to "https://github.com/TokisanGames/Terrain3D"),
        "Polyforge" to ("Edit Mode com extrude, inset, bevel, loop cut, mirror, Boolean e seleção." to "https://github.com/Seba313/polyforge"),
        "GoBuild" to ("Edição de mesh por vértice, aresta e face." to "https://github.com/marcel-b-roodt/gobuild"),
        "Blender BMesh" to ("API de topologia para operações de malha." to "https://developer.blender.org/docs/features/objects/mesh/bmesh/"),
        "Materializr CAD" to ("CAD paramétrico e operações de sólidos." to "https://github.com/materializr-cad/materializr"),
        "Strata:3D" to ("Viewer Android e referência para formatos/meshes grandes." to "https://github.com/damzenquis/Strata-3D")
    )
    AlertDialog(onDismissRequest = onClose, title = { Text("Extensões e pesquisa") }, text = {
        Column(Modifier.fillMaxWidth().verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(7.dp)) {
            Text("Addons Godot/Blender não se instalam diretamente neste app nativo. Esta biblioteca reúne fontes para adaptar recursos em Kotlin/C++/OpenGL; integração real exige portar, validar licença e testar.")
            items.forEach { (name, data) ->
                Text(name, color = MaterialTheme.colorScheme.primary, style = MaterialTheme.typography.titleSmall)
                Text(data.first, style = MaterialTheme.typography.bodySmall)
                TextButton(onClick = { runCatching { context.startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(data.second))) } }) { Text("Abrir fonte") }
                androidx.compose.material3.HorizontalDivider()
            }
        }
    }, confirmButton = { TextButton(onClick = onClose) { Text("Fechar") } })
}

@Composable
private fun ToolButton(label: String, onClick: () -> Unit) {
    FilledTonalButton(
        onClick = onClick,
        contentPadding = PaddingValues(horizontal = 13.dp, vertical = 7.dp),
        shape = androidx.compose.foundation.shape.RoundedCornerShape(13.dp),
        colors = ButtonDefaults.filledTonalButtonColors(
            containerColor = MaterialTheme.colorScheme.surfaceVariant,
            contentColor = MaterialTheme.colorScheme.onSurface
        )
    ) {
        Text(label, style = MaterialTheme.typography.labelMedium, maxLines = 1)
    }
}

@Composable
private fun VertexPaintControls(
    brushSize: Float,
    onBrushSize: (Float) -> Unit,
    opacity: Float,
    onOpacity: (Float) -> Unit,
    selectedColorIndex: Int,
    onSelectColor: (Int) -> Unit,
    onDone: () -> Unit
) {
    Column(Modifier.fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(5.dp)) {
        Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.SpaceBetween) {
            Text("VERTEX PAINT", color = MaterialTheme.colorScheme.primary, style = MaterialTheme.typography.labelLarge)
            TextButton(onClick = onDone) { Text("Concluir") }
        }
        Row(Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()), verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            palette.forEachIndexed { index, color ->
                Button(
                    onClick = { onSelectColor(index) },
                    modifier = Modifier.size(34.dp),
                    contentPadding = PaddingValues(0.dp),
                    colors = ButtonDefaults.buttonColors(containerColor = Color(color.first, color.second, color.third), contentColor = Color.Black),
                    shape = androidx.compose.foundation.shape.CircleShape
                ) { if (selectedColorIndex == index) Text("✓", style = MaterialTheme.typography.labelMedium) }
            }
        }
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text("Pincel", style = MaterialTheme.typography.labelSmall, modifier = Modifier.padding(end = 8.dp))
            Slider(value = brushSize, onValueChange = onBrushSize, valueRange = 0.04f..1.25f, modifier = Modifier.weight(1f))
            Text(String.format("%.2f", brushSize), style = MaterialTheme.typography.labelSmall, modifier = Modifier.padding(start = 6.dp))
        }
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text("Opacidade", style = MaterialTheme.typography.labelSmall, modifier = Modifier.padding(end = 8.dp))
            Slider(value = opacity, onValueChange = onOpacity, valueRange = 0.05f..1.0f, modifier = Modifier.weight(1f))
            Text((opacity * 100).toInt().toString() + "%", style = MaterialTheme.typography.labelSmall, modifier = Modifier.padding(start = 6.dp))
        }
        Text("Pintura por vértice · as cores seguem a malha e podem ser desfeitas com Undo.", style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
    }
}
@Composable
private fun SelectionTools(id: Int, refresh: () -> Unit, onMaterial: () -> Unit, onVertexPaint: () -> Unit) {
    val scroll1 = rememberScrollState()
    val scroll2 = rememberScrollState()

    Row(Modifier.horizontalScroll(scroll1), horizontalArrangement = Arrangement.spacedBy(2.dp)) {
        IconButton(onClick = { NativeBridge.moveBy(id, -MOVE_STEP, 0f, 0f); refresh() }) {
            Icon(Icons.Outlined.ArrowBack, "Mover para X negativo", tint = MaterialTheme.colorScheme.onSurface)
        }
        IconButton(onClick = { NativeBridge.moveBy(id, MOVE_STEP, 0f, 0f); refresh() }) {
            Icon(Icons.Outlined.ArrowForward, "Mover para X positivo", tint = MaterialTheme.colorScheme.onSurface)
        }
        IconButton(onClick = { NativeBridge.moveBy(id, 0f, MOVE_STEP, 0f); refresh() }) {
            Icon(Icons.Outlined.ArrowUpward, "Mover para Y positivo", tint = MaterialTheme.colorScheme.onSurface)
        }
        IconButton(onClick = { NativeBridge.moveBy(id, 0f, -MOVE_STEP, 0f); refresh() }) {
            Icon(Icons.Outlined.ArrowDownward, "Mover para Y negativo", tint = MaterialTheme.colorScheme.onSurface)
        }
        ToolButton("Z−") { NativeBridge.moveBy(id, 0f, 0f, -MOVE_STEP); refresh() }
        ToolButton("MOLDAR +") { NativeBridge.sculptMesh(id, 0f, 0f, 0f, 0.85f, 0f, 0.10f, 0f); refresh() }
        ToolButton("MOLDAR −") { NativeBridge.sculptMesh(id, 0f, 0f, 0f, 0.85f, 0f, -0.10f, 0f); refresh() }
        ToolButton("CLONE") {
            val copy = NativeBridge.duplicateObject(id, 0.8f, 0f, 0f)
            if (copy >= 0) NativeBridge.select(copy)
            refresh()
        }
        ToolButton("MATERIAL") { onMaterial() }
        ToolButton("VERTEX PAINT") { onVertexPaint() }
        ToolButton("Z+") { NativeBridge.moveBy(id, 0f, 0f, MOVE_STEP); refresh() }
    }

    Row(Modifier.horizontalScroll(scroll2), horizontalArrangement = Arrangement.spacedBy(2.dp)) {
        IconButton(onClick = { NativeBridge.rotateBy(id, 0f, -15f, 0f); refresh() }) {
            Icon(Icons.Outlined.RotateLeft, "Rodar -15 graus", tint = MaterialTheme.colorScheme.onSurface)
        }
        IconButton(onClick = { NativeBridge.rotateBy(id, 0f, 15f, 0f); refresh() }) {
            Icon(Icons.Outlined.RotateRight, "Rodar +15 graus", tint = MaterialTheme.colorScheme.onSurface)
        }
        IconButton(onClick = { NativeBridge.scaleBy(id, 1.1f, 1.1f, 1.1f); refresh() }) {
            Icon(Icons.Outlined.ZoomIn, "Aumentar", tint = MaterialTheme.colorScheme.onSurface)
        }
        IconButton(onClick = { NativeBridge.scaleBy(id, 0.9f, 0.9f, 0.9f); refresh() }) {
            Icon(Icons.Outlined.ZoomOut, "Diminuir", tint = MaterialTheme.colorScheme.onSurface)
        }
        IconButton(onClick = {
            val duplicate = NativeBridge.duplicateObject(id, 1f, 0f, 0f)
            if (duplicate >= 0) NativeBridge.select(duplicate)
            refresh()
        }) {
            Icon(Icons.Outlined.ContentCopy, "Duplicar objeto", tint = MaterialTheme.colorScheme.onSurface)
        }
        IconButton(onClick = { NativeBridge.deleteObject(id); refresh() }) {
            Icon(Icons.Outlined.DeleteOutline, "Apagar objeto", tint = Color(0xFFFF8D8D))
        }
        Icon(Icons.Outlined.Palette, "Cores", tint = Color(0xFFBFC7D1), modifier = Modifier.align(Alignment.CenterVertically).padding(horizontal = 5.dp))
        palette.forEach { (r, g, b) ->
            Button(
                onClick = { NativeBridge.setColor(id, r, g, b); refresh() },
                colors = ButtonDefaults.buttonColors(containerColor = Color(r, g, b)),
                contentPadding = PaddingValues(0.dp),
                modifier = Modifier.padding(vertical = 5.dp).size(30.dp),
                shape = androidx.compose.foundation.shape.CircleShape
            ) { }
        }
    }

    val context = androidx.compose.ui.platform.LocalContext.current
    fun meshAction(operation: String, parameter: Int = 1, value: Float = 0f) {
        val applied = NativeBridge.meshOperation(id, operation, intArrayOf(), parameter, value, -1)
        if (!applied) Toast.makeText(context, "Operação não aplicada: verifica se a malha suporta esta ação.", Toast.LENGTH_SHORT).show()
        refresh()
    }
    Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(5.dp)) {
        ToolButton("SUBDIVIDIR") { meshAction("subdivide", 1) }
        ToolButton("SUAVIZAR") { meshAction("smooth", 5, 0.28f) }
        ToolButton("BEVEL") { meshAction("bevel", 1, 0.08f) }
        ToolButton("ESPESSAR") { meshAction("solidify", 1, 0.035f) }
        ToolButton("MIRROR X") { meshAction("mirror", 0) }
        ToolButton("TRIANGULAR") { meshAction("triangulate") }
        ToolButton("DIVIDE") { meshAction("divide") }
        ToolButton("NORMAIS") { meshAction("flip_normals") }
        ToolButton("WELD") { meshAction("weld", 1, 0.0001f) }
        ToolButton("CREASE 30°") { meshAction("crease_by_angle", 1, 30f) }
    }
    Row(
        Modifier.horizontalScroll(rememberScrollState()),
        horizontalArrangement = Arrangement.spacedBy(5.dp),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Text("MATERIAL", color = MaterialTheme.colorScheme.primary, style = MaterialTheme.typography.labelSmall)
        ToolButton("METAL") { if (NativeBridge.setMaterial(id, 0.92f, 0.18f, 0.35f, 0f)) refresh() else Toast.makeText(context, "Material não aplicado", Toast.LENGTH_SHORT).show() }
        ToolButton("PLÁSTICO") { if (NativeBridge.setMaterial(id, 0.02f, 0.42f, 0.08f, 0f)) refresh() else Toast.makeText(context, "Material não aplicado", Toast.LENGTH_SHORT).show() }
        ToolButton("VIDRO") { if (NativeBridge.setMaterial(id, 0.12f, 0.08f, 0.85f, 0f)) refresh() else Toast.makeText(context, "Material não aplicado", Toast.LENGTH_SHORT).show() }
        ToolButton("EMISSIVO") { if (NativeBridge.setMaterial(id, 0.05f, 0.28f, 0.18f, 1.8f)) refresh() else Toast.makeText(context, "Material não aplicado", Toast.LENGTH_SHORT).show() }
        ToolButton("MATTE") { if (NativeBridge.setMaterial(id, 0f, 0.86f, 0f, 0f)) refresh() else Toast.makeText(context, "Material não aplicado", Toast.LENGTH_SHORT).show() }
    }
}

@Composable
private fun PrimitiveBar(refresh: () -> Unit, onAdvanced: () -> Unit) {
    Row(
        Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(7.dp)
    ) {
        Text("MALHAS", color = MaterialTheme.colorScheme.primary, style = MaterialTheme.typography.labelSmall)
        listOf(
            PrimitiveType.CUBE, PrimitiveType.SPHERE, PrimitiveType.CYLINDER,
            PrimitiveType.CONE, PrimitiveType.TORUS, PrimitiveType.PLANE
        ).forEach { type ->
            ToolButton(type.label) {
                val size = 1f
                val y = if (type == PrimitiveType.PLANE) 0f else size / 2f
                val id = NativeBridge.addPrimitiveDetailed(type.id, size, 0f, y, 0f, 8)
                if (id >= 0) NativeBridge.select(id)
                refresh()
            }
        }
        ToolButton("MAIS MALHAS +") { onAdvanced() }
    }
}

@Composable
private fun AdvancedPrimitiveDialog(
    onClose: () -> Unit,
    onCreate: (PrimitiveType, Float, Int) -> Unit
) {
    var selected by remember { mutableStateOf(PrimitiveType.SPHERE) }
    var size by remember { mutableStateOf(1.0f) }
    var detail by remember { mutableStateOf(8.0f) }

    AlertDialog(
        onDismissRequest = onClose,
        title = { Text("Criar malha avançada") },
        text = {
            Column(
                Modifier.fillMaxWidth().heightIn(max = 470.dp).verticalScroll(rememberScrollState()),
                verticalArrangement = Arrangement.spacedBy(9.dp)
            ) {
                Text("FORMAS POLIGONAIS", style = MaterialTheme.typography.labelMedium, color = MaterialTheme.colorScheme.primary)
                PrimitiveType.values().filter { it != PrimitiveType.CUSTOM_MESH }.chunked(2).forEach { pair ->
                    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(7.dp)) {
                        pair.forEach { type ->
                            Button(
                                onClick = { selected = type },
                                modifier = Modifier.weight(1f),
                                colors = ButtonDefaults.buttonColors(
                                    containerColor = if (selected == type) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.surfaceVariant,
                                    contentColor = if (selected == type) MaterialTheme.colorScheme.onPrimary else MaterialTheme.colorScheme.onSurface
                                ),
                                contentPadding = PaddingValues(horizontal = 8.dp, vertical = 9.dp)
                            ) {
                                Text(type.label, style = MaterialTheme.typography.labelSmall, maxLines = 1)
                            }
                        }
                        if (pair.size == 1) androidx.compose.foundation.layout.Spacer(Modifier.weight(1f))
                    }
                }
                Text("Tamanho · ${"%.2f".format(size)} m", style = MaterialTheme.typography.labelMedium)
                Slider(value = size, onValueChange = { size = it }, valueRange = 0.2f..4.0f)
                Text("Densidade da malha · ${detail.toInt()}", style = MaterialTheme.typography.labelMedium)
                Slider(value = detail, onValueChange = { detail = it }, valueRange = 1f..64f, steps = 62)
                Text(
                    "Mais densidade cria mais vértices e faces para subdivisão e escultura, mas usa mais memória e processamento. Começa baixo e aumenta quando necessário.",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
            }
        },
        confirmButton = {
            Button(onClick = { onCreate(selected, size, detail.toInt().coerceIn(1, 64)) }) {
                Text("Criar malha")
            }
        },
        dismissButton = { TextButton(onClick = onClose) { Text("Cancelar") } }
    )
}
