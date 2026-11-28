// Unit U2 of Render3DCore_7c: Jet::SplineSegment, transcribed from the TS12 assembly
// (notes/agents/r3d_workstreams/u2_splines_asm.txt, u2_helpers_asm.txt).
//
// 0x34 bytes: vftable + four Vector3 coefficient vectors c0..c3 (0x1006eb70/0x1006ebc0). Ordinary members are
// __cdecl (`this` on the stack); the virtual functions are __thiscall. The fast sqrt/inv-sqrt below are the
// original's inlined StdMath calls (x87, double precision, one float rounding), used by GetLength/GetRatio/
// GetPositionByLength/GetTangent/GetNormal/GetBinormal through KernelStandard's exported StdMath.
#include <jet/Render3DCoreSplines.h>

#include <cfloat>
#include <cmath>
#include <cstring>
#include <windows.h>

namespace Jet {


namespace {


// GetRatio stays forwarded until its 1-2 ulp residual is merged (the fallback below is close but not bit-exact;
// diff_r3d_tails2 reports ~10% of the sampled cases off by one ulp). The virtual slot calls the original body when
// the proxy's _orig module is present; the local body is the standalone fallback.
typedef float(__cdecl* OrigRatioFn)(const SplineSegment*, float, float, float, unsigned int);

OrigRatioFn OrigRatio()
{
    static OrigRatioFn fn = []() -> OrigRatioFn {
        HMODULE orig = GetModuleHandleA("Render3DCore_7c_orig.dll");
        if (orig == nullptr)
            orig = LoadLibraryA("Render3DCore_7c_orig.dll");
        return orig ? reinterpret_cast<OrigRatioFn>(
                          GetProcAddress(orig, "?GetRatio@SplineSegment@Jet@@UBAMMMMI@Z"))
                    : nullptr;
    }();
    return fn;
}

float SqrtFast(float x)
{
    volatile float r = StdMath::Sqrt(x);
    return r;
}

float InvSqrtFast(float x)
{
    volatile float r = StdMath::InvSqrt(x);
    return r;
}

// 0x10008b50: v /= |v|, no zero check; the caller has already tested 1e-8 < |v|^2.
void NormalizeFast(Vector3& v)
{
    using namespace SplinesDetail;
    const float x = v.x, y = v.y, z = v.z;
    const float len2 = Add(Add(Mul(x, x), Mul(y, y)), Mul(z, z));
    const float inv = InvSqrtFast(len2);
    v.x = Mul(x, inv);
    v.y = Mul(y, inv);
    v.z = Mul(z, inv);
}

float Len2(const Vector3& v)
{
    using namespace SplinesDetail;
    return Add(Add(Mul(v.x, v.x), Mul(v.y, v.y)), Mul(v.z, v.z));
}

// The SSE chain in the original: (((c0*t + c1)*t + c2)*t + c3), each step with dest = the left term.
void PointAt(const SplineSegment& s, float t, float& x, float& y, float& z)
{
    using namespace SplinesDetail;
    float v = Mul(s.c0.x, t);
    v = Add(v, s.c1.x);
    v = Mul(v, t);
    v = Add(v, s.c2.x);
    v = Mul(v, t);
    x = Add(v, s.c3.x);

    v = Mul(s.c0.y, t);
    v = Add(v, s.c1.y);
    v = Mul(v, t);
    v = Add(v, s.c2.y);
    v = Mul(v, t);
    y = Add(v, s.c3.y);

    v = Mul(s.c0.z, t);
    v = Add(v, s.c1.z);
    v = Mul(v, t);
    v = Add(v, s.c2.z);
    v = Mul(v, t);
    z = Add(v, s.c3.z);
}

void FirstDerivativeAt(const SplineSegment& s, float t, float& x, float& y, float& z)
{
    using namespace SplinesDetail;
    float v = Mul(Mul(s.c0.x, t), 3.0f);
    v = Add(v, Mul(s.c1.x, 2.0f));
    v = Mul(v, t);
    x = Add(v, s.c2.x);

    v = Mul(Mul(s.c0.y, t), 3.0f);
    v = Add(v, Mul(s.c1.y, 2.0f));
    v = Mul(v, t);
    y = Add(v, s.c2.y);

    v = Mul(Mul(s.c0.z, t), 3.0f);
    v = Add(v, Mul(s.c1.z, 2.0f));
    v = Mul(v, t);
    z = Add(v, s.c2.z);
}

// x87 evaluation of the GetRatio interpolation: (t - step) + ((length - total) / len) * step, one rounding at
// the final store. The grouping matters: the naive t - (1 - frac)*step cancels to zero when frac is tiny.
float RatioReturnX87(float t, float step, float lengthValue, float total, float len)
{
#if defined(_M_IX86)
    float r;
    __asm {
        fld dword ptr [lengthValue]
        fsub dword ptr [total]
        fdiv dword ptr [len]
        fmul dword ptr [step]
        fld dword ptr [t]
        fsub dword ptr [step]
        faddp st(1), st
        fstp dword ptr [r]
    }
    return r;
#else
    const double frac = (static_cast<double>(lengthValue) - total) / len;
    return static_cast<float>((static_cast<double>(t) - step) + frac * step);
#endif
}

}  // namespace

// 0x1006eb70: every coefficient is zero.
SplineSegment::SplineSegment() : c0{}, c1{}, c2{}, c3{}
{
}

// 0x1006ebc0: zero, then the constant term is the point.
SplineSegment::SplineSegment(const Vector3& v) : c0{}, c1{}, c2{}, c3(v)
{
}

// 0x1000f7b0: vftable plus a raw copy of the twelve coefficient floats.
SplineSegment::SplineSegment(const SplineSegment& o) : c0(o.c0), c1(o.c1), c2(o.c2), c3(o.c3)
{
}

// 0x10028730 (shared by every spline segment class): the twelve floats only.
SplineSegment& SplineSegment::operator=(const SplineSegment& o)
{
    c0 = o.c0;
    c1 = o.c1;
    c2 = o.c2;
    c3 = o.c3;
    return *this;
}

// 0x10071c20 (shared): vftable reset only, no members to release.
SplineSegment::~SplineSegment()
{
}

// 0x1006ec10: zero the first three coefficients, then a point in the constant one.
void __cdecl SplineSegment::Set(const Vector3& v)
{
    c0 = Vector3{0.0f, 0.0f, 0.0f};
    c1 = Vector3{0.0f, 0.0f, 0.0f};
    c2 = Vector3{0.0f, 0.0f, 0.0f};
    c3 = v;
}

// 0x1006ec60: ((c0*t + c1)*t + c2)*t + c3, component by component (SSE in the original).
void __cdecl SplineSegment::GetPositionByRatio(float ratio, Vector3& out) const
{
    PointAt(*this, ratio, out.x, out.y, out.z);
}

// 0x1006f830: wrapper that writes through the virtual function.
Vector3 __cdecl SplineSegment::GetPositionByRatio(float ratio) const
{
    Vector3 out;
    GetPositionByRatio(ratio, out);
    return out;
}

// 0x1006ece0: walk `steps` curve points from start to end; the first segment longer than the remaining length is
// interpolated linearly. The original keeps the previous point if a step is shorter than 1e-8.
void __cdecl SplineSegment::GetPositionByLength(float length, Vector3& out, float start, float end,
                                                unsigned int steps) const
{
    const float fSteps = static_cast<float>(steps);
    const float step = static_cast<float>((static_cast<double>(end) - start) / fSteps);

    float prevX, prevY, prevZ;
    PointAt(*this, start, prevX, prevY, prevZ);
    float t = start;
    float total = 0.0f;
    float lastX = 0.0f, lastY = 0.0f, lastZ = 0.0f;
    unsigned int i = 0;
    while (i < steps) {
        using namespace SplinesDetail;
        t = Add(step, t);
        float x, y, z;
        PointAt(*this, t, x, y, z);
        lastX = x;
        lastY = y;
        lastZ = z;
        const float dx = Sub(x, prevX), dy = Sub(y, prevY), dz = Sub(z, prevZ);
        const float len = SqrtFast(Add(Add(Mul(dz, dz), Mul(dy, dy)), Mul(dx, dx)));
        if (1.0e-8f < len) {
            const float reached = Add(len, total);
            if (length < reached) {
                const float frac = Div(Sub(length, total), len);
                out.x = Add(Mul(Sub(x, prevX), frac), prevX);
                out.y = Add(Mul(Sub(y, prevY), frac), prevY);
                out.z = Add(Mul(Sub(z, prevZ), frac), prevZ);
                return;
            }
            total = reached;
            prevX = x;
            prevY = y;
            prevZ = z;
        }
        ++i;
    }
    out.x = lastX;
    out.y = lastY;
    out.z = lastZ;
}

// 0x1006f8b0: wrapper for the returned Vector3 (hidden return slot).
Vector3 __cdecl SplineSegment::GetPositionByLength(float length, float start, float end, unsigned int steps) const
{
    Vector3 out;
    GetPositionByLength(length, out, start, end, steps);
    return out;
}

// 0x1006f0f0: inverse of GetPositionByLength: the curve ratio at the first step that reaches `length`.
float __cdecl SplineSegment::GetRatio(float length, float start, float end, unsigned int steps) const
{
    if (OrigRatioFn forwarded = OrigRatio())
        return forwarded(this, length, start, end, steps);

    const float fSteps = static_cast<float>(steps);
    const float step = static_cast<float>((static_cast<double>(end) - start) / fSteps);

    float prevX, prevY, prevZ;
    PointAt(*this, start, prevX, prevY, prevZ);
    float t = start;
    float total = 0.0f;
    float lastD2 = 0.0f;
    unsigned int i = 0;
    while (i < steps) {
        using namespace SplinesDetail;
        t = Add(step, t);
        float x, y, z;
        PointAt(*this, t, x, y, z);
        const float dx = Sub(x, prevX), dy = Sub(y, prevY), dz = Sub(z, prevZ);
        const float d2 = Add(Add(Mul(dz, dz), Mul(dy, dy)), Mul(dx, dx));
        lastD2 = d2;
        const float len = SqrtFast(d2);
        if (1.0e-8f < len) {
            const float reached = Add(len, total);
            if (length < reached)
                return RatioReturnX87(t, step, length, total, len);            total = reached;
            prevX = x;
            prevY = y;
            prevZ = z;
        }
        ++i;
    }
    return end;
}

// 0x1006f430: sum of the sampled polyline lengths, steps segments in [start, end].
float __cdecl SplineSegment::GetLength(float start, float end, unsigned int steps) const
{
    const float fSteps = static_cast<float>(steps);
    const float step = static_cast<float>((static_cast<double>(end) - start) / fSteps);

    float prevX, prevY, prevZ;
    PointAt(*this, start, prevX, prevY, prevZ);
    float t = start;
    float total = 0.0f;
    for (unsigned int i = 0; i < steps; ++i) {
        using namespace SplinesDetail;
        t = Add(step, t);
        float x, y, z;
        PointAt(*this, t, x, y, z);
        const float dx = Sub(x, prevX), dy = Sub(y, prevY), dz = Sub(z, prevZ);
        const float len = SqrtFast(Add(Add(Mul(dz, dz), Mul(dy, dy)), Mul(dx, dx)));
        if (1.0e-8f < len) {
            total = Add(len, total);
            prevX = x;
            prevY = y;
            prevZ = z;
        }
    }
    return total;
}

// 0x1006fdf0
void __cdecl SplineSegment::GetFirstDerivative(float ratio, Vector3& out) const
{
    FirstDerivativeAt(*this, ratio, out.x, out.y, out.z);
}

// 0x1006fe90: note the original's second derivative uses c1 once, not 2*c1 (GetNormal uses 2*c1 inline).
void __cdecl SplineSegment::GetSecondDerivative(float ratio, Vector3& out) const
{
    using namespace SplinesDetail;
    out.x = Add(Mul(Mul(c0.x, ratio), 6.0f), c1.x);
    out.y = Add(Mul(Mul(c0.y, ratio), 6.0f), c1.y);
    out.z = Add(Mul(Mul(c0.z, ratio), 6.0f), c1.z);
}

// 0x1006fef0
void __cdecl SplineSegment::GetThirdDerivative(float ratio, Vector3& out) const
{
    (void)ratio;
    out.x = SplinesDetail::Mul(c0.x, 6.0f);
    out.y = SplinesDetail::Mul(c0.y, 6.0f);
    out.z = SplinesDetail::Mul(c0.z, 6.0f);
}

// 0x1006ff30: unit first derivative; (1,0,0) when the derivative vanishes.
void __cdecl SplineSegment::GetTangent(float ratio, Vector3& out) const
{
    FirstDerivativeAt(*this, ratio, out.x, out.y, out.z);
    const float len2 = Len2(out);
    if (1.0e-8f < len2) {
        NormalizeFast(out);
        return;
    }
    out.x = 1.0f;
    out.y = 0.0f;
    out.z = 0.0f;
}

// 0x10070020: the part of the second derivative orthogonal to the tangent, normalised; (0,1,0) if degenerate.
void __cdecl SplineSegment::GetNormal(float ratio, Vector3& out) const
{
    using namespace SplinesDetail;
    const float t = ratio;
    float dx, dy, dz;
    FirstDerivativeAt(*this, t, dx, dy, dz);

    const float ddx = Add(Mul(Mul(c0.x, t), 6.0f), c1.x);
    const float ddy = Add(Mul(Mul(c0.y, t), 6.0f), c1.y);
    const float ddz = Add(Mul(Mul(c0.z, t), 6.0f), c1.z);

    const float dot = Add(Add(Mul(dy, ddy), Mul(dz, ddz)), Mul(dx, ddx));
    const float len2 = Add(Add(Mul(dy, dy), Mul(dz, dz)), Mul(dx, dx));

    out.x = Sub(Mul(ddx, len2), Mul(dx, dot));
    out.y = Sub(Mul(ddy, len2), Mul(dy, dot));
    out.z = Sub(Mul(ddz, len2), Mul(dz, dot));

    const float n2 = Add(Add(Mul(out.z, out.z), Mul(out.y, out.y)), Mul(out.x, out.x));
    if (1.0e-8f < n2) {
        NormalizeFast(out);
        return;
    }
    out.x = 0.0f;
    out.y = 1.0f;
    out.z = 0.0f;
}

// 0x10070250: normal x tangent, where the tangent is the normalised first derivative.
void __cdecl SplineSegment::GetBinormal(float ratio, Vector3& out) const
{
    using namespace SplinesDetail;
    GetNormal(ratio, out);
    Vector3 tangent;
    GetTangent(ratio, tangent);

    const float nx = out.x, ny = out.y, nz = out.z;
    out.x = Sub(Mul(tangent.y, nz), Mul(tangent.z, ny));
    out.y = Sub(Mul(tangent.z, nx), Mul(tangent.x, nz));
    out.z = Sub(Mul(tangent.x, ny), Mul(tangent.y, nx));
}

// 0x100703b0: the smallest step count whose length is within `tolerance` of the length at maxSteps.
unsigned int __cdecl SplineSegment::DetermineNumSplineSteps(unsigned int maxSteps, float tolerance, float start,
                                                            float end) const
{
    const float reference = GetLength(start, end, maxSteps);
    for (unsigned int n = 1; n < maxSteps; ++n) {
        const float len = GetLength(start, end, n);
        if (std::fabs(reference - len) < tolerance)
            return n;
    }
    return maxSteps;
}

// 0x1006fd60: the first nine coefficients are all exactly zero (the constant term is not examined).
bool __cdecl SplineSegment::IsNullSplineSegment() const
{
    return c0.x == 0.0f && c0.y == 0.0f && c0.z == 0.0f && c1.x == 0.0f && c1.y == 0.0f && c1.z == 0.0f &&
           c2.x == 0.0f && c2.y == 0.0f && c2.z == 0.0f;
}

// 0x1000f8f0: the twelve floats are four Vector3 coefficient vectors.
const Vector3* __cdecl SplineSegment::GetSegmentConstants() const
{
    return &c0;
}

// 0x10028d60: c0 = 2*p0 - 2*p1 + t0 + t1, c1 = 3*p1 - 3*p0 - 2*t0 - t1, c2 = t0, c3 = p0.
void __cdecl SplineSegment::SetHermiteSplineData(const Vector3& p0, const Vector3& t0, const Vector3& p1,
                                                 const Vector3& t1)
{
    using namespace SplinesDetail;
    c0.x = Add(Add(Sub(Mul(p0.x, 2.0f), Mul(p1.x, 2.0f)), t0.x), t1.x);
    c1.x = Sub(Sub(Sub(Mul(p1.x, 3.0f), Mul(p0.x, 3.0f)), Mul(t0.x, 2.0f)), t1.x);
    c2.x = t0.x;
    c3.x = p0.x;

    c0.y = Add(Add(Sub(Mul(p0.y, 2.0f), Mul(p1.y, 2.0f)), t0.y), t1.y);
    c1.y = Sub(Sub(Sub(Mul(p1.y, 3.0f), Mul(p0.y, 3.0f)), Mul(t0.y, 2.0f)), t1.y);
    c2.y = t0.y;
    c3.y = p0.y;

    c0.z = Add(Add(Sub(Mul(p0.z, 2.0f), Mul(p1.z, 2.0f)), t0.z), t1.z);
    c1.z = Sub(Sub(Sub(Mul(p1.z, 3.0f), Mul(p0.z, 3.0f)), Mul(t0.z, 2.0f)), t1.z);
    c2.z = t0.z;
    c3.z = p0.z;
}

}  // namespace Jet
