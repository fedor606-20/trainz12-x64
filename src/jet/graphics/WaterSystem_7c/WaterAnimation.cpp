// Unit U4 of WaterSystem_7c: the water animation plugin hierarchy. Behaviour follows the TS12 assembly
// (addresses per function); float math follows the module-wide x87 rule (double intermediates, float rounding
// where the assembly stores to a float).
#include <jet/WaterAnimation.h>

#include <jet/FrameAnimation.h>
#include <jet/KernelStandard.h>  // Alloc
#include <jet/VCall.h>
#include <jet/WaterManagerConfig.h>  // AnimProp

#include <cfloat>
#include <cstring>

namespace Jet {

// 0x10019810
const UId WaterAnimation::groupUid("2D54F63B-FF0A-4877-9561-0BBC3E2AC6DB");
// 0x10019830
const PString WaterAnimationPN::ANIM_NAME("WaterAnimationPN");

// 0x100047c0 (the size and the xy-frame pointer are set by the derived classes)
WaterAnimation::WaterAnimation() : animMaxHeight(0.0f)
{
}

WaterAnimation::~WaterAnimation() = default;

// 0x10015db0 (shared body: returns null in the original)
WaterGrid<Vector2>* WaterAnimation::GetXYFrame(unsigned int frame)
{
    return nullptr;
}

// 0x10003470
const Size& WaterAnimation::GetSize() const
{
    return size;
}

// 0x10003480
float WaterAnimation::GetAnimMaxHeight() const
{
    return animMaxHeight;
}

// 0x10013f10 (shared body with WaterManager::Init and the material plugins' IsSupported stubs)
bool WaterAnimation::Init()
{
    return true;
}

// 0x10004890: the factory registered under groupUid, its slot-3 CreateInstance called raw, then released
WaterAnimation* WaterAnimation::CreateInstance(const PString& name)
{
    PluginType* plugin = nullptr;
    PluginBase::LookupPluginType(&plugin, groupUid, name);
    WaterAnimation* instance = nullptr;
    if (plugin)
        instance = VCall<WaterAnimation*>(plugin, 0xC);
    if (plugin)
        plugin->ReleaseReference();
    return instance;
}

// 0x10004a90
WaterAnimationPN::WaterAnimationPN() : frameCount(0), frameCapacity(0), frames(nullptr)
{
}

// 0x10004b70 (the grids are freed through their destructors, as the original's inlined body does)
WaterAnimationPN::~WaterAnimationPN()
{
    for (int i = 0; i < frameCount; ++i)
        delete frames[i];
    if (frames)
        Alloc::Delete(frames);
}

// 0x10004c50
bool WaterAnimationPN::Initialize(const FrameAnimation* frameAnimation, const Size& gridSize, const AnimProp& anim)
{
    numFrames = frameAnimation->GetNumFrames();
    size = gridSize;
    animMaxHeight = anim.animMaxHeight;
    for (int i = 0; i < numFrames; ++i) {
        WaterGrid<float>* grid = new WaterGrid<float>(gridSize);
        if (frameCount + 1 > frameCapacity) {
            int newCapacity = (frameCount + 128) >> 7 << 7;  // a multiple of 128, as the original
            auto* grown = static_cast<WaterGrid<float>**>(Alloc::New(newCapacity * 4));
            if (frames) {
                std::memcpy(grown, frames, 4 * frameCount);
                Alloc::Delete(frames);
            }
            frames = grown;
            frameCapacity = newCapacity;
        }
        frames[frameCount] = grid;
        ++frameCount;
    }
    CalcAnimation();
    return true;
}

// 0x10004a80
WaterGrid<float>* WaterAnimationPN::GetFrame(unsigned int frame)
{
    return frames[frame];
}

// 0x10004dc0: one frame of tiled Perlin noise over the grid, looping in z every 25 with a period of 10
void WaterAnimationPN::SampleFrame(float frame, int index, float& min, float& max)
{
    float dx = static_cast<float>(10.0 / size.w);
    float dy = static_cast<float>(10.0 / size.h);
    int row = 0;
    float y = 0.0f;
    if (size.h > 0) {
        do {
            int col = 0;
            float x = 0.0f;
            if (size.w > 0) {
                float phase = frame * 25.0f;
                for (;;) {
                    float z = static_cast<float>(static_cast<double>(phase) / numFrames);
                    float t0 = static_cast<float>(noise.TurbulenceTiledD(x, y, z - 25.0f, 10.0f) * z);
                    double h = (noise.TurbulenceTiledD(x, y, z, 10.0f) * (25.0 - z) + t0)
                               * 0.039999999f;  // 0x3d23d70a, the original's constant (1/25)
                    float height = static_cast<float>(h);
                    if (h <= max) {
                        if (min > height)
                            min = height;
                    } else {
                        max = height;
                    }
                    frames[index]->Set(col, row, height);
                    ++col;
                    x += dx;
                    if (col >= size.w)
                        break;
                }
            }
            ++row;
            y += dy;
        } while (row < size.h);
    }
}

// 0x10004f60
void WaterAnimationPN::RescaleFrame(int index, float scale, float offset)
{
    int row = 0;
    if (size.h > 0) {
        do {
            for (int col = 0; col < size.w; ++col) {
                WaterGrid<float>* grid = frames[index];
                grid->Set(col, row, (grid->Get(col, row) + offset) * scale);
            }
            ++row;
        } while (row < size.h);
    }
}

// 0x10004fe0: sample every frame, then rescale the heights into +-animMaxHeight
void WaterAnimationPN::CalcAnimation()
{
    if (numFrames <= 0)
        return;
    float min = FLT_MAX;
    float max = -FLT_MAX;
    for (int i = 0; i < numFrames; ++i)
        SampleFrame(static_cast<float>(i), i, min, max);
    if (max > min) {
        float height2 = static_cast<float>(static_cast<double>(animMaxHeight) * 2.0);
        float scale = static_cast<float>(static_cast<double>(height2) / (max - min));
        float offset = (max + min) * -0.5f;
        for (int i = 0; i < numFrames; ++i)
            RescaleFrame(i, scale, offset);
    }
}

// 0x100160c0 (called from JetGetPluginsWaterSystem): the factory registered under groupUid
void RegisterWaterAnimationFactory()
{
    new PluginFactoryOf<WaterAnimationPN, WaterAnimation>(
        WaterAnimationPN::ANIM_NAME, UId("E43947F9-6280-4141-B8BC-8847196B62AF"), WaterAnimation::groupUid);
}

}  // namespace Jet
