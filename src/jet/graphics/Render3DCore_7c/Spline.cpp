// Unit U2 of Render3DCore_7c: Jet::Spline, a sorted-insertion-free FastDynamicArray of SplineSegment pointers
// (0x1006cec0..0x1006d45f). Ordinary members are __cdecl; the vftable starts with the destructor,
// GetPositionByRatio(float, Vector3&) and GetPositionByLength(float, Vector3&, unsigned).
#include <jet/Render3DCoreSplines.h>

#include <climits>

namespace Jet {

namespace {

// The original converts the ratio to an integer with x87 FISTP under the truncate rounding mode and keeps the low
// 32 bits; NaN and out-of-range values become the indefinite 0x8000000000000000, whose low dword is 0.
unsigned int RatioIndex(float ratio)
{
    if (!(ratio >= -9223372036854775808.0 && ratio < 9223372036854775808.0))
        return 0u;
    const long long v = static_cast<long long>(ratio);
    return static_cast<unsigned int>(v);
}

// The original evaluates the cubic with x87 (0x1006d1d8) after converting the index with FILD/FSUB. Inline x87
// keeps the NaN operand selection and the extended-precision intermediates identical; a double fallback is used
// only when this unit is built for another architecture.
static void EvalCubicX87(const SplineSegment& s, unsigned int index, float ratio, Vector3& out)
{
#if defined(_M_IX86)
    const int idxValue = static_cast<int>(index);
    const float two32 = 4294967296.0f;
    const float c0x = s.c0.x, c1x = s.c1.x, c2x = s.c2.x, c3x = s.c3.x;
    const float c0y = s.c0.y, c1y = s.c1.y, c2y = s.c2.y, c3y = s.c3.y;
    const float c0z = s.c0.z, c1z = s.c1.z, c2z = s.c2.z, c3z = s.c3.z;
    float ox, oy, oz;
    __asm {
        fild dword ptr [idxValue]
        mov eax, idxValue
        test eax, eax
        jge short t_ready
        fadd dword ptr [two32]
    t_ready:
        fsubr dword ptr [ratio]
        fld dword ptr [c0x]
        fmul st, st(1)
        fadd dword ptr [c1x]
        fmul st, st(1)
        fadd dword ptr [c2x]
        fmul st, st(1)
        fadd dword ptr [c3x]
        fstp dword ptr [ox]
        fld dword ptr [c0y]
        fmul st, st(1)
        fadd dword ptr [c1y]
        fmul st, st(1)
        fadd dword ptr [c2y]
        fmul st, st(1)
        fadd dword ptr [c3y]
        fstp dword ptr [oy]
        fld dword ptr [c0z]
        fmul st, st(1)
        fadd dword ptr [c1z]
        fmul st, st(1)
        fadd dword ptr [c2z]
        fmul st, st(1)
        fadd dword ptr [c3z]
        fstp dword ptr [oz]
        fstp st(0)
    }
    out.x = ox;
    out.y = oy;
    out.z = oz;
#else
    const double t = static_cast<double>(ratio) - static_cast<double>(static_cast<unsigned int>(index));
    out.x = static_cast<float>(((static_cast<double>(s.c0.x) * t + s.c1.x) * t + s.c2.x) * t + s.c3.x);
    out.y = static_cast<float>(((static_cast<double>(s.c0.y) * t + s.c1.y) * t + s.c2.y) * t + s.c3.y);
    out.z = static_cast<float>(((static_cast<double>(s.c0.z) * t + s.c1.z) * t + s.c2.z) * t + s.c3.z);
#endif
}

// The end point of a segment is the sum of its four coefficient vectors; the original adds c1 + c0 + c2 + c3.
Vector3 SegmentEnd(const SplineSegment& s)
{
    using namespace SplinesDetail;
    Vector3 out;
    out.x = Add(Add(Add(s.c1.x, s.c0.x), s.c2.x), s.c3.x);
    out.y = Add(Add(Add(s.c1.y, s.c0.y), s.c2.y), s.c3.y);
    out.z = Add(Add(Add(s.c1.z, s.c0.z), s.c2.z), s.c3.z);
    return out;
}

}  // namespace

// 0x1006cec0
Spline::Spline()
{
}

// 0x1000f900: copy the chunk layout itself (the segment pointers are shared, not cloned).
Spline::Spline(const Spline& o)
{
    segments.AppendChunks(o.segments);
}

// 0x1000f930: free our chunks, then copy the source's chunk layout.
Spline& Spline::operator=(const Spline& o)
{
    segments.RemoveAll();
    segments.AppendChunks(o.segments);
    return *this;
}

// 0x1006ced0: delete every segment, then every chunk.
Spline::~Spline()
{
    RemoveDeleteAll();
}

// 0x1006cf70: append when the pointer is not already present.
void __cdecl Spline::Add(SplineSegment* segment)
{
    segments.AddUnique(segment);
}

// 0x1006cfa0
bool __cdecl Spline::Remove(const SplineSegment* segment)
{
    SplineSegmentArray::Iterator it = segments.Find(const_cast<SplineSegment*>(segment));
    if (!it.node)
        return false;
    segments.Remove(it);
    return true;
}

// 0x1006cfe0: remove, then delete through the virtual deleting destructor.
bool __cdecl Spline::RemoveDelete(SplineSegment* segment)
{
    SplineSegmentArray::Iterator it = segments.Find(segment);
    if (!it.node)
        return false;
    SplineSegment* found = *it;
    segments.Remove(it);
    delete found;
    return true;
}

// 0x1006d030: delete every segment (null entries are skipped by delete), then free the chunks.
void __cdecl Spline::RemoveDeleteAll()
{
    for (SplineSegmentArray::Iterator it = segments.First(); it != segments.End(); ++it) {
        SplineSegment* segment = *it;
        if (segment != nullptr)
            delete segment;
    }
    segments.RemoveAll();
}

// 0x1006d0c0: ratio is a segment index plus local t. Out of range it returns the last segment evaluated at t=1.
void __cdecl Spline::GetPositionByRatio(float ratio, Vector3& out) const
{
    const unsigned int total = static_cast<unsigned int>(segments.Count());
    const unsigned int index = RatioIndex(ratio);
    if (index < total) {
        const SplineSegment* segment = *segments.Index(index);
        EvalCubicX87(*segment, index, ratio, out);
        return;
    }
    out = SegmentEnd(**segments.Index(total - 1));
}

// 0x1006f9b0
Vector3 __cdecl Spline::GetPositionByRatio(float ratio) const
{
    Vector3 out;
    GetPositionByRatio(ratio, out);
    return out;
}

// 0x1006d240: walk the accumulated length of each segment; the last segment's end is the fallback.
void __cdecl Spline::GetPositionByLength(float length, Vector3& out, unsigned int steps) const
{
    float total = 0.0f;
    for (SplineSegmentArray::ConstIterator it = segments.First(); it != segments.End(); ++it) {
        const SplineSegment* segment = *it;
        const float len = segment->GetLength(0.0f, 1.0f, steps);
        const float reached = SplinesDetail::Add(len, total);
        if (length < reached) {
            segment->GetPositionByLength(SplinesDetail::Sub(length, total), out, 0.0f, 1.0f, steps);
            return;
        }
        total = reached;
    }
    out = SegmentEnd(**segments.Index(static_cast<unsigned int>(segments.Count()) - 1));
}

// 0x1006f9e0
Vector3 __cdecl Spline::GetPositionByLength(float length, unsigned int steps) const
{
    Vector3 out;
    GetPositionByLength(length, out, steps);
    return out;
}

// 0x1006d3e0
float __cdecl Spline::GetLength(unsigned int steps) const
{
    float total = 0.0f;
    for (SplineSegmentArray::ConstIterator it = segments.First(); it != segments.End(); ++it)
        total = SplinesDetail::Add((*it)->GetLength(0.0f, 1.0f, steps), total);
    return total;
}

// 0x1000f8f0
const SplineSegmentArray& __cdecl Spline::GetSplineSegmentArray() const
{
    return segments;
}

}  // namespace Jet
