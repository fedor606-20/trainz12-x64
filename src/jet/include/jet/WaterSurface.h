// WaterSystem_7c unit U9 (notes/WaterSystem.md): the water surface height grid. The class extends
// WaterGrid<float> with a scale and a normals hook; the normals are never filled by this module.
// Layout and behaviour follow the TS12 assembly (addresses per function in WaterSurface.cpp).
#pragma once

#include <jet/Math.h>
#include <jet/Platform.h>
#include <jet/WaterGrid.h>

namespace Jet {

class WaterSurface : public WaterGrid<float> {
public:
    WaterSurface();
    WaterSurface(const Size& size, const Vector2& scale);
    WaterSurface(const WaterSurface& o);
    virtual ~WaterSurface();
    WaterSurface& operator=(const WaterSurface& o);

    // WaterGrid<float> has a virtual Initialize(const Size&) in slot 0; this overload does not override it and
    // gets its own slot after the (appended) destructor, as the original's table does.
    virtual void __thiscall Initialize(const Size& size, const Vector2& scale);  // slot 2
    virtual void __thiscall GetNormal(int x, int y, Vector3& normal);            // slot 3, a no-op

    const Vector2& __thiscall GetScale();  // +0x10

    Vector2 scale;  // +0x10 (the constructor ignores its scale argument, as the original does)
};

JET_LAYOUT32(sizeof(WaterSurface) == 0x18);

}  // namespace Jet
