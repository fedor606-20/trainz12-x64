// WaterSystem_7c unit U4 (notes/WaterSystem.md): the water animation plugin hierarchy. WaterAnimation is the
// plugin base (its GetXYFrame is a null stub in the original); WaterAnimationPN renders the frames by sampling
// tiled Perlin noise into WaterGrid<float> grids. Layouts and behaviour follow the TS12 assembly (addresses per
// function in WaterAnimation.cpp); the vtables, vtordisps and adjustor thunks come from the compiler.
#pragma once

#include <jet/Math.h>  // Size
#include <jet/Platform.h>
#include <jet/Plugin.h>
#include <jet/PNoise.h>
#include <jet/UId.h>
#include <jet/WaterGrid.h>

namespace Jet {

class FrameAnimation;
struct AnimProp;

class WaterAnimation : public PluginInstance, protected PluginBase {
public:
    WaterAnimation();
    virtual ~WaterAnimation();

    virtual WaterGrid<Vector2>* __cdecl GetXYFrame(unsigned int frame);  // the original's stub returns null

    const Size& __cdecl GetSize() const;   // +0x0c
    float __cdecl GetAnimMaxHeight() const;  // +0x14
    bool __cdecl Init();                     // returns true (a stub in the original, 0x10013f10)
    static WaterAnimation* __cdecl CreateInstance(const PString& name);

    static const UId groupUid;  // 2D54F63B-FF0A-4877-9561-0BBC3E2AC6DB

protected:
    Size size;            // +0x0c
    float animMaxHeight;  // +0x14
};
JET_LAYOUT32(sizeof(WaterAnimation) == 0x24);

class WaterAnimationPN : public WaterAnimation {
public:
    WaterAnimationPN();
    virtual ~WaterAnimationPN();

    virtual bool __cdecl Initialize(const FrameAnimation* frameAnimation, const Size& gridSize,
                                    const AnimProp& anim);

    virtual WaterGrid<float>* __cdecl GetFrame(unsigned int frame);

    static const PString ANIM_NAME;  // "WaterAnimationPN"

protected:
    void __cdecl SampleFrame(float frame, int index, float& min, float& max);  // 0x10004dc0
    void __cdecl RescaleFrame(int index, float scale, float offset);           // 0x10004f60
    void __cdecl CalcAnimation();                                              // 0x10004fe0

private:
    int frameCount;             // +0x18
    int frameCapacity;          // +0x1c
    WaterGrid<float>** frames;  // +0x20
    int numFrames;              // +0x24
    PNoise noise;               // +0x28
};
JET_LAYOUT32(sizeof(WaterAnimationPN) == 0x3870);

// called from JetGetPluginsWaterSystem (unit U10), as the original's factory constructor is
void RegisterWaterAnimationFactory();

}  // namespace Jet
