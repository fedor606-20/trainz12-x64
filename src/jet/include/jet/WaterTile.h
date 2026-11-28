// WaterSystem_7c unit U9 (notes/WaterSystem.md): a water tile, the drawable unit a Water collects. It derives
// RenderSystem_7c's Element (the visited/last-culled fields) and holds a position, a geometry, a material and
// the light list handed in by the owner. Layout and behaviour follow the TS12 assembly (addresses per function
// in WaterTile.cpp); the vtable order is the compiler's:
//   0 ~WaterTile, 1 SetVisited, 2 SetLastCulledFrame, 3 Cull, 4 Render, 5 RenderDebug,
//   6 CullTile, 7 SetGeometry, 8 SetMaterial, 9 SetPosition.
#pragma once

#include <jet/FastDynamicArray.h>
#include <jet/Math.h>
#include <jet/Platform.h>
#include <jet/RenderTypes.h>        // Element, Display, DisplayLight, ViewPort

namespace Jet {

class Camera;
class ViewPoint;
class WaterGeometry;
class WaterMaterial;

using WaterTileLightArray = FastDynamicArray<DisplayLight*, 13, Less<DisplayLight*, DisplayLight*>>;

class WaterTile : public Element {
public:
    WaterTile();
    WaterTile(const WaterTile& o);
    virtual ~WaterTile();
    WaterTile& operator=(const WaterTile& o);

    virtual void __cdecl Render(Display* display, ViewPort& viewport, Matrix4& matrix) override;  // slot 4
    virtual void __thiscall CullTile(ViewPoint& view, const Matrix4& matrix, const Vector3& position);  // slot 6
    virtual void __thiscall SetGeometry(WaterGeometry* geometry);                                       // slot 7
    virtual void __thiscall SetMaterial(WaterMaterial* material);                                       // slot 8
    virtual void __thiscall SetPosition(const Vector3& position);                                       // slot 9

    WaterGeometry* __thiscall GetGeometry();           // +0x18
    WaterMaterial* __thiscall GetMaterial();           // +0x1c
    const Vector3& __thiscall GetPosition() const;     // +0x0c
    void __thiscall SetLight(const WaterTileLightArray* lights);  // +0x20

    Vector3 position;                     // +0x0c
    WaterGeometry* geometry;              // +0x18
    WaterMaterial* material;              // +0x1c
    const WaterTileLightArray* lights;    // +0x20
};

JET_LAYOUT32(sizeof(WaterTile) == 0x24);
JET_LAYOUT32(offsetof(WaterTile, position) == 0x0c && offsetof(WaterTile, geometry) == 0x18);
JET_LAYOUT32(offsetof(WaterTile, material) == 0x1c && offsetof(WaterTile, lights) == 0x20);

}  // namespace Jet
