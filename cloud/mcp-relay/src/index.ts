interface Env { LINKS: DurableObjectNamespace }
const SECRET=/^[A-Za-z0-9_-]{43}$/;
export default {async fetch(req:Request,env:Env):Promise<Response>{
 const u=new URL(req.url),device=u.pathname==="/device",m=u.pathname.match(/^\/c\/([A-Za-z0-9_-]{43})\/mcp\/?$/);
 if(device){const s=u.searchParams.get("link")||"";if(!SECRET.test(s)||req.headers.get("Upgrade")?.toLowerCase()!=="websocket")return new Response("bad device link",{status:400});return env.LINKS.get(env.LINKS.idFromName(s)).fetch(req);}
 if(!m)return new Response("Manus 3D MCP relay",{status:404});
 if(!["POST","GET","DELETE"].includes(req.method))return new Response("method not allowed",{status:405});
 return env.LINKS.get(env.LINKS.idFromName(m[1])).fetch(req);
}};
export class McpLink {
 private socket?:WebSocket; private pending=new Map<string,{resolve:(r:Response)=>void;parts:string[];status:number;headers:Headers}>();
 constructor(private state:DurableObjectState,private env:Env){}
 async fetch(req:Request):Promise<Response>{
  if(new URL(req.url).pathname==="/device"){const pair=new WebSocketPair(),client=pair[0],server=pair[1];this.state.acceptWebSocket(server);this.socket=server;server.addEventListener("message",e=>this.receive(String(e.data)));server.addEventListener("close",()=>{if(this.socket===server)this.socket=undefined;});return new Response(null,{status:101,webSocket:client});}
  if(!this.socket||this.socket.readyState!==WebSocket.OPEN)return new Response(JSON.stringify({error:"Manus app offline; enable MCP Cloud in app"}),{status:503,headers:{"content-type":"application/json"}});
  const device=this.socket; const id=crypto.randomUUID(),body=req.method==="GET"?"":await req.text(),headers:Record<string,string>={};
  for(const k of ["accept","content-type","mcp-session-id","mcp-protocol-version","last-event-id"]){const v=req.headers.get(k);if(v)headers[k]=v;}
  const n=Math.max(1,Math.ceil(body.length/180000));const promise=new Promise<Response>(resolve=>this.pending.set(id,{resolve,parts:new Array(n),status:502,headers:new Headers({"content-type":"application/json"})}));
  for(let i=0;i<n;i++)device.send(JSON.stringify({t:"req",id,method:req.method,headers,i,n,data:body.slice(i*180000,(i+1)*180000)}));
  const timer=setTimeout(()=>{const p=this.pending.get(id);if(p){this.pending.delete(id);p.resolve(new Response(JSON.stringify({error:"MCP call timed out"}),{status:504}));}},115000);
  const result=await promise;clearTimeout(timer);return result;
 }
 private receive(raw:string){let f:any;try{f=JSON.parse(raw);}catch{return;}if(f.t!=="res"||typeof f.id!=="string")return;const p=this.pending.get(f.id);if(!p)return;const i=Number(f.i);if(!Number.isInteger(i)||i<0||i>=p.parts.length)return;p.parts[i]=String(f.data||"");if(i===0){p.status=Number(f.status)||502;p.headers=new Headers();for(const [k,v]of Object.entries(f.headers||{}))if(typeof v==="string")p.headers.set(k,v);}if(p.parts.every(x=>x!==undefined)){this.pending.delete(f.id);p.resolve(new Response(p.parts.join(""),{status:p.status,headers:p.headers}));}}
}
