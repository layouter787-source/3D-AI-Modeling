# Remote MCP connectors (Manus 3D AI)

The Android app can act as an MCP **client** for remote tool servers. The connector is available in the AI editor toolbar under **MCP**.

## Configuration format

Paste a CodeAssist-compatible `mcpServers` JSON object. Both `httpUrl` (Android Studio-style) and `url` with `type: "http"` (CodeAssist / portable MCP format) are accepted.

```json
{
  "mcpServers": {
    "documentation": {
      "type": "http",
      "url": "https://your-mcp-host.example/mcp",
      "headers": {
        "Authorization": "Bearer YOUR_TOKEN"
      }
    }
  }
}
```

- Remote HTTPS endpoints are supported; plain HTTP is allowed only for loopback development.
- `stdio` servers are not supported inside the Android app because it cannot launch arbitrary local MCP processes.
- Header values, including tokens, are encrypted through Android Keystore-backed `SecretStore`.
- The connector performs MCP initialize, `tools/list`, and `tools/call`; it forwards discovered tools into the same AI tool loop.
- The remote server must be reachable from the phone. A server on a PC's `localhost` is the PC, not the phone; use a reachable HTTPS endpoint or a deliberate local-network setup.
- Only connect servers you trust. Remote tools can access external accounts or modify data according to the server's own credentials and permissions.

## Scene inspection and agent verification

The native `inspect_scene` tool returns the current scene and a bounded topology preview for up to 32 objects. The agent is instructed to inspect before/after significant edits and compare tool results.

This is **structured geometry evidence**, not pixel-level vision of the rendered viewport. A later visual-observation feature must capture the rendered frame and pass it as an image to a vision-capable provider before the agent can truthfully claim to see the viewport.

## Native vs. research-only capabilities

The tool catalog distinguishes operations implemented in this app from research references. Blender/Godot add-ons and full Blender subsystems are not directly executable in the APK; each capability must be ported to the native mesh engine and tested, or remain clearly marked as not implemented.
