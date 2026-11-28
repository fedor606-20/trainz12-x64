// Render3DCore_7c unit U6: Geometry and TriangleGeometry (13 exports), transcribed from the TS12 assembly
// (addresses per member). Layout: vftable +0x00, the stored Matrix4 at +0x04, 0x44 bytes in all. The exported ordinary
// members are __cdecl (this is the first stack argument, ahead of a by-value return slot when there is one), the
// constructors, the destructor and the assignment __thiscall.
//
// Facts from the assembly:
//   0x10020cd0 copy constructor: the vftable first, then 0x40 bytes from +4 of the source.
//   0x10020d50 default constructor: the vftable only (the matrix is whatever the memory held).
//   0x10020d60 destructor: the vftable only (TriangleGeometry's is the same code, so the linker folds both).
//   0x10020db0 Geometry::operator= and TriangleGeometry::operator= are the same code (folded): copy +4..+0x43.
//   0x10027ac0 GetRenderMatrix: copies +4..+0x43 to the hidden return slot; the ViewPort argument is not read.
//   0x10027aa0 StoreMatrix: stores only when ViewPort::viewportNumber (+0x58) is 0.
//   0x10027ae0 InitInstance: copies +4..+0x43 into the target.
#include <jet/Render3DCoreGeometry.h>

#include <cstring>

namespace Jet {

namespace {

// The original passes the 0x40-byte matrix copies of operator=, StoreMatrix and InitInstance through one shared
// out-of-line helper (0x1000bf00): sixteen `fld dword ptr [ecx+n]; fstp dword ptr [eax+n]` pairs. The x87 load/store
// is what quiets a signalling NaN, while an SSE or memcpy copy would keep the bits, so those paths have to stay x87.
// (The copy constructor is different: 0x10020cd0 uses `REP MOVSD` and is bitwise.)
void CopyMatrix(Matrix4& to, const Matrix4& from)
{
#if defined(_M_IX86)
    const float* src = from.m;
    float* dst = to.m;
    __asm {
        mov ecx, src
        mov eax, dst
        xor edx, edx
    copyLoop:
        fld dword ptr [ecx + edx * 4]
        fstp dword ptr [eax + edx * 4]
        inc edx
        cmp edx, 16
        jne copyLoop
    }
#else
    std::memcpy(to.m, from.m, sizeof to.m);
#endif
}

}  // namespace

Geometry::Geometry() = default;

Geometry::Geometry(const Geometry& o)
{
    // 0x10020cd0: `REP MOVSD`, i.e. a bitwise copy (it does not go through the x87 helper, so an sNaN keeps its bits)
    std::memcpy(modelViewMatrix.m, o.modelViewMatrix.m, sizeof modelViewMatrix.m);
}

Geometry::~Geometry() = default;

Geometry& Geometry::operator=(const Geometry& o)
{
    CopyMatrix(modelViewMatrix, o.modelViewMatrix);
    return *this;
}

void __cdecl Geometry::StoreMatrix(const ViewPort& view, const Matrix4& matrix)
{
    // ViewPort's viewport number sits at +0x58 (U8); only the main viewport stores a render matrix.
    if (*reinterpret_cast<const int*>(reinterpret_cast<const unsigned char*>(&view) + 0x58) == 0)
        CopyMatrix(modelViewMatrix, matrix);
}

Matrix4 __cdecl Geometry::GetRenderMatrix(const ViewPort&) const
{
    return modelViewMatrix;
}

void __cdecl Geometry::InitInstance(Geometry& to) const
{
    CopyMatrix(to.modelViewMatrix, modelViewMatrix);
}

TriangleGeometry::TriangleGeometry() = default;

TriangleGeometry::TriangleGeometry(const TriangleGeometry& o) : Geometry(o) {}

TriangleGeometry::~TriangleGeometry() = default;

TriangleGeometry& TriangleGeometry::operator=(const TriangleGeometry& o)
{
    CopyMatrix(modelViewMatrix, o.modelViewMatrix);
    return *this;
}

}  // namespace Jet
