// Unit U1 of WaterSystem_7c: the frame clock. Behaviour follows the TS12 assembly (addresses per function).
#include <jet/FrameAnimation.h>

#include <cmath>

namespace Jet {

// 0x10001440
FrameAnimation::FrameAnimation(int sampleRate_, int period_) : time(0.0f), deltaFrame(0.0f)
{
    numFrames = period_ * sampleRate_;
    currentFrame = 0;
    sampleRate = sampleRate_;
    period = period_;
}

// 0x100013D0
FrameAnimation& FrameAnimation::operator=(const FrameAnimation& o)
{
    numFrames = o.numFrames;
    currentFrame = o.currentFrame;
    time = o.time;
    sampleRate = o.sampleRate;
    period = o.period;
    deltaFrame = o.deltaFrame;
    return *this;
}

// 0x10091D0 (shared empty body in the original)
FrameAnimation::~FrameAnimation()
{
}

// 0x10001420
int FrameAnimation::GetNumFrames() const
{
    return numFrames;
}

// 0x10001400
int FrameAnimation::GetCurrentFrame() const
{
    return currentFrame;
}

// 0x10001410
float FrameAnimation::GetDeltaFrame() const
{
    return deltaFrame;
}

// 0x10001430
int FrameAnimation::GetPeriod() const
{
    return period;
}

// 0x10008750 (shares its body with WaterGrid<float>::GetData)
int FrameAnimation::GetSampleRate() const
{
    return sampleRate;
}

// 0x10001470 (the original computes the position in x87 at 53-bit precision, so the product is double)
void FrameAnimation::Update(float deltaTime)
{
    time += deltaTime;
    float pos = static_cast<float>(static_cast<double>(sampleRate) * time);
    currentFrame = static_cast<int>(pos) % numFrames;
    // frac() through the double floor, as the original (the x87 result is rounded once, on the store)
    deltaFrame = static_cast<float>(static_cast<double>(pos) - std::floor(static_cast<double>(pos)));
}

}  // namespace Jet
