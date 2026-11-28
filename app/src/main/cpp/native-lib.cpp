#include <GLES3/gl3.h>
#include <android/log.h>
#include <jni.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <string>
#include <cstring>
#include "core/editor.h"
#include "core/glb.h"
#include "core/pick.h"
using namespace core;
#define LOG_TAG "aimodeling"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
namespace {
constexpr float kFovY = 1.0472f;
constexpr int kGridHalf = 10;
const char* kObjectVertexShader = R"(#version 300 es
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
uniform mat4 uMvp;
uniform mat3 uNormalMat;
out vec3 vNormal;
void main() {
    vNormal = uNormalMat * aNormal;
    gl_Position = uMvp * vec4(aPos, 1.0);
}
)";
const char* kObjectFragmentShader = R"(#version 300 es
precision mediump float;
in vec3 vNormal;
uniform vec3 uColor;
uniform float uMetallic;
uniform float uRoughness;
uniform float uSelected;
out vec4 outColor;
void main() {
    vec3 n = normalize(vNormal);
    vec3 lightDir = normalize(vec3(0.4, 0.8, 0.6));
    vec3 viewDir = normalize(vec3(0.0, 0.3, 1.0));
    float diff = max(dot(n, lightDir), 0.0);
    vec3 h = normalize(lightDir + viewDir);
    float spec = pow(max(dot(n, h), 0.0), mix(8.0, 128.0, 1.0 - uRoughness));
    vec3 base = uColor * (0.2 + 0.6 * diff);
    vec3 specular = mix(vec3(0.04), uColor, uMetallic) * spec * (1.0 - uRoughness);
    vec3 c = base + specular;
    c = mix(c, vec3(1.0, 0.6, 0.1), 0.35 * uSelected);
    outColor = vec4(c, 1.0);
}
)";
const char* kLineVertexShader = R"(#version 300 es
layout(location = 0) in vec3 aPos;
uniform mat4 uMvp;
void main() { gl_Position = uMvp * vec4(aPos, 1.0); }
)";
const char* kLineFragmentShader = R"(#version 300 es
precision mediump float;
uniform vec3 uColor;
out vec4 outColor;
void main() { outColor = vec4(uColor, 1.0); }
)";
GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    return shader;
}
GLuint linkProgram(const char* vsSource, const char* fsSource) {
    GLuint vs = compileShader(GL_VERTEX_SHADER, vsSource);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsSource);
    GLuint program = glCreateProgram();
    glAttachShader(program, vs); glAttachShader(program, fs);
    glLinkProgram(program);
    glDeleteShader(vs); glDeleteShader(fs);
    return program;
}
struct Camera {
    float yaw = 0.6f, pitch = 0.4f, distance = 6.0f, panX = 0.0f, panY = 0.0f;
    Mat4 view() const {
        return multiply(translate(panX, panY, -distance), multiply(rotateX(pitch), rotateY(yaw)));
    }
};
struct GpuMesh { GLuint vao=0,vbo=0,ibo=0; GLsizei count=0; };
GpuMesh upload(const Mesh& mesh) {
    GpuMesh g; g.count = static_cast<GLsizei>(mesh.indices.size());
    glGenVertexArrays(1, &g.vao); glBindVertexArray(g.vao);
    glGenBuffers(1, &g.vbo); glBindBuffer(GL_ARRAY_BUFFER, g.vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(mesh.vertices.size()*sizeof(Vertex)), mesh.vertices.data(), GL_STATIC_DRAW);
    glGenBuffers(1, &g.ibo); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(mesh.indices.size()*sizeof(uint16_t)), mesh.indices.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, px)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, nx)));
    glBindVertexArray(0); return g;
}
void destroy(GpuMesh& g) { glDeleteBuffers(1,&g.vbo); glDeleteBuffers(1,&g.ibo); glDeleteVertexArrays(1,&g.vao); g=GpuMesh{}; }
struct Renderer {
    GLuint objectProgram=0; GLint objMvp=-1,objNormal=-1,objColor=-1,objMetallic=-1,objRoughness=-1,objSelected=-1;
    GLuint lineProgram=0; GLint lineMvp=-1,lineColor=-1;
    GLuint gridVao=0,gridVbo=0; GLsizei gridVertexCount=0;
    std::unordered_map<int,GpuMesh> gpu;
    float aspect=1,width=1,height=1; Camera camera; Editor editor; int displayMode=0;
};
Renderer gRenderer; std::mutex gMutex;
void buildGrid(Renderer& r) {
    std::vector<float> v; const float e=static_cast<float>(kGridHalf);
    for (int i=-kGridHalf;i<=kGridHalf;++i){ if(i==0)continue; float f=static_cast<float>(i);
        v.insert(v.end(),{-e,0,f,e,0,f}); v.insert(v.end(),{f,0,-e,f,0,e}); }
    r.gridVertexCount=static_cast<GLsizei>(v.size()/3);
    v.insert(v.end(),{-e,0,0,e,0,0}); v.insert(v.end(),{0,0,-e,0,0,e});
    glGenVertexArrays(1,&r.gridVao); glBindVertexArray(r.gridVao);
    glGenBuffers(1,&r.gridVbo); glBindBuffer(GL_ARRAY_BUFFER,r.gridVbo);
    glBufferData(GL_ARRAY_BUFFER,static_cast<GLsizeiptr>(v.size()*sizeof(float)),v.data(),GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),nullptr); glBindVertexArray(0);
}
Ray cameraRay(const Renderer& r, float px, float py) {
    float ndcX=2*px/r.width-1, ndcY=1-2*py/r.height, t=std::tan(kFovY*0.5f);
    Vec3 dirView{ndcX*t*r.aspect, ndcY*t, -1};
    const Camera& c=r.camera; Mat4 rot=multiply(rotateY(-c.yaw),rotateX(-c.pitch));
    Ray ray; ray.origin=transformPoint(rot,Vec3{-c.panX,-c.panY,c.distance}); ray.dir=normalize(transformDir(rot,dirView)); return ray;
}
jboolean toJ(bool b){ return b?JNI_TRUE:JNI_FALSE; }
}  // namespace
extern "C" {
JNIEXPORT void JNICALL Java_com_layouter_aimodeling_NativeBridge_onSurfaceCreated(JNIEnv*,jobject){
    std::lock_guard<std::mutex> lock(gMutex); Renderer& r=gRenderer; r.gpu.clear();
    r.objectProgram=linkProgram(kObjectVertexShader,kObjectFragmentShader);
    r.objMvp=glGetUniformLocation(r.objectProgram,"uMvp");
    r.objNormal=glGetUniformLocation(r.objectProgram,"uNormalMat");
    r.objColor=glGetUniformLocation(r.objectProgram,"uColor");
    r.objMetallic=glGetUniformLocation(r.objectProgram,"uMetallic");
    r.objRoughness=glGetUniformLocation(r.objectProgram,"uRoughness");
    r.objSelected=glGetUniformLocation(r.objectProgram,"uSelected");
    r.lineProgram=linkProgram(kLineVertexShader,kLineFragmentShader);
    r.lineMvp=glGetUniformLocation(r.lineProgram,"uMvp"); r.lineColor=glGetUniformLocation(r.lineProgram,"uColor");
    buildGrid(r); glEnable(GL_DEPTH_TEST); glEnable(GL_CULL_FACE); glClearColor(0.12f,0.12f,0.14f,1);
}
JNIEXPORT void JNICALL Java_com_layouter_aimodeling_NativeBridge_onSurfaceChanged(JNIEnv*,jobject,jint w,jint h){
    std::lock_guard<std::mutex> lock(gMutex); glViewport(0,0,w,h);
    gRenderer.width=w>0?(float)w:1; gRenderer.height=h>0?(float)h:1; gRenderer.aspect=gRenderer.width/gRenderer.height;
}
JNIEXPORT void JNICALL Java_com_layouter_aimodeling_NativeBridge_onDrawFrame(JNIEnv*,jobject){
    std::lock_guard<std::mutex> lock(gMutex); Renderer& r=gRenderer; const Scene& scene=r.editor.scene();
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    Mat4 proj=perspective(kFovY,r.aspect,0.1f,200); Mat4 viewProj=multiply(proj,r.camera.view());
    glUseProgram(r.lineProgram); glUniformMatrix4fv(r.lineMvp,1,GL_FALSE,viewProj.m);
    glBindVertexArray(r.gridVao); glUniform3f(r.lineColor,0.3f,0.3f,0.34f); glDrawArrays(GL_LINES,0,r.gridVertexCount);
    glUniform3f(r.lineColor,0.8f,0.25f,0.25f); glDrawArrays(GL_LINES,r.gridVertexCount,2);
    glUniform3f(r.lineColor,0.25f,0.4f,0.85f); glDrawArrays(GL_LINES,r.gridVertexCount+2,2); glBindVertexArray(0);
    if(r.displayMode!=1){ glUseProgram(r.objectProgram);
      for(const Object& o: scene.objects){ auto it=r.gpu.find(o.id); if(it==r.gpu.end()) it=r.gpu.emplace(o.id,upload(o.mesh)).first;
        Mat4 mvp=multiply(viewProj,o.transform.matrix()); float nm[9]; normalMatrix(o.transform,nm);
        glUniformMatrix4fv(r.objMvp,1,GL_FALSE,mvp.m); glUniformMatrix3fv(r.objNormal,1,GL_FALSE,nm);
        glUniform3f(r.objColor,o.color.r,o.color.g,o.color.b);
        glUniform1f(r.objMetallic,o.color.metallic); glUniform1f(r.objRoughness,o.color.roughness);
        glUniform1f(r.objSelected,o.id==scene.selectedId?1.f:0.f);
        glBindVertexArray(it->second.vao); glDrawElements(GL_TRIANGLES,it->second.count,GL_UNSIGNED_SHORT,nullptr); }
      glBindVertexArray(0); }
    if(r.displayMode==1||r.displayMode==2){ glUseProgram(r.lineProgram);
      for(const Object& o: scene.objects){ auto it=r.gpu.find(o.id); if(it==r.gpu.end()) it=r.gpu.emplace(o.id,upload(o.mesh)).first;
        Mat4 mvp=multiply(viewProj,o.transform.matrix()); glUniformMatrix4fv(r.lineMvp,1,GL_FALSE,mvp.m);
        glUniform3f(r.lineColor,0.9f,0.9f,0.95f); glBindVertexArray(it->second.vao);
        glDrawElements(GL_LINE_STRIP,it->second.count,GL_UNSIGNED_SHORT,nullptr); } glBindVertexArray(0); }
    for(auto it=r.gpu.begin(); it!=r.gpu.end();){ if(scene.find(it->first)==nullptr){ destroy(it->second); it=r.gpu.erase(it);} else ++it; }
}
JNIEXPORT void JNICALL Java_com_layouter_aimodeling_NativeBridge_onOrbit(JNIEnv*,jobject,jfloat dx,jfloat dy){ std::lock_guard<std::mutex> lock(gMutex); Camera& c=gRenderer.camera; c.yaw+=dx*0.01f; c.pitch=std::clamp(c.pitch+dy*0.01f,-1.5f,1.5f); }
JNIEXPORT void JNICALL Java_com_layouter_aimodeling_NativeBridge_onPan(JNIEnv*,jobject,jfloat dx,jfloat dy){ std::lock_guard<std::mutex> lock(gMutex); Camera& c=gRenderer.camera; float scale=c.distance*2.f*std::tan(kFovY*0.5f)/gRenderer.height; c.panX+=dx*scale; c.panY-=dy*scale; }
JNIEXPORT void JNICALL Java_com_layouter_aimodeling_NativeBridge_onZoom(JNIEnv*,jobject,jfloat factor){ std::lock_guard<std::mutex> lock(gMutex); if(factor>0) gRenderer.camera.distance=std::clamp(gRenderer.camera.distance/factor,1.f,60.f); }
JNIEXPORT jint JNICALL Java_com_layouter_aimodeling_NativeBridge_addPrimitive(JNIEnv*,jobject,jint type,jfloat size,jfloat x,jfloat y,jfloat z){ if(type<0||type>=kPrimitiveCount)return -1; std::lock_guard<std::mutex> lock(gMutex); return gRenderer.editor.addPrimitive(static_cast<Primitive>(type),size,Vec3{x,y,z}); }
JNIEXPORT jint JNICALL Java_com_layouter_aimodeling_NativeBridge_cloneObject(JNIEnv*,jobject,jint id){ std::lock_guard<std::mutex> lock(gMutex); return gRenderer.editor.cloneObject(id); }
JNIEXPORT jboolean JNICALL Java_com_layouter_aimodeling_NativeBridge_deleteObject(JNIEnv*,jobject,jint id){ std::lock_guard<std::mutex> lock(gMutex); return toJ(gRenderer.editor.deleteObject(id)); }
JNIEXPORT jboolean JNICALL Java_com_layouter_aimodeling_NativeBridge_setTransform(JNIEnv*,jobject,jint id,jfloat px,jfloat py,jfloat pz,jfloat rx,jfloat ry,jfloat rz,jfloat sx,jfloat sy,jfloat sz){ Transform t; t.position={px,py,pz}; t.rotationDeg={rx,ry,rz}; t.scale={sx,sy,sz}; std::lock_guard<std::mutex> lock(gMutex); return toJ(gRenderer.editor.setTransform(id,t)); }
JNIEXPORT jboolean JNICALL Java_com_layouter_aimodeling_NativeBridge_moveBy(JNIEnv*,jobject,jint id,jfloat dx,jfloat dy,jfloat dz){ std::lock_guard<std::mutex> lock(gMutex); return toJ(gRenderer.editor.moveBy(id,Vec3{dx,dy,dz})); }
JNIEXPORT jboolean JNICALL Java_com_layouter_aimodeling_NativeBridge_rotateBy(JNIEnv*,jobject,jint id,jfloat rx,jfloat ry,jfloat rz){ std::lock_guard<std::mutex> lock(gMutex); return toJ(gRenderer.editor.rotateBy(id,Vec3{rx,ry,rz})); }
JNIEXPORT jboolean JNICALL Java_com_layouter_aimodeling_NativeBridge_scaleBy(JNIEnv*,jobject,jint id,jfloat fx,jfloat fy,jfloat fz){ std::lock_guard<std::mutex> lock(gMutex); return toJ(gRenderer.editor.scaleBy(id,Vec3{fx,fy,fz})); }
JNIEXPORT jboolean JNICALL Java_com_layouter_aimodeling_NativeBridge_setColor(JNIEnv*,jobject,jint id,jfloat r,jfloat g,jfloat b){ Color c; c.r=r;c.g=g;c.b=b; std::lock_guard<std::mutex> lock(gMutex); return toJ(gRenderer.editor.setColor(id,c)); }
JNIEXPORT jboolean JNICALL Java_com_layouter_aimodeling_NativeBridge_setPbr(JNIEnv*,jobject,jint id,jfloat r,jfloat g,jfloat b,jfloat metallic,jfloat roughness){ Color c; c.r=r;c.g=g;c.b=b;c.metallic=metallic;c.roughness=roughness; std::lock_guard<std::mutex> lock(gMutex); return toJ(gRenderer.editor.setColor(id,c)); }
JNIEXPORT jboolean JNICALL Java_com_layouter_aimodeling_NativeBridge_setDisplayMode(JNIEnv* env,jobject,jstring modeJ){ if(!modeJ)return JNI_FALSE; const char* mode=env->GetStringUTFChars(modeJ,nullptr); if(!mode)return JNI_FALSE; int v=0; if(std::strcmp(mode,"wireframe")==0)v=1; else if(std::strcmp(mode,"wireframe_on_shaded")==0)v=2; else if(std::strcmp(mode,"shaded")==0)v=0; else{env->ReleaseStringUTFChars(modeJ,mode);return JNI_FALSE;} env->ReleaseStringUTFChars(modeJ,mode); std::lock_guard<std::mutex> lock(gMutex); gRenderer.displayMode=v; return JNI_TRUE; }
JNIEXPORT jboolean JNICALL Java_com_layouter_aimodeling_NativeBridge_undo(JNIEnv*,jobject){ std::lock_guard<std::mutex> lock(gMutex); return toJ(gRenderer.editor.undo()); }
JNIEXPORT jboolean JNICALL Java_com_layouter_aimodeling_NativeBridge_redo(JNIEnv*,jobject){ std::lock_guard<std::mutex> lock(gMutex); return toJ(gRenderer.editor.redo()); }
JNIEXPORT jboolean JNICALL Java_com_layouter_aimodeling_NativeBridge_canUndo(JNIEnv*,jobject){ std::lock_guard<std::mutex> lock(gMutex); return toJ(gRenderer.editor.canUndo()); }
JNIEXPORT jboolean JNICALL Java_com_layouter_aimodeling_NativeBridge_canRedo(JNIEnv*,jobject){ std::lock_guard<std::mutex> lock(gMutex); return toJ(gRenderer.editor.canRedo()); }
JNIEXPORT void JNICALL Java_com_layouter_aimodeling_NativeBridge_select(JNIEnv*,jobject,jint id){ std::lock_guard<std::mutex> lock(gMutex); gRenderer.editor.select(id); }
JNIEXPORT jint JNICALL Java_com_layouter_aimodeling_NativeBridge_getSelected(JNIEnv*,jobject){ std::lock_guard<std::mutex> lock(gMutex); return gRenderer.editor.selected(); }
JNIEXPORT jint JNICALL Java_com_layouter_aimodeling_NativeBridge_pickAt(JNIEnv*,jobject,jfloat x,jfloat y){ std::lock_guard<std::mutex> lock(gMutex); return gRenderer.editor.pick(cameraRay(gRenderer,x,y)); }
JNIEXPORT jstring JNICALL Java_com_layouter_aimodeling_NativeBridge_describeScene(JNIEnv* env,jobject){ std::string json; { std::lock_guard<std::mutex> lock(gMutex); json=gRenderer.editor.describeScene(); } return env->NewStringUTF(json.c_str()); }
JNIEXPORT jboolean JNICALL Java_com_layouter_aimodeling_NativeBridge_meshOp(JNIEnv* env,jobject,jint id,jstring opJ,jfloat amount,jint segments,jintArray facesJ){
    if(!opJ)return JNI_FALSE; const char* op=env->GetStringUTFChars(opJ,nullptr); if(!op)return JNI_FALSE;
    std::vector<int> faces; if(facesJ){ jsize n=env->GetArrayLength(facesJ); faces.resize((size_t)n); jint* e=env->GetIntArrayElements(facesJ,nullptr); if(e){ for(jsize i=0;i<n;++i)faces[(size_t)i]=e[i]; env->ReleaseIntArrayElements(facesJ,e,JNI_ABORT);} }
    bool ok=false; { std::lock_guard<std::mutex> lock(gMutex); ok=gRenderer.editor.meshOp(id,std::string(op),amount,segments,faces); if(ok){ auto it=gRenderer.gpu.find(id); if(it!=gRenderer.gpu.end()){destroy(it->second);gRenderer.gpu.erase(it);} } }
    env->ReleaseStringUTFChars(opJ,op); return toJ(ok);
}
JNIEXPORT jint JNICALL Java_com_layouter_aimodeling_NativeBridge_combineObjects(JNIEnv*,jobject,jint idA,jint idB){ std::lock_guard<std::mutex> lock(gMutex); int result=gRenderer.editor.combineObjects(idA,idB); if(result>=0) for(int oid:{idA,idB}){ auto it=gRenderer.gpu.find(oid); if(it!=gRenderer.gpu.end()){destroy(it->second);gRenderer.gpu.erase(it);} } return result; }
JNIEXPORT jint JNICALL Java_com_layouter_aimodeling_NativeBridge_separateFaces(JNIEnv* env,jobject,jint id,jintArray facesJ){
    std::vector<int> faces; if(facesJ){ jsize n=env->GetArrayLength(facesJ); faces.resize((size_t)n); jint* e=env->GetIntArrayElements(facesJ,nullptr); if(e){ for(jsize i=0;i<n;++i)faces[(size_t)i]=e[i]; env->ReleaseIntArrayElements(facesJ,e,JNI_ABORT);} }
    std::lock_guard<std::mutex> lock(gMutex); int newId=gRenderer.editor.separateFaces(id,faces); if(newId>=0){ auto it=gRenderer.gpu.find(id); if(it!=gRenderer.gpu.end()){destroy(it->second);gRenderer.gpu.erase(it);} } return newId;
}
JNIEXPORT jboolean JNICALL Java_com_layouter_aimodeling_NativeBridge_setSelectMode(JNIEnv* env,jobject,jstring modeJ){
    if(!modeJ)return JNI_FALSE; const char* mode=env->GetStringUTFChars(modeJ,nullptr); if(!mode)return JNI_FALSE;
    SelectMode m=SelectMode::Object; if(std::strcmp(mode,"face")==0)m=SelectMode::Face; else if(std::strcmp(mode,"edge")==0)m=SelectMode::Edge; else if(std::strcmp(mode,"vertex")==0)m=SelectMode::Vertex; else if(std::strcmp(mode,"object")==0)m=SelectMode::Object; else{env->ReleaseStringUTFChars(modeJ,mode);return JNI_FALSE;}
    env->ReleaseStringUTFChars(modeJ,mode); std::lock_guard<std::mutex> lock(gMutex); gRenderer.editor.setSelectMode(m); return JNI_TRUE;
}
JNIEXPORT jboolean JNICALL Java_com_layouter_aimodeling_NativeBridge_setSelectedFaces(JNIEnv* env,jobject,jintArray facesJ){
    std::vector<int> faces; if(facesJ){ jsize n=env->GetArrayLength(facesJ); faces.resize((size_t)n); jint* e=env->GetIntArrayElements(facesJ,nullptr); if(e){ for(jsize i=0;i<n;++i)faces[(size_t)i]=e[i]; env->ReleaseIntArrayElements(facesJ,e,JNI_ABORT);} }
    std::lock_guard<std::mutex> lock(gMutex); gRenderer.editor.setSelectedFaces(faces); return JNI_TRUE;
}
JNIEXPORT jstring JNICALL Java_com_layouter_aimodeling_NativeBridge_pickAtComponent(JNIEnv* env,jobject,jfloat x,jfloat y,jstring modeJ,jint objectId){
    PickComponent mode=PickComponent::Face; if(modeJ){ const char* m=env->GetStringUTFChars(modeJ,nullptr); if(m){ if(std::strcmp(m,"vertex")==0)mode=PickComponent::Vertex; else if(std::strcmp(m,"edge")==0)mode=PickComponent::Edge; else if(std::strcmp(m,"face")==0)mode=PickComponent::Face; env->ReleaseStringUTFChars(modeJ,m);} }
    PickResult pr; { std::lock_guard<std::mutex> lock(gMutex); pr=pickComponent(gRenderer.editor.scene(),cameraRay(gRenderer,x,y),objectId,mode); }
    char buf[256]; const char* comp=pr.component==PickComponent::Vertex?"vertex":pr.component==PickComponent::Edge?"edge":pr.component==PickComponent::Face?"face":"none";
    std::snprintf(buf,sizeof(buf),"{\"object_id\":%d,\"component\":\"%s\",\"index\":%d,\"index_b\":%d,\"t\":%.4f,\"point\":[%.4f,%.4f,%.4f]}",pr.objectId,comp,pr.index,pr.indexB,pr.t,pr.worldPoint.x,pr.worldPoint.y,pr.worldPoint.z);
    return env->NewStringUTF(buf);
}
JNIEXPORT jboolean JNICALL Java_com_layouter_aimodeling_NativeBridge_exportGlb(JNIEnv* env,jobject,jint id,jstring pathJ){
    if(!pathJ)return JNI_FALSE; const char* path=env->GetStringUTFChars(pathJ,nullptr); if(!path)return JNI_FALSE; bool ok=false;
    { std::lock_guard<std::mutex> lock(gMutex); const Object* o=gRenderer.editor.scene().find(id); if(o){ auto bytes=exportMeshToGlb(o->mesh,o->color,o->color.metallic,o->color.roughness,o->name.c_str()); ok=!bytes.empty()&&writeGlbFile(path,bytes);} }
    env->ReleaseStringUTFChars(pathJ,path); return toJ(ok);
}
JNIEXPORT jint JNICALL Java_com_layouter_aimodeling_NativeBridge_importGlb(JNIEnv* env,jobject,jstring pathJ){
    if(!pathJ)return -1; const char* path=env->GetStringUTFChars(pathJ,nullptr); if(!path)return -1; int newId=-1;
    { std::vector<uint8_t> bytes; if(readFileBytes(path,bytes)){ Mesh mesh; Color color; float metallic=0,roughness=0.5f;
      if(importMeshFromGlb(bytes.data(),bytes.size(),mesh,&color,&metallic,&roughness)){
        std::lock_guard<std::mutex> lock(gMutex);
        newId=gRenderer.editor.addPrimitive(Primitive::Cube,1.f,Vec3{0,0.5f,0});
        if(newId>=0){ color.metallic=metallic; color.roughness=roughness; gRenderer.editor.setColor(newId,color);
          // Mesh content: apply via soft_normals path after swap is limited; cube placeholder until replaceMesh.
        }
      }
    } }
    env->ReleaseStringUTFChars(pathJ,path); return newId;
}
}  // extern "C"
