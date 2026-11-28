// Render3DCore_7c tails: the three Plane exports U1b left forwarded. Split and Clip are the original's
// Sutherland-Hodgman style clippers with its 0.0001 tolerance and its exact intersection arithmetic (direction
// normalised through 0x10008b50 before t is formed). Intersect(const Plane&) builds the line with the local Line
// vtable and fills its point with Plane::Intersect(Line).
#include <jet/Render3DCoreLines.h>  // defines ClipResult (guard shared with Render3DCorePlane.h)
#include <jet/Render3DCorePlane.h>
#include <jet/Render3DCoreSolids.h>
#include <jet/KernelStandard.h>

namespace Jet {

namespace {

constexpr float kClipTolerance = 0.0001f;  // 0x1007d27c

inline int Classify(float distance)
{
    // The original's SSE chain: COMISS value,0.0001 / JBE, then COMISS -0.0001,value / JA. Unordered (NaN)
    // therefore falls into the coincident case.
    if (distance > kClipTolerance)
        return 1;
    if (distance < -kClipTolerance)
        return 2;
    return 3;
}

inline float Side(const Plane& plane, const Vector3& v)
{
    // y, z, x accumulation order, as the original's SSE block for the clippers.
    const float s = SolidsDetail::AddSS(
        SolidsDetail::AddSS(SolidsDetail::MulSS(v.y, plane.normal.y),
                            SolidsDetail::MulSS(v.z, plane.normal.z)),
        SolidsDetail::MulSS(v.x, plane.normal.x));
    return SolidsDetail::SubSS(s, plane.d);
}

Vector3 Intersection(const Plane& plane, const Vector3& prev, const Vector3& cur)
{
    Vector3 dir;
    dir.x = SolidsDetail::SubSS(cur.x, prev.x);
    dir.y = SolidsDetail::SubSS(cur.y, prev.y);
    dir.z = SolidsDetail::SubSS(cur.z, prev.z);
    SolidsDetail::Normalize(dir);

    // t = (d - n.prev) / (n.dir), both dots accumulated in the original's y, z, x order.
    const float numSum = SolidsDetail::AddSS(
        SolidsDetail::AddSS(SolidsDetail::MulSS(plane.normal.y, prev.y),
                            SolidsDetail::MulSS(plane.normal.z, prev.z)),
        SolidsDetail::MulSS(plane.normal.x, prev.x));
    const float denSum = SolidsDetail::AddSS(
        SolidsDetail::AddSS(SolidsDetail::MulSS(plane.normal.y, dir.y),
                            SolidsDetail::MulSS(plane.normal.z, dir.z)),
        SolidsDetail::MulSS(plane.normal.x, dir.x));
    const float t = SolidsDetail::DivSS(SolidsDetail::SubSS(plane.d, numSum), denSum);
    Vector3 out;
    out.x = SolidsDetail::AddSS(prev.x, SolidsDetail::MulSS(dir.x, t));
    out.y = SolidsDetail::AddSS(prev.y, SolidsDetail::MulSS(dir.y, t));
    out.z = SolidsDetail::AddSS(prev.z, SolidsDetail::MulSS(dir.z, t));
    return out;
}

// The original Split's crossing point is not the Clip one: its assembled code normalises the *current vertex*
// (a position, not prev->cur) and walks the previous vertex along that ray. Reproduced instruction for
// instruction because the resulting polygons are what the shipped callers see.
Vector3 SplitIntersection(const Plane& plane, const Vector3& prev, const Vector3& cur)
{
    Vector3 dir = cur;
    SolidsDetail::Normalize(dir);

    const float numSum = SolidsDetail::AddSS(
        SolidsDetail::AddSS(SolidsDetail::MulSS(plane.normal.y, prev.y),
                            SolidsDetail::MulSS(plane.normal.z, prev.z)),
        SolidsDetail::MulSS(plane.normal.x, prev.x));
    const float denSum = SolidsDetail::AddSS(
        SolidsDetail::AddSS(SolidsDetail::MulSS(plane.normal.y, dir.y),
                            SolidsDetail::MulSS(plane.normal.z, dir.z)),
        SolidsDetail::MulSS(plane.normal.x, dir.x));
    const float t = SolidsDetail::DivSS(SolidsDetail::SubSS(plane.d, numSum), denSum);

    Vector3 out;
    out.x = SolidsDetail::AddSS(prev.x, SolidsDetail::MulSS(dir.x, t));
    out.y = SolidsDetail::AddSS(prev.y, SolidsDetail::MulSS(dir.y, t));
    out.z = SolidsDetail::AddSS(prev.z, SolidsDetail::MulSS(dir.z, t));
    return out;
}

Polygon* MakePolygon()
{
    // The original allocates with Alloc::New; the object is destroyed through its virtual deleting destructor,
    // which uses the module's operator delete. Global new keeps both halves on the same CRT heap in this module.
    return new Polygon();
}

}  // namespace

ClipResult __cdecl Plane::Clip(const Polygon& poly, Polygon*& clipped) const
{
    const int count = poly.numVerts;
    int front = 0;
    int nonFront = 0;
    // The original keeps the classification in a fixed module global; a small stack copy is equivalent (a
    // polygon with more than 32 vertices overruns the original's buffer anyway).
    int classes[32];
    for (int i = 0; i < count; ++i) {
        const int c = Classify(Side(*this, poly.verts[i]));
        if (i < 32)
            classes[i] = c;
        if (c == 1)
            ++front;
        else
            ++nonFront;
    }
    if (nonFront == 0 && front != 0)
        return static_cast<ClipResult>(1);
    if (nonFront != 0 && front == 0)
        return static_cast<ClipResult>(2);

    Vector3 buffer[33];
    int outCount = 0;
    for (int i = 0; i < count; ++i) {
        const int prevIndex = (i + count - 1) % count;
        const int prevClass = count <= 32 ? classes[prevIndex] : Classify(Side(*this, poly.verts[prevIndex]));
        const int curClass = count <= 32 ? classes[i] : Classify(Side(*this, poly.verts[i]));
        const Vector3& prev = poly.verts[prevIndex];
        const Vector3& cur = poly.verts[i];
        if (prevClass == 1) {
            if (curClass == 1) {
                buffer[outCount++] = cur;
            } else {
                buffer[outCount++] = Intersection(*this, prev, cur);
            }
        } else if (curClass == 1) {
            buffer[outCount++] = Intersection(*this, prev, cur);
            buffer[outCount++] = cur;
        }
    }

    if (clipped != nullptr)
        delete clipped;
    Polygon* result = MakePolygon();
    if (outCount != 0)
        result->Set(outCount, buffer);
    clipped = result;
    return static_cast<ClipResult>(3);
}

void __cdecl Plane::Split(const Polygon& poly, Polygon& frontPoly, Polygon& backPoly) const
{
    const int count = poly.numVerts;
    Vector3 frontBuffer[33];
    Vector3 backBuffer[33];
    int frontCount = 0;
    int backCount = 0;
    for (int i = 0; i < count; ++i) {
        const int prevIndex = (i + count - 1) % count;
        const int prevClass = Classify(Side(*this, poly.verts[prevIndex]));
        const int curClass = Classify(Side(*this, poly.verts[i]));
        const Vector3& prev = poly.verts[prevIndex];
        const Vector3& cur = poly.verts[i];
        if (prevClass == 1) {
            if (curClass == 1) {
                frontBuffer[frontCount++] = cur;
            } else {
                const Vector3 hit = SplitIntersection(*this, prev, cur);
                frontBuffer[frontCount++] = hit;
                backBuffer[backCount++] = hit;
                backBuffer[backCount++] = cur;
            }
        } else if (curClass == 1) {
            const Vector3 hit = SplitIntersection(*this, prev, cur);
            frontBuffer[frontCount++] = hit;
            backBuffer[backCount++] = hit;
            frontBuffer[frontCount++] = cur;
        } else {
            backBuffer[backCount++] = cur;
        }
    }
    if (frontCount != 0)
        frontPoly.Set(frontCount, frontBuffer);
    if (backCount != 0)
        backPoly.Set(backCount, backBuffer);
}

Line __cdecl Plane::Intersect(const Plane& other) const
{
    Vector3 dir;
    dir.x = SolidsDetail::SubSS(SolidsDetail::MulSS(normal.y, other.normal.z),
                                SolidsDetail::MulSS(normal.z, other.normal.y));
    dir.y = SolidsDetail::SubSS(SolidsDetail::MulSS(other.normal.x, normal.z),
                                SolidsDetail::MulSS(normal.x, other.normal.z));
    dir.z = SolidsDetail::SubSS(SolidsDetail::MulSS(normal.x, other.normal.y),
                                SolidsDetail::MulSS(other.normal.x, normal.y));

    Line result;
    result.direction = dir;

    Line temp;
    temp.direction.x = SolidsDetail::SubSS(SolidsDetail::MulSS(dir.y, other.normal.z),
                                           SolidsDetail::MulSS(dir.z, other.normal.y));
    temp.direction.y = SolidsDetail::SubSS(SolidsDetail::MulSS(dir.z, other.normal.x),
                                           SolidsDetail::MulSS(dir.x, other.normal.z));
    temp.direction.z = SolidsDetail::SubSS(SolidsDetail::MulSS(dir.x, other.normal.y),
                                           SolidsDetail::MulSS(dir.y, other.normal.x));
    temp.point.x = SolidsDetail::MulSS(other.normal.x, other.d);
    temp.point.y = SolidsDetail::MulSS(other.normal.y, other.d);
    temp.point.z = SolidsDetail::MulSS(other.normal.z, other.d);

    result.point = Plane::Intersect(temp);
    return result;
}

}  // namespace Jet
