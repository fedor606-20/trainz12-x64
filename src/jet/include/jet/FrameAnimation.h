// WaterSystem_7c unit U1 (notes/WaterSystem.md): frame clock for water animation. Layout and behaviour follow the
// TS12 assembly (addresses per member in FrameAnimation.cpp).
#pragma once

#include <jet/Platform.h>

namespace Jet {

class FrameAnimation {
public:
    FrameAnimation(int sampleRate, int period);
    FrameAnimation& operator=(const FrameAnimation& o);
    ~FrameAnimation();  // a no-op in the original (shared empty body)

    int __cdecl GetNumFrames() const;
    int __cdecl GetCurrentFrame() const;
    float __cdecl GetDeltaFrame() const;  // fractional position inside the current frame, 0..1
    int __cdecl GetPeriod() const;
    int __cdecl GetSampleRate() const;
    void __cdecl Update(float deltaTime);

private:
    int numFrames;
    int currentFrame;
    float time;
    int sampleRate;
    int period;
    float deltaFrame;
};

JET_LAYOUT32(sizeof(FrameAnimation) == 0x18);

}  // namespace Jet
