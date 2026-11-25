package com.layouter.aimodeling

import androidx.compose.foundation.background
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.imePadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.systemBarsPadding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.Button
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
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
import androidx.compose.ui.unit.dp
import com.layouter.aimodeling.ai.AiAgent
import com.layouter.aimodeling.ai.AiSettings
import com.layouter.aimodeling.ai.ProviderKind
import com.layouter.aimodeling.ai.SecretStore
import com.layouter.aimodeling.ai.buildProvider
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.launch

private data class ChatLine(val role: String, val text: String)

@Composable
fun AiPanel(onClose: () -> Unit, onSceneChanged: () -> Unit, modifier: Modifier = Modifier) {
    val context = LocalContext.current
    val settings = remember { AiSettings(context) }
    val secrets = remember { SecretStore(context) }
    val scope = rememberCoroutineScope()

    var kind by remember { mutableStateOf(settings.kind) }
    var model by remember { mutableStateOf(settings.model(settings.kind)) }
    var apiKey by remember { mutableStateOf(secrets.get(AiSettings.keyName(settings.kind)) ?: "") }
    var showSettings by remember { mutableStateOf(apiKey.isBlank()) }
    var agent by remember { mutableStateOf<AiAgent?>(null) }
    val lines = remember { mutableStateListOf<ChatLine>() }
    var input by remember { mutableStateOf("") }
    var busy by remember { mutableStateOf(false) }
    val listState = rememberLazyListState()

    LaunchedEffect(lines.size) {
        if (lines.isNotEmpty()) listState.animateScrollToItem(lines.lastIndex)
    }

    fun send() {
        val text = input.trim()
        if (text.isEmpty() || busy) return
        if (apiKey.isBlank()) {
            showSettings = true
            return
        }
        val current = agent ?: AiAgent(buildProvider(kind, apiKey, model)).also { agent = it }
        input = ""
        lines.add(ChatLine("user", text))
        busy = true
        scope.launch {
            try {
                current.ask(
                    userText = text,
                    onText = { lines.add(ChatLine("assistant", it)) },
                    onTool = { call, result ->
                        lines.add(ChatLine("tool", "${call.name}: ${result.message}".take(160)))
                    },
                    onSceneChanged = onSceneChanged
                )
            } catch (e: CancellationException) {
                throw e
            } catch (e: Exception) {
                lines.add(ChatLine("error", e.message ?: "Erro desconhecido"))
            }
            onSceneChanged()
            busy = false
        }
    }

    Column(
        modifier = modifier
            .fillMaxWidth()
            .fillMaxHeight(0.55f)
            .background(Color(0xF01E1E24))
            .systemBarsPadding()
            .imePadding()
            .padding(12.dp),
        verticalArrangement = Arrangement.spacedBy(8.dp)
    ) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Text("Assistente de IA", color = Color.White, style = MaterialTheme.typography.titleMedium)
            Row {
                TextButton(onClick = { showSettings = !showSettings }) { Text("Config") }
                TextButton(onClick = onClose) { Text("Fechar") }
            }
        }

        if (showSettings) {
            Row(
                modifier = Modifier.horizontalScroll(rememberScrollState()),
                horizontalArrangement = Arrangement.spacedBy(6.dp)
            ) {
                ProviderKind.values().forEach { k ->
                    if (k == kind) {
                        Button(onClick = {}) { Text(k.label) }
                    } else {
                        FilledTonalButton(onClick = {
                            kind = k
                            model = settings.model(k)
                            apiKey = secrets.get(AiSettings.keyName(k)) ?: ""
                        }) { Text(k.label) }
                    }
                }
            }
            OutlinedTextField(
                value = apiKey,
                onValueChange = { apiKey = it },
                label = { Text("Chave de API (${kind.keyHint})") },
                singleLine = true,
                visualTransformation = PasswordVisualTransformation(),
                keyboardOptions = KeyboardOptions(keyboardType = KeyboardType.Password),
                modifier = Modifier.fillMaxWidth()
            )
            Text(
                "Onde obter a chave: ${kind.keySource}",
                color = Color(0xFFBBBBC4),
                style = MaterialTheme.typography.bodySmall
            )
            OutlinedTextField(
                value = model,
                onValueChange = { model = it },
                label = { Text("Modelo") },
                singleLine = true,
                modifier = Modifier.fillMaxWidth()
            )
            Text(
                "A chave fica cifrada neste dispositivo e só é enviada ao serviço escolhido. " +
                    "O uso é cobrado na tua conta desse serviço. Se o modelo não existir na tua conta, troca o nome acima.",
                color = Color(0xFFBBBBC4),
                style = MaterialTheme.typography.bodySmall
            )
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Button(onClick = {
                    settings.kind = kind
                    settings.setModel(kind, model.trim())
                    if (apiKey.isNotBlank()) {
                        secrets.put(AiSettings.keyName(kind), apiKey.trim())
                    } else {
                        secrets.remove(AiSettings.keyName(kind))
                    }
                    agent = null
                    showSettings = false
                }) { Text("Guardar") }
                TextButton(onClick = {
                    secrets.remove(AiSettings.keyName(kind))
                    apiKey = ""
                    agent = null
                }) { Text("Apagar chave") }
            }
        } else {
            LazyColumn(
                modifier = Modifier
                    .weight(1f)
                    .fillMaxWidth(),
                state = listState,
                verticalArrangement = Arrangement.spacedBy(6.dp)
            ) {
                if (lines.isEmpty()) {
                    item {
                        Text(
                            "Peça algo, por exemplo: \"faz uma mesa com 4 pernas\".",
                            color = Color(0xFFBBBBC4)
                        )
                    }
                }
                items(lines) { line -> ChatBubble(line) }
            }
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(8.dp),
                verticalAlignment = Alignment.CenterVertically
            ) {
                OutlinedTextField(
                    value = input,
                    onValueChange = { input = it },
                    placeholder = { Text("Escreve o que queres modelar") },
                    enabled = !busy,
                    modifier = Modifier.weight(1f)
                )
                Button(onClick = { send() }, enabled = !busy && input.isNotBlank()) {
                    Text(if (busy) "..." else "Enviar")
                }
            }
        }
    }
}

@Composable
private fun ChatBubble(line: ChatLine) {
    val background = when (line.role) {
        "user" -> Color(0xFF3A5BA0)
        "tool" -> Color(0xFF26352B)
        "error" -> Color(0xFF6B2B2B)
        else -> Color(0xFF2E2E36)
    }
    Box(
        modifier = Modifier
            .fillMaxWidth()
            .background(background, RoundedCornerShape(8.dp))
            .padding(8.dp)
    ) {
        Text(
            text = line.text,
            color = Color.White,
            style = if (line.role == "tool") MaterialTheme.typography.bodySmall
            else MaterialTheme.typography.bodyMedium
        )
    }
}
