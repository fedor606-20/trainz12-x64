// Render3DCore_7c wave 6 tails (Tails3): U1d1 leftovers that need U1d2's matrices plus the two 2D separation
// overloads and the Graham scan. The bodies follow the TS12 assembly; the scan keeps the original's in-place sort
// and its odd same-theta grouping so the vertex order matches.
#include <jet/Render3DCoreLines.h>
#include <jet/Render3DCoreSolids.h>

#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
#include <cstdio>

namespace Jet {

namespace {

float BitsToFloat(unsigned int bits)
{
    float value;
    std::memcpy(&value, &bits, sizeof value);
    return value;
}

unsigned int FloatToBits(float value)
{
    unsigned int bits;
    std::memcpy(&bits, &value, sizeof bits);
    return bits;
}

// The Quake seed and one Newton step, kept in x87 double like the original's inline approximations.
double ApproxRootD(float s)
{
    const double inv = static_cast<double>(BitsToFloat(0x5f3759dfu - (FloatToBits(s) >> 1)));
    return inv * (1.5 - (static_cast<double>(s) * 0.5) * inv * inv) * static_cast<double>(s);
}

float ApproxRoot(float s)
{
    return static_cast<float>(ApproxRootD(s));
}

int SideCompareDir2(const Vector2& q, const Vector2& p, const Vector2& d)
{
    const float lhs = q.x * d.y - q.y * d.x;
    const float rhs = p.x * d.y - p.y * d.x;
    return lhs > rhs ? 1 : 2;
}

// LineSegment2D::Intersects's edge test, direction q0 - q1 as the original builds it.
bool SegmentCrosses(const Vector2& a0, const Vector2& a1, const Vector2& b0, const Vector2& b1)
{
    const Vector2 d{b0.x - b1.x, b0.y - b1.y};
    return SideCompareDir2(a0, b0, d) != SideCompareDir2(a1, b0, d);
}

}  // namespace

// 0x1002ab00: 0 when the line crosses the segment, else the nearer corner's distance. The distance and the
// pair selection are x87 doubles in the original; the arguments keep its SSE operand order.
float __cdecl LineSegment2D::GetSeparation(const Line2D& line) const
{
    const float dirx = line.direction.x;
    const float diry = line.direction.y;
    const float px = line.point.x;
    const float py = line.point.y;

    const int s0 = SideCompareDir2(v0, line.point, line.direction);
    const int s1 = SideCompareDir2(v1, line.point, line.direction);
    if (s0 != s1)
        return 0.0f;

    const float side0 = diry * v0.x - dirx * v0.y;
    const float side1 = diry * v1.x - dirx * v1.y;
    const float sidep = diry * px - dirx * py;
    const float dirx2 = dirx * dirx;  // the original rounds dir.x^2 to float before the x87 sum
    const float num0 = side0 - sidep;
    const float num1 = side1 - sidep;
    const double den = std::sqrt(static_cast<double>(diry) * diry + static_cast<double>(dirx2));
    const float d0 = static_cast<float>(std::fabs(static_cast<double>(num0)) / den);
    const float d1 = static_cast<float>(std::fabs(static_cast<double>(num1)) / den);
    if (d0 < d1)
        return d0;
    return d1;
}

// 0x1002af50: the same side test, then the cross distance through the approximate reciprocal square root.
float __cdecl LineSegment2D::GetApproxSeparation(const Line2D& line) const
{
    const float dirx = line.direction.x;
    const float diry = line.direction.y;
    const float px = line.point.x;
    const float py = line.point.y;

    const int s0 = SideCompareDir2(v0, line.point, line.direction);
    const int s1 = SideCompareDir2(v1, line.point, line.direction);
    if (s0 != s1)
        return 0.0f;

    const float s = diry * diry + dirx * dirx;
    const float c1 = px * diry - py * dirx;
    const float a = (diry * v0.x - dirx * v0.y) - c1;
    const float b = (diry * v1.x - dirx * v1.y) - c1;
    const double root = ApproxRootD(s);
    const double da = static_cast<double>(std::fabs(a)) / root;
    const double db = static_cast<double>(std::fabs(b)) / root;
    return static_cast<float>(da < db ? da : db);
}

// 0x10057930
float __cdecl Polygon2D::GetSeparation(const Polygon2D& polygon) const
{
    return static_cast<float>(std::sqrt(static_cast<double>(GetSeparationSquared(polygon))));
}

// 0x10010e60: one of the module's empty bodies.
float __cdecl Polygon2D::GetApproxSeparation(const Polygon2D&) const
{
    return 0.0f;
}

// 0x10057950: the closest vertex pair, two adjacent edges, then the four point-to-edge distances. The original
// wraps both indices by clamping rather than adding one (a non-last index stays where it is), and the last
// distance uses the first polygon's index into the second polygon's array; both quirks are kept.
float __cdecl Polygon2D::GetSeparationSquared(const Polygon2D& polygon) const
{
    if (Intersects(polygon))
        return 0.0f;

    const Polygon2D& a = *this;
    const Polygon2D& b = polygon;

    float best = FLT_MAX;
    int closestA = 0;
    int closestB = 0;
    for (int i = 0; i < a.numVerts; ++i) {
        const Vector2& va = a.vertices[i];
        for (int j = 0; j < b.numVerts; ++j) {
            const Vector2& vb = b.vertices[j];
            const float dx = va.x - vb.x;
            const float dy = va.y - vb.y;
            const float d2 = dy * dy + dx * dx;
            if (best > d2) {
                closestA = i;
                closestB = j;
                best = d2;
            }
        }
    }

    const int ia = (closestA != 0 ? closestA : a.numVerts) - 1;
    const int ib = (closestB != 0 ? closestB : b.numVerts) - 1;

    LineSegment2D segA;  // a[ia] -> a[closestA]
    segA.v0 = a.vertices[ia];
    segA.v1 = a.vertices[closestA];
    LineSegment2D segB;  // b[ib] -> b[closestB]
    segB.v0 = b.vertices[ib];
    segB.v1 = b.vertices[closestB];

    if (segA.Intersects(segB))
        return 0.0f;

    const int nextB = (closestB >= b.numVerts - 1) ? 0 : closestB;
    LineSegment2D segBNext;
    segBNext.v0 = b.vertices[closestB];
    segBNext.v1 = b.vertices[nextB];
    if (segA.Intersects(segBNext))
        return 0.0f;

    const int nextA = (closestA >= a.numVerts - 1) ? 0 : closestA;
    LineSegment2D segANext;
    segANext.v0 = a.vertices[closestA];
    segANext.v1 = a.vertices[nextA];
    if (segANext.Intersects(segB))
        return 0.0f;
    if (segANext.Intersects(segBNext))
        return 0.0f;

    float result = best;
    if (segA.IsBeside(b.vertices[closestB])) {
        const float ex = a.vertices[closestA].x - a.vertices[ia].x;
        const float ey = a.vertices[closestA].y - a.vertices[ia].y;
        const float q = b.vertices[closestB].x * ey - b.vertices[closestB].y * ex;
        const float p = a.vertices[ia].x * ey - a.vertices[ia].y * ex;
        const float cross = q - p;
        const float d = (cross * cross) / (ex * ex + ey * ey);
        if (result > d)
            result = d;
    }
    if (segANext.IsBeside(b.vertices[closestB])) {
        const float ex = a.vertices[nextA].x - a.vertices[closestA].x;
        const float ey = a.vertices[nextA].y - a.vertices[closestA].y;
        const float q = b.vertices[closestB].x * ey - b.vertices[closestB].y * ex;
        const float p = a.vertices[closestA].x * ey - a.vertices[closestA].y * ex;
        const float cross = q - p;
        const float d = (cross * cross) / (ex * ex + ey * ey);
        if (result > d)
            result = d;
    }
    if (segB.IsBeside(a.vertices[closestA])) {
        const float ex = b.vertices[closestB].x - b.vertices[ib].x;
        const float ey = b.vertices[closestB].y - b.vertices[ib].y;
        const float q = a.vertices[closestA].x * ey - a.vertices[closestA].y * ex;
        const float p = b.vertices[ib].x * ey - b.vertices[ib].y * ex;
        const float cross = q - p;
        const float d = (cross * cross) / (ex * ex + ey * ey);
        if (result > d)
            result = d;
    }
    if (segB.IsBeside(a.vertices[closestA])) {
        const float ex = b.vertices[closestB].x - b.vertices[ib].x;
        const float ey = b.vertices[closestB].y - b.vertices[ib].y;
        const float q = a.vertices[closestA].x * ey - a.vertices[closestA].y * ex;
        const float p = b.vertices[ib].x * ey - b.vertices[ib].y * ex;
        const float cross = q - p;
        const float d = (cross * cross) / (ex * ex + ey * ey);
        if (result > d)
            result = d;
    }
    return result;
}

// 0x10057fb0: the original's in-place Graham scan. Points are first thinned (consecutive distance >= 0.0005),
// the start is the lowest then leftmost point, the pseudo-angle sort runs clockwise, and the hull pass swaps the
// kept points to the front. The final vertex list is the reversed stack, as the original's copy loop.
void __cdecl Polygon2D::GrahamScan()
{
    const int n = numVerts;
    Vector2* const verts = vertices;
    if (n <= 0)
        return;

    std::vector<PointNode> storage(static_cast<size_t>(n) * 4 + 8);
    PointNode* const nodes = storage.data();
    int count = 0;

    for (int i = 1; i < n; ++i) {
        const float dx = verts[i - 1].x - verts[i].x;
        const float dy = verts[i - 1].y - verts[i].y;
        const float d2 = dy * dy + dx * dx;
        const float len = ApproxRoot(d2);
        if (!(0.0005f > len)) {
            nodes[count].point = verts[i - 1];
            ++count;
        }
    }
    nodes[count].point = verts[n - 1];
    ++count;

    int minIdx = 0;
    for (int i = 1; i < count; ++i) {
        if (nodes[minIdx].point.y > nodes[i].point.y)
            minIdx = i;
    }
    for (int i = 0; i < count; ++i) {
        if (nodes[i].point.y == nodes[minIdx].point.y && nodes[minIdx].point.x > nodes[i].point.x)
            minIdx = i;
    }
    if (minIdx != 0) {
        const Vector2 saved = nodes[0].point;
        nodes[0].point = nodes[minIdx].point;
        nodes[minIdx].point = saved;
    }

    for (int i = 0; i < count; ++i)
        nodes[i].theta = Theta(&nodes[0].point, &nodes[i].point);
    ShellSortTheta(nodes, count);
    if (count >= 2)
        nodes[0].theta = nodes[1].theta;

    PointNode* const outBase = nodes;
    PointNode* out = nodes;
    int outCount = 0;
    int outIndex = -1;
    int i = 0;
    int idxA = 0;
    int idxB = 0;
    int step = 0;
    char swapFlag = 0;
    char orientFlag = 0;
    float t = 0.0f;

L0:
    swapFlag = 0;
    step = 0;
    {
        int k = i;
        if (k < 2)
            k = 1;
        t = nodes[k].theta;
    }
L1:
    if (i != 0) {
        const float diff = static_cast<float>(std::fabs(static_cast<double>(t - nodes[i].theta)));
        if (0.0001f <= diff)
            goto APPEND;
    }
    if (i >= count)
        goto APPEND;
    ++step;
    if (step == 1) {
        const int old = i;
        ++i;
        idxA = old;
        goto L1;
    }
    if (step == 2) {
        idxB = i;
        ++i;
        goto L1;
    }
    if (step == 3) {
        const float adx = static_cast<float>(std::fabs(static_cast<double>(nodes[idxA].point.x - nodes[idxB].point.x)));
        const float ady = static_cast<float>(std::fabs(static_cast<double>(nodes[idxA].point.y - nodes[idxB].point.y)));
        if (adx > ady) {
            orientFlag = 1;
            if (nodes[idxA].point.x > nodes[idxB].point.x)
                goto SWAP;
            goto L3BF;
        }
        orientFlag = 0;
        if (nodes[idxA].point.y <= nodes[idxB].point.y)
            goto L3E7;
        goto SWAP;
    }
    goto L3B9;
SWAP:
    {
        const int saved = idxA;
        idxA = idxB;
        idxB = saved;
    }
    swapFlag = 1;
L3B9:
    if (orientFlag == 0)
        goto L3E7;
L3BF:
    if (nodes[idxA].point.x > nodes[i].point.x) {
        idxA = i;
        ++i;
        goto L1;
    }
    if (nodes[i].point.x > nodes[idxB].point.x) {
        idxB = i;
        ++i;
        goto L1;
    }
    ++i;
    goto L1;
L3E7:
    if (nodes[idxA].point.y > nodes[i].point.y) {
        idxA = i;
        ++i;
        goto L1;
    }
    if (nodes[i].point.y > nodes[idxB].point.y) {
        idxB = i;
        ++i;
        goto L1;
    }
    ++i;
    goto L1;

APPEND:
    {
        const PointNode saved = *out;
        const bool swapped = swapFlag != 0;
        if (swapped) {
            *out = nodes[idxB];
            outCount += 2;
            const int nextIndex = outIndex + 1;
            if (idxA == nextIndex) {
                out[1] = saved;
                outIndex = nextIndex + 1;
                out += 2;
            } else {
                outIndex = nextIndex;
                out[1] = nodes[idxA];
                out += 2;
            }
        } else {
            *out = nodes[idxA];
            ++outCount;
            ++outIndex;
            ++out;
            if (step <= 1) {
                // one point only
            } else {
                ++outCount;
                if (idxB == outIndex) {
                    *out = saved;
                    ++outIndex;
                    ++out;
                } else {
                    ++outIndex;
                    *out = nodes[idxB];
                    ++out;
                }
            }
        }
    }
    if (i < count)
        goto L0;

    if (outCount <= 2)
        return;

    const int hull = outCount;
    if (hull > 3) {
        int m = 2;
        for (int p = 3; p < hull; ++p) {
            while (m >= 1 && CounterClockwise(&outBase[m - 1].point, &outBase[m].point, &outBase[p].point) == 1)
                --m;
            ++m;
            const PointNode saved = outBase[p];
            outBase[p] = outBase[m];
            outBase[m] = saved;
        }
        outCount = m + 1;
    }

    if (outCount > n) outCount = n;
    for (int k = 1; k <= outCount; ++k)
        verts[k - 1] = outBase[outCount - k].point;
    int kept = 0;
    for (int k = 0; k < outCount; ++k) {
        if (kept != 0 && verts[k].x == verts[kept - 1].x && verts[k].y == verts[kept - 1].y)
            continue;
        verts[kept++] = verts[k];
    }
    Set(kept);
}

}  // namespace Jet
