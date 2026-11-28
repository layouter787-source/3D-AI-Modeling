package com.layouter.aimodeling

import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.net.Uri
import android.content.ClipData
import android.content.ClipboardManager
import android.util.Base64
import java.io.ByteArrayOutputStream
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.verticalScroll
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.imePadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.systemBarsPadding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.gestures.detectVerticalDragGestures
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.draw.clip
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.graphics.asImageBitmap
import androidx.activity.result.PickVisualMediaRequest
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.SmartToy
import androidx.compose.material.icons.outlined.Add
import androidx.compose.material.icons.outlined.CameraAlt
import androidx.compose.material.icons.outlined.CheckCircle
import androidx.compose.material.icons.outlined.Close
import androidx.compose.material.icons.outlined.ContentCopy
import androidx.compose.material.icons.outlined.DeleteOutline
import androidx.compose.material.icons.outlined.KeyboardArrowDown
import androidx.compose.material.icons.outlined.Menu
import androidx.compose.material.icons.outlined.History
import androidx.compose.material.icons.outlined.Image
import androidx.compose.material.icons.outlined.FolderOpen
import androidx.compose.material.icons.outlined.Key
import androidx.compose.material.icons.outlined.Person
import androidx.compose.material.icons.outlined.Refresh
import androidx.compose.material.icons.outlined.Send
import androidx.compose.material.icons.outlined.Settings
import androidx.compose.material.icons.outlined.Share
import androidx.compose.material.icons.outlined.ThumbDown
import androidx.compose.material.icons.outlined.ThumbUp
import androidx.compose.material.icons.outlined.Tune
import androidx.compose.material.icons.outlined.Warning
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.text.SpanStyle
import androidx.compose.ui.text.buildAnnotatedString
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextDecoration
import androidx.compose.ui.text.withStyle
import androidx.compose.ui.unit.dp
import com.layouter.aimodeling.ai.AiAgent
import com.layouter.aimodeling.ai.AiModel
import com.layouter.aimodeling.ai.AiSettings
import com.layouter.aimodeling.ai.ModelCatalog
import com.layouter.aimodeling.ai.ProviderKind
import com.layouter.aimodeling.ai.buildProvider
import com.layouter.aimodeling.ai.SecretStore
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.launch
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob

private data class ChatLine(val role: String, val text: String, val preview: Bitmap? = null)
private data class SavedChat(val id: String, val title: String, val messages: List<ChatLine>)
private val AiWorkScope = CoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)


@Composable
fun AiPanel(onClose: () -> Unit, onSceneChanged: () -> Unit, modifier: Modifier = Modifier, visible: Boolean = true) {
    val context = LocalContext.current
    val settings = remember { AiSettings(context) }
    val secrets = remember { SecretStore(context) }
    val scope = rememberCoroutineScope()

    var kind by remember { mutableStateOf(settings.kind) }
    var model by remember { mutableStateOf(settings.model(settings.kind)) }
    var apiKey by remember { mutableStateOf(secrets.get(AiSettings.keyName(settings.kind)) ?: "") }
    var showSettings by remember { mutableStateOf(apiKey.isBlank() || model.isBlank()) }
    var providerMenu by remember { mutableStateOf(false) }
    var modelMenu by remember { mutableStateOf(false) }
    var modelOptions by remember { mutableStateOf<List<AiModel>>(emptyList()) }
    var loadingModels by remember { mutableStateOf(false) }
    var modelError by remember { mutableStateOf<String?>(null) }
    var agent by remember { mutableStateOf<AiAgent?>(null) }
    val lines = remember { mutableStateListOf<ChatLine>() }
    var input by remember { mutableStateOf("") }
    var attachedImage by remember { mutableStateOf<String?>(null) }
    var attachedMime by remember { mutableStateOf("image/jpeg") }
    var attachedPreview by remember { mutableStateOf<Bitmap?>(null) }
    var attachmentMenu by remember { mutableStateOf(false) }
    var historyMenu by remember { mutableStateOf(false) }
    val chatPrefs = remember { context.getSharedPreferences("ai_chat_history", android.content.Context.MODE_PRIVATE) }
    val savedChats = remember { mutableStateListOf<SavedChat>() }
    var activeChatId by remember { mutableStateOf(System.currentTimeMillis().toString()) }
    var activeChatTitle by remember { mutableStateOf("") }
    var renameDialog by remember { mutableStateOf(false) }
    var renameValue by remember { mutableStateOf("") }
    var deleteChatId by remember { mutableStateOf<String?>(null) }
    fun storeChats(sessions: List<SavedChat>) {
        savedChats.clear()
        savedChats.addAll(sessions)
        val array = org.json.JSONArray()
        sessions.forEach { session ->
            val obj = org.json.JSONObject().put("id", session.id).put("title", session.title)
            val msgArray = org.json.JSONArray()
            session.messages.forEach { msg -> msgArray.put(org.json.JSONObject().put("role", msg.role).put("text", msg.text)) }
            obj.put("messages", msgArray)
            array.put(obj)
        }
        chatPrefs.edit().putString("sessions", array.toString()).apply()
    }
    fun persistChats() {
        val current = lines.map { ChatLine(it.role, it.text) }
        if (current.isEmpty()) return
        val fallback = current.firstOrNull { it.role == "user" }?.text?.take(48)?.ifBlank { "Conversa" } ?: "Conversa"
        val title = activeChatTitle.ifBlank { fallback }.take(64)
        activeChatTitle = title
        val sessions = (listOf(SavedChat(activeChatId, title, current)) +
            savedChats.filterNot { it.id == activeChatId }).take(25)
        storeChats(sessions)
    }
    fun loadSavedChats() {
        savedChats.clear()
        runCatching {
            val array = org.json.JSONArray(chatPrefs.getString("sessions", "[]"))
            for (i in 0 until array.length()) {
                val obj = array.getJSONObject(i); val msgs = obj.getJSONArray("messages")
                val list = (0 until msgs.length()).map { j -> val m = msgs.getJSONObject(j); ChatLine(m.getString("role"), m.getString("text")) }
                savedChats.add(SavedChat(obj.optString("id", "legacy-" + i + "-" + obj.optString("title")), obj.optString("title", "Conversa"), list))
            }
        }
    }
    LaunchedEffect(Unit) { loadSavedChats() }

    var routeStatus by remember { mutableStateOf("") }
    var busy by remember { mutableStateOf(false) }
    fun attachReference(uri: Uri) {
        try {
            val bitmap = decodeReferenceBitmap(context, uri)
            if (bitmap == null) {
                routeStatus = "Não foi possível descodificar esta imagem"
                return
            }
            attachedPreview = bitmap
            attachedImage = encodeReferenceImage(bitmap)
            attachedMime = "image/jpeg"
            routeStatus = "Imagem pronta para análise"
        } catch (e: Exception) {
            routeStatus = "Falha ao abrir imagem"
        }
    }
    val galleryPicker = rememberLauncherForActivityResult(ActivityResultContracts.PickVisualMedia()) { uri ->
        if (uri != null) attachReference(uri)
    }
    val filePicker = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri != null) attachReference(uri)
    }
    val cameraPicker = rememberLauncherForActivityResult(ActivityResultContracts.TakePicturePreview()) { bitmap ->
        if (bitmap != null) {
            attachedPreview = bitmap
            attachedImage = encodeReferenceImage(bitmap)
            attachedMime = "image/jpeg"
            routeStatus = "Fotografia pronta para análise"
        }
    }
    val listState = rememberLazyListState()

    LaunchedEffect(lines.size) {
        if (lines.isNotEmpty()) listState.animateScrollToItem(lines.lastIndex)
    }

    if (!visible) return

    fun loadModels() {
        if (apiKey.isBlank()) {
            modelError = "Introduz a chave de API antes de consultar os modelos."
            return
        }
        scope.launch {
            loadingModels = true
            modelError = null
            try {
                modelOptions = ModelCatalog.load(kind, apiKey.trim())
                if (modelOptions.isEmpty()) {
                    modelError = "A API não devolveu modelos disponíveis para esta chave."
                }
            } catch (e: Exception) {
                modelError = e.message ?: "Não foi possível consultar a API."
            } finally {
                loadingModels = false
            }
        }
    }

    fun saveSettings() {
        settings.kind = kind
        settings.setModel(kind, model)
        if (apiKey.isNotBlank()) secrets.put(AiSettings.keyName(kind), apiKey.trim())
        else secrets.remove(AiSettings.keyName(kind))
        agent = null
        showSettings = false
    }

    fun send() {
        val text = input.trim()
        if ((text.isEmpty() && attachedImage == null) || busy) return
        if (apiKey.isBlank() || model.isBlank()) {
            showSettings = true
            return
        }
        val current = agent ?: run {
            val directProvider = buildProvider(kind, apiKey.trim(), model.trim())
            AiAgent(directProvider).also { agent = it }
        }
        input = ""
        val outgoingImage = attachedImage
        val outgoingMime = attachedMime
        val outgoingPreview = attachedPreview
        attachedImage = null
        attachedPreview = null
        lines.add(ChatLine("user", text.ifBlank { "Analisa esta imagem de referência." }, outgoingPreview))
        persistChats()
        busy = true
        AiWorkScope.launch {
            try {
                current.ask(
                    userText = text,
                    imageBase64 = outgoingImage,
                    imageMimeType = outgoingMime,
                    onText = { answer -> if (answer.isNotBlank()) { lines.add(ChatLine("assistant", answer)); persistChats() } },
                    onTool = { call, result ->
                        lines.add(ChatLine("tool", if (result.ok) "Executado: ${call.name}" else "Falhou: ${call.name}")); persistChats()
                    },
                    onSceneChanged = onSceneChanged
                )
            } catch (e: CancellationException) {
                throw e
            } catch (e: Exception) {
                lines.add(ChatLine("error", e.message ?: "Falha na ligação. Confirma a chave e o modelo.")); persistChats()
            } finally {
                onSceneChanged()
                busy = false
            }
        }
    }

    Column(
        modifier = modifier
            .fillMaxSize()
            .background(MaterialTheme.colorScheme.background)
            .systemBarsPadding()
            .imePadding()
            .padding(horizontal = 14.dp, vertical = 10.dp),
        verticalArrangement = Arrangement.spacedBy(10.dp)
    ) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Surface(shape = RoundedCornerShape(13.dp), color = Color(0xFF2A3439)) {
                Icon(Icons.Filled.SmartToy, null, tint = MaterialTheme.colorScheme.primary, modifier = Modifier.padding(9.dp).size(23.dp))
            }
            Column(Modifier.weight(1f).padding(start = 10.dp)) {
                Text("Assistente", style = MaterialTheme.typography.titleMedium, color = MaterialTheme.colorScheme.onSurface)
                Text(
                    routeStatus.ifBlank { "${kind.label} · ${model.ifBlank { "Modelo por escolher" }}" },
                    style = MaterialTheme.typography.labelSmall,
                    color = if (routeStatus.contains("falh", ignoreCase = true)) Color(0xFFFFB66E) else MaterialTheme.colorScheme.onSurfaceVariant,
                    maxLines = 1
                )
            }
            Box {
                IconButton(onClick = { historyMenu = true }) {
                    Icon(Icons.Outlined.History, "Histórico de conversas", tint = MaterialTheme.colorScheme.onSurfaceVariant)
                }
                DropdownMenu(expanded = historyMenu, onDismissRequest = { historyMenu = false }) {
                    DropdownMenuItem(text = { Text("Nova conversa") }, leadingIcon = { Icon(Icons.Outlined.Add, null) }, onClick = {
                        persistChats()
                        lines.clear(); agent = null; input = ""; activeChatTitle = ""
                        activeChatId = System.currentTimeMillis().toString()
                        historyMenu = false
                    })
                    DropdownMenuItem(text = { Text("Renomear conversa atual") }, onClick = {
                        renameValue = activeChatTitle.ifBlank { lines.firstOrNull { it.role == "user" }?.text?.take(48) ?: "Conversa" }
                        renameDialog = true; historyMenu = false
                    })
                    savedChats.forEach { saved ->
                        DropdownMenuItem(text = { Text(saved.title, maxLines = 1) }, trailingIcon = {
                            IconButton(onClick = { deleteChatId = saved.id; historyMenu = false }) {
                                Icon(Icons.Outlined.DeleteOutline, "Apagar conversa", tint = Color(0xFFFF8D8D))
                            }
                        }, onClick = {
                            persistChats()
                            lines.clear(); lines.addAll(saved.messages)
                            activeChatId = saved.id; activeChatTitle = saved.title
                            agent = AiAgent(buildProvider(kind, apiKey.trim(), model.trim()))
                            agent?.restoreConversation(saved.messages.map { it.role to it.text })
                            input = ""; historyMenu = false
                        })
                    }
                }
            }
            IconButton(onClick = { showSettings = !showSettings }) {
                Icon(Icons.Outlined.Settings, "Configuração", tint = if (showSettings) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.onSurfaceVariant)
            }
            Box(Modifier.padding(start = 5.dp).pointerInput(Unit) { detectVerticalDragGestures(onVerticalDrag = { _, dy -> if (dy > 18f) onClose() }) }) {
                IconButton(onClick = onClose) { Icon(Icons.Outlined.KeyboardArrowDown, "Recolher painel (a tarefa continua)", tint = MaterialTheme.colorScheme.primary) }
            }
        }

        if (showSettings) {
            Column(
                modifier = Modifier
                    .weight(1f)
                    .fillMaxWidth()
                    .verticalScroll(rememberScrollState())
                    .background(MaterialTheme.colorScheme.surfaceVariant, RoundedCornerShape(18.dp))
                    .padding(14.dp),
                verticalArrangement = Arrangement.spacedBy(10.dp)
            ) {
                Text("Ligação à IA", color = Color.White, style = MaterialTheme.typography.titleSmall)
                Text("Escolhe o serviço e consulta os modelos que a tua chave pode ver.", color = MaterialTheme.colorScheme.onSurfaceVariant, style = MaterialTheme.typography.bodySmall)

                Box {
                    OutlinedButton(
                        onClick = { providerMenu = true },
                        modifier = Modifier.fillMaxWidth(),
                        shape = RoundedCornerShape(13.dp)
                    ) {
                        Text(kind.label, modifier = Modifier.weight(1f), color = Color.White)
                        Icon(Icons.Outlined.KeyboardArrowDown, null)
                    }
                    DropdownMenu(expanded = providerMenu, onDismissRequest = { providerMenu = false }) {
                        ProviderKind.values().forEach { provider ->
                            DropdownMenuItem(
                                text = { Text(provider.label) },
                                onClick = {
                                    kind = provider
                                    model = settings.model(provider)
                                    apiKey = secrets.get(AiSettings.keyName(provider)) ?: ""
                                    modelOptions = emptyList()
                                    modelError = null
                                    agent = null
                                    providerMenu = false
                                }
                            )
                        }
                    }
                }

                OutlinedTextField(
                    value = apiKey,
                    onValueChange = { apiKey = it },
                    label = { Text("Chave de API") },
                    placeholder = { Text(kind.keyHint) },
                    leadingIcon = { Icon(Icons.Outlined.Key, null) },
                    singleLine = true,
                    visualTransformation = PasswordVisualTransformation(),
                    keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Password),
                    modifier = Modifier.fillMaxWidth(),
                    shape = RoundedCornerShape(13.dp)
                )
                Text("Chave: ${kind.keySource}", color = MaterialTheme.colorScheme.onSurfaceVariant, style = MaterialTheme.typography.labelSmall)

                Row(verticalAlignment = Alignment.CenterVertically) {
                    OutlinedTextField(
                        value = model,
                        onValueChange = { model = it; agent = null },
                        label = { Text("ID do modelo") },
                        placeholder = { Text("Seleciona na lista da API") },
                        modifier = Modifier.weight(1f),
                        singleLine = true,
                        shape = RoundedCornerShape(13.dp)
                    )
                    Box {
                        IconButton(onClick = { modelMenu = true }, enabled = modelOptions.isNotEmpty()) {
                            Icon(Icons.Outlined.KeyboardArrowDown, "Modelos consultados")
                        }
                        DropdownMenu(expanded = modelMenu, onDismissRequest = { modelMenu = false }) {
                            modelOptions.forEach { option ->
                                DropdownMenuItem(
                                    text = {
                                        Column {
                                            Text(option.displayName, maxLines = 1)
                                            if (option.displayName != option.id) {
                                                Text(option.id, color = MaterialTheme.colorScheme.onSurfaceVariant, style = MaterialTheme.typography.labelSmall)
                                            }
                                        }
                                    },
                                    onClick = {
                                        model = option.id
                                        agent = null
                                        modelMenu = false
                                    }
                                )
                            }
                        }
                    }
                }

                Row(verticalAlignment = Alignment.CenterVertically) {
                    Button(
                        onClick = { loadModels() },
                        enabled = !loadingModels && apiKey.isNotBlank(),
                        colors = ButtonDefaults.buttonColors(containerColor = MaterialTheme.colorScheme.primary)
                    ) {
                        Icon(Icons.Outlined.Refresh, null, modifier = Modifier.size(18.dp))
                        Text(if (loadingModels) "A consultar…" else "Consultar modelos", modifier = Modifier.padding(start = 6.dp))
                    }
                    TextButton(onClick = { saveSettings() }) {
                        Icon(Icons.Outlined.CheckCircle, null, tint = MaterialTheme.colorScheme.primary, modifier = Modifier.size(18.dp))
                        Text("Guardar", color = MaterialTheme.colorScheme.primary)
                    }
                }
                modelError?.let {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Icon(Icons.Outlined.Warning, null, tint = Color(0xFFFFB66E), modifier = Modifier.size(17.dp))
                        Text(it, color = Color(0xFFFFB66E), style = MaterialTheme.typography.bodySmall, modifier = Modifier.padding(start = 6.dp))
                    }
                }
                Text(
                    "A chave é guardada cifrada no dispositivo. A lista vem da API do provedor; o modelo escolhido é validado pelo próprio serviço.",
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    style = MaterialTheme.typography.labelSmall
                )
                TextButton(onClick = {
                    secrets.remove(AiSettings.keyName(kind))
                    apiKey = ""
                    agent = null
                }) {
                    Icon(Icons.Outlined.DeleteOutline, null, modifier = Modifier.size(18.dp))
                    Text("Remover chave")
                }
            }
        } else {
            LazyColumn(
                modifier = Modifier.weight(1f).fillMaxWidth(),
                state = listState,
                verticalArrangement = Arrangement.spacedBy(13.dp)
            ) {
                if (lines.isEmpty()) {
                    item {
                        Column(
                            modifier = Modifier.fillMaxWidth().padding(top = 34.dp),
                            horizontalAlignment = Alignment.CenterHorizontally,
                            verticalArrangement = Arrangement.spacedBy(12.dp)
                        ) {
                            Surface(shape = CircleShape, color = MaterialTheme.colorScheme.surfaceVariant) {
                                Icon(Icons.Filled.SmartToy, null, tint = MaterialTheme.colorScheme.primary, modifier = Modifier.padding(19.dp).size(38.dp))
                            }
                            Text("O que vamos criar?", color = Color.White, style = MaterialTheme.typography.titleLarge)
                            Text(
                                "Descreve um objeto 3D. A IA pode criar e editar a cena.",
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                                style = MaterialTheme.typography.bodyMedium
                            )
                        }
                    }
                }
                items(lines) { line -> ChatBubble(line, showActions = !busy) }
                if (busy) {
                    item {
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            Icon(Icons.Filled.SmartToy, null, tint = MaterialTheme.colorScheme.primary, modifier = Modifier.size(19.dp))
                            Text(" A pensar…", color = MaterialTheme.colorScheme.onSurfaceVariant, style = MaterialTheme.typography.bodySmall)
                        }
                    }
                }
            }

            Surface(color = MaterialTheme.colorScheme.surfaceVariant, shape = RoundedCornerShape(22.dp)) {
                Column(Modifier.fillMaxWidth().padding(horizontal = 8.dp, vertical = 6.dp), verticalArrangement = Arrangement.spacedBy(5.dp)) {
                    attachedPreview?.let { preview ->
                        Row(
                            modifier = Modifier.fillMaxWidth().padding(horizontal = 6.dp, vertical = 2.dp),
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Image(
                                bitmap = preview.asImageBitmap(),
                                contentDescription = "Pré-visualização da imagem anexada",
                                contentScale = ContentScale.Crop,
                                modifier = Modifier.size(66.dp).clip(RoundedCornerShape(12.dp))
                            )
                            Column(Modifier.weight(1f).padding(start = 10.dp)) {
                                Text("Imagem de referência", style = MaterialTheme.typography.labelLarge, color = MaterialTheme.colorScheme.onSurface)
                                Text("Pronta para enviar à IA", style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                            }
                            IconButton(onClick = { attachedPreview = null; attachedImage = null }, enabled = !busy) {
                                Icon(Icons.Outlined.Close, "Remover imagem", tint = MaterialTheme.colorScheme.onSurfaceVariant)
                            }
                        }
                    }
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Box {
                            IconButton(onClick = { attachmentMenu = true }, enabled = !busy) {
                                Icon(Icons.Outlined.Add, "Adicionar referência", tint = MaterialTheme.colorScheme.primary)
                            }
                            DropdownMenu(expanded = attachmentMenu, onDismissRequest = { attachmentMenu = false }) {
                                DropdownMenuItem(text = { Text("Galeria") }, leadingIcon = { Icon(Icons.Outlined.Image, null) }, onClick = {
                                    attachmentMenu = false
                                    galleryPicker.launch(PickVisualMediaRequest(ActivityResultContracts.PickVisualMedia.ImageOnly))
                                })
                                DropdownMenuItem(text = { Text("Ficheiros") }, leadingIcon = { Icon(Icons.Outlined.FolderOpen, null) }, onClick = {
                                    attachmentMenu = false
                                    filePicker.launch(arrayOf("image/*"))
                                })
                                DropdownMenuItem(text = { Text("Câmara") }, leadingIcon = { Icon(Icons.Outlined.CameraAlt, null) }, onClick = {
                                    attachmentMenu = false
                                    cameraPicker.launch(null)
                                })
                            }
                        }
                        OutlinedTextField(
                            value = input,
                            onValueChange = { input = it },
                            placeholder = { Text(if (attachedImage != null) "Descreve o que queres modelar…" else "Escreve uma mensagem…") },
                            enabled = !busy,
                            modifier = Modifier.weight(1f),
                            maxLines = 4,
                            singleLine = false,
                            shape = RoundedCornerShape(16.dp),
                            colors = androidx.compose.material3.OutlinedTextFieldDefaults.colors(
                                unfocusedBorderColor = Color.Transparent,
                                focusedBorderColor = Color.Transparent,
                                unfocusedContainerColor = Color.Transparent,
                                focusedContainerColor = Color.Transparent
                            )
                        )
                        IconButton(
                            onClick = { send() },
                            enabled = !busy && (input.isNotBlank() || attachedImage != null),
                            modifier = Modifier.background(if (!busy && (input.isNotBlank() || attachedImage != null)) MaterialTheme.colorScheme.primary else Color(0xFF3A414B), CircleShape)
                        ) {
                            Icon(Icons.Outlined.Send, "Enviar", tint = Color(0xFF10151A))
                        }
                    }
                }
            }
        }
    }

    if (renameDialog) {
        AlertDialog(
            onDismissRequest = { renameDialog = false },
            title = { Text("Renomear conversa") },
            text = { OutlinedTextField(value = renameValue, onValueChange = { renameValue = it }, singleLine = true, label = { Text("Nome") }) },
            confirmButton = { TextButton(onClick = { activeChatTitle = renameValue.trim().ifBlank { "Conversa" }.take(64); persistChats(); renameDialog = false }) { Text("Guardar") } },
            dismissButton = { TextButton(onClick = { renameDialog = false }) { Text("Cancelar") } }
        )
    }
    deleteChatId?.let { target ->
        AlertDialog(
            onDismissRequest = { deleteChatId = null },
            title = { Text("Apagar conversa?") },
            text = { Text("Esta conversa será removida do histórico local. Esta ação não pode ser desfeita.") },
            confirmButton = { TextButton(onClick = {
                storeChats(savedChats.filterNot { it.id == target })
                if (activeChatId == target) { lines.clear(); agent = null; activeChatId = System.currentTimeMillis().toString(); activeChatTitle = "" }
                deleteChatId = null
            }) { Text("Apagar") } },
            dismissButton = { TextButton(onClick = { deleteChatId = null }) { Text("Cancelar") } }
        )
    }
}

private fun decodeReferenceBitmap(context: android.content.Context, uri: android.net.Uri): Bitmap? {
    val resolver = context.contentResolver
    val bounds = BitmapFactory.Options().apply { inJustDecodeBounds = true }
    resolver.openInputStream(uri)?.use { BitmapFactory.decodeStream(it, null, bounds) }
    if (bounds.outWidth <= 0 || bounds.outHeight <= 0) return null
    var sample = 1
    while (bounds.outWidth / sample > 1280 || bounds.outHeight / sample > 1280) sample *= 2
    val options = BitmapFactory.Options().apply { inSampleSize = sample; inPreferredConfig = Bitmap.Config.RGB_565 }
    return resolver.openInputStream(uri)?.use { BitmapFactory.decodeStream(it, null, options) }
}

private fun encodeReferenceImage(source: Bitmap): String {
    val maxSide = 1280f
    val scale = minOf(1f, maxSide / maxOf(source.width, source.height))
    val bitmap = if (scale < 1f) Bitmap.createScaledBitmap(source, (source.width * scale).toInt().coerceAtLeast(1), (source.height * scale).toInt().coerceAtLeast(1), true) else source
    val output = ByteArrayOutputStream()
    bitmap.compress(Bitmap.CompressFormat.JPEG, 82, output)
    if (bitmap !== source) bitmap.recycle()
    return Base64.encodeToString(output.toByteArray(), Base64.NO_WRAP)
}

@Composable
private fun ChatBubble(line: ChatLine, showActions: Boolean) {
    if (line.role == "tool") {
        Row(
            modifier = Modifier.fillMaxWidth().padding(start = 8.dp, top = 2.dp, bottom = 2.dp),
            verticalAlignment = Alignment.CenterVertically
        ) {
            Box(Modifier.size(7.dp).background(Color(0xFF79CDA4), CircleShape))
            Text("  ${line.text}", color = MaterialTheme.colorScheme.onSurfaceVariant, style = MaterialTheme.typography.labelSmall)
        }
        return
    }

    val isUser = line.role == "user"
    val isError = line.role == "error"
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .padding(vertical = 7.dp),
        verticalArrangement = Arrangement.spacedBy(5.dp)
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Icon(
                when {
                    isError -> Icons.Outlined.Warning
                    isUser -> Icons.Outlined.Person
                    else -> Icons.Filled.SmartToy
                },
                null,
                tint = when {
                    isError -> Color(0xFFFFB66E)
                    isUser -> MaterialTheme.colorScheme.primary
                    else -> MaterialTheme.colorScheme.primary
                },
                modifier = Modifier.size(16.dp)
            )
            Text(
                if (isError) "ERRO" else if (isUser) "TU" else "AGENTE",
                color = if (isError) Color(0xFFFFB66E) else MaterialTheme.colorScheme.onSurfaceVariant,
                style = MaterialTheme.typography.labelSmall,
                modifier = Modifier.padding(start = 8.dp)
            )
        }
        if (isUser) {
            Surface(
                color = MaterialTheme.colorScheme.primaryContainer.copy(alpha = 0.82f),
                shape = RoundedCornerShape(17.dp),
                modifier = Modifier.padding(start = 24.dp, end = 10.dp)
            ) {
                Column(Modifier.padding(horizontal = 10.dp, vertical = 9.dp), verticalArrangement = Arrangement.spacedBy(7.dp)) {
                    line.preview?.let { preview ->
                        Image(
                            bitmap = preview.asImageBitmap(),
                            contentDescription = "Imagem enviada como referência",
                            contentScale = ContentScale.Fit,
                            modifier = Modifier.fillMaxWidth().heightIn(max = 240.dp).clip(RoundedCornerShape(12.dp))
                        )
                    }
                    if (line.text.isNotBlank()) {
                        Text(
                            line.text,
                            color = MaterialTheme.colorScheme.onPrimaryContainer,
                            style = MaterialTheme.typography.bodyMedium
                        )
                    }
                }
            }
        } else {
            MarkdownText(
                markdown = line.text,
                color = if (isError) Color(0xFFFFB66E) else MaterialTheme.colorScheme.onSurface,
                modifier = Modifier.padding(start = 24.dp, end = 10.dp)
            )
            if (!isError && showActions) {
                AssistantActions(line.text)
            }
        }
    }
}

@Composable
private fun AssistantActions(text: String) {
    val context = LocalContext.current
    var feedback by remember(text) { mutableStateOf<Int?>(null) }
    Row(
        modifier = Modifier.padding(start = 18.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(1.dp)
    ) {
        IconButton(onClick = {
            val clipboard = context.getSystemService(ClipboardManager::class.java)
            clipboard?.setPrimaryClip(ClipData.newPlainText("Resposta da IA", text))
            android.widget.Toast.makeText(context, "Resposta copiada", android.widget.Toast.LENGTH_SHORT).show()
        }) {
            Icon(Icons.Outlined.ContentCopy, "Copiar resposta", modifier = Modifier.size(17.dp), tint = MaterialTheme.colorScheme.onSurfaceVariant)
        }
        IconButton(onClick = { feedback = if (feedback == 1) null else 1 }) {
            Icon(Icons.Outlined.ThumbUp, "Boa resposta", modifier = Modifier.size(17.dp), tint = if (feedback == 1) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.onSurfaceVariant)
        }
        IconButton(onClick = { feedback = if (feedback == -1) null else -1 }) {
            Icon(Icons.Outlined.ThumbDown, "Má resposta", modifier = Modifier.size(17.dp), tint = if (feedback == -1) Color(0xFFFF9A9A) else MaterialTheme.colorScheme.onSurfaceVariant)
        }
        IconButton(onClick = {
            val sendIntent = android.content.Intent(android.content.Intent.ACTION_SEND).apply {
                type = "text/plain"
                putExtra(android.content.Intent.EXTRA_TEXT, text)
            }
            context.startActivity(android.content.Intent.createChooser(sendIntent, "Partilhar resposta"))
        }) {
            Icon(Icons.Outlined.Share, "Partilhar resposta", modifier = Modifier.size(17.dp), tint = MaterialTheme.colorScheme.onSurfaceVariant)
        }
        if (feedback != null) {
            Text(
                if (feedback == 1) "Obrigado pelo feedback" else "Feedback registado",
                style = MaterialTheme.typography.labelSmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
                modifier = Modifier.padding(start = 4.dp)
            )
        }
    }
}

@Composable
private fun MarkdownText(markdown: String, color: Color, modifier: Modifier = Modifier) {
    Text(
        text = markdownToAnnotatedString(markdown, color),
        style = MaterialTheme.typography.bodyMedium,
        modifier = modifier,
        lineHeight = MaterialTheme.typography.bodyMedium.lineHeight * 1.18f
    )
}

private fun markdownToAnnotatedString(markdown: String, color: Color): AnnotatedString = buildAnnotatedString {
    var inCodeBlock = false
    val normalized = markdown.replace("\r\n", "\n")
    normalized.split("\n").forEachIndexed { index, originalLine ->
        var line = originalLine
        if (line.trimStart().startsWith("```") || line.trimStart().startsWith("~~~")) {
            inCodeBlock = !inCodeBlock
        } else {
            if (inCodeBlock) {
                withStyle(SpanStyle(fontFamily = FontFamily.Monospace, color = color.copy(alpha = 0.92f))) { append(line) }
            } else {
                val heading = Regex("^\\s*#{1,6}\\s+").find(line)
                val bullet = Regex("^\\s*(?:[-*+]\\s+|\\d+[.)]\\s+)").find(line)
                val prefixStyle = when {
                    heading != null -> SpanStyle(fontWeight = FontWeight.Bold, color = color)
                    bullet != null -> SpanStyle(color = color)
                    else -> SpanStyle(color = color)
                }
                if (heading != null) line = line.removeRange(heading.range)
                if (bullet != null) {
                    line = "• " + line.removeRange(bullet.range)
                }
                appendInlineMarkdown(line, prefixStyle, color)
            }
        }
        if (index < normalized.count { it == '\n' }) append("\n")
    }
}

private fun androidx.compose.ui.text.AnnotatedString.Builder.appendInlineMarkdown(
    source: String,
    baseStyle: SpanStyle,
    color: Color
) {
    val token = Regex("(\\*\\*[^*]+\\*\\*|__[^_]+__|\\*[^*]+\\*|_[^_]+_|`[^`]+`|\\[[^]]+\\]\\([^)]*\\))")
    var cursor = 0
    token.findAll(source).forEach { match ->
        if (match.range.first > cursor) withStyle(baseStyle) { append(source.substring(cursor, match.range.first)) }
        val raw = match.value
        when {
            raw.startsWith("**") || raw.startsWith("__") -> withStyle(baseStyle.copy(fontWeight = FontWeight.Bold)) { append(raw.substring(2, raw.length - 2)) }
            raw.startsWith("*") || raw.startsWith("_") -> withStyle(baseStyle.copy(fontStyle = FontStyle.Italic)) { append(raw.substring(1, raw.length - 1)) }
            raw.startsWith("`") -> withStyle(baseStyle.copy(fontFamily = FontFamily.Monospace, background = color.copy(alpha = 0.12f))) { append(raw.substring(1, raw.length - 1)) }
            raw.startsWith("[") -> withStyle(baseStyle.copy(color = color, textDecoration = TextDecoration.Underline)) { append(raw.substringAfter("[").substringBefore("]")) }
        }
        cursor = match.range.last + 1
    }
    if (cursor < source.length) withStyle(baseStyle) { append(source.substring(cursor)) }
}
