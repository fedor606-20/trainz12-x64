// Render3DCore_7c tails: the U1d1 line/2D members that only needed the local solids classes. The segment-vs-circle
// test first decides whether the segment crosses the circle's plane (signed distances multiplied, exactly as the
// original's COMISS/JC chain) and then clips the segment's supporting line against the circle.
#include <jet/Render3DCoreLines.h>
#include <jet/Render3DCoreSolids.h>

namespace Jet {

namespace {

constexpr float kTolerance = 0.0001f;

inline float PlaneDistance(const Vector3& normal, float d, const Vector3& v)
{
    return SolidsDetail::SubSS(
        SolidsDetail::AddSS(SolidsDetail::AddSS(SolidsDetail::MulSS(normal.y, v.y),
                                                SolidsDetail::MulSS(normal.z, v.z)),
                            SolidsDetail::MulSS(normal.x, v.x)),
        d);
}

}  // namespace

// 0x1002a820: (n.point - d) * (n.endPoint - d) <= 0, with NaN rejecting the segment, then Circle::TestCollision(Line).
bool __cdecl LineSegment::TestCollision(Circle& circle) const
{
    const float startSide = PlaneDistance(circle.normal, circle.d, point);
    const float endSide = PlaneDistance(circle.normal, circle.d, endPoint);
    const float product = SolidsDetail::MulSS(endSide, startSide);
    if (0.0f < product)
        return false;
    return circle.TestCollision(Line(*this));
}

}  // namespace Jet
