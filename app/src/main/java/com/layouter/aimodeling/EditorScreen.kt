package com.layouter.aimodeling

import android.app.Activity
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
import androidx.compose.material.icons.outlined.DarkMode
import androidx.compose.material.icons.outlined.LightMode
import androidx.compose.material.icons.outlined.Extension
import androidx.compose.material.icons.outlined.Build
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
fun EditorScreen() {
    val revision = remember { mutableIntStateOf(0) }
    val rev = revision.intValue
    val refresh: () -> Unit = { revision.intValue++ }
    var showAi by remember { mutableStateOf(false) }
    var showMeshTools by remember { mutableStateOf(false) }
    var showExtensions by remember { mutableStateOf(false) }
    var faceIndex by remember { mutableStateOf("0") }
    val darkTheme = remember { context.getSharedPreferences("app_theme", android.content.Context.MODE_PRIVATE).getBoolean("dark", true) }
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
            factory = { context -> ModelingSurfaceView(context).apply { onSceneChanged = refresh } }
        )

        Row(
            modifier = Modifier.align(Alignment.TopCenter).systemBarsPadding().padding(8.dp)
                .background(MaterialTheme.colorScheme.surface, androidx.compose.foundation.shape.RoundedCornerShape(20.dp))
                .padding(horizontal = 8.dp, vertical = 4.dp),
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(2.dp)
        ) {
            Text("MESH", color = MaterialTheme.colorScheme.primary, style = MaterialTheme.typography.labelMedium, modifier = Modifier.padding(start = 8.dp, end = 4.dp))
            IconButton(onClick = { NativeBridge.undo(); refresh() }, enabled = canUndo) {
                Icon(Icons.Outlined.Undo, "Desfazer", tint = if (canUndo) MaterialTheme.colorScheme.onSurface else MaterialTheme.colorScheme.onSurfaceVariant)
            }
            IconButton(onClick = { NativeBridge.redo(); refresh() }, enabled = canRedo) {
                Icon(Icons.Outlined.Redo, "Refazer", tint = if (canRedo) Color.White else Color.Gray)
            }
            Box(Modifier.size(1.dp, 25.dp).background(MaterialTheme.colorScheme.outlineVariant))
            IconButton(onClick = {
                projectExport = SceneProjectCodec.encode()
                projectSaveLauncher.launch("untitled.3dproj")
            }) {
                Icon(Icons.Outlined.Save, "Guardar projeto", tint = MaterialTheme.colorScheme.onSurfaceVariant)
            }
            IconButton(onClick = {
                projectOpenLauncher.launch(arrayOf("*/*"))
            }) {
                Icon(Icons.Outlined.FolderOpen, "Abrir projeto", tint = MaterialTheme.colorScheme.onSurfaceVariant)
            }
            IconButton(onClick = {
                objExport = NativeBridge.exportObj()
                if (objExport.isBlank()) Toast.makeText(context, "Scene is empty", Toast.LENGTH_SHORT).show()
                else exportLauncher.launch("3d-model.obj")
            }) {
                Icon(Icons.Outlined.FileDownload, "Export OBJ", tint = Color(0xFFBFC7D1))
            }
            IconButton(onClick = { showAi = !showAi }) {
                Icon(Icons.Outlined.SmartToy, "Assistente IA", tint = MaterialTheme.colorScheme.primary)
            }
            IconButton(onClick = { showMeshTools = true }) {
                Icon(Icons.Outlined.Build, "Ferramentas de malha", tint = MaterialTheme.colorScheme.primary)
            }
            IconButton(onClick = { showExtensions = true }) {
                Icon(Icons.Outlined.Extension, "Biblioteca de extensões", tint = MaterialTheme.colorScheme.onSurface)
            }
            IconButton(onClick = {
                val next = !darkTheme
                context.getSharedPreferences("app_theme", android.content.Context.MODE_PRIVATE).edit().putBoolean("dark", next).apply()
                (context as? Activity)?.recreate()
            }) {
                Icon(if (darkTheme) Icons.Outlined.LightMode else Icons.Outlined.DarkMode,
                    if (darkTheme) "Tema claro" else "Tema escuro", tint = MaterialTheme.colorScheme.primary)
            }
        }

        if (showMeshTools) MeshWorkbenchDialog(selected, faceIndex, { faceIndex = it }, { showMeshTools = false }, refresh)
        if (showExtensions) ExtensionLibraryDialog { showExtensions = false }
        AiPanel(
            onClose = { showAi = false },
            onSceneChanged = refresh,
            modifier = Modifier.align(Alignment.BottomCenter),
            visible = showAi
        )
        if (!showAi) {
            Column(
                modifier = Modifier.align(Alignment.BottomCenter).fillMaxWidth(0.96f)
                    .background(MaterialTheme.colorScheme.surface, androidx.compose.foundation.shape.RoundedCornerShape(22.dp)).systemBarsPadding().padding(10.dp),
                verticalArrangement = Arrangement.spacedBy(7.dp)
            ) {
                if (selected >= 0) SelectionTools(selected, refresh)
                PrimitiveBar(refresh)
            }
        }
    }
}


@Composable
private fun MeshWorkbenchDialog(
    selected: Int,
    faceIndex: String,
    onFaceIndex: (String) -> Unit,
    onClose: () -> Unit,
    onChanged: () -> Unit
) {
    val context = androidx.compose.ui.platform.LocalContext.current
    fun apply(operation: String, parameter: Int = 1, value: Float = 0.15f, faces: IntArray = intArrayOf()) {
        if (selected < 0) {
            Toast.makeText(context, "Seleciona primeiro um objeto", Toast.LENGTH_SHORT).show()
            return
        }
        val ok = NativeBridge.meshOperation(selected, operation, faces, parameter, value)
        Toast.makeText(context, if (ok) "Operação aplicada · Undo disponível" else "Falhou: verifica a topologia/face", Toast.LENGTH_SHORT).show()
        if (ok) onChanged()
    }
    AlertDialog(
        onDismissRequest = onClose,
        title = { Text("Mesh Workbench") },
        text = {
            Column(Modifier.fillMaxWidth().verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(7.dp)) {
                Text("Ferramentas nativas da malha selecionada. Todas são editáveis e reversíveis.")
                Text("Para Extrude/Inset, consulta os IDs com a IA: get_mesh_topology.")
                OutlinedTextField(value = faceIndex, onValueChange = onFaceIndex, label = { Text("Índice da face") }, singleLine = true)
                Row(horizontalArrangement = Arrangement.spacedBy(5.dp)) {
                    Button(onClick = { apply("extrude", value = 0.2f, faces = intArrayOf(faceIndex.toIntOrNull() ?: 0)) }) { Text("Extrude") }
                    Button(onClick = { apply("inset", value = 0.08f, faces = intArrayOf(faceIndex.toIntOrNull() ?: 0)) }) { Text("Inset") }
                }
                Text("Refinar")
                Row(horizontalArrangement = Arrangement.spacedBy(5.dp)) {
                    Button(onClick = { apply("subdivide", parameter = 1) }) { Text("Subdividir") }
                    Button(onClick = { apply("smooth", parameter = 3, value = 0.35f) }) { Text("Suavizar") }
                }
                Row(horizontalArrangement = Arrangement.spacedBy(5.dp)) {
                    Button(onClick = { apply("solidify", value = 0.08f) }) { Text("Espessura") }
                    Button(onClick = { apply("weld", value = 0.0001f) }) { Text("Soldar") }
                }
                Text("Topologia")
                Row(horizontalArrangement = Arrangement.spacedBy(5.dp)) {
                    Button(onClick = { apply("triangulate") }) { Text("Triangular") }
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
                Text("Boolean e Loop Cut também existem no núcleo e podem ser solicitados à IA após inspeção de malha e identificação do objeto/arestas.")
            }
        },
        confirmButton = { TextButton(onClick = onClose) { Text("Fechar") } }
    )
}

private data class PluginReference(val title: String, val details: String, val url: String)

@Composable
private fun ExtensionLibraryDialog(onClose: () -> Unit) {
    val context = androidx.compose.ui.platform.LocalContext.current
    val refs = listOf(
        PluginReference("Terrain3D · Godot", "Sculpt de terreno, heightmaps, pintura, clipmap/LOD. Addon Godot não é plug-in binário deste app; os recursos são referências para portar ao motor nativo.", "https://github.com/TokisanGames/Terrain3D"),
        PluginReference("Polyforge · Godot", "Edit Mode: extrude, inset, bevel, loop cut, merge, mirror, Boolean, seleção de componentes.", "https://github.com/Seba313/polyforge"),
        PluginReference("GoBuild · Godot", "Edição de vértices, arestas e faces com ferramentas de transform.", "https://github.com/marcel-b-roodt/gobuild"),
        PluginReference("Blender BMesh", "Referência técnica de conectividade e operações robustas de topologia.", "https://developer.blender.org/docs/features/objects/mesh/bmesh/"),
        PluginReference("Materializr CAD", "CAD paramétrico, operações sólidas e formatos de intercâmbio.", "https://github.com/materializr-cad/materializr"),
        PluginReference("Strata:3D Android", "Referência para visualização/importação de formatos e meshes densas.", "https://github.com/damzenquis/Strata-3D")
    )
    AlertDialog(
        onDismissRequest = onClose,
        title = { Text("Plugins & ferramentas") },
        text = {
            Column(Modifier.fillMaxWidth().verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text("Fontes pesquisadas. Plugins Godot são incompatíveis diretamente com o app Android nativo. O caminho correto é adaptar funcionalidades em Kotlin/C++/OpenGL, respeitando licenças.")
                refs.forEach { ref ->
                    Text(ref.title, color = MaterialTheme.colorScheme.primary, style = MaterialTheme.typography.titleSmall)
                    Text(ref.details, style = MaterialTheme.typography.bodySmall)
                    TextButton(onClick = {
                        runCatching { context.startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(ref.url))) }
                            .onFailure { Toast.makeText(context, "Não foi possível abrir o endereço", Toast.LENGTH_SHORT).show() }
                    }) { Text("Abrir projeto/documentação") }
                    androidx.compose.material3.HorizontalDivider()
                }
            }
        },
        confirmButton = { TextButton(onClick = onClose) { Text("Concluir") } }
    )
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
private fun SelectionTools(id: Int, refresh: () -> Unit) {
    val scroll1 = rememberScrollState()
    val scroll2 = rememberScrollState()

    Row(Modifier.horizontalScroll(scroll1), horizontalArrangement = Arrangement.spacedBy(2.dp)) {
        IconButton(onClick = { NativeBridge.moveBy(id, -MOVE_STEP, 0f, 0f); refresh() }) {
            Icon(Icons.Outlined.ArrowBack, "Mover para X negativo", tint = Color.White)
        }
        IconButton(onClick = { NativeBridge.moveBy(id, MOVE_STEP, 0f, 0f); refresh() }) {
            Icon(Icons.Outlined.ArrowForward, "Mover para X positivo", tint = Color.White)
        }
        IconButton(onClick = { NativeBridge.moveBy(id, 0f, MOVE_STEP, 0f); refresh() }) {
            Icon(Icons.Outlined.ArrowUpward, "Mover para Y positivo", tint = Color.White)
        }
        IconButton(onClick = { NativeBridge.moveBy(id, 0f, -MOVE_STEP, 0f); refresh() }) {
            Icon(Icons.Outlined.ArrowDownward, "Mover para Y negativo", tint = Color.White)
        }
        ToolButton("Z−") { NativeBridge.moveBy(id, 0f, 0f, -MOVE_STEP); refresh() }
        ToolButton("MOLDAR +") { NativeBridge.sculptMesh(id, 0f, 0f, 0f, 0.85f, 0f, 0.10f, 0f); refresh() }
        ToolButton("MOLDAR −") { NativeBridge.sculptMesh(id, 0f, 0f, 0f, 0.85f, 0f, -0.10f, 0f); refresh() }
        ToolButton("Z+") { NativeBridge.moveBy(id, 0f, 0f, MOVE_STEP); refresh() }
    }

    Row(Modifier.horizontalScroll(scroll2), horizontalArrangement = Arrangement.spacedBy(2.dp)) {
        IconButton(onClick = { NativeBridge.rotateBy(id, 0f, -15f, 0f); refresh() }) {
            Icon(Icons.Outlined.RotateLeft, "Rodar -15 graus", tint = Color.White)
        }
        IconButton(onClick = { NativeBridge.rotateBy(id, 0f, 15f, 0f); refresh() }) {
            Icon(Icons.Outlined.RotateRight, "Rodar +15 graus", tint = Color.White)
        }
        IconButton(onClick = { NativeBridge.scaleBy(id, 1.1f, 1.1f, 1.1f); refresh() }) {
            Icon(Icons.Outlined.ZoomIn, "Aumentar", tint = Color.White)
        }
        IconButton(onClick = { NativeBridge.scaleBy(id, 0.9f, 0.9f, 0.9f); refresh() }) {
            Icon(Icons.Outlined.ZoomOut, "Diminuir", tint = Color.White)
        }
        IconButton(onClick = {
            val duplicate = NativeBridge.duplicateObject(id, 1f, 0f, 0f)
            if (duplicate >= 0) NativeBridge.select(duplicate)
            refresh()
        }) {
            Icon(Icons.Outlined.ContentCopy, "Duplicar objeto", tint = Color.White)
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
}

@Composable
private fun PrimitiveBar(refresh: () -> Unit) {
    Row(
        Modifier.horizontalScroll(rememberScrollState()),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(7.dp)
    ) {
        Text("CRIAR", color = MaterialTheme.colorScheme.primary, style = MaterialTheme.typography.labelSmall)
        PrimitiveType.values().forEach { type ->
            ToolButton(type.label) {
                val size = 1f
                val y = if (type == PrimitiveType.PLANE) 0f else size / 2f
                val id = NativeBridge.addPrimitive(type.id, size, 0f, y, 0f)
                if (id >= 0) NativeBridge.select(id)
                refresh()
            }
        }
    }
}
