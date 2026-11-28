// Unit U10 wave 5: the primitive state that needs a live render kernel, a material manager and the material's
// GeometryChunk: every Init and BuildGeometry of the RenderPrimitive family, plus RenderLines' constructors of debug
// primitives. The bodies are transcriptions of the TS12 assembly (notes/agents/r3d_workstreams/u10_primitives_asm.txt);
// the only collaborators are the original's exported MaterialManager::Create (through its vtable slot 3), the four
// Material::Set*Color exports of RenderSystem_7c, and the chunk's own virtual accessors.
#include <jet/Render3DCorePrimitives.h>

#include <jet/Geometry.h>
#include <jet/RenderSystem.h>

#include <windows.h>

#include <cmath>
#include <cstring>
#include <xmmintrin.h>

namespace Jet {

namespace {

using CreateFn = int* (__cdecl*)(void* manager, int* result, Material** material, const PString* name);
using ColorFn = void(__cdecl*)(void* material, const Color* color);

HMODULE Module(const char* name)
{
    static HMODULE renderSystem = [] {
        HMODULE m = GetModuleHandleA("RenderSystem_7c.dll");
        return m ? m : LoadLibraryA("RenderSystem_7c.dll");
    }();
    return renderSystem;
}

template <class F>
F RenderSystemProc(const char* name)
{
    HMODULE m = Module(name);
    return m ? reinterpret_cast<F>(GetProcAddress(m, name)) : nullptr;
}

void SetMaterialColors(void* material, const Color& color)
{
    static ColorFn ambient = RenderSystemProc<ColorFn>("?SetAmbientColor@Material@Jet@@QAAXABVColor@2@@Z");
    static ColorFn diffuse = RenderSystemProc<ColorFn>("?SetDiffuseColor@Material@Jet@@QAAXABVColor@2@@Z");
    static ColorFn specular = RenderSystemProc<ColorFn>("?SetSpecularColor@Material@Jet@@QAAXABVColor@2@@Z");
    static ColorFn emissive = RenderSystemProc<ColorFn>("?SetEmissiveColor@Material@Jet@@QAAXABVColor@2@@Z");
    if (ambient) ambient(material, &color);
    if (diffuse) diffuse(material, &color);
    if (specular) specular(material, &color);
    if (emissive) emissive(material, &color);
}

const char* NoTextureDecoratedText()
{
    static PString type("m.notex");
    return type.Text();
}

float FAdd(float a, float b) { return _mm_cvtss_f32(_mm_add_ss(_mm_load_ss(&a), _mm_load_ss(&b))); }
float FSub(float a, float b) { return _mm_cvtss_f32(_mm_sub_ss(_mm_load_ss(&a), _mm_load_ss(&b))); }
float FMul(float a, float b) { return _mm_cvtss_f32(_mm_mul_ss(_mm_load_ss(&a), _mm_load_ss(&b))); }
float FDiv(float a, float b) { return _mm_cvtss_f32(_mm_div_ss(_mm_load_ss(&a), _mm_load_ss(&b))); }

// x87 fld/fstp quietens a signalling NaN, as the original's colour/field copies do.
float X87Copy(float f)
{
    unsigned int u;
    std::memcpy(&u, &f, 4);
    if ((u & 0x7f800000u) == 0x7f800000u && (u & 0x007fffffu) && !(u & 0x00400000u)) u |= 0x00400000u;
    std::memcpy(&f, &u, 4);
    return f;
}

void CopyColor(Color& to, const Color& from)
{
    to.r = X87Copy(from.r);
    to.g = X87Copy(from.g);
    to.b = X87Copy(from.b);
    to.a = X87Copy(from.a);
}

void SetHint(StandardChunkHint& hint)
{
    ::new (&hint) StandardChunkHint();
    hint.bits |= 1;
}

char* Step(char* p, unsigned int stride) { return p + stride; }

void Finish(GeometryChunk* chunk, RenderKernel* kernel)
{
    chunk->InitializeBuffers(kernel->GetDisplay(), true);
}

// 0x65d10-adjacent: Material::Create through its result-by-value slot, then the material is appended to the
// primitive's material block. The class declarations live in different units; the call is expressed through the
// vtable exactly as the original does.
bool CreateMaterial(RenderKernel* kernel, const Color& color, Material*& material)
{
    void* const manager = *reinterpret_cast<void* const*>(reinterpret_cast<char*>(kernel) + 0x90);
    if (!manager) return false;
    material = nullptr;
    PString name;
    name.Setf("RPM_Color_%X%s", Display::ConvertColor(color), NoTextureDecoratedText());
    void** const table = *reinterpret_cast<void***>(manager);
    CreateFn const create = reinterpret_cast<CreateFn>(table[3]);
    int result = 0;
    create(manager, &result, &material, &name);
    return result == 0;
}

}  // namespace

// 0x68680
bool __cdecl RenderPrimitive::Init(RenderKernel* kernel_, const Color& value, int creationFlags_)
{
    const unsigned int count = materialArray.block ? materialArray.block->count : 0;
    if (count != 0) return false;

    CopyColor(color, value);
    kernel = kernel_;
    creationFlags = creationFlags_;

    Material* material = nullptr;
    if (!CreateMaterial(kernel_, value, material)) return false;

    SetMaterialColors(material, value);
    *reinterpret_cast<float*>(reinterpret_cast<char*>(material) + 0x80) = value.a;
    flags = static_cast<unsigned char>((flags & ~1) | ((creationFlags_ >> 2) & 1));

    AutoReferenceArray<AutoReference<Material>>::Block* const block = materialArray.block;
    if (!block) return false;
    // the original clears the new slot and stores the raw pointer (the material was created with its single
    // reference); a fresh block's slots are unconstructed, so the slot must not be read as an AutoReference.
    *reinterpret_cast<Material**>(&block->data[0]) = nullptr;
    block->count = 1;
    *reinterpret_cast<Material**>(&block->data[0]) = material;
    return true;
}

// 0x60cd0
bool __cdecl RenderBox::Init(RenderKernel* kernel_, const Color& value, float width, float height, float depth,
                             unsigned int xSegments, unsigned int ySegments, unsigned int zSegments,
                             int creationFlags_)
{
    if (!RenderPrimitive::Init(kernel_, value, creationFlags_)) return false;
    extents.x = width * 0.5f;
    extents.y = height * 0.5f;
    extents.z = depth * 0.5f;
    if (!BuildGeometry(kernel_, value, static_cast<int>(xSegments), static_cast<int>(ySegments),
                       static_cast<int>(zSegments), creationFlags_))
        return false;
    CalculatePrimitiveVolume();
    return true;
}

// 0x69050
bool __cdecl RenderSphere::Init(RenderKernel* kernel_, const Color& value, float radius_, unsigned int segments,
                                int creationFlags_)
{
    radius = radius_;
    if (!RenderPrimitive::Init(kernel_, value, creationFlags_)) return false;
    if (!BuildGeometry(kernel_, value, static_cast<int>(segments), creationFlags_)) return false;
    CalculatePrimitiveVolume();
    return true;
}

// 0x62cd0
bool __cdecl RenderCircle::Init(RenderKernel* kernel_, const Color& value, float radius_, unsigned int segments,
                                int creationFlags_)
{
    radius = radius_;
    if (!RenderPrimitive::Init(kernel_, value, creationFlags_)) return false;
    if (!BuildGeometry(kernel_, value, static_cast<int>(segments), creationFlags_)) return false;
    CalculatePrimitiveVolume();
    return true;
}

// 0x64bb0
bool __cdecl RenderEllipse::Init(RenderKernel* kernel_, const Color& value, float radiusX_, float radiusY_,
                                 unsigned int segments, int creationFlags_)
{
    radiusX = radiusX_;
    radiusY = radiusY_;
    if (!RenderPrimitive::Init(kernel_, value, creationFlags_)) return false;
    if (!BuildGeometry(kernel_, value, static_cast<int>(segments), creationFlags_)) return false;
    CalculatePrimitiveVolume();
    return true;
}

// 0x631f0
bool __cdecl RenderCone::Init(RenderKernel* kernel_, const Color& value, float radius_, float height_,
                              unsigned int segments, int creationFlags_)
{
    if (!RenderPrimitive::Init(kernel_, value, creationFlags_)) return false;
    radius = radius_;
    height = height_;
    if (!BuildGeometry(kernel_, value, static_cast<int>(segments), creationFlags_)) return false;
    CalculatePrimitiveVolume();
    return true;
}

// 0x63ed0
bool __cdecl RenderCylinder::Init(RenderKernel* kernel_, const Color& value, float radius_, float height_,
                                  unsigned int segments, int creationFlags_)
{
    if (!RenderPrimitive::Init(kernel_, value, creationFlags_)) return false;
    radius = radius_;
    height = height_;
    if (!BuildGeometry(kernel_, value, static_cast<int>(segments), creationFlags_)) return false;
    CalculatePrimitiveVolume();
    return true;
}

// 0x65170
bool __cdecl RenderEllipsoid::Init(RenderKernel* kernel_, const Color& value, float radiusX_, float radiusY_,
                                   float radiusZ_, unsigned int segments, int creationFlags_)
{
    radiusX = radiusX_;
    radiusY = radiusY_;
    radiusZ = radiusZ_;
    if (!RenderPrimitive::Init(kernel_, value, creationFlags_)) return false;
    if (!BuildGeometry(kernel_, value, static_cast<int>(segments), creationFlags_)) return false;
    CalculatePrimitiveVolume();
    return true;
}

// 0x6a150
bool __cdecl RenderTube::Init(RenderKernel* kernel_, const Color& value, float radius_, float height_,
                              float thickness_, unsigned int segments, int creationFlags_)
{
    if (!RenderPrimitive::Init(kernel_, value, creationFlags_)) return false;
    radius = radius_;
    height = height_;
    thickness = thickness_;
    if (!BuildGeometry(kernel_, value, static_cast<int>(segments), creationFlags_)) return false;
    CalculatePrimitiveVolume();
    return true;
}

// 0x67150
bool __cdecl RenderPlane::Init(RenderKernel* kernel_, const Color& value, float width_, float height_,
                               unsigned int xSegments, unsigned int ySegments, int creationFlags_)
{
    if (!RenderPrimitive::Init(kernel_, value, creationFlags_)) return false;
    if (!BuildGeometry(kernel_, value, width_, height_, static_cast<int>(xSegments),
                       static_cast<int>(ySegments), creationFlags_))
        return false;
    CalculatePrimitiveVolume();
    return true;
}

// 0x68080
bool __cdecl RenderPolyline::Init(RenderKernel* kernel_, const Color& value, const Vector3* points, int count,
                                  int creationFlags_)
{
    if (!RenderPrimitive::Init(kernel_, value, creationFlags_)) return false;
    if (!BuildGeometry(kernel_, value, points, count, creationFlags_)) return false;
    CalculatePrimitiveVolume();
    return true;
}

// 0x65c50
bool __cdecl RenderLine::Init(RenderKernel* kernel_, const Color& value, const Vector3& from, const Vector3& to,
                              int creationFlags_)
{
    Vector3 points[2] = {from, to};
    if (!RenderPrimitive::Init(kernel_, value, creationFlags_)) return false;
    if (!BuildGeometry(kernel_, value, points, 2, creationFlags_)) return false;
    CalculatePrimitiveVolume();
    return true;
}

// 0x69ae0
bool __cdecl RenderSpline::Init(RenderKernel* kernel_, const Color& value, const Spline& spline, unsigned int steps,
                                int creationFlags_)
{
    unsigned int segments = 0;
    for (auto i = const_cast<Spline&>(spline).segments.First(); i != const_cast<Spline&>(spline).segments.End(); ++i)
        ++segments;
    if (segments == 0 || steps == 0) return false;

    const unsigned int total = segments * steps;
    Vector3* const points = static_cast<Vector3*>(Alloc::New((total + 1) * sizeof(Vector3)));
    if (!points) return false;

    const float step = FDiv(1.0f, static_cast<float>(steps));
    unsigned int out = 0;
    for (auto i = const_cast<Spline&>(spline).segments.First(); i != const_cast<Spline&>(spline).segments.End();
         ++i) {
        const SplineSegment* const segment = *i;
        if (!segment) continue;
        const Vector3* const c0 = &segment->c0;
        const Vector3* const c1 = &segment->c1;
        const Vector3* const c2 = &segment->c2;
        const Vector3* const c3 = &segment->c3;
        float t = 0.0f;
        for (unsigned int k = 0; k < steps; ++k, out += 1) {
            points[out].x = FAdd(FMul(FAdd(FMul(FAdd(FMul(c0->x, t), c1->x), t), c2->x), t), c3->x);
            points[out].y = FAdd(FMul(FAdd(FMul(FAdd(FMul(c0->y, t), c1->y), t), c2->y), t), c3->y);
            points[out].z = FAdd(FMul(FAdd(FMul(FAdd(FMul(c0->z, t), c1->z), t), c2->z), t), c3->z);
            t = FAdd(t, step);
        }
    }
    if (const SplineSegment* const last = *const_cast<Spline&>(spline).segments.Last()) {
        points[out].x = FAdd(FAdd(FAdd(last->c0.x, last->c1.x), last->c2.x), last->c3.x);
        points[out].y = FAdd(FAdd(FAdd(last->c0.y, last->c1.y), last->c2.y), last->c3.y);
        points[out].z = FAdd(FAdd(FAdd(last->c0.z, last->c1.z), last->c2.z), last->c3.z);
    }

    const bool result = RenderPrimitive::Init(kernel_, value, creationFlags_) &&
                        BuildGeometry(kernel_, value, points, static_cast<int>(total + 1), creationFlags_);
    if (result) CalculatePrimitiveVolume();
    Alloc::Delete(points);
    return result;
}

// 0x67c10
bool __cdecl RenderPolygon::Init(RenderKernel* kernel_, const Color& value, const Polygon& polygon,
                                 int creationFlags_)
{
    if (!RenderPrimitive::Init(kernel_, value, creationFlags_)) return false;
    if (!BuildGeometry(kernel_, value, polygon, creationFlags_)) return false;
    CalculatePrimitiveVolume();
    return true;
}

// 0x67c70
bool __cdecl RenderPolygon::Init(RenderKernel* kernel_, const Color& value, int count, const Vector3* points,
                                 int creationFlags_)
{
    Polygon polygon;
    polygon.Set(count, points);
    return Init(kernel_, value, polygon, creationFlags_);
}

}  // namespace Jet
