// Unit U6 of WaterSystem_7c: the per-frame bump textures. Behaviour follows the TS12 assembly (addresses per
// function). The texture objects carry the ORIGINAL's Texture vftable (the game calls their virtuals through
// it): the original writes `*v14 = &Jet::Texture::vftable` after the ctor, and so do we, via a linker alias
// onto the imported symbol. The RenderKernel's texture manager is reached through its raw vtable slot 38 and
// the returned DisplayTexture's slots 1 and 3 are called raw, exactly as the original does.
#include <jet/WaterTextureAnimation.h>

#include <jet/FrameAnimation.h>
#include <jet/KernelStandard.h>  // Alloc
#include <jet/PString.h>
#include <jet/RenderTypes.h>  // DisplayFormat
#include <jet/VCall.h>
#include <jet/WaterAnimation.h>

#include <cmath>
#include <new>

#include "RenderSystemShell.h"

namespace Jet {

namespace {

// the original's `*v14 = &Jet::Texture::vftable` (an import from RenderSystem_7c.dll); the linker aliases our
// reference onto the import stub so the object gets the original's vftable, not a local one
extern "C" const void* WaterSystemTextureVftable;
#pragma comment(linker, "/alternatename:_WaterSystemTextureVftable=??_7Texture@Jet@@6B@")

// *(RenderKernel + 124): the texture manager; its vtable slot 38 hands back the DisplayTexture used to make
// and fill a texture (the original's raw call chain)
void* GetDisplayTexture(RenderKernel* kernel)
{
    void* manager = *reinterpret_cast<void**>(reinterpret_cast<char*>(kernel) + 124);
    return VCall<void*>(manager, 0x98);
}

// the DisplayTexture slot 1: make a texture for `size`; false when the display refused it. `out` receives a
// reference the caller releases.
bool DisplayMakeTexture(void* display, const Size& size, const DisplayFormat& format, ReferenceCount** out)
{
    return VCall<bool>(display, 4, &size, &format, 0, 1, 0, 0, 1.0f, 1, 1, 1, 0, 2, 2, out);
}

// the DisplayTexture slot 3: fill the texture from the bitmap
void DisplaySetTextureBits(void* display, const DisplayFormat& format, const Bitmap& bitmap)
{
    VCall<void>(display, 12, 0, &format, &bitmap, 0);
}

}  // namespace

int WaterTextureAnimation::instance = 0;

// 0x10016500 (scale, maxHeight and the size are set by Initialize)
WaterTextureAnimation::WaterTextureAnimation()
{
}

// 0x10016510
WaterTextureAnimation::~WaterTextureAnimation()
{
    Destroy();
}

// 0x10003490
WaterTextureAnimation::WaterTextureAnimation(const WaterTextureAnimation& o) : textures(o.textures),
    scale(o.scale), maxHeight(o.maxHeight), size(o.size)
{
}

// 0x100034e0
WaterTextureAnimation& WaterTextureAnimation::operator=(const WaterTextureAnimation& o)
{
    textures = o.textures;
    scale = o.scale;
    maxHeight = o.maxHeight;
    size = o.size;
    return *this;
}

// 0x10016690
bool WaterTextureAnimation::Initialize(RenderKernel* kernel, FrameAnimation* frameAnimation,
                                       WaterAnimation* animation, float scale_, float bumpScale)
{
    ++instance;
    size = animation->GetSize();
    scale = scale_ / static_cast<float>(size.w);
    maxHeight = animation->GetAnimMaxHeight();
    Destroy();
    DisplayFormat format(DFORMAT_DEFAULT);
    if (frameAnimation->GetNumFrames() <= 0)
        return true;
    for (int frame = 0; frame < frameAnimation->GetNumFrames();) {
        void* display = GetDisplayTexture(kernel);
        ReferenceCount* out[4] = {nullptr, nullptr, nullptr, nullptr};
        bool made = DisplayMakeTexture(display, size, format, &out[0]);
        if (out[0])
            out[0]->ReleaseReference();
        if (!made)
            break;
        Bitmap bitmap(size, Bitmap::ARGB, nullptr);
        CalculateBumpMap(animation, frame, bitmap, bumpScale);
        DisplaySetTextureBits(display, format, bitmap);
        PString name;
        name.Setf("WaterBumpTex%d_%d", instance, frame);
        auto* texture = static_cast<Texture*>(Alloc::New(0x34));
        if (texture) {
            new (texture) Texture(name, *reinterpret_cast<TextureManager**>(reinterpret_cast<char*>(kernel) + 148),
                                  static_cast<DisplayTexture*>(display), static_cast<TextureAlpha>(0));
            *reinterpret_cast<const void**>(texture) = WaterSystemTextureVftable;  // the original's explicit write
        }
        textures.Add(texture);
        ++frame;
        if (frame >= frameAnimation->GetNumFrames())
            return true;
    }
    return false;
}

// 0x10016a10: the raw form over a caller's frame buffer
bool WaterTextureAnimation::Initialize(RenderKernel* kernel, int frameCount, Size size_, unsigned char* frameData,
                                       float scale_, float maxHeight_)
{
    size = size_;
    scale = scale_ / static_cast<float>(size.w);
    maxHeight = maxHeight_;
    Destroy();
    unsigned int stride = 4 * size.w * size.h;
    DisplayFormat format(DFORMAT_DEFAULT);
    if (frameCount <= 0)
        return true;
    unsigned char* frame = frameData;
    for (int i = 0; i < frameCount;) {
        void* display = GetDisplayTexture(kernel);
        ReferenceCount* out[4] = {nullptr, nullptr, nullptr, nullptr};
        bool made = DisplayMakeTexture(display, size, format, &out[0]);
        if (out[0])
            out[0]->ReleaseReference();
        if (!made)
            break;
        Bitmap bitmap(size, Bitmap::ARGB, frame);
        DisplaySetTextureBits(display, format, bitmap);
        PString name;
        name.Setf("BumpTex_%d", i);
        auto* texture = static_cast<Texture*>(Alloc::New(0x34));
        if (texture) {
            new (texture) Texture(name, *reinterpret_cast<TextureManager**>(reinterpret_cast<char*>(kernel) + 148),
                                  static_cast<DisplayTexture*>(display), static_cast<TextureAlpha>(0));
            *reinterpret_cast<const void**>(texture) = WaterSystemTextureVftable;
        }
        textures.Add(texture);
        frame += stride;
        ++i;
        if (i >= frameCount)
            return true;
    }
    return false;
}

// 0x10016900: the bump frames as one raw ARGB buffer (no textures made)
unsigned char* WaterTextureAnimation::CreateAnimationTexture(FrameAnimation* frameAnimation,
                                                             WaterAnimation* animation, float scale_,
                                                             float bumpScale)
{
    size = animation->GetSize();
    scale = scale_ / static_cast<float>(size.w);
    maxHeight = animation->GetAnimMaxHeight();
    Destroy();
    unsigned int stride = 4 * size.w * size.h;
    auto* result = static_cast<unsigned char*>(Alloc::New(stride * frameAnimation->GetNumFrames()));
    unsigned char* frame = result;
    for (int i = 0; i < frameAnimation->GetNumFrames(); ++i) {
        Bitmap bitmap(size, Bitmap::ARGB, frame);
        CalculateBumpMap(animation, i, bitmap, bumpScale);
        frame += stride;
    }
    return result;
}

// 0x10003520
float WaterTextureAnimation::GetMaxHeight() const
{
    return maxHeight;
}

// 0x10005690
Texture* WaterTextureAnimation::GetTexture(int index) const
{
    return textures[index];
}

// 0x100165a0: release every texture in index order and drop the chunks
void WaterTextureAnimation::Destroy()
{
    for (auto i = textures.First(); i != textures.End(); ++i)
        if (*i)
            (*i)->ReleaseReference();
    textures.RemoveAll();
}

// 0x10016c70: a normal map from the animation's height grid; the gradients wrap at the edges, the dx lags one
// sample behind (the original's behaviour), and the channels are 127.5-scaled normals with the height clamp
void WaterTextureAnimation::CalculateBumpMap(WaterAnimation* animation, int frameIndex, Bitmap& bitmap,
                                             float bumpScale)
{
    unsigned char* dst = bitmap.GetPtr();
    WaterGrid<float>* grid = VCall<WaterGrid<float>*>(animation, 4, frameIndex);  // the animation's vtable slot 1
    float* data = grid->GetData();
    float scale2 = scale * 2.0f;
    if (size.h <= 0)
        return;
    int w = size.w;
    double b127 = 127.5;
    float bs = bumpScale;
    for (int y = 0; y < size.h; ++y) {
        float* center = &data[y * w];
        const float* top = y > 0 ? &data[(y - 1) * w] : &data[(size.h - 1) * w];
        const float* bottom = y < size.h - 1 ? &data[(y + 1) * w] : data;
        float prev = *center;  // v16
        float cur = *center;   // v18
        if (w > 0) {
            float z2 = scale2 * scale2;
            for (int x = 0; x < w; ++x) {
                int right = x < w - 1 ? x + 1 : 0;
                float rv = center[right];  // v29
                float cc = cur;            // v22: the centre clamped to +-maxHeight
                if (cc <= maxHeight) {
                    if (-maxHeight > cc)
                        cc = -maxHeight;
                } else {
                    cc = maxHeight;
                }
                float dx = (bottom[x] - top[x]) * bs;  // v32
                float dy = (prev - rv) * bs;           // v33
                double n = b127 / std::sqrt(static_cast<double>(dx * dx + dy * dy + z2));
                float z = static_cast<float>(b127 + n * scale2 - 0.5);
                int r = static_cast<int>(static_cast<float>(static_cast<float>(n * dx) + 127.5f - 0.5f));
                int g = static_cast<int>(static_cast<float>(static_cast<float>(n * dy) + 127.5f - 0.5f));
                int a = static_cast<int>(static_cast<float>(
                    static_cast<float>(static_cast<double>(cc * 127.5f) / maxHeight) + 127.5f - 0.5f));
                *reinterpret_cast<unsigned int*>(dst) = static_cast<unsigned int>(static_cast<int>(z))
                                                        | (static_cast<unsigned int>(r) << 8)
                                                        | (static_cast<unsigned int>(g) << 16)
                                                        | (static_cast<unsigned int>(a) << 24);
                dst += 4;
                prev = cur;
                cur = rv;
            }
        }
    }
}

}  // namespace Jet
