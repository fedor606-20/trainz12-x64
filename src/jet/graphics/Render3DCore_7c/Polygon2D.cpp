// Unit U1d1 of Render3DCore_7c: Jet::Polygon2D, from the TS12 assembly.
//
// 0xc bytes: count +0, vertices +4, ownsVertices +8. The copy constructor deep-copies but leaves the ownership flag
// alone (as the original does); assignment is the original's shallow three-dword copy. GetCenter accumulates in the
// original's unrolled order. GrahamScan, GetSeparation(Polygon2D) and GetSeparationSquared(Polygon2D) still forward
// to Render3DCore_7c_orig.dll because their decompiled control flow is not reproducible with confidence.
#include <jet/Render3DCoreLines.h>

#include <cmath>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <cstdlib>

namespace Jet {

namespace {

int SideCompareDir2(const Vector2& q, const Vector2& p, const Vector2& d)
{
    const float lhs = q.x * d.y - q.y * d.x;
    const float rhs = p.x * d.y - p.y * d.x;
    return lhs > rhs ? 1 : 2;
}

// The LineSegment2D::Intersects side test with the segment's direction given as q0 - q1 (as the caller builds it).
bool SegmentCrosses(const Vector2& a0, const Vector2& a1, const Vector2& b0, const Vector2& b1)
{
    // 0x10057450's edge test: both ends of the polygon edge on different sides of the segment's line (the
    // direction is q0 - q1, the reference point q0, exactly as the original).
    const Vector2 d{b0.x - b1.x, b0.y - b1.y};
    return SideCompareDir2(a0, b0, d) != SideCompareDir2(a1, b0, d);
}

}  // namespace

// 0x10054ac0
Polygon2D::Polygon2D() : numVerts(0), vertices(nullptr), ownsVertices(false)
{
}

// 0x10054ad0
Polygon2D::Polygon2D(int count, Vector2* verts, bool owns)
    : numVerts(count), vertices(verts), ownsVertices(owns)
{
}

// 0x10057210: deep copy; the ownership flag is not written.
Polygon2D::Polygon2D(const Polygon2D& polygon) : numVerts(polygon.numVerts)
{
    vertices = static_cast<Vector2*>(Alloc::New(static_cast<unsigned int>(numVerts * 8)));
    for (int i = 0; i < numVerts; ++i)
        vertices[i] = polygon.vertices[i];
}

// 0x10054af0
Polygon2D::~Polygon2D()
{
    if (!ownsVertices) {
        if (vertices)
            Alloc::Delete(vertices);
        vertices = nullptr;
    }
}

// 0x10054e40: the original's shallow copy.
Polygon2D& Polygon2D::operator=(const Polygon2D& polygon)
{
    numVerts = polygon.numVerts;
    vertices = polygon.vertices;
    ownsVertices = polygon.ownsVertices;
    return *this;
}

// 0x10054b20
void __cdecl Polygon2D::SetNumVerts(int count)
{
    numVerts = count;
}

// 0x1000a870
int __cdecl Polygon2D::GetNumVerts() const
{
    return numVerts;
}

// 0x10006970
Vector2* __cdecl Polygon2D::GetVertices()
{
    return vertices;
}

const Vector2* __cdecl Polygon2D::GetConstVertices() const
{
    return vertices;
}

// 0x10054b30
void __cdecl Polygon2D::Set(int count, Vector2* verts, bool owns)
{
    if (!ownsVertices) {
        if (vertices)
            Alloc::Delete(vertices);
        vertices = nullptr;
    }
    numVerts = count;
    vertices = verts;
    ownsVertices = owns;
}

// 0x10054b70: grow or shrink in place; the buffer is kept when shrinking.
bool __cdecl Polygon2D::Set(int count)
{
    if (vertices == nullptr) {
        if (count == 0) {
            numVerts = 0;
            vertices = nullptr;
            return true;
        }
        numVerts = count;
        vertices = static_cast<Vector2*>(Alloc::New(static_cast<unsigned int>(count * 8)));
        return true;
    }
    if (count < numVerts) {
        numVerts = count;
        return true;
    }
    if (numVerts < count) {
        Vector2* block = static_cast<Vector2*>(Alloc::New(static_cast<unsigned int>(count * 8)));
        for (int i = 0; i < numVerts; ++i)
            block[i] = vertices[i];
        if (vertices)
            Alloc::Delete(vertices);
        vertices = block;
        numVerts = count;
        return true;
    }
    return true;
}

// 0x10057270: copy the whole chunk list, take ownership.
void __cdecl Polygon2D::Set(const FastDynamicArray<Vector2, 13, Less<Vector2, Vector2>>& list)
{
    if (!ownsVertices) {
        if (vertices)
            Alloc::Delete(vertices);
        vertices = nullptr;
    }
    typedef FastDynamicArray<Vector2, 13, Less<Vector2, Vector2>> List;
    unsigned int total = 0;
    typename List::Chunk* head = list.head;
    if (head) {
        typename List::Chunk* c = head;
        do {
            total += c->count;
            c = c->next;
        } while (c != head);
    }
    numVerts = static_cast<int>(total);
    vertices = static_cast<Vector2*>(Alloc::New(total * 8));
    if (head) {
        int out = 0;
        typename List::Chunk* c = head;
        do {
            for (uint32_t k = 0; k < c->count; ++k)
                vertices[out++] = c->data[k];
            c = c->next;
        } while (c != head);
    }
    ownsVertices = false;
}

// 0x10054c40: the unrolled sum, then the reciprocal count (SSE float).
Vector2 __cdecl Polygon2D::GetCenter() const
{
    float sumx = 0.0f;
    float sumy = 0.0f;
    int i = 0;
    if (numVerts >= 4) {
        for (; i + 4 <= numVerts; i += 4) {
            float x = vertices[i].x + sumx;
            float y = vertices[i].y + sumy;
            x = vertices[i + 1].x + x;
            y = vertices[i + 1].y + y;
            x = vertices[i + 2].x + x;
            y = vertices[i + 2].y + y;
            sumx = vertices[i + 3].x + x;
            sumy = vertices[i + 3].y + y;
        }
    }
    for (; i < numVerts; ++i) {
        sumx = vertices[i].x + sumx;
        sumy = vertices[i].y + sumy;
    }
    if (numVerts == 0)
        return Vector2{0.0f, 0.0f};
    const float inv = 1.0f / static_cast<float>(numVerts);
    return Vector2{inv * sumx, inv * sumy};
}

// 0x10054da0
int __cdecl Polygon2D::CounterClockwise(const Vector2* a, const Vector2* b, const Vector2* c)
{
    const float bx = b->x - a->x;
    const float by = b->y - a->y;
    const float cx = c->x - a->x;
    const float cy = c->y - a->y;
    if (cy * bx > cx * by)
        return 1;
    if (cx * by > cy * bx)
        return -1;
    if (0.0f > cx * bx)
        return -1;
    if (0.0f > cy * by)
        return -1;
    if ((cx * cx + cy * cy) > (bx * bx + by * by))
        return 1;
    return 0;
}

// 0x100587a0
float __cdecl Polygon2D::Theta(const Vector2* a, const Vector2* b)
{
    const float dx = a->x - b->x;
    const float dy = b->y - a->y;
    float t;
    if (dx == 0.0f && dy == 0.0f) {
        t = 0.0f;
    } else {
        const double adx = std::fabs(static_cast<double>(dx));
        const double ady = std::fabs(static_cast<double>(dy));
        t = static_cast<float>(static_cast<double>(dy) / (ady + adx));
    }
    if (dx < 0.0f)
        return 2.0f - t;
    if (dy < 0.0f)
        t = t + 4.0f;
    return t;
}

// 0x100586a0: Knuth-gap shell sort by theta, ascending; a NaN theta stops the shift.
void __cdecl Polygon2D::ShellSortTheta(PointNode* nodes, int& count)
{
    const int n = count;
    int gap = 1;
    if (n / 9 >= 1) {
        gap = 1;
        do {
            gap = gap * 3 + 1;
        } while (gap <= n / 9);
        if (gap < 1)
            return;
    }
    do {
        for (int i = gap + 1; i <= n; ++i) {
            const PointNode tmp = nodes[i - 1];
            int j = i;
            while (j > gap) {
                const PointNode& prev = nodes[j - gap - 1];
                if (!(prev.theta > tmp.theta))
                    break;
                nodes[j - 1] = prev;
                j -= gap;
            }
            nodes[j - 1] = tmp;
        }
        gap /= 3;
    } while (gap > 0);
}

// 0x10057320: inside when the point is not right of any edge.
bool __cdecl Polygon2D::Intersects(const Vector2& point) const
{
    if (numVerts == 0)
        return true;
    for (int i = 0; i < numVerts; ++i) {
        const Vector2& cur = vertices[i];
        const Vector2& prev = vertices[(i + numVerts - 1) % numVerts];
        if (0.0f < (cur.y - prev.y) * (point.x - prev.x) - (cur.x - prev.x) * (point.y - prev.y))
            return false;
    }
    return true;
}

// 0x100573a0: the corners on different sides of the line.
bool __cdecl Polygon2D::Intersects(const Line2D& line) const
{
    const int side = SideCompareDir2(vertices[0], line.point, line.direction);
    for (int i = 1; i < numVerts; ++i) {
        if (SideCompareDir2(vertices[i], line.point, line.direction) != side)
            return true;
    }
    return false;
}

// 0x10057450: separating-axis test on the segment's line plus the edge crossings.
bool __cdecl Polygon2D::Intersects(const LineSegment2D& line) const
{
    bool all0 = true;
    bool all1 = true;
    if (numVerts == 0)
        return true;
    for (int i = 0; i < numVerts; ++i) {
        const Vector2& cur = vertices[i];
        const Vector2& prev = vertices[(i + numVerts - 1) % numVerts];
        const float edx = cur.x - prev.x;
        const float edy = cur.y - prev.y;
        const bool b0 = ((line.v0.x - prev.x) * edy - (line.v0.y - prev.y) * edx) < 0.0f;
        const bool b1 = ((line.v1.x - prev.x) * edy - (line.v1.y - prev.y) * edx) < 0.0f;
        if (b0 == b1) {
            if (!b0)
                return false;
        } else {
            if (SegmentCrosses(prev, cur, line.v0, line.v1))
                return true;
        }
        all0 = all0 && b0;
        all1 = all1 && b1;
    }
    if (all0 || all1)
        return true;
    return false;
}

// 0x10057630
bool __cdecl Polygon2D::Intersects(const Circle2D& circle) const
{
    bool result = true;
    const int n = numVerts;
    if (n < 1)
        return true;
    const float r2 = circle.radius * circle.radius;
    for (int i = 0; i < n; ++i) {
        const Vector2& cur = vertices[i];
        const Vector2& prev = vertices[(i + n - 1) % n];
        const float ax = circle.center.x - prev.x;
        const float ay = circle.center.y - prev.y;
        if (ax * ax + ay * ay < r2)
            return true;
        const float edx = cur.x - prev.x;
        const float edy = cur.y - prev.y;
        const float cross = ax * edy - edx * ay;
        bool keep;
        if (!(r2 >= (cross * cross) / (edx * edx + edy * edy))) {
            if (0.0f < cross)
                return false;
            keep = true;
        } else {
            const float d1 = edx * ax + edy * ay;
            const float bx = circle.center.x - cur.x;
            const float by = circle.center.y - cur.y;
            const float d2 = bx * edx + by * edy;
            if (0.0f <= d1 && d2 <= 0.0f)
                return true;
            keep = !(0.0f < cross);
        }
        result = result && keep;
    }
    return result;
}

// 0x100577a0
bool __cdecl Polygon2D::Intersects(const Polygon2D& polygon) const
{
    if (numVerts > 0) {
        for (int i = 0; i < numVerts; ++i) {
            const Vector2& p0 = vertices[i];
            const Vector2& p1 = vertices[(i + numVerts - 1) % numVerts];
            for (int j = 0; j < polygon.numVerts; ++j) {
                const Vector2& q0 = polygon.vertices[j];
                const Vector2& q1 = polygon.vertices[(j + polygon.numVerts - 1) % polygon.numVerts];
                LineSegment2D edge;
                edge.v0 = p1;
                edge.v1 = p0;
                LineSegment2D otherEdge;
                otherEdge.v0 = q1;
                otherEdge.v1 = q0;
                if (edge.Intersects(otherEdge))
                    return true;
            }
        }
    }
    if (numVerts != 0) {
        for (int i = 0; i < numVerts; ++i) {
            const Vector2& cur = vertices[i];
            const Vector2& prev = vertices[(i + numVerts - 1) % numVerts];
            const Vector2& other0 = polygon.vertices[0];
            if (0.0f <
                (cur.y - prev.y) * (other0.x - prev.x) - (other0.y - prev.y) * (cur.x - prev.x))
                return polygon.Intersects(vertices[0]);
        }
    }
    return true;
}

// All the Polygon2D / other-shape separations are the same empty function 0x100e60.
float __cdecl Polygon2D::GetSeparation(const Vector2&) const
{
    return 0.0f;
}

float __cdecl Polygon2D::GetSeparation(const Line2D&) const
{
    return 0.0f;
}

float __cdecl Polygon2D::GetSeparation(const LineSegment2D&) const
{
    return 0.0f;
}

float __cdecl Polygon2D::GetSeparation(const Circle2D&) const
{
    return 0.0f;
}

float __cdecl Polygon2D::GetApproxSeparation(const Vector2&) const
{
    return 0.0f;
}

float __cdecl Polygon2D::GetApproxSeparation(const Line2D&) const
{
    return 0.0f;
}

float __cdecl Polygon2D::GetApproxSeparation(const LineSegment2D&) const
{
    return 0.0f;
}

float __cdecl Polygon2D::GetApproxSeparation(const Circle2D&) const
{
    return 0.0f;
}

float __cdecl Polygon2D::GetSeparationSquared(const Vector2&) const
{
    return 0.0f;
}

float __cdecl Polygon2D::GetSeparationSquared(const Line2D&) const
{
    return 0.0f;
}

float __cdecl Polygon2D::GetSeparationSquared(const LineSegment2D&) const
{
    return 0.0f;
}

float __cdecl Polygon2D::GetSeparationSquared(const Circle2D&) const
{
    return 0.0f;
}

}  // namespace Jet
