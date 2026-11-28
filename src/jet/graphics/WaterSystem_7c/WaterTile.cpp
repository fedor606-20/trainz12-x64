// Unit U9 of WaterSystem_7c: WaterTile. Behaviour follows the TS12 assembly (addresses per function).
//
// The material is a PluginInstance (ReferenceCount is a virtual base), so its reference count is reached through
// the vbptr at +4; WaterGeometry derives ReferenceCount directly. The reference sequences of SetGeometry and
// SetMaterial are kept as the original has them (two AddReference calls and two ReleaseReference calls per set).
#include <jet/WaterTile.h>

#include <jet/KernelStandard.h>
#include <jet/WaterGeometry.h>

#include "WaterUnit9Shell.h"

namespace Jet {

namespace {

// WaterMaterial is a PluginInstance: ReferenceCount is a virtual base, reached through the vbptr at +4, as the
// original's AddReference/ReleaseReference sequences do.
ReferenceCount* MaterialReferenceCount(WaterMaterial* material)
{
    char* object = reinterpret_cast<char*>(material);
    int* vbtable = *reinterpret_cast<int**>(object + 4);
    return reinterpret_cast<ReferenceCount*>(object + 4 + vbtable[1]);
}

void MaterialAddReference(WaterMaterial* material)
{
    if (material)
        MaterialReferenceCount(material)->AddReference();
}

void MaterialReleaseReference(WaterMaterial* material)
{
    if (material)
        MaterialReferenceCount(material)->ReleaseReference();
}

}  // namespace

// 0x10016ec0
WaterTile::WaterTile() : position(0.0f, 0.0f, 0.0f), geometry(nullptr), material(nullptr), lights(nullptr)
{
}

// 0x10003700
WaterTile::WaterTile(const WaterTile& o)
    : Element(o), position(o.position), geometry(o.geometry), material(o.material), lights(o.lights)
{
    if (geometry)
        geometry->AddReference();
    if (material)
        MaterialAddReference(material);
}

// 0x10016ef0: the material is released before the geometry
WaterTile::~WaterTile()
{
    if (material)
        MaterialReleaseReference(material);
    if (geometry)
        geometry->ReleaseReference();
}

// 0x100037b0
WaterTile& __thiscall WaterTile::operator=(const WaterTile& o)
{
    Element::operator=(o);
    position = o.position;

    WaterGeometry* oldGeometry = geometry;
    geometry = o.geometry;
    if (geometry)
        geometry->AddReference();
    if (oldGeometry)
        oldGeometry->ReleaseReference();

    WaterMaterial* oldMaterial = material;
    material = o.material;
    if (material)
        MaterialAddReference(material);
    if (oldMaterial)
        MaterialReleaseReference(oldMaterial);

    lights = o.lights;
    return *this;
}

// 0x10017170
void __cdecl WaterTile::Render(Display* display, ViewPort& viewport, Matrix4& matrix)
{
    if (geometry)
        geometry->Render(display, viewport, lights, matrix);
}

// 0x10016fa0: clip the tile's world position, then hand the viewpoint a copy of the matrix with the tile's
// transformed position in the translation row. The four sums keep the assembly's operand and grouping order.
void __thiscall WaterTile::CullTile(ViewPoint& view, const Matrix4& matrix, const Vector3& origin)
{
    const Vector3 tilePos = position;
    const Vector3 world(origin.x + tilePos.x, origin.y + tilePos.y, origin.z + tilePos.z);

    const float radius = geometry->GetRadius();
    if (view.camera->Clip3D(world, radius) == BACK)
        return;

    using ActivateFn = void(__cdecl*)(void*, int);
    (*reinterpret_cast<ActivateFn* const*>(material))[4](material, -1);

    Matrix4 local = matrix;
    const float* m = matrix.m;
    const float tx = tilePos.x, ty = tilePos.y, tz = tilePos.z;
    local.m[12] = ((m[8] * tz + m[4] * ty) + m[0] * tx) + m[12];
    local.m[13] = ((m[9] * tz + m[5] * ty) + m[1] * tx) + m[13];
    local.m[14] = ((m[10] * tz + m[6] * ty) + m[2] * tx) + m[14];
    local.m[15] = ((m[15] + m[11]) + m[7]) + m[3];
    view.Add(this, local);
}

// 0x10016f90
void __thiscall WaterTile::SetLight(const WaterTileLightArray* value)
{
    lights = value;
}

// 0x100038a0
void __thiscall WaterTile::SetGeometry(WaterGeometry* value)
{
    if (value)
        value->AddReference();
    WaterGeometry* old = geometry;
    geometry = value;
    if (value)
        value->AddReference();
    if (old)
        old->ReleaseReference();
    if (value)
        value->ReleaseReference();
}

// 0x10003930
WaterGeometry* __thiscall WaterTile::GetGeometry()
{
    return geometry;
}

// 0x10003940
void __thiscall WaterTile::SetMaterial(WaterMaterial* value)
{
    if (value)
        MaterialAddReference(value);
    WaterMaterial* old = material;
    material = value;
    if (value)
        MaterialAddReference(value);
    if (old)
        MaterialReleaseReference(old);
    if (value)
        MaterialReleaseReference(value);
}

// 0x100039f0
WaterMaterial* __thiscall WaterTile::GetMaterial()
{
    return material;
}

// 0x100056e0
const Vector3& __thiscall WaterTile::GetPosition() const
{
    return position;
}

// 0x10017150
void __thiscall WaterTile::SetPosition(const Vector3& value)
{
    position = value;
}

}  // namespace Jet
