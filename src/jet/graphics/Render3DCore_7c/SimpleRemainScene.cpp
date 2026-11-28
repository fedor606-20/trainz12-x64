// Render3DCore_7c wave 6: Primitive3D, ParticleFXRenderItem, DebugRender and LightLoader.
//
// Primitive3D (0x10004a10/0x10004a20 ctors, 0x10004a30 Transform, 0x10004a50 InverseTransform): vftable only, 12
// slots. Transform/InverseTransform call slots 2/3 with (Transformation const&, Vector3 const&) where the vector is
// the transformation's translation (+0x24); the original's slot 2/3 and all other slots but 4 are the shared
// "xor eax,eax; ret" stub. Intersects (slot 4) is the shared "xor al,al; ret" (0x1004f940).
// ParticleFXRenderItem (0x100052eb0/0x100052ec0 ctors): vftable only, the same 12-slot shape.
// DebugRender (0x100021c60 ctor, 0x100021cb0 assignment, 0x100073920 Cull/setters, 0x100021c70/0x100079e0/
// 0x10004f940 getters, 0x1000224f0..0x100022520 the DebugInterface statics): an empty class; only the static guard
// and cx at 0x1000dd0ac/0x1000dd0b0 hold state.
// LightLoader (0x100021cb0 assignment, 0x100079e0 Load, 0x100028ee0 Save): empty as well. Load always answers null;
// Save opens the base PString as a read-mode file and writes the three literal strings of 0x1007f264/70/78.
#include "SimpleRemain.h"

// The two data exports the original emits for DebugRender::GetBoundingVolumeColor's function-local static
// (ordinals 772/2478). The function itself keeps its own function-local static; these are the export mirrors,
// zero-initialised like the original's .bss objects (a Color of four zeros and an untouched guard word).
alignas(4) unsigned char JetR3dDebugVolumeGuard[4];
alignas(4) unsigned char JetR3dDebugVolumeColor[16];

namespace Jet {

class DebugAccess {
protected:
    friend class DebugRender;
    static __declspec(dllimport) bool globalBoundingVolumes;       // ?globalBoundingVolumes@DebugAccess@Jet@@1_NA
    static __declspec(dllimport) unsigned long boundingVolumeMask; // ?boundingVolumeMask@DebugAccess@Jet@@1KA
};

// ---- Primitive3D ------------------------------------------------------------------------------------------------

Primitive3D::Primitive3D() = default;

Primitive3D::Primitive3D(const Primitive3D&) {}

Primitive3D& Primitive3D::operator=(const Primitive3D&)
{
    return *this;
}

void* __cdecl Primitive3D::vf0() { return nullptr; }
void* __cdecl Primitive3D::vf1() { return nullptr; }
void __cdecl Primitive3D::TransformInstance(const Transformation&, const Vector3&) {}
void __cdecl Primitive3D::InverseTransformInstance(const Transformation&, const Vector3&) {}
bool __cdecl Primitive3D::Intersects(const IndexedMeshInstance&, const Transformation&, CollisionResult&) const
{
    return false;
}
void* __cdecl Primitive3D::vf5() { return nullptr; }
void* __cdecl Primitive3D::vf6() { return nullptr; }
void* __cdecl Primitive3D::vf7() { return nullptr; }
void* __cdecl Primitive3D::vf8() { return nullptr; }
void* __cdecl Primitive3D::vf9() { return nullptr; }
void* __cdecl Primitive3D::vf10() { return nullptr; }
void* __cdecl Primitive3D::vf11() { return nullptr; }

void __cdecl Primitive3D::Transform(const Transformation& transformation)
{
    TransformInstance(transformation, transformation.translation);
}

void __cdecl Primitive3D::InverseTransform(const Transformation& transformation)
{
    InverseTransformInstance(transformation, transformation.translation);
}

// ---- ParticleFXRenderItem ---------------------------------------------------------------------------------------

ParticleFXRenderItem::ParticleFXRenderItem() = default;

ParticleFXRenderItem::ParticleFXRenderItem(const ParticleFXRenderItem&) {}

ParticleFXRenderItem& ParticleFXRenderItem::operator=(const ParticleFXRenderItem&)
{
    return *this;
}

void* __cdecl ParticleFXRenderItem::vf0() { return nullptr; }
void* __cdecl ParticleFXRenderItem::vf1() { return nullptr; }
void* __cdecl ParticleFXRenderItem::vf2() { return nullptr; }
void* __cdecl ParticleFXRenderItem::vf3() { return nullptr; }
void* __cdecl ParticleFXRenderItem::vf4() { return nullptr; }
void* __cdecl ParticleFXRenderItem::vf5() { return nullptr; }
void* __cdecl ParticleFXRenderItem::vf6() { return nullptr; }
void* __cdecl ParticleFXRenderItem::vf7() { return nullptr; }
void* __cdecl ParticleFXRenderItem::vf8() { return nullptr; }
void* __cdecl ParticleFXRenderItem::vf9() { return nullptr; }
void* __cdecl ParticleFXRenderItem::vf10() { return nullptr; }
void* __cdecl ParticleFXRenderItem::vf11() { return nullptr; }

// ---- DebugRender ------------------------------------------------------------------------------------------------

DebugRender::DebugRender(const Color&, unsigned long) {}

DebugRender& DebugRender::operator=(const DebugRender&)
{
    return *this;
}

void __cdecl DebugRender::Cull(RenderKernel*, ViewPoint&, Particle*) {}

bool __cdecl DebugRender::GetShowAllBoundingVolumes()
{
    return DebugAccess::globalBoundingVolumes;
}

void __cdecl DebugRender::SetShowAllBoundingVolumes(bool show)
{
    DebugAccess::globalBoundingVolumes = show;
}

unsigned long __cdecl DebugRender::GetShowAllBoundingTestMask()
{
    return DebugAccess::boundingVolumeMask;
}

void __cdecl DebugRender::SetBoundingVolumesTestMask(unsigned long mask)
{
    DebugAccess::boundingVolumeMask = mask;
}

bool __cdecl DebugRender::GetShowBoundingVolume() const
{
    return false;
}

void __cdecl DebugRender::SetShowBoundingVolume(bool) {}

unsigned long __cdecl DebugRender::GetBoundingVolumeMask() const
{
    return 0;
}

void __cdecl DebugRender::SetBoundingVolumeMask(unsigned long) {}

void __cdecl DebugRender::SetBoundingVolumeColor(const Color&) {}

namespace {

// A non-trivial initialiser keeps the original's function-local static guard alive.
Color ZeroColor()
{
    return Color{0.0f, 0.0f, 0.0f, 0.0f};
}

}  // namespace

const Color& __cdecl DebugRender::GetBoundingVolumeColor() const
{
    static Color cx = ZeroColor();  // ?cx@?1??GetBoundingVolumeColor@DebugRender@Jet@@QBAABVColor@3@XZ@4V43@A
    return cx;
}

// ---- LightLoader ------------------------------------------------------------------------------------------------

LightLoader& LightLoader::operator=(const LightLoader&)
{
    return *this;
}

Light* __cdecl LightLoader::Load(const PString&, const PString&)
{
    return nullptr;
}

bool __cdecl LightLoader::Save(const PString& base, const PString&, Light*)
{
    File file;
    if (file.Open(base.Text(), 0x10, 0).code != 0) return false;
    file.Write(String("*LIGHT\n{\n"));
    file.Write(String("*TYPE\t"));
    file.Write(String("\n}\n"));
    return true;
}

}  // namespace Jet
