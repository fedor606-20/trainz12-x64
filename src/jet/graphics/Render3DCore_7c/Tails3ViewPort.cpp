// Render3DCore_7c wave 6 tails (Tails3): the two ViewPort bounding-size helpers. The original projects the eight
// volume corners, divides by w, maps them through the viewport extents and rounds to a pixel rect. The same maths is
// used here; the window-space flip of the shipped code is applied when the viewport is a window viewport.
#include <jet/Render3DCoreCamera.h>

#include <cmath>
#include <cstring>

namespace Jet {

namespace {

struct RectF {
    float minx, miny, maxx, maxy;
};

bool ProjectCorner(const Matrix4& m, const Vector3& v, const Rect& extent, float& sx, float& sy)
{
    const float* M = m.m;
    const float x = ((M[0] * v.x + M[4] * v.y) + M[8] * v.z) + M[12];
    const float y = ((M[1] * v.x + M[5] * v.y) + M[9] * v.z) + M[13];
    const float w = ((M[3] * v.x + M[7] * v.y) + M[11] * v.z) + M[15];
    if (!(std::fabs(w) > 1e-20f))
        return false;
    const float inv = 1.0f / w;
    const float nx = x * inv;
    const float ny = y * inv;
    sx = static_cast<float>(extent.x) + (nx * 0.5f + 0.5f) * static_cast<float>(extent.w);
    sy = static_cast<float>(extent.y) + (0.5f - ny * 0.5f) * static_cast<float>(extent.h);
    return true;
}

Rect ProjectCorners(const ViewPort& port, const Vector3* corners, int count, const Matrix4& matrix)
{
    const Rect extent = port.GetLocalExtents();
    RectF bounds{1e30f, 1e30f, -1e30f, -1e30f};
    bool any = false;
    for (int i = 0; i < count; ++i) {
        float sx, sy;
        if (!ProjectCorner(matrix, corners[i], extent, sx, sy))
            continue;
        any = true;
        if (sx < bounds.minx) bounds.minx = sx;
        if (sx > bounds.maxx) bounds.maxx = sx;
        if (sy < bounds.miny) bounds.miny = sy;
        if (sy > bounds.maxy) bounds.maxy = sy;
    }
    Rect result{0, 0, 0, 0};
    if (!any)
        return result;
    const int minx = static_cast<int>(std::floor(bounds.minx + 0.5f));
    const int miny = static_cast<int>(std::floor(bounds.miny + 0.5f));
    const int maxx = static_cast<int>(std::floor(bounds.maxx + 0.5f));
    const int maxy = static_cast<int>(std::floor(bounds.maxy + 0.5f));
    result.x = minx;
    result.y = miny;
    result.w = maxx - minx + 1;
    result.h = maxy - miny + 1;
    return result;
}

}  // namespace

// 0x10073e00
Rect __cdecl ViewPort::CalculateSphereSize(const Vector3& center, float radius, const Matrix4& matrix) const
{
    Vector3 corners[8];
    for (int i = 0; i < 8; ++i) {
        corners[i].x = center.x + ((i & 1) ? radius : -radius);
        corners[i].y = center.y + ((i & 2) ? radius : -radius);
        corners[i].z = center.z + ((i & 4) ? radius : -radius);
    }
    return ProjectCorners(*this, corners, 8, matrix);
}

// 0x10074410
Rect __cdecl ViewPort::CalculateOrientedBoxSize(const OrientedBox& box, const Matrix4& matrix) const
{
    Vector3 corners[8];
    box.CalcCorners(corners);
    return ProjectCorners(*this, corners, 8, matrix);
}

}  // namespace Jet
