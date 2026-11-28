package com.layouter.aimodeling.ai

import android.content.Context
import java.net.URL

/** User-owned relay hostname and one revocable per-device capability URL. */
object McpCloudLinkManager {
 private lateinit var context:Context
 private var local:McpLocalServer?=null
 private var relay:McpRelayClient?=null
 fun init(ctx:Context){context=ctx.applicationContext}
 private fun prefs()=context.getSharedPreferences("mcp_cloud_link",Context.MODE_PRIVATE)
 fun relayBase()=prefs().getString("relay_base","")?: ""
 fun setRelayBase(value:String){
  val clean=value.trim().trimEnd('/')
  require(clean.startsWith("https://")){"Use o hostname HTTPS do deploy do relay."}
  URL(clean)
  prefs().edit().putString("relay_base",clean).apply()
 }
 fun start():String{
  check(::context.isInitialized){"MCP Cloud manager not initialized"}
  if(local==null){local=McpLocalServer().also{it.start(NanoHTTPD_TIMEOUT, false)}}
  val base=relayBase();require(base.isNotBlank()){"Configure primeiro o hostname do relay publicado."}
  val secret=SecretStore(context).get(SECRET_KEY).takeUnless{it.isNullOrBlank()}?:McpRelayClient.newSecret().also{SecretStore(context).put(SECRET_KEY,it)}
  relay?.close()
  relay=McpRelayClient(base,secret,secret).also{it.start()}
  return McpRelayClient.publicUrl(base,secret)
 }
 fun url():String{
  val secret=SecretStore(context).get(SECRET_KEY)?:return ""
  val base=relayBase();return if(base.isBlank())"" else McpRelayClient.publicUrl(base,secret)
 }
 fun stop(){relay?.close();relay=null;local?.stop();local=null}
 fun regenerate():String{stop();SecretStore(context).remove(SECRET_KEY);return start()}
 private const val SECRET_KEY="mcp_cloud_link_secret"
 private const val NanoHTTPD_TIMEOUT=10_000
}
