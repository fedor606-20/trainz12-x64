// Unit U1c of Render3DCore_7c: Jet::OrientedBox, transcribed from the TS12 assembly (addresses per member).
//
// Layout: centre Vector3 +0, three axes +0xc..+0x2c (each a Vector3), extents Vector3 +0x30; 0x3c bytes, no vftable.
// Ordinary members are __cdecl (this on the stack), constructors and operator= __thiscall, CalcLocalDir/Pos
// __thiscall (QBEX). Float copies the original makes through the x87 stack (fld/fstp) go through X87Copy so a
// signalling NaN is quieted the same way. Arithmetic that the original runs on the x87 stack at the CRT's 53-bit
// precision is done in double here and rounded once on the store; the remaining arithmetic is SSE single precision
// with the assembly's operand and grouping order.
#include <jet/Render3DCoreOrientedBox.h>

#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstring>

namespace Jet {

namespace {

// ---- x87 helpers -----------------------------------------------------------------------------------------------

// fld dword / fstp dword: float -> float, but a signalling NaN comes out quiet (the original copies many fields this
// way; the bits are visible to callers that compare raw memory).
void X87Copy(float& dst, float src)
{
#if defined(_M_IX86)
    float* p = &dst;
    __asm {
        fld dword ptr [src]
        mov eax, p
        fstp dword ptr [eax]
    }
#else
    dst = src;
#endif
}

void X87CopyVec(Vector3& dst, const Vector3& src)
{
    X87Copy(dst.x, src.x);
    X87Copy(dst.y, src.y);
    X87Copy(dst.z, src.z);
}

// The 512-entry square-root seed table the original reads (KernelStandard_7c.dll at RVA 0x72078, the same values
// tools/extract_data.py produces for KernelStandard's StdMathLUT.h). It is private data of KernelStandard's StdMath,
// so Render3DCore keeps its own copy of the 1/2 exponent-fold table for the cross-product normalisation.
static const unsigned int kSqrtLut[512] = {
    0x00350000u, 0x00348000u, 0x00340000u, 0x00340000u, 0x00338000u, 0x00330000u, 0x00330000u, 0x00328000u,
    0x00320000u, 0x00320000u, 0x00318000u, 0x00310000u, 0x00310000u, 0x00308000u, 0x00300000u, 0x00300000u,
    0x002f8000u, 0x002f0000u, 0x002f0000u, 0x002e8000u, 0x002e0000u, 0x002e0000u, 0x002d8000u, 0x002d8000u,
    0x002d0000u, 0x002c8000u, 0x002c8000u, 0x002c0000u, 0x002b8000u, 0x002b8000u, 0x002b0000u, 0x002b0000u,
    0x002a8000u, 0x002a0000u, 0x002a0000u, 0x00298000u, 0x00298000u, 0x00290000u, 0x00290000u, 0x00288000u,
    0x00280000u, 0x00280000u, 0x00278000u, 0x00278000u, 0x00270000u, 0x00270000u, 0x00268000u, 0x00268000u,
    0x00260000u, 0x00258000u, 0x00258000u, 0x00250000u, 0x00250000u, 0x00248000u, 0x00248000u, 0x00240000u,
    0x00240000u, 0x00238000u, 0x00238000u, 0x00230000u, 0x00230000u, 0x00228000u, 0x00228000u, 0x00220000u,
    0x00220000u, 0x00218000u, 0x00218000u, 0x00210000u, 0x00210000u, 0x00208000u, 0x00208000u, 0x00200000u,
    0x00200000u, 0x001f8000u, 0x001f8000u, 0x001f0000u, 0x001f0000u, 0x001e8000u, 0x001e8000u, 0x001e0000u,
    0x001e0000u, 0x001d8000u, 0x001d8000u, 0x001d0000u, 0x001d0000u, 0x001c8000u, 0x001c8000u, 0x001c8000u,
    0x001c0000u, 0x001c0000u, 0x001b8000u, 0x001b8000u, 0x001b0000u, 0x001b0000u, 0x001a8000u, 0x001a8000u,
    0x001a0000u, 0x001a0000u, 0x001a0000u, 0x00198000u, 0x00198000u, 0x00190000u, 0x00190000u, 0x00188000u,
    0x00188000u, 0x00188000u, 0x00180000u, 0x00180000u, 0x00178000u, 0x00178000u, 0x00178000u, 0x00170000u,
    0x00170000u, 0x00168000u, 0x00168000u, 0x00160000u, 0x00160000u, 0x00160000u, 0x00158000u, 0x00158000u,
    0x00150000u, 0x00150000u, 0x00150000u, 0x00148000u, 0x00148000u, 0x00148000u, 0x00140000u, 0x00140000u,
    0x00138000u, 0x00138000u, 0x00138000u, 0x00130000u, 0x00130000u, 0x00128000u, 0x00128000u, 0x00128000u,
    0x00120000u, 0x00120000u, 0x00120000u, 0x00118000u, 0x00118000u, 0x00110000u, 0x00110000u, 0x00110000u,
    0x00108000u, 0x00108000u, 0x00108000u, 0x00100000u, 0x00100000u, 0x00100000u, 0x000f8000u, 0x000f8000u,
    0x000f8000u, 0x000f0000u, 0x000f0000u, 0x000e8000u, 0x000e8000u, 0x000e8000u, 0x000e0000u, 0x000e0000u,
    0x000e0000u, 0x000d8000u, 0x000d8000u, 0x000d8000u, 0x000d0000u, 0x000d0000u, 0x000d0000u, 0x000c8000u,
    0x000c8000u, 0x000c8000u, 0x000c0000u, 0x000c0000u, 0x000c0000u, 0x000b8000u, 0x000b8000u, 0x000b8000u,
    0x000b0000u, 0x000b0000u, 0x000b0000u, 0x000a8000u, 0x000a8000u, 0x000a8000u, 0x000a8000u, 0x000a0000u,
    0x000a0000u, 0x000a0000u, 0x00098000u, 0x00098000u, 0x00098000u, 0x00090000u, 0x00090000u, 0x00090000u,
    0x00088000u, 0x00088000u, 0x00088000u, 0x00088000u, 0x00080000u, 0x00080000u, 0x00080000u, 0x00078000u,
    0x00078000u, 0x00078000u, 0x00070000u, 0x00070000u, 0x00070000u, 0x00070000u, 0x00068000u, 0x00068000u,
    0x00068000u, 0x00060000u, 0x00060000u, 0x00060000u, 0x00060000u, 0x00058000u, 0x00058000u, 0x00058000u,
    0x00050000u, 0x00050000u, 0x00050000u, 0x00050000u, 0x00048000u, 0x00048000u, 0x00048000u, 0x00040000u,
    0x00040000u, 0x00040000u, 0x00040000u, 0x00038000u, 0x00038000u, 0x00038000u, 0x00038000u, 0x00030000u,
    0x00030000u, 0x00030000u, 0x00028000u, 0x00028000u, 0x00028000u, 0x00028000u, 0x00020000u, 0x00020000u,
    0x00020000u, 0x00020000u, 0x00018000u, 0x00018000u, 0x00018000u, 0x00018000u, 0x00010000u, 0x00010000u,
    0x00010000u, 0x00010000u, 0x00008000u, 0x00008000u, 0x00008000u, 0x00008000u, 0x00000000u, 0x00000000u,
    0x007f8000u, 0x007f8000u, 0x007f0000u, 0x007e8000u, 0x007e0000u, 0x007d8000u, 0x007d0000u, 0x007c8000u,
    0x007c0000u, 0x007b8000u, 0x007b0000u, 0x007a8000u, 0x007a0000u, 0x00798000u, 0x00790000u, 0x00788000u,
    0x00780000u, 0x00780000u, 0x00778000u, 0x00770000u, 0x00768000u, 0x00760000u, 0x00758000u, 0x00750000u,
    0x00748000u, 0x00740000u, 0x00740000u, 0x00738000u, 0x00730000u, 0x00728000u, 0x00720000u, 0x00718000u,
    0x00710000u, 0x00710000u, 0x00708000u, 0x00700000u, 0x006f8000u, 0x006f0000u, 0x006f0000u, 0x006e8000u,
    0x006e0000u, 0x006d8000u, 0x006d0000u, 0x006d0000u, 0x006c8000u, 0x006c0000u, 0x006b8000u, 0x006b0000u,
    0x006b0000u, 0x006a8000u, 0x006a0000u, 0x00698000u, 0x00698000u, 0x00690000u, 0x00688000u, 0x00680000u,
    0x00680000u, 0x00678000u, 0x00670000u, 0x00668000u, 0x00668000u, 0x00660000u, 0x00658000u, 0x00650000u,
    0x00650000u, 0x00648000u, 0x00640000u, 0x00640000u, 0x00638000u, 0x00630000u, 0x00628000u, 0x00628000u,
    0x00620000u, 0x00618000u, 0x00618000u, 0x00610000u, 0x00608000u, 0x00608000u, 0x00600000u, 0x005f8000u,
    0x005f8000u, 0x005f0000u, 0x005e8000u, 0x005e8000u, 0x005e0000u, 0x005d8000u, 0x005d8000u, 0x005d0000u,
    0x005c8000u, 0x005c8000u, 0x005c0000u, 0x005c0000u, 0x005b8000u, 0x005b0000u, 0x005b0000u, 0x005a8000u,
    0x005a0000u, 0x005a0000u, 0x00598000u, 0x00598000u, 0x00590000u, 0x00588000u, 0x00588000u, 0x00580000u,
    0x00580000u, 0x00578000u, 0x00570000u, 0x00570000u, 0x00568000u, 0x00568000u, 0x00560000u, 0x00558000u,
    0x00558000u, 0x00550000u, 0x00550000u, 0x00548000u, 0x00540000u, 0x00540000u, 0x00538000u, 0x00538000u,
    0x00530000u, 0x00530000u, 0x00528000u, 0x00528000u, 0x00520000u, 0x00518000u, 0x00518000u, 0x00510000u,
    0x00510000u, 0x00508000u, 0x00508000u, 0x00500000u, 0x00500000u, 0x004f8000u, 0x004f8000u, 0x004f0000u,
    0x004f0000u, 0x004e8000u, 0x004e0000u, 0x004e0000u, 0x004d8000u, 0x004d8000u, 0x004d0000u, 0x004d0000u,
    0x004c8000u, 0x004c8000u, 0x004c0000u, 0x004c0000u, 0x004b8000u, 0x004b8000u, 0x004b0000u, 0x004b0000u,
    0x004a8000u, 0x004a8000u, 0x004a0000u, 0x004a0000u, 0x00498000u, 0x00498000u, 0x00490000u, 0x00490000u,
    0x00488000u, 0x00488000u, 0x00480000u, 0x00480000u, 0x00478000u, 0x00478000u, 0x00478000u, 0x00470000u,
    0x00470000u, 0x00468000u, 0x00468000u, 0x00460000u, 0x00460000u, 0x00458000u, 0x00458000u, 0x00450000u,
    0x00450000u, 0x00448000u, 0x00448000u, 0x00448000u, 0x00440000u, 0x00440000u, 0x00438000u, 0x00438000u,
    0x00430000u, 0x00430000u, 0x00428000u, 0x00428000u, 0x00428000u, 0x00420000u, 0x00420000u, 0x00418000u,
    0x00418000u, 0x00410000u, 0x00410000u, 0x00408000u, 0x00408000u, 0x00408000u, 0x00400000u, 0x00400000u,
    0x003f8000u, 0x003f8000u, 0x003f8000u, 0x003f0000u, 0x003f0000u, 0x003e8000u, 0x003e8000u, 0x003e0000u,
    0x003e0000u, 0x003e0000u, 0x003d8000u, 0x003d8000u, 0x003d0000u, 0x003d0000u, 0x003d0000u, 0x003c8000u,
    0x003c8000u, 0x003c0000u, 0x003c0000u, 0x003c0000u, 0x003b8000u, 0x003b8000u, 0x003b0000u, 0x003b0000u,
    0x003b0000u, 0x003a8000u, 0x003a8000u, 0x003a0000u, 0x003a0000u, 0x003a0000u, 0x00398000u, 0x00398000u,
    0x00398000u, 0x00390000u, 0x00390000u, 0x00388000u, 0x00388000u, 0x00388000u, 0x00380000u, 0x00380000u,
    0x00380000u, 0x00378000u, 0x00378000u, 0x00370000u, 0x00370000u, 0x00370000u, 0x00368000u, 0x00368000u,
    0x00368000u, 0x00360000u, 0x00360000u, 0x00360000u, 0x00358000u, 0x00358000u, 0x00350000u, 0x00350000u,
};

// 0x10008b50: radius = 1/sqrt(|v|^2) by the KernelStandard seed/LUT and two Newton steps, then v *= radius. The sum is
// SSE ((x^2 + y^2) + z^2); the Newton steps run on the x87 stack (doubles here) and the radius is rounded once.
float InvSqrtNormalise(Vector3& v)
{
    const float sum = v.x * v.x + v.y * v.y + v.z * v.z;
    const double h = static_cast<double>(sum) * 0.5;
    uint32_t bits;
    std::memcpy(&bits, &sum, sizeof bits);
    const uint32_t e = (bits >> 23) & 0xffu;
    const uint32_t y = (((0x17cu - e) << 22) & 0xff800000u) | kSqrtLut[(bits >> 15) & 0x1ffu];
    float seed;
    std::memcpy(&seed, &y, sizeof seed);
    double d = seed;
    d = (1.5 - (d * d) * h) * d;
    d = d * (1.5 - (d * d) * h);
    const float scale = static_cast<float>(d);
    v.x *= scale;
    v.y *= scale;
    v.z *= scale;
    return scale;
}

// ---- small field views for classes owned by other streams ------------------------------------------------------

struct SphereFields {
    void* vftable;  // +0x00
    Vector3 center; // +0x04
    float radius;   // +0x10
    float radiusXY; // +0x14
};
static_assert(sizeof(SphereFields) == 0x18, "Sphere");

struct LineFields {
    void* vftable;  // +0x00
    Vector3 dir;    // +0x04
    Vector3 point;  // +0x10
};
struct LineSegmentFields {
    void* vftable;  // +0x00
    Vector3 dir;    // +0x04
    Vector3 point;  // +0x10
    Vector3 end;    // +0x1c
};
static_assert(sizeof(LineSegmentFields) == 0x28, "LineSegment");

// Eigen3's layout (include/jet/Math.h): matrix +0, diagonal +0x24, subdiagonal +0x30. The members are private, but
// IncreasingSort is public; a layout view is enough to hand the original's struct to it.
struct Eigen3Layout {
    float m[9];  // +0x00 (row-major; eigenvectors afterwards)
    float diag[3];
    float subd[3];
};
static_assert(sizeof(Eigen3Layout) == 0x3c, "Eigen3");

// Inverse of a row-major 3x3 in place, from the shared helper at 0x1004bf90 (SSE, exact operand order). The outputs
// are computed from the original entries; the helper stores them only at the end.
void Invert3x3(float* m)
{
    const float m0 = m[0], m1 = m[1], m2 = m[2], m3 = m[3], m4 = m[4], m5 = m[5], m6 = m[6], m7 = m[7], m8 = m[8];
    const float c00 = m4 * m8 - m5 * m7;
    const float c01 = -0.0f - (m1 * m8 - m2 * m7);
    const float c02 = m1 * m5 - m2 * m4;
    const float det = m0 * c00 + m6 * c02 + m3 * c01;
    const float inv = 1.0f / det;
    const float c10 = -0.0f - (m3 * m8 - m6 * m5);
    const float c11 = m0 * m8 - m6 * m2;
    const float c12 = -0.0f - (m0 * m5 - m3 * m2);
    const float c20 = m3 * m7 - m6 * m4;
    const float c21 = -0.0f - (m0 * m7 - m6 * m1);
    const float c22 = m0 * m4 - m3 * m1;
    m[0] = c00 * inv;
    m[1] = c01 * inv;
    m[2] = c02 * inv;
    m[3] = c10 * inv;
    m[4] = c11 * inv;
    m[5] = c12 * inv;
    m[6] = c20 * inv;
    m[7] = c21 * inv;
    m[8] = c22 * inv;
}

}  // namespace

// 0x10004fd0: identity? No: the body is a bare `ret`, every field keeps the caller's storage. (The original shares
// this empty constructor with AxisAlignedBox and several other classes.)
OrientedBox::OrientedBox()
{
}

// 0x100491b0: contiguous Vector3 array (stride 0xc).
OrientedBox::OrientedBox(unsigned int count, const Vector3* points)
{
    Init(static_cast<int>(count), VertexItemPtr<const Vector3, VIPConstHelper>{0xc, points});
}

// 0x100491e0: strided view.
OrientedBox::OrientedBox(unsigned int count, VertexItemPtr<const Vector3, VIPConstHelper> items)
{
    Init(static_cast<int>(count), items);
}

// 0x10049210: centre and extents are plain moves; the axes are copied through the x87 stack in this order.
OrientedBox::OrientedBox(const OrientedBox& o)
{
    center = o.center;
    extents = o.extents;
    for (int i = 0; i < 3; ++i) X87CopyVec(axis[i], o.axis[i]);
}

// 0x10049280: identity axes, centre = box.GetCenter(), extents = (max - min) * 0.5 (subtract, then multiply).
OrientedBox::OrientedBox(const AxisAlignedBox& box)
{
    axis[0] = Vector3{1.0f, 0.0f, 0.0f};
    axis[1] = Vector3{0.0f, 1.0f, 0.0f};
    axis[2] = Vector3{0.0f, 0.0f, 1.0f};
    center = box.GetCenter();
    extents = Vector3{(box.max.x - box.min.x) * 0.5f, (box.max.y - box.min.y) * 0.5f,
                      (box.max.z - box.min.z) * 0.5f};
}

// 0x100036c0: every field through fld/fstp, in offset order.
OrientedBox& OrientedBox::operator=(const OrientedBox& o)
{
    X87Copy(center.x, o.center.x);
    X87Copy(center.y, o.center.y);
    X87Copy(center.z, o.center.z);
    X87Copy(axis[0].x, o.axis[0].x);
    X87Copy(axis[0].y, o.axis[0].y);
    X87Copy(axis[0].z, o.axis[0].z);
    X87Copy(axis[1].x, o.axis[1].x);
    X87Copy(axis[1].y, o.axis[1].y);
    X87Copy(axis[1].z, o.axis[1].z);
    X87Copy(axis[2].x, o.axis[2].x);
    X87Copy(axis[2].y, o.axis[2].y);
    X87Copy(axis[2].z, o.axis[2].z);
    X87Copy(extents.x, o.extents.x);
    X87Copy(extents.y, o.extents.y);
    X87Copy(extents.z, o.extents.z);
    return *this;
}

// 0x10003480 (shared with AxisAlignedBox::SetMin and Plane::SetNormal)
void __cdecl OrientedBox::SetCenter(const Vector3& c)
{
    X87CopyVec(center, c);
}

// 0x1000b210 (shared body)
const Vector3& __cdecl OrientedBox::GetCenter() const
{
    return center;
}

// 0x10003740
void __cdecl OrientedBox::SetExtents(const Vector3& e)
{
    X87CopyVec(extents, e);
}

// 0x10003730
const Vector3& __cdecl OrientedBox::GetExtents() const
{
    return extents;
}

// 0x10003760 (shared with AxisAlignedBox::GetMax)
const Vector3* __cdecl OrientedBox::GetAxis() const
{
    return axis;
}

// 0x10003770
void __cdecl OrientedBox::SetAxis(const Vector3* axes)
{
    for (int i = 0; i < 9; ++i) X87Copy(reinterpret_cast<float*>(axis)[i], reinterpret_cast<const float*>(axes)[i]);
}

// 0x100037b0
void __cdecl OrientedBox::SetAxis(const Vector3& x, const Vector3& y, const Vector3& z)
{
    X87CopyVec(axis[0], x);
    X87CopyVec(axis[1], y);
    X87CopyVec(axis[2], z);
}

// 0x10003800: identity axes and zero centre/extents (plain stores, no x87)
void __cdecl OrientedBox::Zero()
{
    center = Vector3{0.0f, 0.0f, 0.0f};
    extents = Vector3{0.0f, 0.0f, 0.0f};
    axis[0] = Vector3{1.0f, 0.0f, 0.0f};
    axis[1] = Vector3{0.0f, 1.0f, 0.0f};
    axis[2] = Vector3{0.0f, 0.0f, 1.0f};
}

// 0x10003860: all three extents exactly zero (UCOMISS/JP; NaN is not empty)
bool __cdecl OrientedBox::IsEmpty() const
{
    return extents.x == 0.0f && extents.y == 0.0f && extents.z == 0.0f;
}

// 0x100038a0: axes and centre transformed by the transposed matrix (component order (y,z,x)), translation added
// last. Pure SSE in the original; the grouping is kept.
void __cdecl OrientedBox::Transform(const Matrix3& m, const Vector3& t)
{
    const float cx = center.x, cy = center.y, cz = center.z;
    center.x = t.x + ((m.m[3] * cy + m.m[6] * cz) + m.m[0] * cx);
    center.y = t.y + ((m.m[4] * cy + m.m[7] * cz) + m.m[1] * cx);
    center.z = t.z + ((m.m[5] * cy + m.m[8] * cz) + m.m[2] * cx);
    for (int i = 0; i < 3; ++i) {
        const float x = axis[i].x, y = axis[i].y, z = axis[i].z;
        axis[i].x = (m.m[3] * y + m.m[6] * z) + m.m[0] * x;
        axis[i].y = (m.m[4] * y + m.m[7] * z) + m.m[1] * x;
        axis[i].z = (m.m[5] * y + m.m[8] * z) + m.m[2] * x;
        if (i == 2) axis[i].y = (m.m[1] * x + m.m[4] * y) + m.m[7] * z;
    }
}

// 0x1004a440: full Matrix4 (row-major; the upper 3x3 with translation at m[12..14]). Grouping per row as in the
// assembly.
void __cdecl OrientedBox::Transform(const Matrix4& m)
{
    const float cx = center.x, cy = center.y, cz = center.z;
    center.x = ((m.m[8] * cz + m.m[4] * cy) + m.m[0] * cx) + m.m[12];
    center.y = ((m.m[1] * cx + m.m[9] * cz) + m.m[5] * cy) + m.m[13];
    center.z = ((m.m[6] * cy + m.m[2] * cx) + m.m[10] * cz) + m.m[14];
    for (int i = 0; i < 3; ++i) {
        const float x = axis[i].x, y = axis[i].y, z = axis[i].z;
        if (i == 0) {
            axis[i].x = (m.m[4] * y + m.m[0] * x) + m.m[8] * z;
            axis[i].y = (m.m[1] * x + m.m[5] * y) + m.m[9] * z;
            axis[i].z = (m.m[10] * z + m.m[2] * x) + m.m[6] * y;
        } else if (i == 1) {
            axis[i].x = (m.m[4] * y + m.m[8] * z) + m.m[0] * x;
            axis[i].y = (m.m[5] * y + m.m[9] * z) + m.m[1] * x;
            axis[i].z = (m.m[2] * x + m.m[6] * y) + m.m[10] * z;
        } else {
            axis[i].x = (m.m[4] * y + m.m[8] * z) + m.m[0] * x;
            axis[i].y = (m.m[9] * z + m.m[5] * y) + m.m[1] * x;
            axis[i].z = (m.m[2] * x + m.m[10] * z) + m.m[6] * y;
        }
    }
}

// 0x10003af0: inverse transform (centre - t, then the transposed matrix; no translation for the axes).
void __cdecl OrientedBox::InverseTransform(const Matrix3& m, const Vector3& t)
{
    const float dx = center.x - t.x, dy = center.y - t.y, dz = center.z - t.z;
    center.x = (m.m[0] * dx + m.m[2] * dz) + m.m[1] * dy;
    center.y = (m.m[3] * dx + m.m[5] * dz) + m.m[4] * dy;
    center.z = (m.m[8] * dz + m.m[7] * dy) + m.m[6] * dx;
    for (int i = 0; i < 3; ++i) {
        const float x = axis[i].x, y = axis[i].y, z = axis[i].z;
        axis[i].x = (m.m[1] * y + m.m[2] * z) + m.m[0] * x;
        axis[i].y = (m.m[4] * y + m.m[5] * z) + m.m[3] * x;
        axis[i].z = (m.m[7] * y + m.m[8] * z) + m.m[6] * x;
    }
}

// 0x10003d20: in place, row by row; later rows use the already-updated earlier components and the original z.
void __thiscall OrientedBox::CalcLocalDir(const Vector3& dir, Vector3& out) const
{
    const float x0 = out.x, y0 = out.y, z0 = out.z;
    (void)dir;  // the original transforms the second argument in place and never reads the first
    const float nx = (axis[0].y * y0 + axis[0].z * z0) + axis[0].x * x0;
    const float ny = (axis[1].y * y0 + axis[1].z * z0) + axis[1].x * nx;
    const float nz = (axis[2].y * ny + axis[2].z * z0) + axis[2].x * nx;
    out = Vector3{nx, ny, nz};
}

// 0x10003db0
void __thiscall OrientedBox::CalcLocalPos(const Vector3& pos, Vector3& out) const
{
    const float dx = pos.x - center.x, dy = pos.y - center.y, dz = pos.z - center.z;
    out = Vector3{(axis[0].z * dz + axis[0].y * dy) + axis[0].x * dx,
                  (axis[1].z * dz + axis[1].y * dy) + axis[1].x * dx,
                  (axis[2].z * dz + axis[2].y * dy) + axis[2].x * dx};
}

// 0x10049780: eight corners in the AxisAlignedBox order, each centre + sx*axis0 + sy*axis1 + sz*axis2 with the
// assembly's per-coordinate ((c -/+ a) -/+ b) -/+ c grouping.
void __cdecl OrientedBox::CalcCorners(Vector3* corners) const
{
    const float ax = axis[0].x * extents.x, ay = axis[0].y * extents.x, az = axis[0].z * extents.x;
    const float bx = axis[1].x * extents.y, by = axis[1].y * extents.y, bz = axis[1].z * extents.y;
    const float cx = axis[2].x * extents.z, cy = axis[2].y * extents.z, cz = axis[2].z * extents.z;
    const float x = center.x, y = center.y, z = center.z;
    corners[0] = Vector3{((x - ax) - bx) - cx, ((y - ay) - by) - cy, ((z - az) - bz) - cz};
    corners[1] = Vector3{((x + ax) - bx) - cx, ((y + ay) - by) - cy, ((z + az) - bz) - cz};
    corners[2] = Vector3{((x + ax) + bx) - cx, ((y + ay) + by) - cy, ((z + az) + bz) - cz};
    corners[3] = Vector3{((x - ax) + bx) - cx, ((y - ay) + by) - cy, ((z - az) + bz) - cz};
    corners[4] = Vector3{((x - ax) - bx) + cx, ((y - ay) - by) + cy, ((z - az) - bz) + cz};
    corners[5] = Vector3{((x + ax) - bx) + cx, ((y + ay) - by) + cy, ((z + az) - bz) + cz};
    corners[6] = Vector3{((x + ax) + bx) + cx, ((y + ay) + by) + cy, ((z + az) + bz) + cz};
    corners[7] = Vector3{((x - ax) + bx) + cx, ((y - ay) + by) + cy, ((z - az) + bz) + cz};
}

// 0x10049730: fit this box around both boxes' corners.
void __cdecl OrientedBox::Maximize(const OrientedBox& box)
{
    Vector3 corners[16];
    CalcCorners(corners);
    box.CalcCorners(corners + 8);
    Init(16, VertexItemPtr<const Vector3, VIPConstHelper>{0xc, corners});
}

// 0x100495f0: this box's corners plus the axis-aligned box's corners.
void __cdecl OrientedBox::Maximize(const AxisAlignedBox& box)
{
    Vector3 corners[16];
    CalcCorners(corners);
    box.CalcCorners(corners + 8);
    Init(16, VertexItemPtr<const Vector3, VIPConstHelper>{0xc, corners});
}

// 0x10049f40: mean, covariance eigenvectors, cross(normalised), project. Empty input gives the identity box.
void __cdecl OrientedBox::Init(int count, VertexItemPtr<const Vector3, VIPConstHelper> items)
{
    if (count == 0) {
        Zero();
        return;
    }
    const Vector3* base = items.data;
    const int stride = static_cast<int>(items.stride);

    // mean: the first point seeds the sum (through the x87 stack), the rest are added in order
    Vector3 sum;
    X87CopyVec(sum, *base);
    for (int i = 1; i < count; ++i) {
        const Vector3& p = *reinterpret_cast<const Vector3*>(reinterpret_cast<const char*>(base) + i * stride);
        sum.x = p.x + sum.x;
        sum.y = p.y + sum.y;
        sum.z = p.z + sum.z;
    }
    const float invCount = 1.0f / static_cast<float>(count);
    center.x = sum.x * invCount;
    center.y = sum.y * invCount;
    center.z = sum.z * invCount;

    // covariance (symmetric, rows in this order); accumulated per point in float
    float cxx = 0.0f, cxy = 0.0f, cxz = 0.0f, cyy = 0.0f, czy = 0.0f, czz = 0.0f;
    for (int i = 0; i < count; ++i) {
        const Vector3& p = *reinterpret_cast<const Vector3*>(reinterpret_cast<const char*>(base) + i * stride);
        const float dx = p.x - center.x, dy = p.y - center.y, dz = p.z - center.z;
        cxx = dx * dx + cxx;
        cxy = dx * dy + cxy;
        cxz = dx * dz + cxz;
        cyy = dy * dy + cyy;
        czy = dz * dy + czy;
        czz = dz * dz + czz;
    }
    Eigen3Layout e;
    e.m[0] = cxx * invCount;
    e.m[1] = cxy * invCount;
    e.m[2] = cxz * invCount;
    e.m[3] = cxy * invCount;
    e.m[4] = cyy * invCount;
    e.m[5] = czy * invCount;
    e.m[6] = cxz * invCount;
    e.m[7] = czy * invCount;
    e.m[8] = czz * invCount;
    reinterpret_cast<Eigen3*>(&e)->IncreasingSort();
    // the original stores the eigenvector matrix transposed (columns become the axes)
    axis[0] = Vector3{e.m[0], e.m[3], e.m[6]};
    axis[1] = Vector3{e.m[1], e.m[4], e.m[7]};
    axis[2] = Vector3{e.m[2], e.m[5], e.m[8]};

    // third axis = cross(axis0, axis1), normalised by the module's own seed/LUT routine
    axis[2] = Vector3{axis[0].y * axis[1].z - axis[0].z * axis[1].y, axis[1].x * axis[0].z - axis[0].x * axis[1].z,
                      axis[0].x * axis[1].y - axis[1].x * axis[0].y};
    InvSqrtNormalise(axis[2]);

    // project every point on the three axes, keep the min/max, recentre and set the extents
    auto project = [&](const Vector3& p, int k) {
        const float dx = p.x - center.x, dy = p.y - center.y, dz = p.z - center.z;
        return (axis[k].z * dz + axis[k].y * dy) + axis[k].x * dx;
    };
    float lo[3], hi[3];
    for (int k = 0; k < 3; ++k) {
        const float v = project(*base, k);
        lo[k] = v;
        hi[k] = v;
    }
    for (int i = 1; i < count; ++i) {
        const Vector3& p = *reinterpret_cast<const Vector3*>(reinterpret_cast<const char*>(base) + i * stride);
        for (int k = 0; k < 3; ++k) {
            const float v = project(p, k);
            if (v < lo[k])
                lo[k] = v;
            else if (v > hi[k])
                hi[k] = v;
        }
    }
    const float m0 = (hi[0] + lo[0]) * 0.5f;
    const float m1 = (hi[1] + lo[1]) * 0.5f;
    const float m2 = (hi[2] + lo[2]) * 0.5f;
    const float c0 = center.x, c1 = center.y, c2 = center.z;
    center.x = c0 + ((axis[0].x * m0 + axis[1].x * m1) + axis[2].x * m2);
    center.y = c1 + ((axis[0].y * m0 + axis[1].y * m1) + axis[2].y * m2);
    center.z = c2 + ((axis[0].z * m0 + axis[1].z * m1) + axis[2].z * m2);
    extents.x = (hi[0] - lo[0]) * 0.5f;
    extents.y = (hi[1] - lo[1]) * 0.5f;
    extents.z = (hi[2] - lo[2]) * 0.5f;
}

// 0x1004b030: transform point and direction into the box's frame, then the axis-aligned slab test against
// [-extents, +extents].
bool __cdecl OrientedBox::GetMinMaxCollision(const Vector3& point, const Vector3& dir, float& tMin,
                                             float& tMax) const
{
    const float dx = point.x - center.x, dy = point.y - center.y, dz = point.z - center.z;
    const Vector3 lp{(axis[0].z * dz + axis[0].y * dy) + axis[0].x * dx,
                     (axis[1].z * dz + axis[1].y * dy) + axis[1].x * dx,
                     (axis[2].z * dz + axis[2].y * dy) + axis[2].x * dx};
    const Vector3 ld{(axis[0].y * dir.y + axis[0].z * dir.z) + axis[0].x * dir.x,
                     (axis[1].x * dir.x + axis[1].y * dir.y) + axis[1].z * dir.z,
                     (axis[2].x * dir.x + axis[2].y * dir.y) + axis[2].z * dir.z};
    const AxisAlignedBox local(Vector3{-0.0f - extents.x, -0.0f - extents.y, -0.0f - extents.z}, extents);
    return local.GetMinMaxCollision(lp, ld, tMin, tMax);
}

// 0x1004af00
bool __cdecl OrientedBox::TestCollision(const Line& line) const
{
    const LineFields* l = reinterpret_cast<const LineFields*>(&line);
    float tMin, tMax;
    return GetMinMaxCollision(l->point, l->dir, tMin, tMax);
}

// 0x1004af30
bool __cdecl OrientedBox::TestCollision(const Ray& ray) const
{
    const LineFields* l = reinterpret_cast<const LineFields*>(&ray);
    float tMin, tMax;
    if (!GetMinMaxCollision(l->point, l->dir, tMin, tMax)) return false;
    return !(0.0f > tMax);
}

// 0x1004af70
bool __cdecl OrientedBox::TestCollision(const LineSegment& segment) const
{
    const LineSegmentFields* s = reinterpret_cast<const LineSegmentFields*>(&segment);
    float tMin, tMax;
    if (!GetMinMaxCollision(s->point, s->dir, tMin, tMax)) return false;
    if (tMin < 0.0f) tMin = 0.0f;
    const float length = s->dir.z * (s->end.z - s->point.z) + s->dir.y * (s->end.y - s->point.y) +
                         s->dir.x * (s->end.x - s->point.x);
    if (length < tMax) tMax = length;
    return !(tMin > tMax);
}

// 0x1004f940 (shared): no implementation in TS12, answers false.
bool __cdecl OrientedBox::TestCollision(const Cylinder&) const
{
    return false;
}

// 0x1004f940
bool __cdecl OrientedBox::TestCollision(const Ellipsoid&) const
{
    return false;
}

// 0x1004a9a0: build the oriented box of the axis-aligned box and run the box-box test.
bool __cdecl OrientedBox::TestCollision(const AxisAlignedBox& box) const
{
    const OrientedBox other(box);
    return TestCollision(other);
}

// 0x1004a6d0: the sphere centre is taken into the box's frame with the inverse of the transposed axes matrix, then
// clamped against the extents (the (d +/- extent) comparisons keep the assembly's order).
bool __cdecl OrientedBox::TestCollision(const Sphere& sphere) const
{
    const SphereFields* s = reinterpret_cast<const SphereFields*>(&sphere);
    const Vector3* a = axis;
    float tm[9] = {a[0].x, a[1].x, a[2].x, a[0].y, a[1].y, a[2].y, a[0].z, a[1].z, a[2].z};
    Invert3x3(tm);

    auto apply = [&](const Vector3& v) {
        return Vector3{(tm[1] * v.y + tm[2] * v.z) + tm[0] * v.x, (tm[5] * v.z + tm[4] * v.y) + tm[3] * v.x,
                       (tm[8] * v.z + tm[7] * v.y) + tm[6] * v.x};
    };
    const Vector3 u = apply(s->center);  // sphere centre in the box frame
    const Vector3 v = apply(center);     // box centre in the box frame (not zero for a skewed box)

    float dist2 = 0.0f;
    const float ex = extents.x, ey = extents.y, ez = extents.z;
    const float lo[3] = {v.x - ex, v.y - ey, v.z - ez};
    const float hi[3] = {ex + v.x, ey + v.y, ez + v.z};
    const float c[3] = {u.x, u.y, u.z};
    for (int i = 0; i < 3; ++i) {
        float t = 0.0f;
        if (!(lo[i] > c[i])) {
            if (c[i] > hi[i]) t = c[i] - hi[i];
        } else {
            t = c[i] - lo[i];
        }
        dist2 += t * t;
    }
    const float r2 = s->radius * s->radius;
    return r2 >= dist2;
}

// 0x1004a9d0: separating-axis test between two oriented boxes (15 axes). R[k][j] = axis_k . other.axis_j in the
// assembly's component orders; TB[k] = axis_k . (other.centre - centre).
bool __cdecl OrientedBox::TestCollision(const OrientedBox& o) const
{
    const float eA[3] = {extents.x, extents.y, extents.z};
    const float eB[3] = {o.extents.x, o.extents.y, o.extents.z};
    const float T[3] = {o.center.x - center.x, o.center.y - center.y, o.center.z - center.z};

    float TB[3];
    for (int k = 0; k < 3; ++k)
        TB[k] = (axis[k].z * T[2] + axis[k].y * T[1]) + axis[k].x * T[0];

    float R[3][3];
    for (int k = 0; k < 3; ++k) {
        const float ax = axis[k].x, ay = axis[k].y, az = axis[k].z;
        R[k][0] = (ax * o.axis[0].x + az * o.axis[0].z) + ay * o.axis[0].y;
        R[k][1] = (az * o.axis[1].z + ay * o.axis[1].y) + ax * o.axis[1].x;
        R[k][2] = (az * o.axis[2].z + ay * o.axis[2].y) + ax * o.axis[2].x;
    }

    // The 15 axis tests run on the x87 stack in the original: doubles reproduce the 53-bit precision there, and the
    // comparisons use the same "JA only on a real greater" rule (unordered continues).
    for (int k = 0; k < 3; ++k) {
        const double lhs = fabs(R[k][2]) * eB[2] + fabs(R[k][1]) * eB[1] + fabs(R[k][0]) * eB[0] + eA[k];
        if (lhs < fabs(TB[k])) return false;
    }
    for (int k = 0; k < 3; ++k) {
        const double lhs = fabs(R[2][k]) * eA[2] + fabs(R[0][k]) * eA[0] + fabs(R[1][k]) * eA[1] + eB[k];
        const double rhs = fabs((TB[1] * R[1][k] + TB[2] * R[2][k]) + TB[0] * R[0][k]);
        if (lhs < rhs) return false;
    }

    const double a00 = fabs(R[0][0]), a01 = fabs(R[0][1]), a02 = fabs(R[0][2]);
    const double a10 = fabs(R[1][0]), a11 = fabs(R[1][1]), a12 = fabs(R[1][2]);
    const double a20 = fabs(R[2][0]), a21 = fabs(R[2][1]), a22 = fabs(R[2][2]);

    if (!(fabs(TB[2] * R[1][0] - TB[1] * R[2][0]) >
          eB[1] * a02 + eB[2] * a01 + eA[2] * a10 + eA[1] * a20)) {
        if (!(fabs(TB[2] * R[1][1] - TB[1] * R[2][1]) >
              eB[0] * a02 + eB[2] * a00 + eA[2] * a11 + eA[1] * a21)) {
            if ((!(fabs(TB[2] * R[1][2] - TB[1] * R[2][2]) >
                   eB[0] * a01 + eB[1] * a00 + eA[2] * a12 + eA[1] * a22)) &&
                (!(fabs(TB[0] * R[2][0] - TB[2] * R[0][0]) >
                   eB[2] * a11 + eB[1] * a12 + eA[0] * a20 + eA[2] * a00)) &&
                (!(fabs(TB[0] * R[2][1] - TB[2] * R[0][1]) >
                   eA[2] * a01 + eA[0] * a21 + eB[2] * a10 + eB[0] * a12))) {
                if ((eB[0] * a11 + eB[1] * a10 + eA[2] * a02 + a22 * eA[0] <
                     fabs(R[2][2] * TB[0] - TB[2] * R[0][2])) ||
                    (eB[2] * a21 + eB[1] * a22 + eA[1] * a00 + eA[0] * a10 <
                     fabs(TB[1] * R[0][0] - TB[0] * R[1][0])) ||
                    (eB[0] * a22 + eB[2] * a20 + eA[1] * a01 + eA[0] * a11 <
                     fabs(TB[1] * R[0][1] - TB[0] * R[1][1]))) {
                    return false;
                }
                if (!(fabs(TB[1] * R[0][2] - TB[0] * R[1][2]) >
                      eB[0] * a21 + eB[1] * a20 + eA[1] * a02 + eA[0] * a12))
                    return true;
            }
        }
    }
    return false;
}

}  // namespace Jet
