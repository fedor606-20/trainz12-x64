// WaterSystem_7c unit U6 (notes/WaterSystem.md): the per-frame bump textures of a water animation, built by
// sampling the animation's height grids into normal maps. Layout and behaviour follow the TS12 assembly
// (addresses per function in WaterTextureAnimation.cpp).
#pragma once

#include <jet/Bitmap.h>
#include <jet/FastDynamicArray.h>
#include <jet/Platform.h>

namespace Jet {

class FrameAnimation;
class RenderKernel;
class Texture;
class WaterAnimation;

class WaterTextureAnimation {
public:
    WaterTextureAnimation();
    WaterTextureAnimation(const WaterTextureAnimation& o);
    ~WaterTextureAnimation();
    WaterTextureAnimation& operator=(const WaterTextureAnimation& o);

    // vtable slots 0..2, in declaration order as in the original
    virtual bool __thiscall Initialize(RenderKernel* kernel, int frameCount, Size size, unsigned char* frameData,
                                    float scale, float maxHeight);
    virtual bool __thiscall Initialize(RenderKernel* kernel, FrameAnimation* frameAnimation,
                                    WaterAnimation* animation, float scale, float bumpScale);
    virtual unsigned char* __thiscall CreateAnimationTexture(FrameAnimation* frameAnimation,
                                                          WaterAnimation* animation, float scale, float bumpScale);

    float __thiscall GetMaxHeight() const;   // +0x0c
    Texture* __thiscall GetTexture(int index) const;

protected:
    static int instance;  // counts the initialized animations; the texture names use it
    virtual void __thiscall Destroy();  // slot 3
    void __thiscall CalculateBumpMap(WaterAnimation* animation, int frameIndex, Bitmap& bitmap, float bumpScale);

private:
    FastDynamicArray<Texture*, 13> textures;  // +0x04
    float scale;                              // +0x08
    float maxHeight;                          // +0x0c
    Size size;                                // +0x10
};
JET_LAYOUT32(sizeof(WaterTextureAnimation) == 0x18);

}  // namespace Jet
