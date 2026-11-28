// WaterSystem_7c unit U9 (notes/WaterSystem.md): Water, a particle that owns a set of water tiles and an active
// light. Layout and behaviour follow the TS12 assembly (addresses per function in Water.cpp); the vtable is
// Particle's (0..47) plus this class's five new virtuals at 48..52:
//   48 AddTile, 49 RemoveTile, 50 SetLight, 51 GetLight, 52 GetAxisAlignedBoundingBox(Vector3&, Vector3&).
#pragma once

#include <jet/Math.h>
#include <jet/Platform.h>
#include <jet/Render3DCoreParticle.h>
#include <jet/RenderTypes.h>

namespace Jet {

class WaterTile;

class Water : public Particle {
public:
    Water();
    Water(const Water& o);
    virtual ~Water();
    Water& operator=(const Water& o);

    virtual void __cdecl Cull(ViewPoint& view, const Matrix4& matrix) override;  // slot 3

    virtual void __cdecl AddTile(WaterTile* tile);                 // slot 48
    virtual void __cdecl RemoveTile(WaterTile* tile);              // slot 49
    virtual bool __cdecl SetLight(DisplayLight* light);            // slot 50
    virtual DisplayLight* __cdecl GetLight();                      // slot 51
    virtual void __cdecl GetAxisAlignedBoundingBox(Vector3& min, Vector3& max);  // slot 52

    bool __thiscall IsEmpty();

    FastDynamicArray<WaterTile*, 13> tiles;                 // +0x12c
    FastDynamicArray<DisplayLight*, 13> lightList;          // +0x130
};

JET_LAYOUT32(sizeof(Water) == 0x134);
JET_LAYOUT32(offsetof(Water, tiles) == 0x12c && offsetof(Water, lightList) == 0x130);

}  // namespace Jet
