// The remaining pixel conversion and scratch-buffer exports (0x1000de80..0x1000ef4b).
// Row helpers are reproduced exactly, including the 32-bit loads from three-byte pixels.

#include <jet/KernelStandard.h>
#include <jet/RenderTypes.h>

#include <cstring>

namespace Jet {
namespace {

using RowFn = void(__cdecl*)(const unsigned char*, unsigned char*, int);

unsigned int Load32(const unsigned char* p)
{
    unsigned int value;
    std::memcpy(&value, p, sizeof value);
    return value;
}

void Store16(unsigned char* p, unsigned int value)
{
    const unsigned short word = static_cast<unsigned short>(value);
    std::memcpy(p, &word, sizeof word);
}

void __cdecl SwapBGR32(const unsigned char* src, unsigned char* dst, int count)
{
    for (int i = 0; i < count; ++i, src += 4, dst += 4) {
        dst[0] = src[2];
        dst[1] = src[1];
        dst[2] = src[0];
        dst[3] = src[3];
    }
}
void __cdecl Reverse32(const unsigned char* src, unsigned char* dst, int count)
{
    for (int i = 0; i < count; ++i, src += 4, dst += 4) {
        dst[0] = src[3];
        dst[1] = src[2];
        dst[2] = src[1];
        dst[3] = src[0];
    }
}
void __cdecl SwapBGR32KeepAlpha(const unsigned char* src, unsigned char* dst, int count)
{
    for (int i = 0; i < count; ++i, src += 4, dst += 4) {
        dst[0] = src[2];
        dst[1] = src[1];
        dst[2] = src[0];
    }
}
void __cdecl BGRToRGB24(const unsigned char* src, unsigned char* dst, int count)
{
    for (int i = 0; i < count; ++i, src += 4, dst += 3) {
        dst[0] = src[2];
        dst[1] = src[1];
        dst[2] = src[0];
    }
}
void __cdecl SwapRGB24(const unsigned char* src, unsigned char* dst, int count)
{
    for (int i = 0; i < count; ++i, src += 3, dst += 3) {
        dst[0] = src[2];
        dst[1] = src[1];
        dst[2] = src[0];
    }
}
void __cdecl RGBToRGB24(const unsigned char* src, unsigned char* dst, int count)
{
    for (int i = 0; i < count; ++i, src += 4, dst += 3) {
        dst[0] = src[0];
        dst[1] = src[1];
        dst[2] = src[2];
    }
}
void __cdecl BGR24ToRGB24(const unsigned char* src, unsigned char* dst, int count)
{
    for (int i = 0; i < count; ++i, src += 3, dst += 4) {
        dst[0] = src[2];
        dst[1] = src[1];
        dst[2] = src[0];
    }
}
void __cdecl RGB24ToRGB24(const unsigned char* src, unsigned char* dst, int count)
{
    for (int i = 0; i < count; ++i, src += 3, dst += 4) {
        dst[0] = src[0];
        dst[1] = src[1];
        dst[2] = src[2];
    }
}

void __cdecl Argb8888ToArgb4444(const unsigned char* src, unsigned char* dst, int count)
{
    for (int i = 0; i < count; ++i, src += 4, dst += 2) {
        const unsigned int s = Load32(src);
        Store16(dst, ((s >> 16) & 0xf000) | ((s >> 12) & 0x0f00) | ((s >> 8) & 0x00f0) | ((s >> 4) & 0x000f));
    }
}
void __cdecl Argb8888ToRgba4444(const unsigned char* src, unsigned char* dst, int count)
{
    for (int i = 0; i < count; ++i, src += 4, dst += 2) {
        const unsigned int s = Load32(src);
        Store16(dst, ((s >> 8) & 0xf000) | ((s >> 4) & 0x0f00) | (s & 0x00f0) | ((s >> 28) & 0x000f));
    }
}
void __cdecl Abgr8888ToArgb4444(const unsigned char* src, unsigned char* dst, int count)
{
    for (int i = 0; i < count; ++i, src += 4, dst += 2) {
        const unsigned int s = Load32(src);
        Store16(dst, ((s >> 16) & 0xf000) | ((s << 4) & 0x0f00) | ((s >> 8) & 0x00f0) | ((s >> 20) & 0x000f));
    }
}
void __cdecl Abgr8888ToRgba4444(const unsigned char* src, unsigned char* dst, int count)
{
    for (int i = 0; i < count; ++i, src += 4, dst += 2) {
        const unsigned int s = Load32(src);
        Store16(dst, ((s & 0xf0) << 8) | ((s >> 4) & 0x0f00) | ((s >> 16) & 0x00f0) | ((s >> 28) & 0x000f));
    }
}
void __cdecl Rgb32ToRgb565(const unsigned char* src, unsigned char* dst, int count)
{
    for (int i = 0; i < count; ++i, src += 4, dst += 2) {
        const unsigned int s = Load32(src);
        Store16(dst, ((s >> 8) & 0xf800) | ((s >> 5) & 0x07e0) | ((s >> 3) & 0x001f));
    }
}
void __cdecl Rgb24ToRgb565(const unsigned char* src, unsigned char* dst, int count)
{
    for (int i = 0; i < count; ++i, src += 3, dst += 2) {
        const unsigned int s = Load32(src);
        Store16(dst, ((s >> 8) & 0xf800) | ((s >> 5) & 0x07e0) | ((s >> 3) & 0x001f));
    }
}
void __cdecl Bgr24ToRgb565(const unsigned char* src, unsigned char* dst, int count)
{
    for (int i = 0; i < count; ++i, src += 3, dst += 2) {
        const unsigned int s = Load32(src);
        Store16(dst, ((s & 0xf8) << 8) | ((s >> 5) & 0x07e0) | ((s >> 19) & 0x001f));
    }
}
void __cdecl L8ToRgba(const unsigned char* src, unsigned char* dst, int count)
{
    for (int i = 0; i < count; ++i, src += 1, dst += 4) {
        dst[0] = dst[1] = dst[2] = src[0];
        dst[3] = 0xff;
    }
}
void __cdecl L8ToRgb(const unsigned char* src, unsigned char* dst, int count)
{
    for (int i = 0; i < count; ++i, src += 1, dst += 3) dst[0] = dst[1] = dst[2] = src[0];
}

// The original loads the word into x87 and uses fistp: round to nearest, ties to even.
int ScaleL16(unsigned short value)
{
    volatile unsigned long wideValue = value;
    volatile float scale = 0.0038910505827516317f;
    volatile float half = 0.5f;
    int result;
    __asm {
        fild dword ptr [wideValue]
        fld dword ptr [scale]
        fmul
        fld dword ptr [half]
        fsub
        fistp dword ptr [result]
    }
    return result;
}

void __cdecl L16ToRgba(const unsigned char* src, unsigned char* dst, int count)
{
    for (int i = 0; i < count; ++i, src += 2, dst += 4) {
        unsigned short word;
        std::memcpy(&word, src, sizeof word);
        const unsigned char v = static_cast<unsigned char>(ScaleL16(word));
        dst[0] = dst[1] = dst[2] = v;
        dst[3] = 0xff;
    }
}
void __cdecl L16ToRgb(const unsigned char* src, unsigned char* dst, int count)
{
    for (int i = 0; i < count; ++i, src += 2, dst += 3) {
        unsigned short word;
        std::memcpy(&word, src, sizeof word);
        const unsigned char v = static_cast<unsigned char>(ScaleL16(word));
        dst[0] = dst[1] = dst[2] = v;
    }
}
void __cdecl L16ToL8(const unsigned char* src, unsigned char* dst, int count)
{
    for (int i = 0; i < count; ++i, src += 2, dst += 1) {
        unsigned short word;
        std::memcpy(&word, src, sizeof word);
        dst[0] = static_cast<unsigned char>(ScaleL16(word));
    }
}
void __cdecl Al88ToArgb(const unsigned char* src, unsigned char* dst, int count)
{
    for (int i = 0; i < count; ++i, src += 2, dst += 4) dst[0] = dst[1] = dst[2] = src[0], dst[3] = src[1];
}
void __cdecl A8ToArgb(const unsigned char* src, unsigned char* dst, int count)
{
    for (int i = 0; i < count; ++i, src += 1, dst += 4) dst[0] = dst[1] = dst[2] = 0, dst[3] = src[0];
}

RowFn Row(unsigned from, unsigned to)
{
    switch (from) {
    case 1:
        switch (to) {
        case 2: return SwapBGR32;
        case 7: return Argb8888ToArgb4444;
        case 21: return Reverse32;
        case 22: return Argb8888ToRgba4444;
        }
        break;
    case 2:
        switch (to) {
        case 1: return SwapBGR32;
        case 7: return Abgr8888ToArgb4444;
        case 21: return Reverse32;
        case 22: return Abgr8888ToRgba4444;
        }
        break;
    case 3:
        switch (to) {
        case 4: return SwapBGR32KeepAlpha;
        case 5: return RGBToRGB24;
        case 6: return BGRToRGB24;
        case 8: return Rgb32ToRgb565;
        }
        break;
    case 4:
        switch (to) {
        case 3: return SwapBGR32KeepAlpha;
        case 5: return BGRToRGB24;
        case 6: return RGBToRGB24;
        }
        break;
    case 5:
        switch (to) {
        case 3: return RGB24ToRGB24;
        case 4: return BGR24ToRGB24;
        case 6: return SwapRGB24;
        case 8: return Rgb24ToRgb565;
        }
        break;
    case 6:
        switch (to) {
        case 5: return SwapRGB24;
        case 8: return Bgr24ToRgb565;
        }
        break;
    case 13:
        switch (to) {
        case 3:
        case 4: return L8ToRgba;
        case 6: return L8ToRgb;
        }
        break;
    case 14:
        switch (to) {
        case 3:
        case 4: return L16ToRgba;
        case 6: return L16ToRgb;
        case 13: return L16ToL8;
        }
        break;
    case 15: return to == 1 || to == 2 ? Al88ToArgb : nullptr;
    case 16: return to == 1 || to == 2 ? A8ToArgb : nullptr;
    case 21: return to == 1 || to == 2 ? Reverse32 : nullptr;
    }
    return nullptr;
}

unsigned BytesPerPixel(const DisplayFormat& format) { return static_cast<unsigned>(format.bits) >> 3; }

void* scratchData;
int scratchSize;

void PrepareScratch(unsigned need)
{
    if (static_cast<unsigned>(need) == static_cast<unsigned>(scratchSize) || scratchSize > static_cast<int>(need))
        return;
    if (scratchData) Alloc::Delete(scratchData);
    scratchData = nullptr;
    scratchData = Alloc::New(need);
    scratchSize = static_cast<int>(need);
}

}  // namespace

bool __cdecl RsConvertRaw(const DisplayFormat& from, const DisplayFormat& to, void* src, void* dst, int srcPitch,
                          int dstPitch, int width, int height)
{
    if (!srcPitch) srcPitch = width;
    const RowFn row = Row(from.id, to.id);
    if (!row) {
        if (from.bits != 0x20 || static_cast<unsigned char>(to.bits - 0xf) >= 2) return false;
        auto* out = static_cast<unsigned char*>(dst);
        const unsigned stride = BytesPerPixel(to) * static_cast<unsigned>(dstPitch);
        for (int y = 0; y < height; ++y, out += stride) std::memset(out, 0, 2 * static_cast<unsigned>(width));
        return true;
    }

    const unsigned srcStride = BytesPerPixel(from) * static_cast<unsigned>(srcPitch);
    const unsigned dstStride = BytesPerPixel(to) * static_cast<unsigned>(dstPitch);
    auto* in = static_cast<unsigned char*>(src);
    auto* out = static_cast<unsigned char*>(dst);
    for (int y = 0; y < height; ++y, in += srcStride, out += dstStride) row(in, out, width);
    return true;
}

bool __cdecl RsConvertAlloc(const DisplayFormat& from, const DisplayFormat& to, void* src, void*& out, int srcPitch,
                            int width, int height)
{
    const unsigned need = BytesPerPixel(to) * static_cast<unsigned>(width) * static_cast<unsigned>(height);
    void* dst = Alloc::New(need);
    out = dst;
    return RsConvertRaw(from, to, src, dst, srcPitch, width, width, height);
}

bool __cdecl RsConvertScratch(const DisplayFormat& from, const DisplayFormat& to, void*& data, int& pitch, int& a,
                              int& b, int width, int height)
{
    const unsigned need = BytesPerPixel(to) * static_cast<unsigned>(width) * static_cast<unsigned>(height);
    PrepareScratch(need);
    if (!RsConvertRaw(from, to, data, scratchData, pitch, width, width, height)) return false;
    data = scratchData;
    pitch = width;
    return true;
}

void __cdecl RsRepitch(DisplayFormat& format, void*& data, int& pitch, int& a, int& b, int newPitch, int height)
{
    const unsigned bpp = BytesPerPixel(format);
    const unsigned need = bpp * static_cast<unsigned>(newPitch) * static_cast<unsigned>(height);
    PrepareScratch(need);
    const auto* source = static_cast<const unsigned char*>(data) +
                         (static_cast<unsigned>(b) * static_cast<unsigned>(pitch) + static_cast<unsigned>(a)) * bpp;
    auto* out = static_cast<unsigned char*>(scratchData);
    const unsigned rowBytes = bpp * static_cast<unsigned>(newPitch);
    for (int y = 0; y < height; ++y, source += static_cast<unsigned>(pitch) * bpp, out += rowBytes)
        std::memcpy(out, source, rowBytes);
    data = static_cast<unsigned char*>(scratchData) + need;
    pitch = newPitch;
    b = 0;
    a = 0;
}

void __cdecl RsFlushScratchBuffer()
{
    if (scratchData) Alloc::Delete(scratchData);
    scratchData = nullptr;
    scratchSize = 0;
}

}  // namespace Jet
