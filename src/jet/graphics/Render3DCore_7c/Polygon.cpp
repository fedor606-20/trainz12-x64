// Jet::Polygon (U1d2): 0x1c bytes, vftable +0x00, Plane base (normal +0x04, d +0x10), numVerts +0x14,
// verts +0x18. The vertex array is allocated through KernelStandard's Alloc::New/Delete, like NewArray/DeleteArray
// in the SDK. Only Intersect is still forwarded (it needs Plane::Intersect from U1b).
#include <jet/Render3DCoreSolids.h>
#include <jet/KernelStandard.h>

#include <cfloat>
#include <cmath>
#include <cstring>

namespace Jet {

namespace {

// Polygon2D's TS12 layout: {int numVerts; Vector2* verts;} (8 bytes).
struct Polygon2DFields {
    int numVerts;
    const Vector2* verts;
};
static_assert(sizeof(Polygon2DFields) == 8, "Polygon2D");

Vector3* AllocateVerts(int count)
{
    return static_cast<Vector3*>(Alloc::New(static_cast<unsigned int>(count) * 0xcu));
}

void FreeVerts(Vector3* verts)
{
    if (verts != nullptr)
        Alloc::Delete(verts);
}

float AbsNeg(float v)
{
    if (v < 0.0f)
        v = -0.0f - v;
    return v;
}

}  // namespace

Polygon::Polygon()
{
    numVerts = 0;
    verts = nullptr;
}

Polygon::Polygon(const Polygon& poly)
{
    SolidsDetail::X87Copy(normal.x, poly.normal.x);
    SolidsDetail::X87Copy(normal.y, poly.normal.y);
    SolidsDetail::X87Copy(normal.z, poly.normal.z);
    SolidsDetail::X87Copy(d, poly.d);
    numVerts = poly.numVerts;
    verts = AllocateVerts(numVerts);
    std::memcpy(verts, poly.verts, static_cast<size_t>(numVerts) * sizeof(Vector3));
}

Polygon::Polygon(const Polygon2D& poly)
{
    verts = nullptr;
    const Polygon2DFields* p = reinterpret_cast<const Polygon2DFields*>(&poly);
    Set(p->numVerts, p->verts);
}

Polygon::Polygon(int numVerts_, const Vector3* vList)
{
    verts = nullptr;
    Set(numVerts_, vList);
}

Polygon::~Polygon()
{
    FreeVerts(verts);
    verts = nullptr;
}

// 0x10054fd0
Polygon& __cdecl Polygon::operator=(const Polygon& other)
{
    FreeVerts(verts);
    if (other.numVerts != 0) {
        numVerts = other.numVerts;
        verts = AllocateVerts(numVerts);
        std::memcpy(verts, other.verts, static_cast<size_t>(numVerts) * sizeof(Vector3));
    } else {
        numVerts = 0;
        verts = nullptr;
    }
    SolidsDetail::X87Copy(normal.x, other.normal.x);
    SolidsDetail::X87Copy(normal.y, other.normal.y);
    SolidsDetail::X87Copy(normal.z, other.normal.z);
    SolidsDetail::X87Copy(d, other.d);
    return *this;
}

// 0x10055060
Polygon& __cdecl Polygon::operator=(const Polygon2D& other)
{
    FreeVerts(verts);
    const Polygon2DFields* p = reinterpret_cast<const Polygon2DFields*>(&other);
    if (p->numVerts != 0) {
        numVerts = p->numVerts;
        verts = AllocateVerts(numVerts);
        for (int i = 0; i < numVerts; ++i) {
            SolidsDetail::X87Copy(verts[i].x, p->verts[i].x);
            SolidsDetail::X87Copy(verts[i].y, p->verts[i].y);
            verts[i].z = 0.0f;
        }
    } else {
        numVerts = 0;
        verts = nullptr;
    }
    Reset();
    return *this;
}

int __cdecl Polygon::GetNumVerts() const
{
    return numVerts;
}

// 0x10054eb0: normal = (v1 - v0) x (v2 - v1), Normalize, d = normal . v0.
void __cdecl Polygon::Reset()
{
    if (verts != nullptr) {
        const Vector3& v0 = verts[0];
        const Vector3& v1 = verts[1];
        const Vector3& v2 = verts[2];
#if defined(_M_IX86)
        const float* base = &verts[0].x;
        float* out = &normal.x;
        __asm {
            mov eax, base
            mov ecx, out
            movss xmm0, dword ptr [eax + 01ch]
            subss xmm0, dword ptr [eax + 010h]
            movss xmm4, dword ptr [eax + 018h]
            subss xmm4, dword ptr [eax + 00ch]
            movss xmm2, dword ptr [eax + 020h]
            subss xmm2, dword ptr [eax + 014h]
            movss xmm5, dword ptr [eax + 00ch]
            subss xmm5, dword ptr [eax]
            movss xmm1, dword ptr [eax + 014h]
            subss xmm1, dword ptr [eax + 008h]
            movss xmm3, dword ptr [eax + 010h]
            subss xmm3, dword ptr [eax + 004h]
            movaps xmm7, xmm1
            movaps xmm6, xmm2
            mulss xmm7, xmm0
            mulss xmm6, xmm3
            mulss xmm1, xmm4
            mulss xmm2, xmm5
            mulss xmm0, xmm5
            mulss xmm3, xmm4
            subss xmm6, xmm7
            subss xmm1, xmm2
            subss xmm0, xmm3
            movss dword ptr [ecx], xmm6
            movss dword ptr [ecx + 4], xmm1
            movss dword ptr [ecx + 8], xmm0
        }
#else
        const float ax = v2.x - v1.x;
        const float ay = v2.y - v1.y;
        const float az = v2.z - v1.z;
        const float bx = v1.x - v0.x;
        const float by = v1.y - v0.y;
        const float bz = v1.z - v0.z;
        normal.x = SolidsDetail::SubSS(SolidsDetail::MulSS(az, by), SolidsDetail::MulSS(bz, ay));
        normal.y = SolidsDetail::SubSS(SolidsDetail::MulSS(bz, ax), SolidsDetail::MulSS(az, bx));
        normal.z = SolidsDetail::SubSS(SolidsDetail::MulSS(ay, bx), SolidsDetail::MulSS(by, ax));
#endif
        SolidsDetail::Normalize(normal);
        d = SolidsDetail::AddSS(SolidsDetail::AddSS(SolidsDetail::MulSS(normal.z, v0.z),
                                                    SolidsDetail::MulSS(normal.y, v0.y)),
                                SolidsDetail::MulSS(normal.x, v0.x));
    } else {
        normal = Vector3{0.0f, 0.0f, 1.0f};
        d = 0.0f;
    }
}

// 0x10055e80
void __cdecl Polygon::Set(int n)
{
    if (verts == nullptr) {
        if (n != 0) {
            numVerts = n;
            verts = AllocateVerts(n);
        } else {
            numVerts = 0;
            verts = nullptr;
        }
        return;
    }
    if (n < numVerts) {
        numVerts = n;
        return;
    }
    if (numVerts < n) {
        Vector3* fresh = AllocateVerts(n);
        for (int i = 0; i < numVerts; ++i) {
            SolidsDetail::X87Copy(fresh[i].x, verts[i].x);
            SolidsDetail::X87Copy(fresh[i].y, verts[i].y);
            SolidsDetail::X87Copy(fresh[i].z, verts[i].z);
        }
        FreeVerts(verts);
        verts = fresh;
        numVerts = n;
    }
}

// 0x10055f50
void __cdecl Polygon::Set(int n, const Vector3* vList)
{
    FreeVerts(verts);
    numVerts = n;
    verts = AllocateVerts(n);
    for (int i = 0; i < numVerts; ++i) {
        SolidsDetail::X87Copy(verts[i].x, vList[i].x);
        SolidsDetail::X87Copy(verts[i].y, vList[i].y);
        SolidsDetail::X87Copy(verts[i].z, vList[i].z);
    }
    Reset();
}

// 0x10055fd0
void __cdecl Polygon::Set(int n, const Vector2* vList)
{
    FreeVerts(verts);
    numVerts = n;
    verts = AllocateVerts(n);
    for (int i = 0; i < numVerts; ++i) {
        SolidsDetail::X87Copy(verts[i].x, vList[i].x);
        SolidsDetail::X87Copy(verts[i].y, vList[i].y);
        verts[i].z = 0.0f;
    }
    Reset();
}

const Vector3& __cdecl Polygon::GetVertex(int i) const
{
    return verts[i];
}

Vector3 __cdecl Polygon::GetVertex(int i)
{
    return verts[i];
}

void __cdecl Polygon::GetVertex(Vector3& out, int i) const
{
    SolidsDetail::X87Copy(out.x, verts[i].x);
    SolidsDetail::X87Copy(out.y, verts[i].y);
    SolidsDetail::X87Copy(out.z, verts[i].z);
}

void __cdecl Polygon::GetVertex(Vector2& out, int i) const
{
    SolidsDetail::X87Copy(out.x, verts[i].x);
    SolidsDetail::X87Copy(out.y, verts[i].y);
}

const Vector3* __cdecl Polygon::GetVertexArray() const
{
    return verts;
}

void __cdecl Polygon::SetVertex(const Vector3& v, int i)
{
    SolidsDetail::X87Copy(verts[i].x, v.x);
    SolidsDetail::X87Copy(verts[i].y, v.y);
    SolidsDetail::X87Copy(verts[i].z, v.z);
}

// 0x10056060
bool __cdecl Polygon::IsInsideXY(const Vector3& point) const
{
    const float px = point.x;
    const float py = point.y;
    int n = numVerts;
    if (n != 0) {
        const Vector3* cur = verts;
        const Vector3* prev = verts + n - 1;
        do {
            if (0.0f < (cur->y - prev->y) * (px - prev->x) - (cur->x - prev->x) * (py - prev->y))
                return false;
            prev = cur;
            ++cur;
        } while (--n != 0);
    }
    return true;
}

// 0x100560e0
bool __cdecl Polygon::IsInsideYZ(const Vector3& point) const
{
    int n = numVerts;
    if (n != 0) {
        const Vector3* cur = verts;
        const Vector3* prev = verts + n - 1;
        do {
            if (0.0f < (cur->z - prev->z) * (point.y - prev->y) - (cur->y - prev->y) * (point.z - prev->z))
                return false;
            prev = cur;
            ++cur;
        } while (--n != 0);
    }
    return true;
}

// 0x10056160
bool __cdecl Polygon::IsInsideZX(const Vector3& point) const
{
    int n = numVerts;
    if (n != 0) {
        const Vector3* cur = verts;
        const Vector3* prev = verts + n - 1;
        do {
            if (0.0f < (cur->x - prev->x) * (point.z - prev->z) - (cur->z - prev->z) * (point.x - prev->x))
                return false;
            prev = cur;
            ++cur;
        } while (--n != 0);
    }
    return true;
}

// 0x100561e0
bool __cdecl Polygon::IsInsideXYNegative(const Vector3& point) const
{
    int n = numVerts;
    if (n != 0) {
        const Vector3* cur = verts;
        const Vector3* prev = verts + n - 1;
        do {
            if ((cur->y - prev->y) * (point.x - prev->x) - (cur->x - prev->x) * (point.y - prev->y) < 0.0f)
                return false;
            prev = cur;
            ++cur;
        } while (--n != 0);
    }
    return true;
}

// 0x10056260
bool __cdecl Polygon::IsInsideYZNegative(const Vector3& point) const
{
    int n = numVerts;
    if (n != 0) {
        const Vector3* cur = verts;
        const Vector3* prev = verts + n - 1;
        do {
            if ((cur->z - prev->z) * (point.y - prev->y) - (cur->y - prev->y) * (point.z - prev->z) < 0.0f)
                return false;
            prev = cur;
            ++cur;
        } while (--n != 0);
    }
    return true;
}

// 0x100562e0
bool __cdecl Polygon::IsInsideZXNegative(const Vector3& point) const
{
    int n = numVerts;
    if (n != 0) {
        const Vector3* cur = verts;
        const Vector3* prev = verts + n - 1;
        do {
            if ((cur->x - prev->x) * (point.z - prev->z) - (cur->z - prev->z) * (point.x - prev->x) < 0.0f)
                return false;
            prev = cur;
            ++cur;
        } while (--n != 0);
    }
    return true;
}

// 0x10056360: the dominant normal axis selects the 2D projection; the sign selects the winding test.
bool __cdecl Polygon::IsInside(const Vector3& point) const
{
    const float ax = AbsNeg(normal.x);
    const float ay = AbsNeg(normal.y);
    const float az = AbsNeg(normal.z);
    if (!(ax > ay)) {
        if (!(ay > az))
            return !(normal.z > 0.0f) ? IsInsideXYNegative(point) : IsInsideXY(point);
        return !(normal.y > 0.0f) ? IsInsideZXNegative(point) : IsInsideZX(point);
    }
    if (!(ax > az))
        return !(normal.z > 0.0f) ? IsInsideXYNegative(point) : IsInsideXY(point);
    return !(normal.x > 0.0f) ? IsInsideYZNegative(point) : IsInsideYZ(point);
}

// 0x10056420: distance to the nearest edge (squared) when the point is outside; the original's unrolled loops
// are four copies of the same edge step.
bool __cdecl Polygon::IsInsideXY(const Vector3& point, float& distSq) const
{
    const int n = numVerts;
    float nearest = 3.4028235e+38f;
    bool inside = true;
    if (n != 0) {
        const Vector3* prev = verts + n - 1;
        for (int i = 0; i < n; ++i) {
            const Vector3* cur = verts + i;
            const float dx = point.x - prev->x;
            const float dy = point.y - prev->y;
            float d2 = dy * dy + dx * dx;
            if (d2 < nearest)
                nearest = d2;
            const float ex = cur->x - prev->x;
            const float ey = cur->y - prev->y;
            const float cross = ey * dx - ex * dy;
            if (0.0f < cross) {
                if (0.0f <= dy * ey + ex * dx && (point.x - cur->x) * ex + (point.y - cur->y) * ey <= 0.0f) {
                    const float edgeDist = (cross * cross) / (ex * ex + ey * ey);
                    if (edgeDist < nearest)
                        nearest = edgeDist;
                }
                inside = false;
            }
            prev = cur;
        }
    }
    if (!inside)
        distSq = nearest;
    return inside;
}

// 0x10056910
bool __cdecl Polygon::CalculateCenter(Vector3& center) const
{
    if (numVerts == 0)
        return false;
    center.x = 0.0f;
    center.y = 0.0f;
    center.z = 0.0f;
    for (int i = 0; i < numVerts; ++i) {
        center.x = SolidsDetail::AddSS(verts[i].x, center.x);
        center.y = SolidsDetail::AddSS(verts[i].y, center.y);
        center.z = SolidsDetail::AddSS(verts[i].z, center.z);
    }
    const float scale = 1.0f / static_cast<float>(numVerts);
    center.x = SolidsDetail::MulSS(center.x, scale);
    center.y = SolidsDetail::MulSS(scale, center.y);
    center.z = SolidsDetail::MulSS(center.z, scale);
    return true;
}

// 0x100569c0: centre = average vertex, radius = farthest distance. The original starts the sums from the
// sphere's current centre and walks the vertices in order; both are kept for NaN/rounding parity.
bool __cdecl Polygon::CalculateBoundingSphere(Sphere& sphere) const
{
    if (numVerts == 0)
        return false;
    float sx = sphere.center.x;
    float sy = sphere.center.y;
    float sz = sphere.center.z;
    for (int i = 0; i < numVerts; ++i) {
        sx = SolidsDetail::AddSS(verts[i].x, sx);
        sy = SolidsDetail::AddSS(verts[i].y, sy);
        sz = SolidsDetail::AddSS(verts[i].z, sz);
    }
    const float inv = 1.0f / static_cast<float>(numVerts);
    sphere.center.x = SolidsDetail::MulSS(inv, sx);
    sphere.center.y = SolidsDetail::MulSS(inv, sy);
    sphere.center.z = SolidsDetail::MulSS(inv, sz);

    const float dx0 = SolidsDetail::SubSS(verts[0].x, sphere.center.x);
    const float dy0 = SolidsDetail::SubSS(verts[0].y, sphere.center.y);
    const float dz0 = SolidsDetail::SubSS(verts[0].z, sphere.center.z);
    float farthest = SolidsDetail::AddSS(SolidsDetail::AddSS(SolidsDetail::MulSS(dz0, dz0),
                                                             SolidsDetail::MulSS(dy0, dy0)),
                                         SolidsDetail::MulSS(dx0, dx0));
    for (int i = 1; i < numVerts; ++i) {
        const float dx = SolidsDetail::SubSS(verts[i].x, sphere.center.x);
        const float dy = SolidsDetail::SubSS(verts[i].y, sphere.center.y);
        const float dz = SolidsDetail::SubSS(verts[i].z, sphere.center.z);
        const float d2 = SolidsDetail::AddSS(
            SolidsDetail::AddSS(SolidsDetail::MulSS(dz, dz), SolidsDetail::MulSS(dy, dy)),
            SolidsDetail::MulSS(dx, dx));
        if (farthest < d2)
            farthest = d2;
    }
    if (farthest != 0.0f) {
        sphere.radius = SolidsDetail::SqrtF(farthest);
        return true;
    }
    sphere.radius = 0.0f;
    return false;
}

// 0x10056df0
void __cdecl Polygon::Flip()
{
    for (int i = 0; i < numVerts / 2; ++i) {
        const Vector3 tmp = verts[i];
        verts[i] = verts[numVerts - 1 - i];
        verts[numVerts - 1 - i] = tmp;
    }
}

// 0x10056ea0
void __cdecl Polygon::RemoveDuplicates()
{
    if (numVerts <= 1)
        return;
    bool changed = false;
    Vector3* v = verts;
    const int last = numVerts - 1;
    if (v[0].x == v[last].x && v[0].y == v[last].y && v[0].z == v[last].z) {
        numVerts = last;
        changed = true;
    }
    if (numVerts > 1) {
        int off = 1;
        do {
            const Vector3* prev = verts + off - 1;
            const Vector3* cur = verts + off;
            if (prev->x == cur->x && prev->y == cur->y && prev->z == cur->z) {
                for (int j = off + 1; j < numVerts; ++j)
                    verts[j - 1] = verts[j];
                --numVerts;
                changed = true;
            } else {
                ++off;
            }
        } while (off < numVerts);
    }
    if (changed) {
        Vector3* fresh = AllocateVerts(numVerts);
        std::memcpy(fresh, verts, static_cast<size_t>(numVerts) * sizeof(Vector3));
        FreeVerts(verts);
        verts = fresh;
    }
}

// 0x10056fe0: drop the wrap pair, then each vertex closer than the tolerance to its predecessor.
void __cdecl Polygon::ReduceWithTolerance(float tolerance)
{
    if (numVerts <= 1)
        return;
    bool changed = false;
    Vector3 diff{verts[0].x - verts[numVerts - 1].x, verts[0].y - verts[numVerts - 1].y,
                 verts[0].z - verts[numVerts - 1].z};
    if (static_cast<double>(tolerance) > SolidsDetail::Length(diff)) {
        --numVerts;
        changed = true;
    }
    if (numVerts > 1) {
        int off = 1;
        do {
            const Vector3& prev = verts[off - 1];
            const Vector3& cur = verts[off];
            Vector3 delta{prev.x - cur.x, prev.y - cur.y, prev.z - cur.z};
            if (static_cast<double>(tolerance) > SolidsDetail::Length(delta)) {
                for (int j = off + 1; j < numVerts; ++j)
                    verts[j - 1] = verts[j];
                --numVerts;
                changed = true;
            } else {
                ++off;
            }
        } while (off < numVerts);
    }
    if (changed) {
        Vector3* fresh = AllocateVerts(numVerts);
        std::memcpy(fresh, verts, static_cast<size_t>(numVerts) * sizeof(Vector3));
        FreeVerts(verts);
        verts = fresh;
    }
}

// 0x100571b0
void __cdecl Polygon::Offset(const Vector3& offset)
{
    for (int i = 0; i < numVerts; ++i) {
        verts[i].x = SolidsDetail::AddSS(verts[i].x, offset.x);
        verts[i].y = SolidsDetail::AddSS(offset.y, verts[i].y);
        verts[i].z = SolidsDetail::AddSS(offset.z, verts[i].z);
    }
}

}  // namespace Jet
