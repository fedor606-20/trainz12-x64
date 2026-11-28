// Unit U9 of WaterSystem_7c: WaterTiled. Behaviour follows the TS12 assembly (addresses per function).
//
// The grid quads are anchored to the camera position truncated to whole cells; each quad is presented to
// Camera::Clip3D as eight points (the quad twice), then drawn through the single tile with
// Translate(cellX, cellY, 0) * matrix as the world matrix.
#include <jet/WaterTiled.h>

#include <jet/KernelStandard.h>  // Alloc
#include <jet/VCall.h>
#include <jet/WaterGeometry.h>
#include <jet/WaterTile.h>

#include <cstring>
#include <new>

#include "WaterUnit9Shell.h"

namespace Jet {

namespace {

// the original's 0x10005180: result = a * b, row-major, with the assembly's per-row operand order
void Multiply4(const Matrix4& a, Matrix4& b)
{
    const float* A = a.m;
    float B[16];
    std::memcpy(B, b.m, sizeof B);
    for (int j = 0; j < 4; ++j) {
        b.m[0 + j] = ((A[1] * B[4 + j] + A[3] * B[12 + j]) + A[2] * B[8 + j]) + A[0] * B[j];
        b.m[4 + j] = ((A[7] * B[12 + j] + A[6] * B[8 + j]) + A[4] * B[j]) + A[5] * B[4 + j];
        b.m[8 + j] = ((A[11] * B[12 + j] + A[10] * B[8 + j]) + A[8] * B[j]) + A[9] * B[4 + j];
        b.m[12 + j] = ((A[15] * B[12 + j] + A[14] * B[8 + j]) + A[12] * B[j]) + A[13] * B[4 + j];
    }
}

// the ViewPort's camera pointer at +0x20
Camera* ViewCamera(ViewPort& viewport)
{
    return *reinterpret_cast<Camera* const*>(reinterpret_cast<char*>(&viewport) + 0x20);
}

}  // namespace

// 0x100171a0
WaterTiled::WaterTiled(const WaterTiled& o) : Particle(o), tile(o.tile), size(o.size)
{
}

// 0x100172c0
WaterTiled::WaterTiled(WaterMaterial* material, WaterGeometry* geometry, const Size& size_)
{
    size = size_;

    WaterTile* t = static_cast<WaterTile*>(Alloc::New(sizeof(WaterTile)));
    if (t)
        ::new (static_cast<void*>(t)) WaterTile();
    tile = t;
    t->SetMaterial(material);
    t->SetGeometry(geometry);

    void* memory = Alloc::New(0xdc);
    ParticleVisibleBoundingBox* volume =
        memory ? ::new (memory) ParticleVisibleBoundingBox(this) : nullptr;
    if (!Particle::SetBoundingVolume(reinterpret_cast<ParticleBoundingVolume*>(volume)) && volume) {
        using DtorFn = void*(__thiscall*)(void*, unsigned);
        (*reinterpret_cast<DtorFn* const*>(volume))[0](volume, 1);
    }
}

// 0x100173e0
WaterTiled::~WaterTiled()
{
    if (tile)
        VDelete(tile);
}

// 0x100171e0
WaterTiled& __thiscall WaterTiled::operator=(const WaterTiled& o)
{
    Particle::operator=(o);
    tile = o.tile;
    size = o.size;
    return *this;
}

// 0x10017280
WaterTile* __cdecl WaterTiled::GetTile()
{
    return tile;
}

// 0x10017810
void __cdecl WaterTiled::Cull(ViewPoint& view, const Matrix4& matrix)
{
    view.Add(this, matrix);
}

// 0x10017450
void __cdecl WaterTiled::Render(Display* display, ViewPort& viewport, Matrix4& matrix)
{
    Camera* camera = ViewCamera(viewport);
    const Vector3& cameraPosition = *reinterpret_cast<const Vector3*>(reinterpret_cast<char*>(camera) + 0x2c);
    const Vector2 cameraXY(cameraPosition);

    WaterGeometry* geometry = tile->GetGeometry();
    const Size& gridSize = geometry->GetSize();
    const Vector3 scale = geometry->GetScale();
    const float cellWidth = (static_cast<float>(gridSize.w) - 1.0f) * scale.x;
    const float cellHeight = (static_cast<float>(gridSize.h) - 1.0f) * scale.y;
    const float anchorX = static_cast<float>(static_cast<int>(cameraXY.x / cellWidth)) * cellWidth;
    const float anchorY = static_cast<float>(static_cast<int>(cameraXY.y / cellHeight)) * cellHeight;

    const int startX = -(size.w / 2);
    const int startY = -(size.h / 2);
    for (int row = 0; row < size.h; ++row) {
        const float y0 = static_cast<float>(startY + row) * cellHeight + anchorY;
        const float y1 = y0 + cellHeight;
        for (int col = 0; col < size.w; ++col) {
            const float x0 = static_cast<float>(startX + col) * cellWidth + anchorX;
            const float x1 = x0 + cellWidth;

            Vector3 points[8];
            for (int i = 0; i < 4; ++i)
                points[i] = Vector3(i == 0 || i == 3 ? x0 : x1, i < 2 ? y0 : y1, 0.0f);
            for (int i = 4; i < 8; ++i)
                points[i] = points[i - 4];

            if (camera->Clip3D(points, 8) == BACK)
                continue;

            Matrix4 offset;
            offset.m[0] = 1.0f;
            offset.m[1] = 0.0f;
            offset.m[2] = 0.0f;
            offset.m[3] = 0.0f;
            offset.m[4] = 0.0f;
            offset.m[5] = 1.0f;
            offset.m[6] = 0.0f;
            offset.m[7] = 0.0f;
            offset.m[8] = 0.0f;
            offset.m[9] = 0.0f;
            offset.m[10] = 1.0f;
            offset.m[11] = 0.0f;
            offset.m[12] = x0;
            offset.m[13] = y0;
            offset.m[14] = 0.0f;
            offset.m[15] = 1.0f;

            Matrix4 result = matrix;
            Multiply4(offset, result);

            using SetWorldMatrixFn = void(__cdecl*)(void*, Matrix4*);
            (*reinterpret_cast<SetWorldMatrixFn* const*>(display))[0x54 / 4](display, &result);
            tile->Render(display, viewport, result);
        }
    }
}

}  // namespace Jet
