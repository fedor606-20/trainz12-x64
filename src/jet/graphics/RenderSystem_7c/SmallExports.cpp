// Small/tail exports for RenderSystem_7c: Vec4, Tex2Pair/Tex3Pair, Streamed1/2/3Vertex
// template instances. Behaviour/layout follows the TS12 binary (addresses in decomp/exports).
#include <jet/KernelStandard.h>
#include <jet/Math.h>
#include <jet/Platform.h>

namespace Jet {

// Vec4: a 4-float value used by shader/program symbols (0x1000fc90, 0x1000fca0, 0x1000fce0, 0x1003e930).
struct Vec4 {
    float v[4];

    // 0x1000fc90 ?Set@Vec4@Jet@@QAAXM@Z
    void __cdecl Set(float x)
    {
        v[0] = x;
    }

    // 0x1000fca0 ?Set@Vec4@Jet@@QAAXMMMM@Z
    void __cdecl Set(float x, float y, float z, float w)
    {
        v[0] = x;
        v[1] = y;
        v[2] = z;
        v[3] = w;
    }

    // 0x1000fce0 ?GetVec4@Vec4@Jet@@QBA?AVVector4@2@XZ
    Vector4 __cdecl GetVec4() const
    {
        Vector4 out;
        out.x = v[0];
        out.y = v[1];
        out.z = v[2];
        out.w = v[3];
        return out;
    }

    // 0x1003e930 ??4Vec4@Jet@@QAEAAU01@ABU01@@Z
    Vec4& operator=(const Vec4& o)
    {
        v[0] = o.v[0];
        v[1] = o.v[1];
        v[2] = o.v[2];
        v[3] = o.v[3];
        return *this;
    }
};

JET_LAYOUT32(sizeof(Vec4) == 16);

// Tex2Pair / Tex3Pair: trivial texture-coordinate pair records (0x10024c50, 0x1003ed50, 0x1003f210).
struct Tex2Pair {
    float v[4];

    Tex2Pair() {}  // 0x10024c50

    // 0x1003ed50 ??4Tex2Pair@Jet@@QAEAAU01@ABU01@@Z
    Tex2Pair& operator=(const Tex2Pair& o)
    {
        v[0] = o.v[0];
        v[1] = o.v[1];
        v[2] = o.v[2];
        v[3] = o.v[3];
        return *this;
    }
};

struct Tex3Pair {
    float v[6];

    Tex3Pair() {}  // 0x10024c50

    // 0x1003f210 ??4Tex3Pair@Jet@@QAEAAU01@ABU01@@Z
    Tex3Pair& operator=(const Tex3Pair& o)
    {
        v[0] = o.v[0];
        v[1] = o.v[1];
        v[2] = o.v[2];
        v[3] = o.v[3];
        v[4] = o.v[4];
        v[5] = o.v[5];
        return *this;
    }
};

// Streamed vertex reference holders. Each slot is a pointer to a reference-counted block
// whose refcount is at +0 and which is freed with Alloc::Delete when it reaches 0, as in the
// original (0x10042f60, 0x10056050, 0x10056070, 0x100560c0, 0x100560d0, 0x10056110,
// 0x10056170, 0x100561a0).

struct StreamedVertexPCTBase {};
struct StreamedVertexPNTBase {};
struct StreamedVertexP {};
struct StreamedVertexC {};
struct StreamedVertexN {};
struct StreamedVertexT {};

namespace {

inline void AddRef(void* p)
{
    if (p) ++*static_cast<int*>(p);
}

inline void Release(void*& p)
{
    if (!p) return;
    int* refs = static_cast<int*>(p);
    if (--*refs == 0) {
        Alloc::Delete(p);
        p = nullptr;
    }
}

}  // namespace

template <class Base, class S0>
struct Streamed1Vertex {
    void* v[1];

    Streamed1Vertex() { v[0] = nullptr; }  // 0x10042f60
    Streamed1Vertex(const Streamed1Vertex& o)  // 0x10056050
    {
        v[0] = nullptr;
        if (o.v[0]) {
            v[0] = o.v[0];
            AddRef(v[0]);
        }
    }
    ~Streamed1Vertex()  // 0x10056170
    {
        Release(v[0]);
    }
};

template <class Base, class S0, class S1>
struct Streamed2Vertex {
    void* v[3];

    Streamed2Vertex()  // 0x1000f850 (shared body)
    {
        v[0] = nullptr;
        v[1] = nullptr;
        v[2] = nullptr;
    }
    Streamed2Vertex(const Streamed2Vertex& o)  // 0x10056070
    {
        v[0] = nullptr;
        if (o.v[0]) { v[0] = o.v[0]; AddRef(v[0]); }
        v[1] = nullptr;
        if (o.v[1]) { v[1] = o.v[1]; AddRef(v[1]); }
        v[2] = nullptr;
        if (o.v[2]) { v[2] = o.v[2]; AddRef(v[2]); }
    }
    ~Streamed2Vertex()  // 0x100561a0
    {
        Release(v[2]);
        Release(v[1]);
        Release(v[0]);
    }
};

template <class Base, class S0, class S1, class S2>
struct Streamed3Vertex {
    void* v[4];

    Streamed3Vertex()  // 0x100560c0
    {
        v[0] = nullptr;
        v[1] = nullptr;
        v[2] = nullptr;
        v[3] = nullptr;
    }
    Streamed3Vertex(const Streamed3Vertex& o)  // 0x10056110
    {
        v[0] = nullptr;
        if (o.v[0]) { v[0] = o.v[0]; AddRef(v[0]); }
        v[1] = nullptr;
        if (o.v[1]) { v[1] = o.v[1]; AddRef(v[1]); }
        v[2] = nullptr;
        if (o.v[2]) { v[2] = o.v[2]; AddRef(v[2]); }
        v[3] = nullptr;
        if (o.v[3]) { v[3] = o.v[3]; AddRef(v[3]); }
    }
    ~Streamed3Vertex()  // 0x100560d0
    {
        Release(v[3]);
        Release(v[2]);
        Release(v[1]);
        Release(v[0]);
    }
};

// Explicit instantiations for the exports present in RenderSystem_7c.
template struct Streamed1Vertex<StreamedVertexPCTBase, StreamedVertexP>;
template struct Streamed1Vertex<StreamedVertexPNTBase, StreamedVertexP>;

template struct Streamed2Vertex<StreamedVertexPCTBase, StreamedVertexP, StreamedVertexC>;
template struct Streamed2Vertex<StreamedVertexPNTBase, StreamedVertexP, StreamedVertexN>;

template struct Streamed3Vertex<StreamedVertexPCTBase, StreamedVertexP, StreamedVertexC, StreamedVertexT>;
template struct Streamed3Vertex<StreamedVertexPNTBase, StreamedVertexP, StreamedVertexN, StreamedVertexT>;

}  // namespace Jet
