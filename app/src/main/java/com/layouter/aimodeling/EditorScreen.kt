package com.layouter.aimodeling

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
import androidx.compose.foundation.layout.systemBarsPadding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView

private const val MOVE_STEP = 0.25f

private val palette = listOf(
    Triple(0.35f, 0.60f, 0.95f),
    Triple(0.90f, 0.30f, 0.30f),
    Triple(0.30f, 0.80f, 0.40f),
    Triple(0.95f, 0.80f, 0.25f),
    Triple(0.85f, 0.85f, 0.85f),
)

@Composable
fun EditorScreen() {
    // Muda sempre que algo na cena muda; força a UI a reler o estado do núcleo nativo.
    val revision = remember { mutableIntStateOf(0) }
    val rev = revision.intValue
    val refresh: () -> Unit = { revision.intValue = revision.intValue + 1 }
    var showAi by remember { mutableStateOf(false) }

    val selected = remember(rev) { NativeBridge.getSelected() }
    val canUndo = remember(rev) { NativeBridge.canUndo() }
    val canRedo = remember(rev) { NativeBridge.canRedo() }

    Box(Modifier.fillMaxSize()) {
        AndroidView(
            modifier = Modifier.fillMaxSize(),
            factory = { context -> ModelingSurfaceView(context).apply { onSceneChanged = refresh } }
        )

        Row(
            modifier = Modifier
                .align(Alignment.TopCenter)
                .systemBarsPadding()
                .padding(8.dp),
            horizontalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            FilledTonalButton(onClick = { NativeBridge.undo(); refresh() }, enabled = canUndo) {
                Text("Desfazer")
            }
            FilledTonalButton(onClick = { NativeBridge.redo(); refresh() }, enabled = canRedo) {
                Text("Refazer")
            }
            Button(onClick = { showAi = !showAi }) { Text("IA") }
        }

        if (showAi) {
            AiPanel(
                onClose = { showAi = false },
                onSceneChanged = refresh,
                modifier = Modifier.align(Alignment.BottomCenter)
            )
        } else {
            Column(
                modifier = Modifier
                    .align(Alignment.BottomCenter)
                    .fillMaxWidth()
                    .background(Color(0xCC1E1E24))
                    .systemBarsPadding()
                    .padding(8.dp),
                verticalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                if (selected >= 0) SelectionTools(selected, refresh)
                PrimitiveBar(refresh)
            }
        }
    }
}

@Composable
private fun ToolButton(label: String, onClick: () -> Unit) {
    FilledTonalButton(
        onClick = onClick,
        contentPadding = PaddingValues(horizontal = 12.dp, vertical = 6.dp)
    ) { Text(label) }
}

@Composable
private fun SelectionTools(id: Int, refresh: () -> Unit) {
    val scroll1 = rememberScrollState()
    val scroll2 = rememberScrollState()
    val scroll3 = rememberScrollState()

    Row(Modifier.horizontalScroll(scroll1), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
        ToolButton("X-") { NativeBridge.moveBy(id, -MOVE_STEP, 0f, 0f); refresh() }
        ToolButton("X+") { NativeBridge.moveBy(id, MOVE_STEP, 0f, 0f); refresh() }
        ToolButton("Y-") { NativeBridge.moveBy(id, 0f, -MOVE_STEP, 0f); refresh() }
        ToolButton("Y+") { NativeBridge.moveBy(id, 0f, MOVE_STEP, 0f); refresh() }
        ToolButton("Z-") { NativeBridge.moveBy(id, 0f, 0f, -MOVE_STEP); refresh() }
        ToolButton("Z+") { NativeBridge.moveBy(id, 0f, 0f, MOVE_STEP); refresh() }
    }

    Row(Modifier.horizontalScroll(scroll2), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
        ToolButton("Rodar -15") { NativeBridge.rotateBy(id, 0f, -15f, 0f); refresh() }
        ToolButton("Rodar +15") { NativeBridge.rotateBy(id, 0f, 15f, 0f); refresh() }
        ToolButton("Maior") { NativeBridge.scaleBy(id, 1.1f, 1.1f, 1.1f); refresh() }
        ToolButton("Menor") { NativeBridge.scaleBy(id, 0.9f, 0.9f, 0.9f); refresh() }
        ToolButton("Apagar") { NativeBridge.deleteObject(id); refresh() }
    }

    Row(Modifier.horizontalScroll(scroll3), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
        palette.forEach { (r, g, b) ->
            Button(
                onClick = { NativeBridge.setColor(id, r, g, b); refresh() },
                colors = ButtonDefaults.buttonColors(containerColor = Color(r, g, b)),
                contentPadding = PaddingValues(horizontal = 18.dp, vertical = 6.dp)
            ) { Text(" ") }
        }
    }
}

@Composable
private fun PrimitiveBar(refresh: () -> Unit) {
    Row(
        Modifier.horizontalScroll(rememberScrollState()),
        horizontalArrangement = Arrangement.spacedBy(6.dp)
    ) {
        PrimitiveType.values().forEach { type ->
            ToolButton("+ ${type.label}") {
                val size = 1f
                val y = if (type == PrimitiveType.PLANE) 0f else size / 2f
                val id = NativeBridge.addPrimitive(type.id, size, 0f, y, 0f)
                if (id >= 0) NativeBridge.select(id)
                refresh()
            }
        }
    }
}
