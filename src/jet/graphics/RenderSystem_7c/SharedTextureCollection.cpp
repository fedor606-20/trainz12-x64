#include <jet/Math.h>
#include <jet/SmallTypes.h>
#include <jet/Texture.h>
#include <jet/VCall.h>

#include <windows.h>
#include <cmath>
#include <cstdint>
#include <xmmintrin.h>

namespace Jet {

class SharedTextureCollection {
public:
    SharedTextureCollection& operator=(const SharedTextureCollection&);
};

SharedTextureCollection& SharedTextureCollection::operator=(const SharedTextureCollection&) { return *this; }

namespace {

unsigned char SignedColorComponent(float value)
{
    const __m128 scaled = _mm_mul_ss(_mm_set_ss(value), _mm_set_ss(127.0f));
    const __m128 shifted = _mm_sub_ss(scaled, _mm_set_ss(0.5f));
    float roundedInput;
    _mm_store_ss(&roundedInput, shifted);
    int rounded;
#if defined(_M_IX86)
    __asm {
        fld roundedInput
        fistp rounded
    }
#else
    rounded = static_cast<int>(std::nearbyint(roundedInput));
#endif
    return static_cast<unsigned char>(static_cast<unsigned char>(rounded) + 127);
}

}  // namespace

PackedColor* __cdecl RsGetSignedColor(PackedColor* out, const Vector3& vector)
{
    const uint32_t red = SignedColorComponent(vector.x);
    const uint32_t green = SignedColorComponent(vector.y);
    const uint32_t blue = SignedColorComponent(vector.z);
    out->argb = 0xff000000u | (red << 16) | (green << 8) | blue;
    return out;
}

__declspec(noinline) DisplayTexture* __cdecl RsGenerateDefaultTexture(void* display)
{
    const DisplayFormat format(static_cast<DFormatId>(5));
    DisplayFormat pixels(static_cast<DFormatId>(5));
    DisplayTexture* const texture = VCall<DisplayTexture*>(display, 0x98);
    if (!texture) return nullptr;
    void* const memory = Alloc::New(sizeof(Bitmap));
    Bitmap* const bitmap = memory ? ::new (memory) Bitmap(Size(1, 1), Bitmap::RGB, nullptr) : nullptr;
    bitmap->Pixel(Point{0, 0}, PackedColor{0xffffffffu});
    const RenderTextureConstruct construct;
    if (texture->Create(Size(1, 1), format, static_cast<TextureDimension>(0), 1, static_cast<TextureAutoMip>(0),
                        0, 1.0f, static_cast<TextureHint>(1), static_cast<TextureFilter>(0),
                        static_cast<TextureFilter>(0), static_cast<TextureFilter>(0), static_cast<TextureWrap>(2),
                        static_cast<TextureWrap>(2), construct))
        texture->Load(0, pixels, *bitmap, 0);
    bitmap->~Bitmap();
    Alloc::Delete(bitmap);
    return texture;
}

__declspec(noinline) DisplayTexture* __cdecl RsGenerateDefaultBumpTexture(void* display)
{
    const DisplayFormat format(static_cast<DFormatId>(5));
    DisplayFormat pixels(static_cast<DFormatId>(5));
    void* const memory = Alloc::New(sizeof(Bitmap));
    Bitmap* const bitmap = memory ? ::new (memory) Bitmap(Size(1, 1), Bitmap::RGB, nullptr) : nullptr;
    bitmap->Pixel(Point{0, 0}, PackedColor{0xff7f7fffu});
    DisplayTexture* const texture = VCall<DisplayTexture*>(display, 0x98);
    const RenderTextureConstruct construct;
    if (texture->Create(Size(1, 1), format, static_cast<TextureDimension>(0), 1, static_cast<TextureAutoMip>(0),
                        0, 1.0f, static_cast<TextureHint>(1), static_cast<TextureFilter>(0),
                        static_cast<TextureFilter>(0), static_cast<TextureFilter>(0), static_cast<TextureWrap>(2),
                        static_cast<TextureWrap>(2), construct))
        texture->Load(0, pixels, *bitmap, 0);
    bitmap->~Bitmap();
    Alloc::Delete(bitmap);
    return texture;
}

__declspec(noinline) DisplayTexture* __cdecl RsGenerateSubstituteTexture(void* display, unsigned width, unsigned height)
{
    if (!width) width = 32;
    if (!height) height = 32;
    DisplayTexture* const texture = VCall<DisplayTexture*>(display, 0x98);
    if (!texture) return nullptr;
    DisplayFormat format(static_cast<DFormatId>(2));
    const int largest = static_cast<int>(width) > static_cast<int>(height) ? width : height;
    int levels = 1;
    for (int side = largest / 2; side; side /= 2) ++levels;
    const Size size(static_cast<int>(width), static_cast<int>(height));
    const RenderTextureConstruct construct;
    if (!texture->Create(size, format, static_cast<TextureDimension>(0), levels, static_cast<TextureAutoMip>(0),
                         0, 1.0f, static_cast<TextureHint>(0), static_cast<TextureFilter>(1),
                         static_cast<TextureFilter>(1), static_cast<TextureFilter>(0), static_cast<TextureWrap>(2),
                         static_cast<TextureWrap>(2), construct)) {
        VDelete(texture);
        return nullptr;
    }
    Bitmap bitmap(size, Bitmap::ARGB, nullptr);
    unsigned char* pixel = bitmap.GetPtr();
    for (int y = 0; y < static_cast<int>(height); ++y) {
        for (int x = 0; x < static_cast<int>(width); ++x) {
            const bool alternate = ((x + y) >> 1) & 1;
            *pixel++ = alternate ? 0xff : 0x33;
            *pixel++ = alternate ? 0x44 : 0xff;
            *pixel++ = alternate ? 0x44 : 0x33;
            *pixel++ = 0xff;
        }
    }
    if (!texture->Load(0, format, bitmap, 0)) {
        VDelete(texture);
        return nullptr;
    }
    int mipWidth = static_cast<int>(width);
    int mipHeight = static_cast<int>(height);
    for (int level = 1; level < levels; ++level) {
        mipWidth >>= 1;
        mipHeight >>= 1;
        Bitmap* const cropped = bitmap.Crop(Rect{0, 0, mipWidth, mipHeight});
        const bool loaded = texture->Load(level, format, *cropped, 0);
        if (cropped) {
            cropped->~Bitmap();
            Alloc::Delete(cropped);
        }
        if (!loaded) {
            VDelete(texture);
            return nullptr;
        }
    }
    return texture;
}

__declspec(noinline) DisplayTexture* __cdecl RsGenerateD2Texture(void* display, unsigned width, unsigned height)
{
    const DisplayFormat format(static_cast<DFormatId>(5));
    DisplayFormat pixels(static_cast<DFormatId>(5));
    if (!width) width = 64;
    if (!height) height = 64;
    const Size size(static_cast<int>(width), static_cast<int>(height));
    void* const memory = Alloc::New(sizeof(Bitmap));
    Bitmap* const bitmap = memory ? ::new (memory) Bitmap(size, Bitmap::RGB, nullptr) : nullptr;
    const int halfWidth = static_cast<int>(width >> 1);
    const int halfHeight = static_cast<int>(height >> 1);
    const float factor = 255.0f;
    for (unsigned x = 0; x < width; ++x) {
        float deltaX;
#if defined(_M_IX86)
        __asm {
            fild dword ptr [x]
            fisub dword ptr [halfWidth]
            fidiv dword ptr [halfWidth]
            fstp deltaX
        }
#else
        deltaX = (static_cast<float>(x) - halfWidth) / halfWidth;
#endif
        float dxSquared;
        _mm_store_ss(&dxSquared, _mm_mul_ss(_mm_set_ss(deltaX), _mm_set_ss(deltaX)));
        for (unsigned y = 0; y < height; ++y) {
            float value;
#if defined(_M_IX86)
            __asm {
                fild dword ptr [y]
                fisub dword ptr [halfHeight]
                fidiv dword ptr [halfHeight]
                fmul st, st(0)
                fadd dword ptr [dxSquared]
                fmul dword ptr [factor]
                fstp value
            }
#else
            const float dy = (static_cast<float>(y) - halfHeight) / halfHeight;
            value = (dy * dy + dxSquared) * factor;
#endif
            const unsigned char channel = x == 0 || y == 0 || x == width - 1 || y == height - 1 || value > 255.0f
                ? 255 : static_cast<unsigned char>(static_cast<int>(value));
            const uint32_t color = 0xff000000u | (uint32_t(channel) << 16) | (uint32_t(channel) << 8) | channel;
            bitmap->Pixel(Point{static_cast<int>(x), static_cast<int>(y)}, PackedColor{color});
        }
    }
    DisplayTexture* const texture = VCall<DisplayTexture*>(display, 0x98);
    if (texture) {
        const RenderTextureConstruct construct;
        if (texture->Create(size, format, static_cast<TextureDimension>(0), 1, static_cast<TextureAutoMip>(0),
                            0, 1.0f, static_cast<TextureHint>(0), static_cast<TextureFilter>(1),
                            static_cast<TextureFilter>(1), static_cast<TextureFilter>(0), static_cast<TextureWrap>(1),
                            static_cast<TextureWrap>(1), construct) && texture->Load(0, pixels, *bitmap, 0)) {
            bitmap->~Bitmap();
            Alloc::Delete(bitmap);
            return texture;
        }
        VDelete(texture);
    }
    bitmap->~Bitmap();
    Alloc::Delete(bitmap);
    return nullptr;
}

__declspec(noinline) DisplayTexture* __cdecl RsGenerateSpotCubeTexture(void* display, unsigned width)
{
    if (!width) width = 32;
    const DisplayFormat format(static_cast<DFormatId>(5));
    DisplayFormat pixels(static_cast<DFormatId>(5));
    const Size size(static_cast<int>(width), static_cast<int>(width));
    void* const memory = Alloc::New(sizeof(Bitmap));
    Bitmap* const bitmap = memory ? ::new (memory) Bitmap(size, Bitmap::RGB, nullptr) : nullptr;
    DisplayTexture* const texture = VCall<DisplayTexture*>(display, 0x98);
    bool success = false;
    if (texture) {
        const RenderTextureConstruct construct;
        success = texture->Create(size, format, static_cast<TextureDimension>(1), 1, static_cast<TextureAutoMip>(0),
                                  6, 1.0f, static_cast<TextureHint>(0), static_cast<TextureFilter>(1),
                                  static_cast<TextureFilter>(1), static_cast<TextureFilter>(0),
                                  static_cast<TextureWrap>(1), static_cast<TextureWrap>(1), construct);
        if (success) {
            const int half = static_cast<int>(width >> 1);
            const float scale = 1.1f, factor = 255.0f;
            for (unsigned x = 0; x < width; ++x) {
                const int x1 = static_cast<int>(x + 1);
                float xSquared;
#if defined(_M_IX86)
                __asm {
                    fild dword ptr [x1]
                    fisub dword ptr [half]
                    fidiv dword ptr [half]
                    fmul dword ptr [scale]
                    fmul st, st(0)
                    fstp xSquared
                }
#else
                const float normalized = (static_cast<float>(x1) - half) / half * scale;
                xSquared = normalized * normalized;
#endif
                for (unsigned y = 0; y < width; ++y) {
                    const int y1 = static_cast<int>(y + 1);
                    float value;
#if defined(_M_IX86)
                    __asm {
                        fild dword ptr [y1]
                        fisub dword ptr [half]
                        fidiv dword ptr [half]
                        fmul dword ptr [scale]
                        fmul st, st(0)
                        fadd dword ptr [xSquared]
                        fsqrt
                        fld1
                        fsubrp st(1), st
                        fmul dword ptr [factor]
                        fstp value
                    }
#else
                    const float normalized = (static_cast<float>(y1) - half) / half * scale;
                    value = (1.0f - std::sqrt(normalized * normalized + xSquared)) * factor;
#endif
                    unsigned char channel = 0;
                    if (value > 0.0f && x && y && x != width - 1 && y != width - 1)
                        channel = value <= 255.0f ? static_cast<unsigned char>(static_cast<int>(value)) : 255;
                    const uint32_t color = 0xff000000u | (uint32_t(channel) << 16) | (uint32_t(channel) << 8) | channel;
                    bitmap->Pixel(Point{static_cast<int>(x), static_cast<int>(y)}, PackedColor{color});
                }
            }
            success = texture->Load(0, pixels, *bitmap, 0);
            if (success) {
                for (unsigned x = 0; x < width; ++x)
                    for (unsigned y = 0; y < width; ++y)
                        bitmap->Pixel(Point{static_cast<int>(x), static_cast<int>(y)}, PackedColor{0xff000000u});
                for (unsigned face = 1; face < 6 && success; ++face)
                    success = texture->Load(0, pixels, *bitmap, face);
            }
        }
        if (!success) VDelete(texture);
    }
    if (bitmap) {
        bitmap->~Bitmap();
        Alloc::Delete(bitmap);
    }
    return success ? texture : nullptr;
}

__declspec(noinline) DisplayTexture* __cdecl RsGenerateNormalCubeTexture(void* display, unsigned width)
{
    if (!width) width = 256;
    DisplayFormat pixels(static_cast<DFormatId>(1));
    const DisplayFormat format(static_cast<DFormatId>(5));
    const Size size(static_cast<int>(width), static_cast<int>(width));
    void* const memory = Alloc::New(sizeof(Bitmap));
    Bitmap* const bitmap = memory ? ::new (memory) Bitmap(size, Bitmap::ARGB, nullptr) : nullptr;
    DisplayTexture* const texture = VCall<DisplayTexture*>(display, 0x98);
    bool success = false;
    if (texture) {
        const RenderTextureConstruct construct;
        success = texture->Create(size, format, static_cast<TextureDimension>(1), 1, static_cast<TextureAutoMip>(0),
                                  6, 1.0f, static_cast<TextureHint>(0), static_cast<TextureFilter>(1),
                                  static_cast<TextureFilter>(1), static_cast<TextureFilter>(0),
                                  static_cast<TextureWrap>(1), static_cast<TextureWrap>(1), construct);
        const float half = static_cast<float>(width) * 0.5f;
        for (unsigned face = 0; face < 6 && success; ++face) {
            for (unsigned y = 0; y < width; ++y) {
                auto* row = reinterpret_cast<uint32_t*>(bitmap->GetPtr(Point{0, static_cast<int>(y)}));
                const float vertical = static_cast<float>(y) + 0.5f - half;
                for (unsigned x = 0; x < width; ++x) {
                    const float horizontal = static_cast<float>(x) + 0.5f - half;
                    float red, green, blue;
                    switch (face) {
                    case 0: red = half; green = -vertical; blue = -horizontal; break;
                    case 1: red = -half; green = -vertical; blue = horizontal; break;
                    case 2: red = horizontal; green = half; blue = vertical; break;
                    case 3: red = horizontal; green = -half; blue = -vertical; break;
                    case 4: red = horizontal; green = -vertical; blue = half; break;
                    default: red = -horizontal; green = -vertical; blue = -half; break;
                    }
                    const float squared = (horizontal * horizontal + vertical * vertical) + half * half;
                    const float inverse = StdMath::InvSqrt(squared);
                    const auto normalized = [inverse](float component) {
                        float value;
                        _mm_store_ss(&value, _mm_mul_ss(_mm_set_ss(inverse), _mm_set_ss(component)));
                        return SignedColorComponent(value);
                    };
                    row[x] = 0xff000000u | (uint32_t(normalized(red)) << 16) |
                             (uint32_t(normalized(green)) << 8) | normalized(blue);
                }
            }
            success = texture->Load(0, pixels, *bitmap, face);
        }
        if (!success) VDelete(texture);
    }
    if (bitmap) {
        bitmap->~Bitmap();
        Alloc::Delete(bitmap);
    }
    return success ? texture : nullptr;
}

namespace {

uint32_t PowerSample(unsigned index, float inverse, float power, bool specular)
{
#if defined(_M_IX86)
    // The original imports the VC80 x87 helper through this IAT slot. Its CRT is loaded via SxS.
    static FARPROC const ciPow = *reinterpret_cast<FARPROC*>(
        reinterpret_cast<unsigned char*>(GetModuleHandleA("RenderSystem_7c_orig.dll")) + 0x62470);
    const float bias = 0.75f, four = 4.0f, fullByte = 255.0f, fullWord = 65535.0f;
    unsigned short originalControl, truncateControl;
    int64_t converted;
    if (specular) {
        __asm {
            fild dword ptr [index]
            fmul dword ptr [inverse]
            fld dword ptr [power]
            call dword ptr [ciPow]
            fsub dword ptr [bias]
            fmul dword ptr [four]
            fmul dword ptr [fullByte]
            fnstcw originalControl
            mov ax, originalControl
            or ax, 0c00h
            mov truncateControl, ax
            fldcw truncateControl
            fistp qword ptr [converted]
            fldcw originalControl
        }
    } else {
        __asm {
            fild dword ptr [index]
            fmul dword ptr [inverse]
            fld dword ptr [power]
            call dword ptr [ciPow]
            fmul dword ptr [fullWord]
            fnstcw originalControl
            mov ax, originalControl
            or ax, 0c00h
            mov truncateControl, ax
            fldcw truncateControl
            fistp qword ptr [converted]
            fldcw originalControl
        }
    }
    return static_cast<uint32_t>(converted);
#else
    const double value = std::pow(static_cast<double>(index) * inverse, power);
    return static_cast<uint32_t>(static_cast<int64_t>(specular ? (value - 0.75) * 4.0 * 255.0 : value * 65535.0));
#endif
}

DisplayTexture* GeneratePower(void* display, unsigned width, float power, bool specular)
{
    if (!width) width = specular ? 512 : 1024;
    const DisplayFormat format(static_cast<DFormatId>(specular ? 13 : 14));
    DisplayFormat pixels(static_cast<DFormatId>(specular ? 13 : 14));
    const Size size(static_cast<int>(width), 1);
    void* const memory = Alloc::New(sizeof(Bitmap));
    Bitmap* bitmap;
    if (specular) {
        bitmap = memory ? ::new (memory) Bitmap() : nullptr;
        bitmap->Init(size, static_cast<int>(width), Bitmap::UNSPECIFIED, nullptr, static_cast<int>(width));
    } else {
        bitmap = memory ? ::new (memory) Bitmap(size, Bitmap::ARGB, nullptr) : nullptr;
    }
    unsigned char* const data = bitmap->GetPtr();
    const float inverse = 1.0f / static_cast<float>(width - 1);
    for (unsigned i = 0; i < width; ++i) {
        const uint32_t value = PowerSample(i, inverse, power, specular);
        if (specular)
            data[i] = static_cast<unsigned char>(value < 255 ? value : 255);
        else
            reinterpret_cast<uint16_t*>(data)[i] = static_cast<uint16_t>(value < 65535 ? value : 65535);
    }
    DisplayTexture* const texture = VCall<DisplayTexture*>(display, 0x98);
    const RenderTextureConstruct construct;
    if (texture->Create(size, format, static_cast<TextureDimension>(3), 1, static_cast<TextureAutoMip>(0),
                        0, 1.0f, static_cast<TextureHint>(specular ? 0 : 1),
                        static_cast<TextureFilter>(specular ? 1 : 0), static_cast<TextureFilter>(specular ? 1 : 0),
                        static_cast<TextureFilter>(0), static_cast<TextureWrap>(specular ? 1 : 2),
                        static_cast<TextureWrap>(specular ? 1 : 2), construct))
        texture->Load(0, pixels, *bitmap, 0);
    bitmap->~Bitmap();
    Alloc::Delete(bitmap);
    return texture;
}

}  // namespace

__declspec(noinline) DisplayTexture* __cdecl RsGeneratePowerTexture(void* display, unsigned width, float power)
{
    return GeneratePower(display, width, power, false);
}

__declspec(noinline) DisplayTexture* __cdecl RsGenerateSpecPowerTexture(void* display, unsigned width, float power)
{
    return GeneratePower(display, width, power, true);
}

static Texture** SharedTextures()
{
    static auto const textures = reinterpret_cast<Texture**>(GetProcAddress(GetModuleHandleA("RenderSystem_7c_orig.dll"),
        "?textureArray@SharedTextureCollection@Jet@@1PAV?$AutoReference@VTexture@Jet@@@2@A"));
    return textures;
}

static PString* SharedNames()
{
    static auto const names = reinterpret_cast<PString*>(GetProcAddress(GetModuleHandleA("RenderSystem_7c_orig.dll"),
        "?textureNameArray@SharedTextureCollection@Jet@@1PAVPString@2@A"));
    return names;
}

bool __cdecl RsGenerateSharedTexture(int id, const PString& name, TextureManager* manager, Texture*& result,
                                     unsigned width, unsigned height)
{
    if (id > 10 || VCall<Texture*>(manager, 0x1c, &name)) return false;
    void* const kernel = *reinterpret_cast<void**>(reinterpret_cast<char*>(manager) + 0x14);
    void* const display = *reinterpret_cast<void**>(static_cast<char*>(kernel) + 0x7c);
    DisplayTexture* generated = nullptr;
    switch (id) {
    case 0:
        generated = RsGenerateDefaultTexture(display);
        break;
    case 1:
        generated = RsGenerateDefaultBumpTexture(display);
        break;
    case 2:
        generated = RsGenerateSubstituteTexture(display, width, height);
        break;
    case 3: {
        generated = RsGenerateD2Texture(display, width, height);
        break;
    }
    case 4:
        generated = RsGenerateSpotCubeTexture(display, width);
        break;
    case 5: {
        generated = RsGenerateNormalCubeTexture(display, width);
        break;
    }
    case 6:
    case 7:
    case 8:
    case 9:
    case 10: {
        const bool specular = id >= 9;
        const float power = id == 6 ? 8.0f : (id == 7 || id == 9 ? 16.0f : 32.0f);
        generated = specular ? RsGenerateSpecPowerTexture(display, width, power)
                             : RsGeneratePowerTexture(display, width, power);
        break;
    }
    default:
        return false;
    }
    if (!generated) return false;
    Texture* const texture = new Texture(name, manager, generated, static_cast<TextureAlpha>(0));
    if (result) result->ReleaseReference();
    result = texture;
    return true;
}

void __cdecl RsClearSharedTextures()
{
    Texture** const textures = SharedTextures();
    for (int i = 0; i < 11; ++i) {
        Texture* const previous = textures[i];
        textures[i] = nullptr;
        if (previous) previous->ReleaseReference();
    }
}

bool __cdecl RsSetSharedTexture(int id, TextureManager*, Texture* texture)
{
    if (id > 10) {
        if (texture) texture->ReleaseReference();
        return false;
    }
    Texture** const textures = SharedTextures();
    Texture* const previous = textures[id];
    textures[id] = texture;
    if (texture) texture->AddReference();
    if (previous) previous->ReleaseReference();
    if (texture) texture->ReleaseReference();
    return true;
}

bool __cdecl RsGetSharedTexture(int id, TextureManager* manager, Texture*& result)
{
    if (id > 10) return false;
    Texture** const textures = SharedTextures();
    Texture* cached = textures[id];
    if (cached) {
        Texture* const previous = result;
        result = cached;
        cached->AddReference();
        if (previous) previous->ReleaseReference();
        return true;
    }
    PString* const name = SharedNames() + id;
    Texture* const found = VCall<Texture*>(manager, 0x1c, name);
    if (found) {
        found->AddReference();
        if (result) result->ReleaseReference();
        result = found;
    } else {
        if (!RsGenerateSharedTexture(id, *name, manager, result, 0, 0)) return false;
    }
    if (id <= 2) {
        cached = textures[id];
        textures[id] = result;
        if (result) result->AddReference();
        if (cached) cached->ReleaseReference();
    }
    return true;
}

}  // namespace Jet
