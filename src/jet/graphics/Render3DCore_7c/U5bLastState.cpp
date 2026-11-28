// Render3DCore_7c U5b last wave: MultiPassRenderState::StateSetup / SetScissor / LoadRenderProperties and the two
// per-vertex paths. Every body follows the original's assembly (addresses in the comments); the RenderSystem entry
// points are the module's exports, the state helpers are local.
#include "U5bLast.h"

#include <jet/DynamicArray.h>
#include <jet/GfxLanguage.h>
#include <jet/Math.h>

#include <cmath>
#include <cstring>

#include <windows.h>

namespace Jet {
namespace MultiPass {
namespace Last {

using Final::Bytes;
using Final::Local;
using Final::Original;
using Final::StateGetAmbientLights;
using Final::Thunks;

namespace {

constexpr unsigned int kDisplay = 0x04;
constexpr unsigned int kView = 0x08;
constexpr unsigned int kMaterial = 0x0c;
constexpr unsigned int kChunk = 0x10;
constexpr unsigned int kFlag24 = 0x24;
constexpr unsigned int kOpacity = 0x28;
constexpr unsigned int kLists = 0x2c;
constexpr unsigned int kMatrix = 0x44;
constexpr unsigned int kBasis = 0x84;
constexpr unsigned int kViewMatrix = 0xa8;
constexpr unsigned int kScissor = 0xe8;
constexpr unsigned int kProgram = 0xf8;
constexpr unsigned int kIndicesArray = 0xfc;
constexpr unsigned int kTextures = 0x108;

using FnUpdateCachedView = void(__cdecl*)(void*);
using FnClip3D = int(__cdecl*)(void*, const Vector3*, float);
using FnInvert = Matrix4&(__cdecl*)(Matrix4*);
using FnGetScissor = void(__cdecl*)(void*, Rect*);
using FnSetScissor = void(__cdecl*)(void*, const Rect*);
using FnGetTriangle = void(__cdecl*)(const void*, int, unsigned long*, unsigned long*, unsigned long*);
using FnSetLocalVector4 = bool(__cdecl*)(void*, int, const Vector4*);
using FnSetLocalVector3 = bool(__cdecl*)(void*, int, const Vector3*);
using FnSetConstVector4 = bool(__cdecl*)(void*, int, const Vector4*);
using FnSetConstVector3 = bool(__cdecl*)(void*, int, const Vector3*);

void* RenderSystemProc(const char* name)
{
    static HMODULE module = [] {
        HMODULE m = GetModuleHandleA("RenderSystem_7c.dll");
        if (!m) m = LoadLibraryA("RenderSystem_7c.dll");
        return m;
    }();
    return module ? reinterpret_cast<void*>(GetProcAddress(module, name)) : nullptr;
}
void* KernelStandardProc(const char* name)
{
    static HMODULE module = [] {
        HMODULE m = GetModuleHandleA("KernelStandard_7c.dll");
        if (!m) m = LoadLibraryA("KernelStandard_7c.dll");
        return m;
    }();
    return module ? reinterpret_cast<void*>(GetProcAddress(module, name)) : nullptr;
}

FnUpdateCachedView UpdateCachedView()
{
    static FnUpdateCachedView fn = reinterpret_cast<FnUpdateCachedView>(
        Local("?UpdateCachedViewTransform@Camera@Jet@@IBAXXZ"));
    if (!fn) fn = reinterpret_cast<FnUpdateCachedView>(Original("?UpdateCachedViewTransform@Camera@Jet@@IBAXXZ"));
    return fn;
}
FnClip3D Clip3D()
{
    static FnClip3D fn =
        reinterpret_cast<FnClip3D>(Local("?Clip3D@Camera@Jet@@QBA?AW4ClipResult@2@ABVVector3@2@M@Z"));
    if (!fn) fn = reinterpret_cast<FnClip3D>(Original("?Clip3D@Camera@Jet@@QBA?AW4ClipResult@2@ABVVector3@2@M@Z"));
    return fn;
}
FnInvert Invert()
{
    static FnInvert fn = reinterpret_cast<FnInvert>(KernelStandardProc("?Invert@Matrix4@Jet@@QAAAAV12@XZ"));
    return fn;
}
FnGetScissor GetScissor()
{
    static FnGetScissor fn = reinterpret_cast<FnGetScissor>(RenderSystemProc("?GetScissor@Display@Jet@@QAA?AURect@2@XZ"));
    return fn;
}
FnSetScissor SetScissor()
{
    static FnSetScissor fn =
        reinterpret_cast<FnSetScissor>(RenderSystemProc("?SetScissor@Display@Jet@@QAAXABURect@2@@Z"));
    return fn;
}
FnGetTriangle GetTriangleIndexes()
{
    static FnGetTriangle fn = reinterpret_cast<FnGetTriangle>(
        RenderSystemProc("?GetTriangleIndexes@GeometryChunk@Jet@@QBAXHAAK00@Z"));
    return fn;
}
FnSetLocalVector4 SetLocalVector4()
{
    static FnSetLocalVector4 fn = reinterpret_cast<FnSetLocalVector4>(
        RenderSystemProc("?SetLocalConstant@VertexProgram@Jet@@UAA_NHABVVector4@2@@Z"));
    return fn;
}
FnSetLocalVector3 SetLocalVector3()
{
    static FnSetLocalVector3 fn = reinterpret_cast<FnSetLocalVector3>(
        RenderSystemProc("?SetLocalConstant@VertexProgram@Jet@@QBA_NHABVVector3@2@@Z"));
    return fn;
}
FnSetConstVector4 SetConstVector4()
{
    static FnSetConstVector4 fn = reinterpret_cast<FnSetConstVector4>(
        RenderSystemProc("?SetConstantRegister@TextureCombiner@Jet@@UAA_NHABVVector4@2@@Z"));
    return fn;
}
FnSetConstVector3 SetConstVector3()
{
    static FnSetConstVector3 fn = reinterpret_cast<FnSetConstVector3>(
        RenderSystemProc("?SetConstantRegister@TextureCombiner@Jet@@QAA_NHABVVector3@2@@Z"));
    return fn;
}

// The FastDynamicArray<Light*> heads: {Chunk* head}; a chunk is {next, prev, count, data[13]}.
unsigned int ListCount(const void* headAddress)
{
    const unsigned char* const head = *reinterpret_cast<unsigned char* const*>(headAddress);
    if (!head) return 0;
    unsigned int total = 0;
    const unsigned char* chunk = head;
    do {
        total += *reinterpret_cast<const unsigned int*>(chunk + 8);
        chunk = *reinterpret_cast<const unsigned char* const*>(chunk);
    } while (chunk != head);
    return total;
}

void AppendList(void* headAddress, void* item)
{
    unsigned char** const slot = static_cast<unsigned char**>(headAddress);
    unsigned char* head = *slot;
    if (!head) {
        unsigned char* chunk = static_cast<unsigned char*>(Alloc::New(0x40));
        if (!chunk) return;
        *reinterpret_cast<unsigned int*>(chunk + 8) = 0;
        *slot = chunk;
        *reinterpret_cast<unsigned char**>(chunk + 4) = chunk;
        *reinterpret_cast<unsigned char**>(chunk) = chunk;
        *reinterpret_cast<void**>(chunk + 0x0c) = item;
        *reinterpret_cast<unsigned int*>(chunk + 8) = 1;
        return;
    }
    unsigned char* tail = *reinterpret_cast<unsigned char**>(head + 4);
    if (*reinterpret_cast<unsigned int*>(tail + 8) == 13) {
        unsigned char* chunk = static_cast<unsigned char*>(Alloc::New(0x40));
        if (!chunk) return;
        *reinterpret_cast<unsigned int*>(chunk + 8) = 0;
        *reinterpret_cast<unsigned char**>(chunk) = head;
        *reinterpret_cast<unsigned char**>(chunk + 4) = tail;
        *reinterpret_cast<unsigned char**>(head + 4) = chunk;
        *reinterpret_cast<unsigned char**>(tail) = chunk;
        tail = chunk;
    }
    const unsigned int count = *reinterpret_cast<unsigned int*>(tail + 8);
    *reinterpret_cast<void**>(tail + 0x0c + count * 4) = item;
    *reinterpret_cast<unsigned int*>(tail + 8) = count + 1;
}

// The nth item of a FastDynamicArray reachable from a head pointer ({chunk* head}).
void* ListItemAt(const void* headAddress, unsigned int index)
{
    const unsigned char* head = *reinterpret_cast<unsigned char* const*>(headAddress);
    if (!head) return nullptr;
    const unsigned char* chunk = head;
    unsigned int remaining = index;
    for (;;) {
        const unsigned int count = *reinterpret_cast<const unsigned int*>(chunk + 8);
        if (remaining < count) return *reinterpret_cast<void* const*>(chunk + 0x0c + remaining * 4);
        remaining -= count;
        chunk = *reinterpret_cast<const unsigned char* const*>(chunk);
        if (chunk == head) return nullptr;
    }
}

void* LightPosition(void* light)
{
    using Fn = const Vector3* (__cdecl*)(void*);
    static Fn fn = [](void* l) -> const Vector3* {
        return reinterpret_cast<Fn>((*reinterpret_cast<void***>(l))[0x0c / 4])(l);
    };
    return const_cast<Vector3*>(fn(light));
}
void* LightDirection(void* light)
{
    using Fn = const Vector3* (__cdecl*)(void*);
    static Fn fn = [](void* l) -> const Vector3* {
        return reinterpret_cast<Fn>((*reinterpret_cast<void***>(l))[0x14 / 4])(l);
    };
    return const_cast<Vector3*>(fn(light));
}

void CallVoid(void* object, unsigned int slot, void* arg)
{
    using Fn = void(__cdecl*)(void*, void*);
    reinterpret_cast<Fn>((*reinterpret_cast<void***>(object))[slot / 4])(object, arg);
}
void CallVoid3(void* object, unsigned int slot, void* a, void* b, void* c)
{
    using Fn = void(__cdecl*)(void*, void*, void*, void*);
    reinterpret_cast<Fn>((*reinterpret_cast<void***>(object))[slot / 4])(object, a, b, c);
}

// 0x1001ab30: out = left * right, in place on `right`, with the original's SSE accumulation order.
void MultiplyMatricesInPlace(Matrix4* left, Matrix4* right)
{
    const Matrix4 a = *left;
    const Matrix4 b = *right;
    float result[16];
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            float value = a.m[row * 4 + 1] * b.m[4 + column];
            value = a.m[row * 4 + 3] * b.m[12 + column] + value;
            value = value + a.m[row * 4 + 2] * b.m[8 + column];
            value = value + a.m[row * 4 + 0] * b.m[column];
            result[row * 4 + column] = value;
        }
    }
    std::memcpy(right->m, result, sizeof result);
}

// 0x10010930: project a point by a matrix with the perspective divide (ECX = matrix, EDX = point, EAX = out).
Vector3 ProjectPoint(const Matrix4& m, const Vector3& p)
{
    const float x = m.m[0] * p.x + m.m[4] * p.y + m.m[8] * p.z + m.m[12];
    const float y = m.m[1] * p.x + m.m[5] * p.y + m.m[9] * p.z + m.m[13];
    const float z = m.m[2] * p.x + m.m[6] * p.y + m.m[10] * p.z + m.m[14];
    const float w = m.m[3] * p.x + m.m[7] * p.y + m.m[11] * p.z + m.m[15];
    const float inv = 1.0f / w;
    return Vector3{x * inv, y * inv, z * inv};
}

// 0x10008b50: in-place normalise by the KernelStandard LUT seed and two Newton steps.
extern "C" unsigned int* JetSqrtLutPtr;
#if defined(_M_IX86)
#pragma comment(linker, "/alternatename:_JetSqrtLutPtr=__imp_?s_sqrtLUT@StdMath@Jet@@0PAIA")
#endif

float SqrtSeed(unsigned int bits)
{
    const unsigned int* const lut = JetSqrtLutPtr;
    const unsigned int e = (bits >> 23) & 0xffu;
    const unsigned int seedBits = (((0x17cu - e) << 22) & 0xff800000u) | (lut ? lut[(bits >> 15) & 0x1ffu] : 0u);
    float seed;
    std::memcpy(&seed, &seedBits, sizeof seed);
    return seed;
}

void NormaliseDirection(Vector3& v)
{
    const float sum = (v.x * v.x + v.y * v.y) + v.z * v.z;
    unsigned int bits;
    std::memcpy(&bits, &sum, sizeof bits);
    const double half = static_cast<double>(sum) * 0.5;
    double r = SqrtSeed(bits);
    r = (1.5 - (r * r) * half) * r;
    r = r * (1.5 - (r * r) * half);
    const float rf = static_cast<float>(r);
    v.x *= rf;
    v.y *= rf;
    v.z *= rf;
}

// The 3x3 transform the state keeps at +0x84 (column-major basis).
Vector3 TransformDirection(const unsigned char* state, const Vector3& v)
{
    const float* m = reinterpret_cast<const float*>(state + kBasis);
    Vector3 out;
    out.x = m[3] * v.y + m[6] * v.z + m[0] * v.x;
    out.y = m[1] * v.x + m[4] * v.y + m[7] * v.z;
    out.z = m[2] * v.x + m[5] * v.y + m[8] * v.z;
    return out;
}

Vector3 TransformDirection(const unsigned char* state, const float* v)
{
    Vector3 out;
    const float* m = reinterpret_cast<const float*>(state + kBasis);
    out.x = m[3] * v[1] + m[6] * v[2] + m[0] * v[0];
    out.y = m[1] * v[0] + m[4] * v[1] + m[7] * v[2];
    out.z = m[2] * v[0] + m[5] * v[1] + m[8] * v[2];
    return out;
}

Vector3 Cross(const Vector3& a, const Vector3& b)
{
    return Vector3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

// The shared body of the two LoadRenderProperties overloads. `combiner` selects the texture-combiner constant
// setters and the two differing vtable slots.
void LoadProperties(void* self, void* light, const void* properties, void* program, bool combiner)
{
    const unsigned char* const s = Bytes(self);
    const unsigned char* const l = Bytes(light);
    const unsigned char* const mat = *reinterpret_cast<unsigned char* const*>(s + kMaterial);
    const unsigned int count = *reinterpret_cast<const unsigned int*>(properties);
    if (!count) return;

    const float intensity = *reinterpret_cast<const float*>(mat + 0x50);
    const float specularScale = *reinterpret_cast<const float*>(l + 0x24) * intensity;
    const float halfScale = specularScale * 0.5f;
    const unsigned char* const data = *reinterpret_cast<const unsigned char* const*>(
        static_cast<const unsigned char*>(properties) + 8);

    for (unsigned int i = 0; i < count; ++i) {
        const unsigned int* const item = reinterpret_cast<const unsigned int*>(data + i * 8);
        const unsigned int type = item[0];
        const int reg = static_cast<int>(item[1]);
        Vector4 v{0.0f, 0.0f, 0.0f, *reinterpret_cast<const float*>(s + kOpacity)};
        Vector3 v3{0.0f, 0.0f, 0.0f};
        bool use3 = false;

        switch (type) {
        case 0:
            v.x = *reinterpret_cast<const float*>(l + 0x08);
            v.y = *reinterpret_cast<const float*>(l + 0x0c);
            v.z = *reinterpret_cast<const float*>(l + 0x10);
            v.w = 0.0f;
            break;
        case 1:
            v.x = *reinterpret_cast<const float*>(l + 0x08);
            v.y = *reinterpret_cast<const float*>(l + 0x0c);
            v.z = *reinterpret_cast<const float*>(l + 0x10);
            break;
        case 2:
            v.x = *reinterpret_cast<const float*>(l + 0x08) * 0.5f;
            v.y = *reinterpret_cast<const float*>(l + 0x0c) * 0.5f;
            v.z = *reinterpret_cast<const float*>(l + 0x10) * 0.5f;
            v.w = 0.0f;
            break;
        case 3:
            v.x = *reinterpret_cast<const float*>(l + 0x08) * 0.5f;
            v.y = *reinterpret_cast<const float*>(l + 0x0c) * 0.5f;
            v.z = *reinterpret_cast<const float*>(l + 0x10) * 0.5f;
            break;
        case 4:
            v.x = *reinterpret_cast<const float*>(mat + 0x34) * *reinterpret_cast<const float*>(l + 0x08);
            v.y = *reinterpret_cast<const float*>(mat + 0x38) * *reinterpret_cast<const float*>(l + 0x0c);
            v.z = *reinterpret_cast<const float*>(mat + 0x3c) * *reinterpret_cast<const float*>(l + 0x10);
            v.w = 0.0f;
            break;
        case 5:
            v.x = *reinterpret_cast<const float*>(l + 0x08) * *reinterpret_cast<const float*>(mat + 0x34);
            v.y = *reinterpret_cast<const float*>(mat + 0x38) * *reinterpret_cast<const float*>(l + 0x0c);
            v.z = *reinterpret_cast<const float*>(mat + 0x3c) * *reinterpret_cast<const float*>(l + 0x10);
            break;
        case 6:
            v.x = *reinterpret_cast<const float*>(mat + 0x44) * *reinterpret_cast<const float*>(l + 0x18);
            v.y = *reinterpret_cast<const float*>(mat + 0x48) * *reinterpret_cast<const float*>(l + 0x1c);
            v.z = *reinterpret_cast<const float*>(mat + 0x4c) * *reinterpret_cast<const float*>(l + 0x20);
            v.w = 0.0f;
            break;
        case 7:
            v.x = *reinterpret_cast<const float*>(mat + 0x44) * *reinterpret_cast<const float*>(l + 0x18);
            v.y = *reinterpret_cast<const float*>(mat + 0x48) * *reinterpret_cast<const float*>(l + 0x1c);
            v.z = *reinterpret_cast<const float*>(mat + 0x4c) * *reinterpret_cast<const float*>(l + 0x20);
            break;
        case 8:
            v.x = *reinterpret_cast<const float*>(mat + 0x34) * *reinterpret_cast<const float*>(l + 0x08) * specularScale;
            v.y = *reinterpret_cast<const float*>(mat + 0x38) * *reinterpret_cast<const float*>(l + 0x0c) * specularScale;
            v.z = *reinterpret_cast<const float*>(mat + 0x3c) * *reinterpret_cast<const float*>(l + 0x10) * specularScale;
            v.w = 0.0f;
            break;
        case 9:
            v.x = *reinterpret_cast<const float*>(l + 0x08) * *reinterpret_cast<const float*>(mat + 0x34) * specularScale;
            v.y = *reinterpret_cast<const float*>(mat + 0x38) * *reinterpret_cast<const float*>(l + 0x0c) * specularScale;
            v.z = *reinterpret_cast<const float*>(mat + 0x3c) * *reinterpret_cast<const float*>(l + 0x10) * specularScale;
            break;
        case 10:
            v.x = *reinterpret_cast<const float*>(l + 0x18) * *reinterpret_cast<const float*>(mat + 0x44) * specularScale;
            v.y = *reinterpret_cast<const float*>(mat + 0x48) * *reinterpret_cast<const float*>(l + 0x1c) * specularScale;
            v.z = *reinterpret_cast<const float*>(mat + 0x4c) * *reinterpret_cast<const float*>(l + 0x20) * specularScale;
            v.w = 0.0f;
            break;
        case 11:
            v.x = *reinterpret_cast<const float*>(l + 0x18) * *reinterpret_cast<const float*>(mat + 0x44) * specularScale;
            v.y = *reinterpret_cast<const float*>(mat + 0x48) * *reinterpret_cast<const float*>(l + 0x1c) * specularScale;
            v.z = *reinterpret_cast<const float*>(mat + 0x4c) * *reinterpret_cast<const float*>(l + 0x20) * specularScale;
            break;
        case 12:
            v.x = *reinterpret_cast<const float*>(l + 0x08) * *reinterpret_cast<const float*>(mat + 0x34) * halfScale;
            v.y = *reinterpret_cast<const float*>(mat + 0x38) * *reinterpret_cast<const float*>(l + 0x0c) * halfScale;
            v.z = *reinterpret_cast<const float*>(mat + 0x3c) * *reinterpret_cast<const float*>(l + 0x10) * halfScale;
            v.w = 0.0f;
            break;
        case 13:
            v.x = *reinterpret_cast<const float*>(l + 0x08) * *reinterpret_cast<const float*>(mat + 0x34) * halfScale;
            v.y = *reinterpret_cast<const float*>(mat + 0x38) * *reinterpret_cast<const float*>(l + 0x0c) * halfScale;
            v.z = *reinterpret_cast<const float*>(mat + 0x3c) * *reinterpret_cast<const float*>(l + 0x10) * halfScale;
            break;
        case 14:
            v.x = *reinterpret_cast<const float*>(l + 0x18) * *reinterpret_cast<const float*>(mat + 0x44) * halfScale;
            v.y = *reinterpret_cast<const float*>(mat + 0x48) * *reinterpret_cast<const float*>(l + 0x1c) * halfScale;
            v.z = *reinterpret_cast<const float*>(mat + 0x4c) * *reinterpret_cast<const float*>(l + 0x20) * halfScale;
            v.w = 0.0f;
            break;
        case 15:
            v.x = *reinterpret_cast<const float*>(l + 0x18) * *reinterpret_cast<const float*>(mat + 0x44) * halfScale;
            v.y = *reinterpret_cast<const float*>(mat + 0x48) * *reinterpret_cast<const float*>(l + 0x1c) * halfScale;
            v.z = *reinterpret_cast<const float*>(mat + 0x4c) * *reinterpret_cast<const float*>(l + 0x20) * halfScale;
            break;
        case 0x10:
            v.x = *reinterpret_cast<const float*>(l + 0x28) * *reinterpret_cast<const float*>(mat + 0x54);
            v.y = *reinterpret_cast<const float*>(mat + 0x58) * *reinterpret_cast<const float*>(l + 0x2c);
            v.z = *reinterpret_cast<const float*>(mat + 0x5c) * *reinterpret_cast<const float*>(l + 0x30);
            v.w = 1.0f;
            break;
        case 0x11:
            v.x = *reinterpret_cast<const float*>(mat + 0x74);
            v.y = 0.0f;
            v.z = 0.0f;
            v.w = 0.0f;
            use3 = true;
            break;
        case 0x12:
        case 0x16: {
            const Vector3* const src = static_cast<const Vector3*>(LightDirection(light));
            const Vector3 t = TransformDirection(s, *src);
            if (type == 0x12) {
                v3 = t;
            } else {
                // The spot cone basis: n = normalize(1 - t), then two crosses, the first scaled by tan(90 - cutoff).
                Vector3 n = Vector3{1.0f - t.x, 1.0f - t.y, 1.0f - t.z};
                NormaliseDirection(n);
                const Vector3 a{n.x + 1.0f, n.y - 1.0f, n.z + 1.0f};
                Vector3 e1{t.y * a.z - t.z * a.y, t.z * a.x - t.x * a.z, t.x * a.y - t.y * a.x};
                NormaliseDirection(e1);
                Vector3 e2{e1.z * t.y - e1.y * t.z, e1.x * t.z - e1.z * t.x, e1.y * t.x - e1.x * t.y};
                NormaliseDirection(e2);
                const float tangent = static_cast<float>(
                    std::tan((90.0 - static_cast<double>(*reinterpret_cast<const float*>(l + 0x48))) *
                             0.017453292519943295));
                e1.x *= tangent;
                e1.y *= tangent;
                e1.z *= tangent;
                if (combiner) {
                    SetConstVector3()(program, reg, &n);
                    SetConstVector3()(program, reg + 1, &e2);
                    SetConstVector3()(program, reg + 2, &e1);
                } else {
                    SetLocalVector3()(program, reg, &n);
                    SetLocalVector3()(program, reg + 1, &e2);
                    SetLocalVector3()(program, reg + 2, &e1);
                }
                continue;
            }
            use3 = true;
            break;
        }
        case 0x13:
        case 0x14: {
            Vector3 src;
            if (type == 0x13) {
                src = *static_cast<const Vector3*>(LightPosition(light));
            } else {
                void* const view = *reinterpret_cast<void* const*>(s + kView);
                void* const camera = *reinterpret_cast<void* const*>(Bytes(view) + 0x20);
                src = *reinterpret_cast<const Vector3*>(Bytes(camera) + 0x2c);
            }
            const Matrix4 m = *reinterpret_cast<const Matrix4*>(s + kMatrix);
            v3 = ProjectPoint(m, src);
            use3 = true;
            break;
        }
        case 0x15: {
            const float range = *reinterpret_cast<const float*>(l + 0x4c);
            Vector4 tilt{1.0f / (range * 2.0f), 0.5f, 1.0f / range, 0.0f};
            if (combiner)
                reinterpret_cast<FnSetConstVector4>((*reinterpret_cast<void***>(program))[0x30 / 4])(program, reg, &tilt);
            else
                reinterpret_cast<FnSetLocalVector4>((*reinterpret_cast<void***>(program))[0x5c / 4])(program, reg, &tilt);
            continue;
        }
        case 0x17: {
            if (*reinterpret_cast<const int*>(l + 4) != 4) break;
            const float* const lightMatrix = reinterpret_cast<const float*>(l + 0x184);
            const Vector3 c0{lightMatrix[0], lightMatrix[1], lightMatrix[2]};
            const Vector3 c1{lightMatrix[3], lightMatrix[4], lightMatrix[5]};
            const Vector3 c2{lightMatrix[6], lightMatrix[7], lightMatrix[8]};
            if (combiner) {
                SetConstVector3()(program, reg, &c0);
                SetConstVector3()(program, reg + 1, &c1);
                SetConstVector3()(program, reg + 2, &c2);
            } else {
                SetLocalVector3()(program, reg, &c0);
                SetLocalVector3()(program, reg + 1, &c1);
                SetLocalVector3()(program, reg + 2, &c2);
            }
            break;
        }
        case 0x18: {
            if (*reinterpret_cast<const int*>(l + 4) != 4) break;
            void* const texture = *reinterpret_cast<void* const*>(l + 0x180);
            if (!texture) break;
            void** const stateTextures =
                *reinterpret_cast<void***>(const_cast<unsigned char*>(s) + kIndicesArray + 8);
            if (stateTextures[reg] == texture) break;
            CallVoid(texture, 0x44, reinterpret_cast<void*>(static_cast<unsigned long>(reg)));
            stateTextures[reg] = texture;
            break;
        }
        default:
            continue;
        }

        if (use3) {
            if (combiner)
                SetConstVector3()(program, reg, &v3);
            else
                SetLocalVector3()(program, reg, &v3);
        } else {
            if (combiner)
                SetConstVector4()(program, reg, &v);
            else
                SetLocalVector4()(program, reg, &v);
        }
    }
}

}  // namespace

bool __cdecl StateSetup(void* self, const void* lights, const void* modelView)
{
    unsigned char* const p = Bytes(self);
    p[kFlag24] = 1;
    if (p[0x25]) p[0x26] = 0;

    float ambient[3] = {0.0f, 0.0f, 0.0f};
    const unsigned char* const head = *reinterpret_cast<unsigned char* const*>(lights);
    if (head) {
        const unsigned char* chunk = head;
        do {
            const unsigned int n = *reinterpret_cast<const unsigned int*>(chunk + 8);
            for (unsigned int i = 0; i < n; ++i) {
                void* const light = *reinterpret_cast<void* const*>(chunk + 0x0c + i * 4);
                {
                    const unsigned char* const lb = Bytes(light);
                    const int type = *reinterpret_cast<const int*>(lb + 4);
                    void* target = nullptr;
                    bool keep = true;
                    switch (type) {
                    case 0:
                        ambient[0] += *reinterpret_cast<const float*>(lb + 0x08);
                        ambient[1] += *reinterpret_cast<const float*>(lb + 0x0c);
                        ambient[2] += *reinterpret_cast<const float*>(lb + 0x10);
                        break;
                    case 1: {
                        void* const view = *reinterpret_cast<void* const*>(p + kView);
                        void* const camera = *reinterpret_cast<void* const*>(Bytes(view) + 0x20);
                        const Vector3* const pos = static_cast<const Vector3*>(LightPosition(light));
                        const float radius = *reinterpret_cast<const float*>(lb + 0x4c);
                        keep = Clip3D()(camera, pos, radius) != 2;
                        target = p + 0x34;
                        break;
                    }
                    case 2: {
                        void* const view = *reinterpret_cast<void* const*>(p + kView);
                        void* const camera = *reinterpret_cast<void* const*>(Bytes(view) + 0x20);
                        const Vector3* const pos = static_cast<const Vector3*>(LightPosition(light));
                        const float radius = *reinterpret_cast<const float*>(lb + 0x4c);
                        keep = Clip3D()(camera, pos, radius) != 2;
                        target = p + 0x30;
                        break;
                    }
                    case 3:
                        target = p + 0x38;
                        break;
                    case 4: {
                        void* const view = *reinterpret_cast<void* const*>(p + kView);
                        void* const camera = *reinterpret_cast<void* const*>(Bytes(view) + 0x20);
                        const Vector3* const pos = static_cast<const Vector3*>(LightPosition(light));
                        const float radius = *reinterpret_cast<const float*>(lb + 0x4c);
                        keep = Clip3D()(camera, pos, radius) != 2;
                        target = lb[0x1a8] ? p + 0x40 : p + 0x3c;
                        break;
                    }
                    default:
                        break;
                    }
                    if (target && keep) AppendList(target, light);
                }
            }
            chunk = *reinterpret_cast<unsigned char* const*>(chunk);
        } while (chunk != head);
    }

    void* const display = *reinterpret_cast<void* const*>(p + kDisplay);
    const unsigned char* const displayState = *reinterpret_cast<unsigned char* const*>(Bytes(display) + 0x54);
    float r = *reinterpret_cast<const float*>(displayState + 0xa4) + ambient[0];
    float g = *reinterpret_cast<const float*>(displayState + 0xa8) + ambient[1];
    float b = *reinterpret_cast<const float*>(displayState + 0xac) + ambient[2];
    const unsigned char* const mat = *reinterpret_cast<unsigned char* const*>(p + kMaterial);
    r = *reinterpret_cast<const float*>(mat + 0x64) + *reinterpret_cast<const float*>(mat + 0x34) * r;
    g = *reinterpret_cast<const float*>(mat + 0x68) + *reinterpret_cast<const float*>(mat + 0x38) * g;
    b = *reinterpret_cast<const float*>(mat + 0x6c) + *reinterpret_cast<const float*>(mat + 0x3c) * b;
    StateGetAmbientLights(self, r, g, b, *reinterpret_cast<const float*>(p + kOpacity));

    bool any = false;
    static const unsigned int kCheck[5] = {0x34, 0x30, 0x38, 0x3c, 0x40};
    for (unsigned int i = 0; i < 5 && !any; ++i) any = ListCount(p + kCheck[i]) != 0;
    if (any) {
        void* const view = *reinterpret_cast<void* const*>(p + kView);
        void* const camera = *reinterpret_cast<void* const*>(Bytes(view) + 0x20);
        if (Bytes(camera)[0x25c] && UpdateCachedView()) UpdateCachedView()(camera);

        Matrix4 inverted = *static_cast<const Matrix4*>(modelView);
        Invert()(&inverted);
        const void* const viewMatrix = *reinterpret_cast<void* const*>(Bytes(camera) + 0x1b8);
        Matrix4 combined = inverted;
        MultiplyMatricesInPlace(const_cast<Matrix4*>(static_cast<const Matrix4*>(viewMatrix)), &combined);
        std::memcpy(p + kMatrix, combined.m, sizeof combined.m);
        // The 3x3 basis is the matrix transposed: +0x84 gets m0,m4,m8, +0x90 m1,m5,m9, +0x9c m2,m6,m10.
        std::memcpy(p + 0x84, p + 0x44, 4);
        std::memcpy(p + 0x88, p + 0x54, 4);
        std::memcpy(p + 0x8c, p + 0x64, 4);
        std::memcpy(p + 0x90, p + 0x48, 4);
        std::memcpy(p + 0x94, p + 0x58, 4);
        std::memcpy(p + 0x98, p + 0x68, 4);
        std::memcpy(p + 0x9c, p + 0x4c, 4);
        std::memcpy(p + 0xa0, p + 0x5c, 4);
        std::memcpy(p + 0xa4, p + 0x6c, 4);
        CallVoid(display, 0x70, p + kViewMatrix);
    }

    Rect scissor{};
    GetScissor()(display, &scissor);
    std::memcpy(p + kScissor, &scissor, sizeof scissor);
    CallVoid(display, 0x54, const_cast<void*>(modelView));

    *reinterpret_cast<unsigned int*>(p + kIndicesArray) = 0;
    const int stages = *reinterpret_cast<const int*>(Bytes(display) + 0x218);
    for (int i = 0; i < stages; ++i) {
        unsigned int& count = *reinterpret_cast<unsigned int*>(p + kIndicesArray);
        unsigned int& capacity = *reinterpret_cast<unsigned int*>(p + kIndicesArray + 4);
        void** const dataSlot = reinterpret_cast<void**>(p + kIndicesArray + 8);
        if (capacity < count + 1) {
            const unsigned int nextCapacity = (count + 10) / 10 * 10;
            void* const next = Alloc::New(nextCapacity * 4);
            if (*dataSlot && count) std::memcpy(next, *dataSlot, count * 4);
            if (*dataSlot) Alloc::Delete(*dataSlot);
            *dataSlot = next;
            capacity = nextCapacity;
        }
        static_cast<void**>(*dataSlot)[count] = nullptr;
        ++count;
    }

    reinterpret_cast<Thunks*>(self)->StateSetBlendState();
    return true;
}

void __cdecl StateSetScissor(void* self, const void* lights, const void* indices)
{
    unsigned char* const p = Bytes(self);
    void* const view = *reinterpret_cast<void* const*>(p + kView);
    if (*reinterpret_cast<const int*>(Bytes(view) + 0x60) != 3) return;

    RectI rect;
    std::memcpy(&rect, p + kScissor, sizeof rect);
    void* const display = *reinterpret_cast<void* const*>(p + kDisplay);
    if (p[kFlag24]) {
        SetScissor()(display, reinterpret_cast<const Rect*>(&rect));
        return;
    }

    const unsigned int count = *reinterpret_cast<const unsigned int*>(indices);
    const unsigned int* const indexData = *reinterpret_cast<unsigned int* const*>(
        static_cast<const unsigned char*>(indices) + 8);
    if (count) {
        for (unsigned int i = 0; i < count; ++i) {
            void* const light = ListItemAt(lights, indexData[i]);
            if (!light) continue;
            const unsigned char* const lb = Bytes(light);
            const Vector3* const pos = static_cast<const Vector3*>(LightPosition(light));
            const float radius = *reinterpret_cast<const float*>(lb + 0x4c);
            Rect sphere{};
            using Fn = void(__cdecl*)(void*, Rect*, const Vector3*, float, const Matrix4*);
            static Fn fn = reinterpret_cast<Fn>(Local(
                "?CalculateSphereSize@ViewPort@Jet@@QBA?AURect@2@ABVVector3@2@MABVMatrix4@2@@Z"));
            if (!fn)
                fn = reinterpret_cast<Fn>(Original(
                    "?CalculateSphereSize@ViewPort@Jet@@QBA?AURect@2@ABVVector3@2@MABVMatrix4@2@@Z"));
            if (fn) fn(view, &sphere, pos, radius, reinterpret_cast<const Matrix4*>(p + kViewMatrix));
            else std::memset(&sphere, 0, sizeof sphere);
            if (i == 0) {
                std::memcpy(&rect, &sphere, sizeof rect);
            } else {
                RectI other;
                std::memcpy(&other, &sphere, sizeof other);
                const int left = rect.x < other.x ? other.x : rect.x;
                const int right1 = rect.x + rect.w - 1;
                const int right2 = other.x + other.w - 1;
                const int right = right1 > right2 ? right1 : right2;
                const int top = rect.y < other.y ? other.y : rect.y;
                const int bottom1 = rect.y + rect.h - 1;
                const int bottom2 = other.y + other.h - 1;
                const int bottom = bottom1 > bottom2 ? bottom1 : bottom2;
                rect.x = left;
                rect.y = top;
                rect.w = right - left + 1;
                rect.h = bottom - top + 1;
            }
        }
        if (Bytes(view)[0x5c]) {
            void* const kernel = *reinterpret_cast<void* const*>(Bytes(view) + 0x18);
            void* const targetDisplay = kernel ? *reinterpret_cast<void* const*>(Bytes(kernel) + 0x7c) : display;
            using FnSize = const void* (__cdecl*)(void*);
            static FnSize fn = reinterpret_cast<FnSize>(
                RenderSystemProc("?GetTargetSize@Display@Jet@@QAAABUSize@2@XZ"));
            if (fn) {
                const int* const size = static_cast<const int*>(fn(targetDisplay));
                rect.y = size[1] - rect.h - rect.y;
            }
        }
        // Intersect with the state's scissor rect.
        const RectI saved = *reinterpret_cast<const RectI*>(p + kScissor);
        const int left = rect.x > saved.x ? rect.x : saved.x;
        const int top = rect.y > saved.y ? rect.y : saved.y;
        const int right1 = rect.x + rect.w - 1;
        const int right2 = saved.x + saved.w - 1;
        const int bottom1 = rect.y + rect.h - 1;
        const int bottom2 = saved.y + saved.h - 1;
        const int right = right1 < right2 ? right1 : right2;
        const int bottom = bottom1 < bottom2 ? bottom1 : bottom2;
        Rect out;
        out.x = left;
        out.y = top;
        out.w = right - left + 1;
        out.h = bottom - top + 1;
        SetScissor()(display, &out);
        return;
    }
    SetScissor()(display, reinterpret_cast<const Rect*>(&rect));
}

void __cdecl StateLoadPropertiesVP(void* self, void* light, const void* properties, void* program)
{
    if (program) LoadProperties(self, light, properties, program, false);
}

void __cdecl StateLoadPropertiesTC(void* self, void* light, const void* properties, void* combiner)
{
    if (combiner) LoadProperties(self, light, properties, combiner, true);
}

void __cdecl StateRenderSingleLightPasses(void* self, void* lightRender)
{
    unsigned char* const mlr = Bytes(lightRender);
    unsigned char* const state = Bytes(self);
    struct Group {
        unsigned int renderArray;
        unsigned int stateList;
    };
    static const Group groups[6] = {
        {0x08, 0x2c}, {0x14, 0x38}, {0x20, 0x34}, {0x2c, 0x30}, {0x38, 0x3c}, {0x44, 0x40},
    };

    unsigned char* const chunk = static_cast<unsigned char*>(Alloc::New(0x40));
    if (!chunk) return;
    *reinterpret_cast<unsigned char**>(chunk) = chunk;
    *reinterpret_cast<unsigned char**>(chunk + 4) = chunk;
    *reinterpret_cast<unsigned int*>(chunk + 8) = 1;
    void* const single = chunk;

    for (unsigned int g = 0; g < 6; ++g) {
        const unsigned int count = *reinterpret_cast<unsigned int*>(mlr + groups[g].renderArray);
        if (!count) continue;
        void** const data = *reinterpret_cast<void***>(mlr + groups[g].renderArray + 8);
        void* const render = data[count - 1];
        unsigned char* const head = *reinterpret_cast<unsigned char**>(state + groups[g].stateList);
        if (!head) continue;
        unsigned char* walk = head;
        do {
            const unsigned int n = *reinterpret_cast<unsigned int*>(walk + 8);
            for (unsigned int i = 0; i < n; ++i) {
                void* const light = *reinterpret_cast<void**>(walk + 0x0c + i * 4);
                *reinterpret_cast<void**>(chunk + 0x0c) = light;
                Thunks* const thunks = reinterpret_cast<Thunks*>(render);
                thunks->LightRenderRender(self, &single);
            }
            walk = *reinterpret_cast<unsigned char**>(walk);
        } while (walk != head);
    }
    Alloc::Delete(chunk);
}

// ---------------------------------------------------------------------------------------------------------------
// MultiPassOneTextureMaterial::RenderChunkPerVertex (0x37510)

void __cdecl OneTextureRenderChunkPerVertex(void* self, void* display, void* view, void* chunk, const void* lights,
                                            const void* modelView, int pass, unsigned int numIndices,
                                            const unsigned short* indices, unsigned int numPrimitives,
                                            int primitive)
{
    (void)pass;
    unsigned char* const mat = Bytes(self);
    float& cap = *reinterpret_cast<float*>(mat + 0x74);
    const float saved = cap;
    if (cap > 128.0f) cap = 128.0f;

    void* const texture = *reinterpret_cast<void* const*>(mat + 0xc4);
    if (texture) CallVoid(texture, 0x44, nullptr);

    using GetProgramFn = void* (__cdecl*)(void*, int);
    void* const program = reinterpret_cast<GetProgramFn>((*reinterpret_cast<void***>(chunk))[0xf0 / 4])(chunk, 0);
    const float chunkOpacity = *reinterpret_cast<const float*>(Bytes(chunk) + 0x3c);
    void* const camera = *reinterpret_cast<void* const*>(Bytes(view) + 0x20);
    const float materialOpacity = *reinterpret_cast<const float*>(mat + 0x80);
    if (Bytes(camera)[0x25c] && UpdateCachedView()) UpdateCachedView()(camera);

    using SetLightingFn = void(__cdecl*)(void*, void*, const Matrix4*, const void*, const void*, const void*,
                                         const Matrix4*, float, int);
    static SetLightingFn setLighting = reinterpret_cast<SetLightingFn>(RenderSystemProc(
        "?SetLightingState@Material@Jet@@QBAXPAVDisplay@2@PBVMatrix4@2@PBVGeometryChunk@2@PBVVertexProgram@2@"
        "PBV?$FastDynamicArray@PAVDisplayLight@Jet@@$0N@U?$Less@PAVDisplayLight@Jet@@PAV12@@2@@2@1M_N@Z"));
    if (setLighting)
        setLighting(self, display, reinterpret_cast<const Matrix4*>(Bytes(camera) + 0x1b8), chunk, program, lights,
                    static_cast<const Matrix4*>(modelView), materialOpacity * chunkOpacity, 0);

    CallVoid(display, 0x54, const_cast<void*>(modelView));

    if (indices) {
        using Fn = void(__cdecl*)(void*, void*, void*, void*, unsigned int, const unsigned short*, unsigned int,
                                  int, int, int);
        reinterpret_cast<Fn>((*reinterpret_cast<void***>(chunk))[0x3c / 4])(
            chunk, display, view, program, numIndices, indices, numPrimitives, primitive, 1, 0);
    } else {
        using Fn = void(__cdecl*)(void*, void*, void*, void*, int, int);
        reinterpret_cast<Fn>((*reinterpret_cast<void***>(chunk))[0x38 / 4])(chunk, display, view, program, 1, 0);
    }
    cap = saved;
    (void)lights;
    (void)modelView;
}

// ---------------------------------------------------------------------------------------------------------------
// MultiPassBumpChunkBase::CreateTangentVectors (0x2c250)

namespace {

struct ItemView {
    unsigned int stride;
    void* data;
};
JET_LAYOUT32(sizeof(ItemView) == 8);

using GetVector3Fn = void(__cdecl*)(void*, ItemView*, int, int);
using GetVector2Fn = void(__cdecl*)(void*, ItemView*, int, int);

float ApproxMagnitude(const Vector3& v)
{
    const float s = v.x * v.x + v.y * v.y + v.z * v.z;
    unsigned int bits;
    std::memcpy(&bits, &s, sizeof bits);
    const unsigned int e = (bits >> 23) & 0xffu;
    const unsigned int seedBits = (((0x17cu - e) << 22) & 0xff800000u) | 0u;
    float seed;
    std::memcpy(&seed, &seedBits, sizeof seed);
    const double half = static_cast<double>(s) * 0.5;
    double y = seed;
    y = (1.5 - (y * y) * half) * y;
    y = y * (1.5 - (y * y) * half);
    return static_cast<float>(y * static_cast<double>(s));
}

}  // namespace

void __cdecl BumpChunkCreateTangentVectors(void* self, void* chunk)
{
    (void)self;
    unsigned char* const c = Bytes(chunk);
    if (!*reinterpret_cast<unsigned int*>(c + 0x18)) return;

    ItemView tangent{0, nullptr}, normal{0, nullptr}, position{0, nullptr}, uv{0, nullptr};
    reinterpret_cast<GetVector3Fn>((*reinterpret_cast<void***>(chunk))[0xc4 / 4])(chunk, &tangent, 0x1a, 0);
    reinterpret_cast<GetVector3Fn>((*reinterpret_cast<void***>(chunk))[0xc4 / 4])(chunk, &normal, 1, 0);
    reinterpret_cast<GetVector3Fn>((*reinterpret_cast<void***>(chunk))[0xc4 / 4])(chunk, &position, 4, 0);
    reinterpret_cast<GetVector2Fn>((*reinterpret_cast<void***>(chunk))[0xbc / 4])(chunk, &uv, 0xf, 0);

    const unsigned int vertexCount = *reinterpret_cast<unsigned int*>(c + 0x24);
    const unsigned int primitiveCount = *reinterpret_cast<unsigned int*>(c + 0x1c);

    float* const tangents = static_cast<float*>(tangent.data);
    for (unsigned int i = 0; i < vertexCount; ++i) {
        float* const t = reinterpret_cast<float*>(reinterpret_cast<unsigned char*>(tangents) + tangent.stride * i);
        t[0] = 0.0f;
        t[1] = 0.0f;
        t[2] = 0.0f;
    }

    const unsigned char* const posBase = static_cast<const unsigned char*>(position.data);
    const unsigned char* const uvBase = static_cast<const unsigned char*>(uv.data);
    unsigned char* const tanBase = static_cast<unsigned char*>(tangent.data);
    FnGetTriangle const tri = GetTriangleIndexes();
    if (!tri) return;

    for (unsigned int p = 0; p < primitiveCount; ++p) {
        unsigned long a = 0, b = 0, cc = 0;
        tri(chunk, static_cast<int>(p), &a, &b, &cc);
        const float* const pa = reinterpret_cast<const float*>(posBase + position.stride * a);
        const float* const pb = reinterpret_cast<const float*>(posBase + position.stride * b);
        const float* const pc = reinterpret_cast<const float*>(posBase + position.stride * cc);
        const float* const ua = reinterpret_cast<const float*>(uvBase + uv.stride * a);
        const float* const ub = reinterpret_cast<const float*>(uvBase + uv.stride * b);
        const float* const uc = reinterpret_cast<const float*>(uvBase + uv.stride * cc);
        float* const ta = reinterpret_cast<float*>(tanBase + tangent.stride * a);
        float* const tb = reinterpret_cast<float*>(tanBase + tangent.stride * b);
        float* const tc = reinterpret_cast<float*>(tanBase + tangent.stride * cc);

        const float d1x = pb[0] - pa[0];
        const float d1y = pb[1] - pa[1];
        const float d2x = pc[0] - pa[0];
        const float d2y = pc[1] - pa[1];
        const float det = d2y * d1x - d2x * d1y;
        if (std::fabs(det) > 1e-5f) {
            float s = -((uc[0] - ua[0]) * d1y - (ub[0] - ua[0]) * d2y) / det;
            ta[0] += s;
            tb[0] += s;
            tc[0] += s;
        }
        if (std::fabs(det) > 1e-5f) {
            float s = -((uc[1] - ua[1]) * d1y - (ub[1] - ua[1]) * d2y) / det;
            ta[1] += s;
            tb[1] += s;
            tc[1] += s;
        }
        if (std::fabs(det) > 1e-5f) {
            float s = -((uc[2] - ua[2]) * d1y - (ub[2] - ua[2]) * d2y) / det;
            ta[2] += s;
            tb[2] += s;
            tc[2] += s;
        }
    }

    int* const merge = static_cast<int*>(Alloc::New(vertexCount * 4));
    if (!merge) return;
    for (unsigned int i = 0; i < vertexCount; ++i) merge[i] = -1;

    for (unsigned int i = 0; i + 1 < vertexCount; ++i) {
        if (merge[i] != -1) continue;
        for (unsigned int j = i + 1; j < vertexCount; ++j) {
            const float* const pi = reinterpret_cast<const float*>(posBase + position.stride * i);
            const float* const pj = reinterpret_cast<const float*>(posBase + position.stride * j);
            if (!(pj[0] == pi[0] && pj[1] == pi[1] && pj[2] == pi[2])) continue;
            const float* const ni = reinterpret_cast<const float*>(normal.data) + (normal.stride / 4) * i;
            const float* const nj = reinterpret_cast<const float*>(normal.data) + (normal.stride / 4) * j;
            if (!(nj[0] == ni[0] && nj[1] == ni[1] && nj[2] == ni[2])) continue;
            float* const ti = reinterpret_cast<float*>(tanBase + tangent.stride * i);
            const float* const tj = reinterpret_cast<const float*>(tanBase + tangent.stride * j);
            ti[0] = tj[0] + ti[0];
            ti[1] = tj[1] + ti[1];
            ti[2] = tj[2] + ti[2];
            merge[j] = static_cast<int>(i);
        }
    }

    for (unsigned int i = 0; i < vertexCount; ++i) {
        if (merge[i] != -1) continue;
        float* const t = reinterpret_cast<float*>(tanBase + tangent.stride * i);
        Vector3 v{t[0], t[1], t[2]};
        const float len = ApproxMagnitude(v);
        if (len <= 0.0f) {
            t[0] = 1.0f;
            t[1] = 0.0f;
            t[2] = 0.0f;
        } else {
            const float inv = 1.0f / len;
            t[0] *= inv;
            t[1] *= inv;
            t[2] *= inv;
        }
        const float* const n = reinterpret_cast<const float*>(normal.data) + (normal.stride / 4) * i;
        const float qx = n[2] * t[1] - n[1] * t[2];
        const float qy = n[0] * t[2] - n[2] * t[0];
        const float qz = n[1] * t[0] - n[0] * t[1];
        t[0] = qz * n[1] - qy * n[2];
        t[1] = qx * n[2] - n[0] * qz;
        t[2] = n[0] * qy - qx * n[1];
    }

    for (unsigned int i = 0; i < vertexCount; ++i) {
        const int src = merge[i];
        if (src == -1) continue;
        float* const dstT = reinterpret_cast<float*>(tanBase + tangent.stride * i);
        const float* const srcT = reinterpret_cast<const float*>(tanBase + tangent.stride * src);
        dstT[0] = srcT[0];
        dstT[1] = srcT[1];
        dstT[2] = srcT[2];
    }
    Alloc::Delete(merge);
}

}  // namespace Last
}  // namespace MultiPass
}  // namespace Jet
