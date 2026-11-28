package com.layouter.aimodeling

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalClipboardManager
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.AnnotatedString
import androidx.compose.ui.unit.dp
import com.layouter.aimodeling.ai.McpCloudLinkManager

@Composable
fun McpCloudPanel(){
 val context=LocalContext.current
 val clipboard=LocalClipboardManager.current
 var base by remember{mutableStateOf(McpCloudLinkManager.relayBase())}
 var link by remember{mutableStateOf(McpCloudLinkManager.url())}
 var message by remember{mutableStateOf("")}
 Column(Modifier.fillMaxWidth(),verticalArrangement=Arrangement.spacedBy(8.dp)){
  Text("MCP Cloud · Claude",style=MaterialTheme.typography.titleSmall)
  Text("Publica primeiro o relay em cloud/mcp-relay. Depois cola aqui o hostname HTTPS devolvido pelo deploy.",style=MaterialTheme.typography.bodySmall,color=MaterialTheme.colorScheme.onSurfaceVariant)
  OutlinedTextField(value=base,onValueChange={base=it},modifier=Modifier.fillMaxWidth(),singleLine=true,label={Text("Base URL do relay próprio")},placeholder={Text("https://seu-relay.workers.dev")})
  Row(horizontalArrangement=Arrangement.spacedBy(8.dp)){
   Button(onClick={runCatching{McpCloudLinkManager.setRelayBase(base);link=McpCloudLinkManager.start();message="Relay iniciado: copia a URL para o Claude."}.onFailure{message=it.message?: "Não foi possível iniciar."}},modifier=Modifier.weight(1f)){Text("Ligar / gerar URL")}
   OutlinedButton(onClick={runCatching{link=McpCloudLinkManager.regenerate();message="Link secreto renovado."}.onFailure{message=it.message?: "Falha ao renovar."}},modifier=Modifier.weight(1f)){Text("Revogar e renovar")}
  }
  if(link.isNotBlank()){Text(link,style=MaterialTheme.typography.bodySmall);OutlinedButton(onClick={clipboard.setText(AnnotatedString(link));message="URL copiada."}){Text("Copiar URL para Claude")}}
  if(message.isNotBlank())Text(message,style=MaterialTheme.typography.labelSmall,color=MaterialTheme.colorScheme.primary)
 }
}
