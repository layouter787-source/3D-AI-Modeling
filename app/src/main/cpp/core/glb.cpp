#include "core/glb.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cmath>

namespace core {

namespace {

#pragma pack(push, 1)
struct GlbHeader {
    uint32_t magic;
    uint32_t version;
    uint32_t length;
};
#pragma pack(pop)

constexpr uint32_t kGlbMagic = 0x46546C67u;
constexpr uint32_t kJsonChunk = 0x4E4F534Au;
constexpr uint32_t kBinChunk = 0x004E4942u;

void appendU32(std::vector<uint8_t>& b, uint32_t v) {
    b.push_back(static_cast<uint8_t>(v & 0xff));
    b.push_back(static_cast<uint8_t>((v >> 8) & 0xff));
    b.push_back(static_cast<uint8_t>((v >> 16) & 0xff));
    b.push_back(static_cast<uint8_t>((v >> 24) & 0xff));
}

void appendF32(std::vector<uint8_t>& b, float v) {
    uint32_t u;
    std::memcpy(&u, &v, 4);
    appendU32(b, u);
}

void pad4(std::vector<uint8_t>& b, uint8_t padByte) {
    while (b.size() % 4 != 0) b.push_back(padByte);
}

int findCountAfter(const std::string& json, size_t from, int nth) {
    size_t search = from;
    int found = 0;
    const std::string key = "\"count\":";
    while (search < json.size()) {
        size_t c = json.find(key, search);
        if (c == std::string::npos) break;
        c += key.size();
        int val = 0;
        if (std::sscanf(json.c_str() + c, "%d", &val) == 1) {
            if (found == nth) return val;
            found++;
        }
        search = c + 1;
    }
    return 0;
}

}  // namespace

std::vector<uint8_t> exportMeshToGlb(const Mesh& mesh, const Color& color, float metallic,
                                     float roughness, const char* name) {
    if (mesh.vertices.empty() || mesh.indices.size() < 3) return {};

    const size_t nV = mesh.vertices.size();
    const size_t nI = mesh.indices.size();

    std::vector<uint8_t> bin;
    bin.reserve(nV * 24 + nI * 4 + 16);
    for (const Vertex& v : mesh.vertices) {
        appendF32(bin, v.px);
        appendF32(bin, v.py);
        appendF32(bin, v.pz);
    }
    const size_t normalOffset = bin.size();
    for (const Vertex& v : mesh.vertices) {
        appendF32(bin, v.nx);
        appendF32(bin, v.ny);
        appendF32(bin, v.nz);
    }
    const size_t indexOffset = bin.size();
    for (uint32_t idx : mesh.indices) appendU32(bin, idx);
    pad4(bin, 0);

    float minP[3] = {1e30f, 1e30f, 1e30f};
    float maxP[3] = {-1e30f, -1e30f, -1e30f};
    for (const Vertex& v : mesh.vertices) {
        minP[0] = std::min(minP[0], v.px);
        minP[1] = std::min(minP[1], v.py);
        minP[2] = std::min(minP[2], v.pz);
        maxP[0] = std::max(maxP[0], v.px);
        maxP[1] = std::max(maxP[1], v.py);
        maxP[2] = std::max(maxP[2], v.pz);
    }

    char json[2048];
    const char* meshName = name && name[0] ? name : "mesh";
    std::snprintf(
        json, sizeof(json),
        "{\"asset\":{\"version\":\"2.0\",\"generator\":\"3D-AI-Modeling\"},"
        "\"scenes\":[{\"nodes\":[0]}],\"scene\":0,"
        "\"nodes\":[{\"mesh\":0,\"name\":\"%s\"}],"
        "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0,\"NORMAL\":1},"
        "\"indices\":2,\"material\":0}]}],"
        "\"materials\":[{\"pbrMetallicRoughness\":{"
        "\"baseColorFactor\":[%.4f,%.4f,%.4f,1.0],"
        "\"metallicFactor\":%.4f,\"roughnessFactor\":%.4f},\"name\":\"mat\"}],"
        "\"accessors\":[{"
        "\"bufferView\":0,\"componentType\":5126,\"count\":%zu,\"type\":\"VEC3\","
        "\"max\":[%.6f,%.6f,%.6f],\"min\":[%.6f,%.6f,%.6f]},{"
        "\"bufferView\":1,\"componentType\":5126,\"count\":%zu,\"type\":\"VEC3\"},{"
        "\"bufferView\":2,\"componentType\":5125,\"count\":%zu,\"type\":\"SCALAR\"}],"
        "\"bufferViews\":[{"
        "\"buffer\":0,\"byteOffset\":0,\"byteLength\":%zu,\"target\":34962},{"
        "\"buffer\":0,\"byteOffset\":%zu,\"byteLength\":%zu,\"target\":34962},{"
        "\"buffer\":0,\"byteOffset\":%zu,\"byteLength\":%zu,\"target\":34963}],"
        "\"buffers\":[{\"byteLength\":%zu}]}",
        meshName, color.r, color.g, color.b, metallic, roughness, nV, maxP[0], maxP[1], maxP[2],
        minP[0], minP[1], minP[2], nV, nI, nV * 12, normalOffset, nV * 12, indexOffset, nI * 4,
        bin.size());

    std::vector<uint8_t> jsonBytes(json, json + std::strlen(json));
    pad4(jsonBytes, 0x20);

    std::vector<uint8_t> out;
    const uint32_t total =
        static_cast<uint32_t>(12 + 8 + jsonBytes.size() + 8 + bin.size());
    appendU32(out, kGlbMagic);
    appendU32(out, 2);
    appendU32(out, total);
    appendU32(out, static_cast<uint32_t>(jsonBytes.size()));
    appendU32(out, kJsonChunk);
    out.insert(out.end(), jsonBytes.begin(), jsonBytes.end());
    appendU32(out, static_cast<uint32_t>(bin.size()));
    appendU32(out, kBinChunk);
    out.insert(out.end(), bin.begin(), bin.end());
    return out;
}

bool importMeshFromGlb(const uint8_t* data, size_t size, Mesh& outMesh, Color* outColor,
                       float* outMetallic, float* outRoughness) {
    if (!data || size < 20) return false;
    uint32_t magic = 0, version = 0, length = 0;
    std::memcpy(&magic, data, 4);
    std::memcpy(&version, data + 4, 4);
    std::memcpy(&length, data + 8, 4);
    if (magic != kGlbMagic || version != 2 || length > size) return false;

    size_t offset = 12;
    const uint8_t* binData = nullptr;
    size_t binSize = 0;
    std::string json;

    while (offset + 8 <= size) {
        uint32_t chunkLen = 0, chunkType = 0;
        std::memcpy(&chunkLen, data + offset, 4);
        std::memcpy(&chunkType, data + offset + 4, 4);
        offset += 8;
        if (offset + chunkLen > size) return false;
        if (chunkType == kJsonChunk) {
            json.assign(reinterpret_cast<const char*>(data + offset), chunkLen);
        } else if (chunkType == kBinChunk) {
            binData = data + offset;
            binSize = chunkLen;
        }
        offset += chunkLen;
    }
    if (!binData || json.empty()) return false;

    if (outColor) {
        auto pos = json.find("baseColorFactor");
        if (pos != std::string::npos) {
            float r = 0.8f, g = 0.8f, b = 0.8f;
            if (std::sscanf(json.c_str() + pos, "baseColorFactor\":[%f,%f,%f", &r, &g, &b) >= 3) {
                outColor->r = r;
                outColor->g = g;
                outColor->b = b;
            }
        }
    }
    if (outMetallic) {
        auto pos = json.find("metallicFactor");
        float m = 0.0f;
        if (pos != std::string::npos &&
            std::sscanf(json.c_str() + pos, "metallicFactor\":%f", &m) == 1) {
            *outMetallic = m;
        }
    }
    if (outRoughness) {
        auto pos = json.find("roughnessFactor");
        float r = 0.5f;
        if (pos != std::string::npos &&
            std::sscanf(json.c_str() + pos, "roughnessFactor\":%f", &r) == 1) {
            *outRoughness = r;
        }
    }

    size_t vertCount = static_cast<size_t>(findCountAfter(json, 0, 0));
    size_t idxCount = static_cast<size_t>(findCountAfter(json, 0, 2));
    if (vertCount == 0 || vertCount * 12 > binSize) return false;

    const size_t posBytes = vertCount * 12;
    const size_t nrmBytes = vertCount * 12;
    if (posBytes + nrmBytes > binSize) return false;

    outMesh.vertices.clear();
    outMesh.indices.clear();
    outMesh.vertices.resize(vertCount);
    for (size_t i = 0; i < vertCount; ++i) {
        float p[3], n[3];
        std::memcpy(p, binData + i * 12, 12);
        std::memcpy(n, binData + posBytes + i * 12, 12);
        outMesh.vertices[i] = {p[0], p[1], p[2], n[0], n[1], n[2]};
    }

    size_t idxOffset = posBytes + nrmBytes;
    const size_t indexBytes = json.find("\"componentType\":5125") != std::string::npos ? 4u : 2u;
    if (idxOffset > binSize) return false;
    if (idxCount == 0) idxCount = (binSize - idxOffset) / indexBytes;
    if (idxCount > (binSize - idxOffset) / indexBytes) return false;
    outMesh.indices.resize(idxCount);
    for (size_t i = 0; i < idxCount; ++i) {
        uint32_t idx = 0;
        if (indexBytes == 4) std::memcpy(&idx, binData + idxOffset + i * 4, 4);
        else { uint16_t idx16 = 0; std::memcpy(&idx16, binData + idxOffset + i * 2, 2); idx = idx16; }
        if (idx >= vertCount) return false;
        outMesh.indices[i] = idx;
    }
    return !outMesh.indices.empty();
}

bool writeGlbFile(const std::string& path, const std::vector<uint8_t>& bytes) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const size_t n = std::fwrite(bytes.data(), 1, bytes.size(), f);
    std::fclose(f);
    return n == bytes.size();
}

bool readFileBytes(const std::string& path, std::vector<uint8_t>& out) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (sz <= 0) {
        std::fclose(f);
        return false;
    }
    out.resize(static_cast<size_t>(sz));
    size_t n = std::fread(out.data(), 1, out.size(), f);
    std::fclose(f);
    return n == out.size();
}

}  // namespace core
