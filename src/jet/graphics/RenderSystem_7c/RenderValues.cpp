// Win32 adapters for value types whose public layouts do not need to be exposed here.
#include <jet/KernelStandard.h>

namespace {
void CopyFloats(float* dst, const float* src, unsigned count)
{
    // The retail x87 loads quiet signaling NaNs; an SSE or integer copy does not.
    for (unsigned i = 0; i < count; ++i) {
#if defined(_MSC_VER) && defined(_M_IX86)
        const float* source = src + i;
        float* target = dst + i;
        __asm {
            mov ecx, source
            mov edx, target
            fld dword ptr [ecx]
            fstp dword ptr [edx]
        }
#else
        dst[i] = src[i];
#endif
    }
}

template<unsigned Count>
unsigned** CopyStreams(unsigned** dst, unsigned* const* src)
{
    for (unsigned i = 0; i < Count; ++i) {
        dst[i] = nullptr;
        if (auto* stream = src[i]) {
            dst[i] = stream;
            ++*stream;
        }
    }
    return dst;
}

template<unsigned Count>
void ReleaseStreams(unsigned** streams)
{
    for (unsigned i = Count; i != 0;) {
        if (auto* stream = streams[--i]; stream && --*stream == 0) {
            Jet::Alloc::Delete(stream);
            streams[i] = nullptr;
        }
    }
}

struct VisibleElementStorage {
    float matrix[16];
    void* element;
    unsigned flags;
};
}

extern "C" void* __fastcall RsValueDefault(void* dst, void*) { return dst; }
extern "C" unsigned** __fastcall RsStream1Default(unsigned** dst, void*) { dst[0] = nullptr; return dst; }
extern "C" unsigned** __fastcall RsStream2Default(unsigned** dst, void*)
{
    dst[0] = dst[1] = dst[2] = nullptr;
    return dst;
}
extern "C" unsigned** __fastcall RsStream1Copy(unsigned** dst, void*, unsigned* const* src) { return CopyStreams<1>(dst, src); }
extern "C" unsigned** __fastcall RsStream2Copy(unsigned** dst, void*, unsigned* const* src) { return CopyStreams<3>(dst, src); }
extern "C" unsigned** __fastcall RsStream3Copy(unsigned** dst, void*, unsigned* const* src) { return CopyStreams<4>(dst, src); }
extern "C" void __fastcall RsStream1Dtor(unsigned** dst, void*) { ReleaseStreams<1>(dst); }
extern "C" void __fastcall RsStream2Dtor(unsigned** dst, void*) { ReleaseStreams<3>(dst); }
extern "C" void __fastcall RsStream3Dtor(unsigned** dst, void*) { ReleaseStreams<4>(dst); }

extern "C" float* __fastcall RsTex2Assign(float* dst, void*, const float* src) { CopyFloats(dst, src, 4); return dst; }
extern "C" float* __fastcall RsTex3Assign(float* dst, void*, const float* src) { CopyFloats(dst, src, 6); return dst; }
extern "C" unsigned* __fastcall RsVec4Assign(unsigned* dst, void*, const unsigned* src)
{
    for (unsigned i = 0; i < 4; ++i) dst[i] = src[i];
    return dst;
}

namespace Jet {
float* __cdecl RsVec4Get(const float* src, float* out) { CopyFloats(out, src, 4); return out; }
void __cdecl RsVec4Set(float* dst, float a) { dst[0] = a; }
void __cdecl RsVec4SetAll(float* dst, float a, float b, float c, float d)
{
    dst[0] = a; dst[1] = b; dst[2] = c; dst[3] = d;
}
}

extern "C" VisibleElementStorage* __fastcall RsVisibleElementWithFlags(
    VisibleElementStorage* dst, void*, const float* matrix, void* element, unsigned flags)
{
    CopyFloats(dst->matrix, matrix, 16);
    dst->element = element;
    dst->flags = flags;
    return dst;
}
extern "C" VisibleElementStorage* __fastcall RsVisibleElementCreate(
    VisibleElementStorage* dst, void*, const float* matrix, void* element)
{
    return RsVisibleElementWithFlags(dst, nullptr, matrix, element, 0);
}
extern "C" VisibleElementStorage* __fastcall RsVisibleElementAssign(
    VisibleElementStorage* dst, void*, const VisibleElementStorage* src)
{
    return RsVisibleElementWithFlags(dst, nullptr, src->matrix, src->element, src->flags);
}
