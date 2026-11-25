#include <pthread.h>

#include <algorithm>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "core/polymesh.h"

// Booleanas por BSP (algoritmo do csg.js). Funciona com malhas fechadas e com normais para fora.

namespace core {

namespace {

constexpr float kEps = 1e-5f;
constexpr size_t kMaxTriangles = 40000;

struct Plane {
    Vec3 n;
    float w = 0.0f;
    void flip() {
        n = -n;
        w = -w;
    }
};

struct Poly {
    std::vector<Vec3> v;
    Plane plane;
};

void splitPolygon(const Plane& pl, const Poly& poly, std::vector<Poly>& coplanarFront,
                  std::vector<Poly>& coplanarBack, std::vector<Poly>& front, std::vector<Poly>& back) {
    enum { COPLANAR = 0, FRONT = 1, BACK = 2, SPANNING = 3 };
    int polyType = 0;
    std::vector<int> types;
    types.reserve(poly.v.size());
    for (const Vec3& p : poly.v) {
        const float t = dot(pl.n, p) - pl.w;
        const int type = t < -kEps ? BACK : (t > kEps ? FRONT : COPLANAR);
        polyType |= type;
        types.push_back(type);
    }
    switch (polyType) {
        case COPLANAR:
            (dot(pl.n, poly.plane.n) > 0.0f ? coplanarFront : coplanarBack).push_back(poly);
            break;
        case FRONT:
            front.push_back(poly);
            break;
        case BACK:
            back.push_back(poly);
            break;
        default: {
            std::vector<Vec3> f, b;
            const size_t n = poly.v.size();
            for (size_t i = 0; i < n; ++i) {
                const size_t j = (i + 1) % n;
                const int ti = types[i], tj = types[j];
                const Vec3 vi = poly.v[i], vj = poly.v[j];
                if (ti != BACK) f.push_back(vi);
                if (ti != FRONT) b.push_back(vi);
                if ((ti | tj) == SPANNING) {
                    const float denom = dot(pl.n, vj - vi);
                    const float t = std::fabs(denom) < 1e-12f ? 0.5f : (pl.w - dot(pl.n, vi)) / denom;
                    const Vec3 v = lerp(vi, vj, t);
                    f.push_back(v);
                    b.push_back(v);
                }
            }
            if (f.size() >= 3) front.push_back(Poly{f, poly.plane});
            if (b.size() >= 3) back.push_back(Poly{b, poly.plane});
            break;
        }
    }
}

struct Node {
    Plane plane;
    bool hasPlane = false;
    std::unique_ptr<Node> front;
    std::unique_ptr<Node> back;
    std::vector<Poly> polys;

    void invert() {
        for (Poly& p : polys) {
            std::reverse(p.v.begin(), p.v.end());
            p.plane.flip();
        }
        if (hasPlane) plane.flip();
        if (front) front->invert();
        if (back) back->invert();
        std::swap(front, back);
    }

    std::vector<Poly> clipPolygons(const std::vector<Poly>& in) const {
        if (!hasPlane) return in;
        std::vector<Poly> f, b;
        for (const Poly& p : in) splitPolygon(plane, p, f, b, f, b);
        if (front) f = front->clipPolygons(f);
        if (back) {
            b = back->clipPolygons(b);
        } else {
            b.clear();
        }
        f.insert(f.end(), b.begin(), b.end());
        return f;
    }

    void clipTo(const Node& bsp) {
        polys = bsp.clipPolygons(polys);
        if (front) front->clipTo(bsp);
        if (back) back->clipTo(bsp);
    }

    void allPolygons(std::vector<Poly>& out) const {
        out.insert(out.end(), polys.begin(), polys.end());
        if (front) front->allPolygons(out);
        if (back) back->allPolygons(out);
    }

    void build(const std::vector<Poly>& in) {
        if (in.empty()) return;
        if (!hasPlane) {
            plane = in[0].plane;
            hasPlane = true;
        }
        std::vector<Poly> f, b;
        for (const Poly& p : in) splitPolygon(plane, p, polys, polys, f, b);
        if (!f.empty()) {
            if (!front) front = std::make_unique<Node>();
            front->build(f);
        }
        if (!b.empty()) {
            if (!back) back = std::make_unique<Node>();
            back->build(b);
        }
    }
};

std::vector<Poly> toPolys(const PolyMesh& m) {
    const PolyMesh t = triangulate(m);
    std::vector<Poly> out;
    out.reserve(t.faces.size());
    for (const auto& f : t.faces) {
        const Vec3 a = t.verts[f[0]], b = t.verts[f[1]], c = t.verts[f[2]];
        const Vec3 n = cross(b - a, c - a);
        if (length(n) < 1e-10f) continue;
        Poly p;
        p.v = {a, b, c};
        p.plane.n = normalize(n);
        p.plane.w = dot(p.plane.n, a);
        out.push_back(std::move(p));
    }
    return out;
}

struct Job {
    std::vector<Poly> a, b;
    int op = 0;
    std::vector<Poly> result;
    std::string error;
};

void* runJob(void* arg) {
    Job* job = static_cast<Job*>(arg);
    try {
        Node A, B;
        A.build(job->a);
        B.build(job->b);
        if (job->op == 0) {  // união
            A.clipTo(B);
            B.clipTo(A);
            B.invert();
            B.clipTo(A);
            B.invert();
            std::vector<Poly> bp;
            B.allPolygons(bp);
            A.build(bp);
        } else if (job->op == 1) {  // diferença
            A.invert();
            A.clipTo(B);
            B.clipTo(A);
            B.invert();
            B.clipTo(A);
            B.invert();
            std::vector<Poly> bp;
            B.allPolygons(bp);
            A.build(bp);
            A.invert();
        } else {  // interseção
            A.invert();
            B.clipTo(A);
            B.invert();
            A.clipTo(B);
            B.clipTo(A);
            std::vector<Poly> bp;
            B.allPolygons(bp);
            A.build(bp);
            A.invert();
        }
        A.allPolygons(job->result);
    } catch (const std::exception& e) {
        job->error = e.what();
    }
    return nullptr;
}

}  // namespace

PolyMesh booleanOp(const PolyMesh& a, const PolyMesh& b, int op) {
    Job job;
    job.a = toPolys(a);
    job.b = toPolys(b);
    job.op = op;
    if (job.a.empty() || job.b.empty()) throw std::runtime_error("boolean operands need faces");
    if (job.a.size() + job.b.size() > kMaxTriangles) {
        throw std::runtime_error("too many triangles for a boolean; do it before subdividing");
    }

    // A recursão do BSP pode ser funda: corre numa thread com pilha grande.
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 64u * 1024u * 1024u);
    pthread_t thread;
    if (pthread_create(&thread, &attr, runJob, &job) == 0) {
        pthread_join(thread, nullptr);
    } else {
        runJob(&job);
    }
    pthread_attr_destroy(&attr);
    if (!job.error.empty()) throw std::runtime_error(job.error);

    PolyMesh out;
    for (const Poly& p : job.result) {
        std::vector<uint32_t> face;
        for (const Vec3& v : p.v) {
            face.push_back(static_cast<uint32_t>(out.verts.size()));
            out.verts.push_back(v);
        }
        out.faces.push_back(std::move(face));
    }
    if (out.faces.empty()) throw std::runtime_error("boolean result is empty");
    Vec3 mn, mx;
    meshBounds(out, mn, mx);
    const Vec3 ext = mx - mn;
    return weldVertices(out, 1e-5f * std::max({ext.x, ext.y, ext.z, 1e-3f}));
}

}  // namespace core
