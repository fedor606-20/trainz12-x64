// Unit U5 of WaterSystem_7c: the water geometry. Behaviour follows the TS12 assembly (addresses per function).
// The GeometryChunk's arrays are reached through its raw vtable slots as the original does (49 = the item's
// {stride, pointer} pair, 47/51 = the same for the attribute items, 4 = set, 31 = upload); the material's
// CreateGeometryChunk is its vtable slot 46 and RenderChunk slot 14.
#include <jet/WaterGeometry.h>

#include <jet/DisplacementHull.h>
#include <jet/FrameAnimation.h>
#include <jet/KernelStandard.h>  // Alloc
#include <jet/RenderTypes.h>     // DisplayLight
#include <jet/VCall.h>
#include <jet/WaterAnimation.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdlib>  // rand
#include <cstring>

#include "RenderSystemShell.h"
#include "WaterSystemInternal.h"  // GridNewBytes

namespace Jet {

class WaterMaterial;  // opaque until U7

namespace {

// the accumulated time of the static wave field (the original's dword_100398e0)
float gStaticTime = 0.0f;

// a {stride, pointer} pair the chunk's item-array slots fill in
struct ItemArray {
    int stride;
    float* data;
};

// the material's vtable slot 46 (0xb8): CreateGeometryChunk
GeometryChunk* MaterialCreateChunk(WaterMaterial* material, int numVertices, int numPrimitives, int numIndices,
                                   StandardChunkHint& hint, bool flag)
{
    return VCall<GeometryChunk*>(material, 0xb8, numVertices, numPrimitives, numIndices, 0, 0, &hint,
                                 flag ? 1 : 0);
}

// the material's vtable slot 14 (0x38): RenderChunk
void MaterialRenderChunk(WaterMaterial* material, Display* display, ViewPort& viewport, GeometryChunk* chunk,
                         const DisplayLightArray* lights, const Matrix4& world)
{
    VCall<void>(material, 0x38, display, &viewport, chunk, lights, &world);
}

}  // namespace

// ---------------------------------------------------------------------------------------------------------------
// WaterGeometry

// 0x100074d0 (the size and scale are set by Initialize)
WaterGeometry::WaterGeometry() : chunk(nullptr), material(nullptr), boundsMin(0.0f, 0.0f, 0.0f),
    boundsMax(0.0f, 0.0f, 0.0f), radius(0.0f)
{
}

// 0x10007520
WaterGeometry::~WaterGeometry()
{
    if (chunk)
        chunk->ReleaseReference();
}

// 0x10007590
void WaterGeometry::Destroy()
{
    if (chunk)
        chunk->ReleaseReference();
}

// 0x100075a0: the full form over a four-corner vertex record
bool WaterGeometry::Initialize(RenderKernel* kernel, WaterMaterial* material_, const Size& size_,
                               const PackedVertexPCT* corners, bool waveVertexProgram)
{
    size = size_;
    material = material_;
    StandardChunkHint hint;  // the original's ctor (imported)
    hint.data[0] = 0;
    hint.data[1] = 0;
    chunk = MaterialCreateChunk(material_, size.w * size.h, 2 * (size.w - 1) * (size.h - 1),
                                6 * (size.w - 1) * (size.h - 1), hint, waveVertexProgram);
    if (!chunk)
        return false;

    // the bounding box from the four corners (z from the vertex-program flag: +-5)
    boundsMin = Vector3(FLT_MAX, FLT_MAX, FLT_MAX);
    boundsMax = Vector3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    for (int c = 0; c < 4; ++c) {
        boundsMin.x = std::min(boundsMin.x, corners[c].position.x);
        boundsMin.y = std::min(boundsMin.y, corners[c].position.y);
        boundsMax.x = std::max(boundsMax.x, corners[c].position.x);
        boundsMax.y = std::max(boundsMax.y, corners[c].position.y);
    }
    boundsMin.z = waveVertexProgram ? -5.0f : 0.0f;
    boundsMax.z = waveVertexProgram ? 5.0f : 0.0f;
    float ax = boundsMin.x >= 0.0f ? boundsMin.x : -boundsMin.x;
    float bx = boundsMax.x >= 0.0f ? boundsMax.x : -boundsMax.x;
    float x = ax <= bx ? boundsMax.x : boundsMin.x;
    float axx = x >= 0.0f ? x : -x;
    float ay = boundsMin.y >= 0.0f ? boundsMin.y : -boundsMin.y;
    float by = boundsMax.y >= 0.0f ? boundsMax.y : -boundsMax.y;
    float y = ay <= by ? boundsMax.y : boundsMin.y;
    if (y < 0.0f)
        y = -y;
    if (axx > 0.00000011920929f && y > 0.00000011920929f)
        radius = static_cast<float>(std::sqrt(static_cast<double>(axx) * axx + static_cast<double>(y) * y));

    // the vertex arrays: item 1 the positions, item 4 the normals, items 15 and 16 the texcoords
    ItemArray positions;
    VCall<void>(chunk, 0xc4, &positions, 1, 0);
    ItemArray normals;
    VCall<void>(chunk, 0xc4, &normals, 4, 0);
    ItemArray tex0;
    VCall<void>(chunk, 0xbc, &tex0, 15, 0);
    ItemArray tex1;
    VCall<void>(chunk, 0xbc, &tex1, 16, 0);

    // bilinear fill of the quad
    float fy = 0.0f;
    for (int row = 0; row < size.h; ++row) {
        float x0 = (corners[3].position.x - corners[0].position.x) * fy + corners[0].position.x;
        float y0 = (corners[3].position.y - corners[0].position.y) * fy + corners[0].position.y;
        float z0 = (corners[3].position.z - corners[0].position.z) * fy + corners[0].position.z;
        float u0 = (corners[3].texCoord.x - corners[0].texCoord.x) * fy + corners[0].texCoord.x;
        float v0 = (corners[3].texCoord.y - corners[0].texCoord.y) * fy + corners[0].texCoord.y;
        float x1 = (corners[2].position.x - corners[1].position.x) * fy + corners[1].position.x;
        float y1 = (corners[2].position.y - corners[1].position.y) * fy + corners[1].position.y;
        float z1 = (corners[2].position.z - corners[1].position.z) * fy + corners[1].position.z;
        float u1 = (corners[2].texCoord.x - corners[1].texCoord.x) * fy + corners[1].texCoord.x;
        float v1 = (corners[2].texCoord.y - corners[1].texCoord.y) * fy + corners[1].texCoord.y;
        float fx = 0.0f;
        for (int col = 0; col < size.w; ++col) {
            float* v = reinterpret_cast<float*>(reinterpret_cast<char*>(positions.data)
                                                + (col + row * size.w) * positions.stride);
            v[0] = (x1 - x0) * fx + x0;
            v[1] = (y1 - y0) * fx + y0;
            v[2] = (z1 - z0) * fx + z0;
            if (normals.data) {
                float* n = reinterpret_cast<float*>(reinterpret_cast<char*>(normals.data)
                                                    + (col + row * size.w) * normals.stride);
                n[0] = 0.0f;
                n[1] = 0.0f;
                n[2] = 1.0f;
            }
            if (tex0.data) {
                float* t = reinterpret_cast<float*>(reinterpret_cast<char*>(tex0.data)
                                                    + (col + row * size.w) * tex0.stride);
                t[0] = (u1 - u0) * fx + u0;
                t[1] = (v1 - v0) * fx + v0;
                if (tex1.data) {
                    float* t2 = reinterpret_cast<float*>(reinterpret_cast<char*>(tex1.data)
                                                         + (col + row * size.w) * tex1.stride);
                    t2[0] = (u1 - u0) * fx + u0;
                    t2[1] = (v1 - v0) * fx + v0;
                }
            }
            fx += 1.0f / static_cast<float>(size.w - 1);
        }
        fy += 1.0f / static_cast<float>(size.h - 1);
    }

    // the triangle pairs per cell
    unsigned short* indices = chunk->GetIndexArray();
    int v = 0;
    for (int row = 0; row < size.h - 1; ++row) {
        for (int col = 0; col < size.w - 1; ++col) {
            *indices++ = static_cast<unsigned short>(v);
            *indices++ = static_cast<unsigned short>(v + 1);
            *indices++ = static_cast<unsigned short>(v + size.w + 1);
            *indices++ = static_cast<unsigned short>(v);
            *indices++ = static_cast<unsigned short>(v + size.w + 1);
            *indices++ = static_cast<unsigned short>(v + size.w);
            ++v;
        }
        ++v;
    }

    VCall<void>(chunk, 0x10, *reinterpret_cast<void**>(reinterpret_cast<char*>(kernel) + 124), 1);
    return true;
}

// 0x10007cb0: the simple form builds the four-corner record and calls the full form
bool WaterGeometry::Initialize(RenderKernel* kernel, WaterMaterial* material_, const Size& size_,
                               const Vector3& scale_, const Vector2& texScale, bool waveVertexProgram)
{
    scale = scale_;
    PackedVertexPCT corners[4];
    corners[0].position = Vector3(0.0f, 0.0f, 0.0f);
    corners[0].color = ~0u;
    corners[0].texCoord = Vector2(0.0f, 0.0f);
    corners[1].position = Vector3(static_cast<float>(size_.w - 1) * scale_.x, 0.0f, 0.0f);
    corners[1].color = ~0u;
    corners[1].texCoord = Vector2(texScale.x, 0.0f);
    corners[2].position = Vector3(0.0f, static_cast<float>(size_.h - 1) * scale_.y, 0.0f);
    corners[2].color = ~0u;
    corners[2].texCoord = Vector2(0.0f, texScale.y);
    corners[3].position = Vector3(static_cast<float>(size_.w - 1) * scale_.x,
                                  static_cast<float>(size_.h - 1) * scale_.y, 0.0f);
    corners[3].color = ~0u;
    corners[3].texCoord = Vector2(texScale.x, texScale.y);
    return Initialize(kernel, material_, size_, corners, waveVertexProgram);
}

// 0x10007c80
void WaterGeometry::Render(Display* display, ViewPort& viewport, const DisplayLightArray* lights,
                           const Matrix4& world)
{
    MaterialRenderChunk(material, display, viewport, chunk, lights, world);
}

// 0x100091d0 (shared no-op body)
void WaterGeometry::Update(float time)
{
}

GeometryChunk* WaterGeometry::GetChunk()
{
    return chunk;
}

WaterMaterial* WaterGeometry::GetMaterial()
{
    return material;
}

const Size& WaterGeometry::GetSize() const
{
    return size;
}

Vector3 WaterGeometry::GetScale() const
{
    return scale;
}

void WaterGeometry::GetBoundingBox(Vector3& min, Vector3& max) const
{
    min = boundsMin;
    max = boundsMax;
}

float WaterGeometry::GetRadius() const
{
    return radius;
}

// ---------------------------------------------------------------------------------------------------------------
// PhysicsWaterGeometry

// 0x100022e0
PhysicsWaterGeometry::PhysicsWaterGeometry() : buffers{nullptr, nullptr, nullptr}, currentBuffer(0)
{
}

// 0x10002500
PhysicsWaterGeometry::~PhysicsWaterGeometry()
{
    for (float* b : buffers)
        if (b)
            Alloc::Delete(b);
}

// 0x10002330
bool PhysicsWaterGeometry::Initialize(RenderKernel* kernel, WaterMaterial* material_, const Size& size_,
                                      const Vector3& scale_, const Vector2& texScale, float waveSpeed,
                                      float viscosity, float deltaTime, bool staticDisplacement_,
                                      float xSpeed_, float ySpeed_, float maxHeight_)
{
    gridSize = size_.w;
    gridSizeMinus1 = size_.w - 1;
    cellSize = static_cast<float>(size_.w) * scale_.x;
    staticDisplacement = static_cast<float>(staticDisplacement_);
    xSpeed = xSpeed_;
    ySpeed = ySpeed_;
    material = material_;
    maxHeight = maxHeight_;
    for (float*& b : buffers) {
        b = static_cast<float*>(Alloc::New(GridNewBytes(size_.w * size_.w, sizeof(float))));
        std::memset(b, 0, 4 * size_.w * size_.w);
    }
    currentBuffer = 0;
    double v16 = static_cast<double>(waveSpeed) * waveSpeed * viscosity * viscosity
                 / (static_cast<double>(scale_.x) * scale_.x);
    double v17 = 1.0 / (static_cast<double>(waveSpeed) * deltaTime + 2.0);
    wave3 = static_cast<float>(v17 * v16) * 2.0f;
    wave1 = static_cast<float>(4.0 - v16 * 8.0) * static_cast<float>(v17);
    wave2 = static_cast<float>(static_cast<double>(waveSpeed) * deltaTime - 2.0) * static_cast<float>(v17);
    return WaterGeometry::Initialize(kernel, material_, size_, scale_, texScale, maxHeight_ > 0.0f);
}

// 0x10002540: one step of the height field; the static flavour adds the cos wave terms
void PhysicsWaterGeometry::Update(float time)
{
    PNoise noise;  // built by the original, never used in its flow
    gStaticTime += time;
    ItemArray positions;
    VCall<void>(chunk, 0xc4, &positions, 1, 0);
    ItemArray a1;
    VCall<void>(chunk, 0xc4, &a1, 24, 0);
    ItemArray a2;
    VCall<void>(chunk, 0xc4, &a2, 25, 0);
    float* cur = buffers[currentBuffer];
    float* next = buffers[1 - currentBuffer];
    if (staticDisplacement == 0.0f) {
        for (int row = 1; row < gridSizeMinus1; ++row) {
            int base = row * gridSize;
            for (int col = 1; col < gridSizeMinus1; ++col) {
                float v = (cur[base + col - gridSize] + cur[base + col + gridSize] + cur[base + col - 1]
                           + cur[base + col + 1]) * wave3
                          + cur[base + col] * wave1 + next[base + col] * wave2;
                next[base + col] = v;
                *reinterpret_cast<float*>(reinterpret_cast<char*>(positions.data)
                                          + (base + col) * positions.stride + 8) = v;
                if (a2.data && a1.data) {
                    *reinterpret_cast<float*>(reinterpret_cast<char*>(a2.data)
                                              + (base + col) * a2.stride + 8) = next[base + col + gridSize]
                                                                                 - next[base + col];
                    *reinterpret_cast<float*>(reinterpret_cast<char*>(a1.data)
                                              + (base + col) * a1.stride + 8) = next[base + col + 1]
                                                                                 - next[base + col];
                }
            }
        }
    } else {
        double t = gStaticTime;
        for (int row = 1; row < gridSizeMinus1; ++row) {
            int base = row * gridSize;
            float rf = static_cast<float>(row);
            for (int col = 1; col < gridSizeMinus1; ++col) {
                float fx = static_cast<float>(col) * xSpeed;
                float fy = rf * xSpeed;
                double fz = static_cast<double>(ySpeed) * t;
                double r = std::sqrt(static_cast<double>(fx * fx + fy * fy));
                float v = (cur[base + col - gridSize] + cur[base + col + gridSize] + cur[base + col - 1]
                           + cur[base + col + 1]) * wave3
                          + cur[base + col] * wave1 + next[base + col] * wave2;
                next[base + col] = v;
                float wave = static_cast<float>(std::cos(r * xSpeed + fz) * maxHeight
                                                + maxHeight * std::cos(static_cast<double>(fy) + fz)
                                                + std::cos(static_cast<double>(fx) + fz) * maxHeight);
                float h = v + wave;
                *reinterpret_cast<float*>(reinterpret_cast<char*>(positions.data)
                                          + (base + col) * positions.stride + 8) = h;
                if (a2.data && a1.data) {
                    *reinterpret_cast<float*>(reinterpret_cast<char*>(a2.data)
                                              + (base + col) * a2.stride + 8) = next[base + col + gridSize] - h;
                    *reinterpret_cast<float*>(reinterpret_cast<char*>(a1.data)
                                              + (base + col) * a1.stride + 8) = next[base + col + 1]
                                                                                 - next[base + col];
                }
            }
        }
    }
    VCall<void>(chunk, 0x7c, 1, 0, -2);
    currentBuffer = 1 - currentBuffer;
}

// 0x100022a0
void PhysicsWaterGeometry::Displace(int x, int y, float amount)
{
    if (x >= 0 && x < gridSize && y >= 0 && y < gridSize)
        buffers[currentBuffer][x + y * gridSize] = amount;
}

// 0x10002920: overlay a displacement hull's heights (negative only)
void PhysicsWaterGeometry::Displace(DisplacementHull* hull, int x, int y, float amount)
{
    // the original reads the hull's grid fields raw (WaterGrid: width +4, height +8, data +12)
    int hw = *reinterpret_cast<int*>(reinterpret_cast<char*>(hull) + 4);
    int hh = *reinterpret_cast<int*>(reinterpret_cast<char*>(hull) + 8);
    float* data = *reinterpret_cast<float**>(reinterpret_cast<char*>(hull) + 12);
    int row = y - hh / 2;
    for (int r = 0; r < hh; ++r, ++row) {
        int col = x - hw / 2;
        for (int c = 0; c < hw; ++c, ++col) {
            if (col >= 0 && col < gridSize && row >= 0 && row < gridSize) {
                float h = -(data[c + r * hw] * amount);
                if (h < 0.0f)
                    buffers[currentBuffer][col + row * gridSize] = h;
            }
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------
// WaterGeometryAnimation

// 0x10007dc0
WaterGeometryAnimation::WaterGeometryAnimation() : geometry(nullptr), currentFrame(0), width(0), height(0),
    scale(0.1f)
{
}

// 0x10007de0
WaterGeometryAnimation::~WaterGeometryAnimation()
{
    Destroy();
}

// 0x10007ea0: drop the frames, the xy frames and the geometry
void WaterGeometryAnimation::Destroy()
{
    for (auto i = frames.First(); i != frames.End(); ++i)
        if (*i)
            delete *i;
    frames.RemoveAll();
    for (auto i = xyFrames.First(); i != xyFrames.End(); ++i)
        if (*i)
            delete *i;
    xyFrames.RemoveAll();
    if (geometry)
        delete geometry;
}

WaterGeometry* WaterGeometryAnimation::GetGeometry()
{
    return geometry;
}

WaterGrid<float>* WaterGeometryAnimation::GetFrame(int frame)
{
    return frames[frame];
}

// 0x10008320: one geometry, the animation frames and the xy (normal) frames
bool WaterGeometryAnimation::Create(RenderKernel* kernel, WaterMaterial* material_, FrameAnimation* frameAnimation,
                                    WaterAnimation* animation, const Size& size_, const Vector3& scale_,
                                    const Vector2& texScale)
{
    width = size_.w;
    height = size_.h;
    geometry = new WaterGeometry();
    if (!geometry->Initialize(kernel, material_, size_, scale_, texScale, true)) {
        Destroy();
        return false;
    }
    int srcW = animation->GetSize().w;
    int srcH = animation->GetSize().h;
    int sx = srcW / (size_.w - 1);
    int sy = srcH / (size_.h - 1);
    for (int frame = 0; frame < frameAnimation->GetNumFrames(); ++frame) {
        WaterGrid<float>* heights = VCall<WaterGrid<float>*>(animation, 4, frame);   // the animation's slot 1
        WaterGrid<Vector2>* xy = VCall<WaterGrid<Vector2>*>(animation, 8, frame);    // its slot 2
        WaterGrid<float>* f = new WaterGrid<float>(size_);
        frames.Add(f);
        WaterGrid<Vector2>* g = nullptr;
        if (xy) {
            g = new WaterGrid<Vector2>(size_);
            xyFrames.Add(g);
        }
        for (int row = 0; row < size_.h; ++row) {
            for (int col = 0; col < size_.w; ++col) {
                int hx = (col * sx) % srcW;
                int hy = (row * sy) % srcH;
                if (xy) {
                    const Vector2& v = xy->Get(hx, hy);
                    g->Set(col, row, Vector2(col * scale_.x + scale * v.x, row * scale_.y + scale * v.y));
                }
                f->Set(col, row, heights->Get(hx, hy) * scale_.z);
            }
        }
    }
    return true;
}

// 0x100080a0: morph the chunk between the current and the next frame
void WaterGeometryAnimation::Update(FrameAnimation* frameAnimation)
{
    if (currentFrame == frameAnimation->GetCurrentFrame())
        return;
    ItemArray positions;
    VCall<void>(geometry->GetChunk(), 0xc4, &positions, 1, 0);
    ItemArray a1;
    VCall<void>(geometry->GetChunk(), 0xc4, &a1, 24, 0);
    ItemArray a2;
    VCall<void>(geometry->GetChunk(), 0xc4, &a2, 25, 0);
    float* cur = frames[frameAnimation->GetCurrentFrame()]->GetData();
    float* nxt = frames[(frameAnimation->GetCurrentFrame() + 1) % frameAnimation->GetNumFrames()]->GetData();
    Vector2* curXY = xyFrames[frameAnimation->GetCurrentFrame()]
                         ? xyFrames[frameAnimation->GetCurrentFrame()]->GetData() : nullptr;
    float frac = frameAnimation->GetDeltaFrame();
    for (int row = 0; row < height; ++row) {
        float* rowCur = cur + row * width;
        float* rowNxt = nxt + row * width;
        const float* bottom = rowCur + width;  // the last row reads one row past the end, the original's quirk
        for (int col = 0; col < width; ++col) {
            float* v = reinterpret_cast<float*>(reinterpret_cast<char*>(positions.data)
                                                + (col + row * width) * positions.stride);
            v[2] = (rowNxt[col] - rowCur[col]) * frac + rowCur[col];
            if (curXY) {
                v[0] = curXY[col + row * width].x;
                v[1] = curXY[col + row * width].y;
            }
            if (a2.data && a1.data) {
                *reinterpret_cast<float*>(reinterpret_cast<char*>(a2.data)
                                          + (col + row * width) * a2.stride + 8) = *bottom - rowCur[col];
                const float* right = col == width - 2 ? rowCur + 1 : rowCur + col + 1;
                *reinterpret_cast<float*>(reinterpret_cast<char*>(a1.data)
                                          + (col + row * width) * a1.stride + 8) = *right - rowCur[col];
            }
        }
    }
    VCall<void>(geometry->GetChunk(), 0x7c, 1, 0, -2);
    currentFrame = frameAnimation->GetCurrentFrame();
}

// ---------------------------------------------------------------------------------------------------------------
// WaterGeometryVS

// 0x100088e0
WaterGeometryVS::WaterGeometryVS() : frequency(0.0f), material(nullptr)
{
}

// 0x100091c0
WaterGeometryVS::~WaterGeometryVS() = default;

// 0x10008930: the base grid plus the vertex-program attributes (cos/sin wave fields on the border, rand inside)
bool WaterGeometryVS::Initialize(RenderKernel* kernel, WaterMaterial* material_, const Size& size_,
                                 const PackedVertexPCT* corners, const Vector3& origin, float frequency_,
                                 float amplitude)
{
    PNoise noise;  // built by the original, never used in its flow
    material = material_;
    frequency = frequency_;
    scale = Vector3(static_cast<float>(std::fabs(static_cast<double>(corners[3].position.y)
                                                 - corners[0].position.y) / size_.w),
                    static_cast<float>(std::fabs(static_cast<double>(corners[1].position.x)
                                                 - corners[0].position.x) / size_.h),
                    0.0f);
    if (!WaterGeometry::Initialize(kernel, material_, size_, corners,
                                   amplitude > 0.00000011920929f && frequency_ > 0.00000011920929f))
        return false;
    ItemArray positions;
    VCall<void>(chunk, 0xc4, &positions, 1, 0);
    ItemArray a1;
    VCall<void>(chunk, 0xbc, &a1, 24, 0);
    ItemArray a2;
    VCall<void>(chunk, 0xcc, &a2, 25, 0);
    if (a1.data && a2.data) {
        double amp = amplitude;
        auto Wave = [&](int i, float* v) {
            float y = v[1] + origin.y;
            float x = (v[0] + origin.x) * frequency_;
            *reinterpret_cast<float*>(reinterpret_cast<char*>(a1.data) + i * a1.stride) =
                static_cast<float>((std::cos(x) - std::sin(y * frequency_)) * amp);
            *reinterpret_cast<float*>(reinterpret_cast<char*>(a1.data) + i * a1.stride + 4) =
                static_cast<float>((std::cos(y * frequency_) + std::sin(x)) * amp);
            *reinterpret_cast<float*>(reinterpret_cast<char*>(a2.data) + i * a2.stride) = 0.0f;
            *reinterpret_cast<float*>(reinterpret_cast<char*>(a2.data) + i * a2.stride + 4) = 0.0f;
            *reinterpret_cast<float*>(reinterpret_cast<char*>(a2.data) + i * a2.stride + 8) = 0.0f;
            *reinterpret_cast<float*>(reinterpret_cast<char*>(a2.data) + i * a2.stride + 12) = 0.0f;
        };
        // the first row, the last row, the left and right columns
        for (int i = 0; i < size_.w; ++i) {
            Wave(i, reinterpret_cast<float*>(reinterpret_cast<char*>(positions.data) + i * positions.stride));
            float* v = reinterpret_cast<float*>(reinterpret_cast<char*>(positions.data)
                                                + (size_.w * (size_.h - 1) + i) * positions.stride);
            Wave(size_.w * (size_.h - 1) + i, v);
        }
        for (int row = 1; row < size_.h - 1; ++row) {
            Wave(row * size_.w, reinterpret_cast<float*>(reinterpret_cast<char*>(positions.data)
                                                         + row * size_.w * positions.stride));
            Wave(row * size_.w + size_.w - 1,
                 reinterpret_cast<float*>(reinterpret_cast<char*>(positions.data)
                                          + (row * size_.w + size_.w - 1) * positions.stride));
        }
        // the interior: random wave terms (rand()/32768), then their gradients
        for (int row = 1; row < size_.h - 1; ++row) {
            for (int col = 1; col < size_.w - 1; ++col) {
                float* p = reinterpret_cast<float*>(reinterpret_cast<char*>(positions.data)
                                                    + (row * size_.w + col) * positions.stride);
                float x = origin.x + p[0];
                float y = p[1] + origin.y;
                float r1 = static_cast<float>(rand()) * 0.000030518509f * amplitude;
                float r2 = static_cast<float>(rand()) * 0.000030518509f * frequency_;
                float r3 = static_cast<float>(rand()) * 0.000030518509f * amplitude;
                float r4 = static_cast<float>(rand()) * 0.000030518509f * frequency_;
                float* v = reinterpret_cast<float*>(reinterpret_cast<char*>(a1.data)
                                                    + (row * size_.w + col) * a1.stride);
                v[0] = std::cos(r2 * x) * r1 - std::sin(r4 * y) * r3;
                v[1] = r1 * std::sin(r2 * x) - r3 * std::cos(r4 * y);
            }
        }
        for (int row = 1; row < size_.h - 1; ++row) {
            for (int col = 1; col < size_.w - 1; ++col) {
                int i = row * size_.w + col;
                float* v = reinterpret_cast<float*>(reinterpret_cast<char*>(a2.data) + i * a2.stride);
                v[0] = *reinterpret_cast<float*>(reinterpret_cast<char*>(a1.data) + (i + 1) * a1.stride)
                       - *reinterpret_cast<float*>(reinterpret_cast<char*>(a1.data) + (i - 1) * a1.stride);
                v[1] = *reinterpret_cast<float*>(reinterpret_cast<char*>(a1.data) + (i + 1) * a1.stride + 4)
                       - *reinterpret_cast<float*>(reinterpret_cast<char*>(a1.data) + (i - 1) * a1.stride + 4);
                v[2] = *reinterpret_cast<float*>(reinterpret_cast<char*>(a1.data) + (i + size_.w) * a1.stride)
                       - *reinterpret_cast<float*>(reinterpret_cast<char*>(a1.data) + (i - size_.w) * a1.stride);
                v[3] = *reinterpret_cast<float*>(reinterpret_cast<char*>(a1.data) + (i + size_.w) * a1.stride + 4)
                       - *reinterpret_cast<float*>(reinterpret_cast<char*>(a1.data) + (i - size_.w) * a1.stride + 4);
            }
        }
    }
    // the normals: all (0,0,1)
    ItemArray normals;
    VCall<void>(chunk, 0xc4, &normals, 4, 0);
    if (normals.data) {
        int count = *reinterpret_cast<int*>(reinterpret_cast<char*>(chunk) + 36);
        for (int i = 0; i < count; ++i) {
            float* n = reinterpret_cast<float*>(reinterpret_cast<char*>(normals.data) + i * normals.stride);
            n[0] = 0.0f;
            n[1] = 0.0f;
            n[2] = 1.0f;
        }
    }
    VCall<void>(chunk, 0x7c, 1, 0, -2);
    return true;
}

// 0x10009090: the simple form builds the record and calls the full vertex-program form
bool WaterGeometryVS::Initialize(RenderKernel* kernel, WaterMaterial* material_, const Size& size_,
                                 const Vector3& scale_, const Vector2& texScale, const Vector3& origin,
                                 float frequency_, float amplitude)
{
    scale = scale_;
    PackedVertexPCT corners[4];
    corners[0].position = Vector3(0.0f, 0.0f, 0.0f);
    corners[0].color = ~0u;
    corners[0].texCoord = Vector2(0.0f, 0.0f);
    corners[1].position = Vector3(static_cast<float>(size_.w - 1) * scale_.x, 0.0f, 0.0f);
    corners[1].color = ~0u;
    corners[1].texCoord = Vector2(texScale.x, 0.0f);
    corners[2].position = Vector3(0.0f, static_cast<float>(size_.h - 1) * scale_.y, 0.0f);
    corners[2].color = ~0u;
    corners[2].texCoord = Vector2(0.0f, texScale.y);
    corners[3].position = Vector3(static_cast<float>(size_.w - 1) * scale_.x,
                                  static_cast<float>(size_.h - 1) * scale_.y, 0.0f);
    corners[3].color = ~0u;
    corners[3].texCoord = Vector2(texScale.x, texScale.y);
    return Initialize(kernel, material_, size_, corners, origin, frequency_, amplitude);
}

// 0x100091e0
void WaterGeometryVS::Render(Display* display, ViewPort& viewport, const DisplayLightArray* lights,
                             const Matrix4& world)
{
    MaterialRenderChunk(material, display, viewport, chunk, lights, world);
}

}  // namespace Jet
