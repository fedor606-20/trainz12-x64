// Render3DCore_7c Final15: the four OrientedBox best-fit exports.
//
// Status: the exports are local (no .def forwarder) but their bodies call the renamed original's implementations.
// The family is a bounded Hooke-Jeeves search in Euler space (the four unnamed helpers at 0x1004c710 / 0x1004c870 /
// 0x1004c190 / 0x1004cd30 plus the two unnamed minimisers at 0x1004c340 / 0x1004c510) whose 1D parabolic step is
// x87 code with a fixed evaluation budget. Reconstructing it is a separate unit: see notes/agents/final15.md.
//
// The signatures below are the original mangled ones (this on the stack, VertexItemPtr passed by value as its
// {stride, data} pair), so the exported ABI and the delegated call are both exact.
#include "Final15.h"

#include <jet/Render3DCoreMultiPass.h>

namespace Jet {
namespace Final15 {

using MakeBestFitArrayFn = void(__cdecl*)(void*, unsigned int, const Vector3*);
using MakeBestFitItemsFn = void(__cdecl*)(void*, unsigned int, VertexItemPtr<const Vector3, VIPConstHelper>);
using BoxVolumeFn = float(__cdecl*)(const float*, unsigned int, VertexItemPtr<const Vector3, VIPConstHelper>);
using MinimalBoxForAnglesFn = void(__cdecl*)(void*, int, VertexItemPtr<const Vector3, VIPConstHelper>, float*);

namespace {

template <class F>
F OriginalProc(const char* name)
{
    static F fn = reinterpret_cast<F>(MultiPass::OriginalExport(name));
    return fn;
}

}  // namespace

void __cdecl MakeBestFitArray(void* self, unsigned int count, const Vector3* points)
{
    const MakeBestFitArrayFn fn =
        OriginalProc<MakeBestFitArrayFn>("?MakeBestFit@OrientedBox@Jet@@QAAXIPBVVector3@2@@Z");
    fn(self, count, points);
}

void __cdecl MakeBestFitItems(void* self, unsigned int count, VertexItemPtr<const Vector3, VIPConstHelper> items)
{
    const MakeBestFitItemsFn fn =
        OriginalProc<MakeBestFitItemsFn>("?MakeBestFit@OrientedBox@Jet@@QAAXIV?$VertexItemPtr@$$CBVVector3@Jet@@VVIPConstHelper@2@@2@@Z");
    fn(self, count, items);
}

float __cdecl BoxVolume(const float* angles, unsigned int count,
                        VertexItemPtr<const Vector3, VIPConstHelper> items)
{
    const BoxVolumeFn fn =
        OriginalProc<BoxVolumeFn>("?Volume@OrientedBox@Jet@@KAMPBMIV?$VertexItemPtr@$$CBVVector3@Jet@@VVIPConstHelper@2@@2@@Z");
    return fn(angles, count, items);
}

void __cdecl MinimalBoxForAngles(void* self, int depth, VertexItemPtr<const Vector3, VIPConstHelper> items,
                                 float* angles)
{
    const MinimalBoxForAnglesFn fn =
        OriginalProc<MinimalBoxForAnglesFn>("?MinimalBoxForAngles@OrientedBox@Jet@@IAAXHV?$VertexItemPtr@$$CBVVector3@Jet@@VVIPConstHelper@2@@2@QAM@Z");
    fn(self, depth, items, angles);
}

}  // namespace Final15
}  // namespace Jet
