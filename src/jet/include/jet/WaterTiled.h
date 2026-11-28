// WaterSystem_7c unit U9 (notes/WaterSystem.md): WaterTiled, a particle that draws an animated grid of water
// quads anchored to the camera. The original derives from Particle directly (not from Water): the layout is the
// Particle's 0x12c bytes, the single WaterTile at +0x12c, then the grid Size at +0x130; its vtable has Particle's
// 48 slots with Cull and Render overridden.
#pragma once

#include <jet/Math.h>
#include <jet/Platform.h>
#include <jet/Render3DCoreParticle.h>
#include <jet/RenderTypes.h>

namespace Jet {

class WaterGeometry;
class WaterMaterial;
class WaterTile;

class WaterTiled : public Particle {
public:
    WaterTiled(const WaterTiled& o);
    WaterTiled(WaterMaterial* material, WaterGeometry* geometry, const Size& size);
    virtual ~WaterTiled();
    WaterTiled& operator=(const WaterTiled& o);

    virtual void __cdecl Cull(ViewPoint& view, const Matrix4& matrix) override;              // slot 3
    virtual void __cdecl Render(Display* display, ViewPort& viewport, Matrix4& matrix) override;  // slot 4

    WaterTile* __cdecl GetTile();  // +0x12c

    WaterTile* tile;  // +0x12c
    Size size;        // +0x130
};

JET_LAYOUT32(sizeof(WaterTiled) == 0x138);
JET_LAYOUT32(offsetof(WaterTiled, tile) == 0x12c && offsetof(WaterTiled, size) == 0x130);

}  // namespace Jet
