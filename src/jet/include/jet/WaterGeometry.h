// WaterSystem_7c unit U5 (notes/WaterSystem.md): the water's render geometry. WaterGeometry is the base chunk
// holder; PhysicsWaterGeometry adds the height-field simulation; WaterGeometryAnimation morphs between the
// animation frames; WaterGeometryVS fills the vertex-program attributes (cos/sin waves, rand-based interior).
// Layouts and behaviour follow the TS12 assembly (addresses per function in WaterGeometry.cpp); the GeometryChunk
// and Display calls go through the original's vftables (RenderSystemShell.h).
#pragma once

#include <jet/FastDynamicArray.h>
#include <jet/KernelStandard.h>  // ReferenceCount
#include <jet/Math.h>
#include <jet/Platform.h>
#include <jet/WaterGrid.h>

namespace Jet {

class DisplacementHull;
class Display;
class DisplayLight;
class FrameAnimation;
class GeometryChunk;
class RenderKernel;
class ViewPort;
class WaterAnimation;
class WaterMaterial;
struct PackedVertexPCT;

using DisplayLightArray = FastDynamicArray<DisplayLight*, 13, Less<DisplayLight*, DisplayLight*>>;

class WaterGeometry : public ReferenceCount {
public:
    WaterGeometry();
    virtual ~WaterGeometry();

    // vtable slots 2..5, in declaration order as in the original
    virtual bool __cdecl Initialize(RenderKernel* kernel, WaterMaterial* material, const Size& size,
                                    const Vector3& scale, const Vector2& texScale, bool waveVertexProgram);
    virtual bool __cdecl Initialize(RenderKernel* kernel, WaterMaterial* material, const Size& size,
                                    const PackedVertexPCT* corners, bool waveVertexProgram);
    virtual void __cdecl Render(Display* display, ViewPort& viewport, const DisplayLightArray* lights,
                                const Matrix4& world);
    virtual void __cdecl Update(float time);  // a no-op in the original

    GeometryChunk* __cdecl GetChunk();     // +0x08
    WaterMaterial* __cdecl GetMaterial();  // +0x0c
    const Size& __cdecl GetSize() const;   // +0x10
    Vector3 __cdecl GetScale() const;      // +0x18, by value
    void __cdecl GetBoundingBox(Vector3& min, Vector3& max) const;  // +0x24, +0x30
    float __cdecl GetRadius() const;       // +0x3c

protected:
    virtual void __cdecl Destroy();  // slot 6: releases the chunk

    GeometryChunk* chunk;      // +0x08
    WaterMaterial* material;   // +0x0c
    Size size;                 // +0x10
    Vector3 scale;             // +0x18
    Vector3 boundsMin;         // +0x24
    Vector3 boundsMax;         // +0x30
    float radius;              // +0x3c
};
JET_LAYOUT32(sizeof(WaterGeometry) == 0x40);

class PhysicsWaterGeometry : public WaterGeometry {
public:
    PhysicsWaterGeometry();
    virtual ~PhysicsWaterGeometry();

    virtual bool __cdecl Initialize(RenderKernel* kernel, WaterMaterial* material, const Size& size,
                                    const Vector3& scale, const Vector2& texScale, float waveSpeed, float viscosity,
                                    float deltaTime, bool staticDisplacement, float xSpeed, float ySpeed,
                                    float maxHeight);
    virtual void __cdecl Update(float time);
    virtual void __cdecl Displace(int x, int y, float amount);
    void __cdecl Displace(DisplacementHull* hull, int x, int y, float amount);

    float* buffers[3];        // +0x40  the ping-ponged height fields (w*w floats)
    int currentBuffer;        // +0x4c
    int gridSize;             // +0x50  size.w
    int gridSizeMinus1;       // +0x54
    float cellSize;           // +0x58  size.w * scale.x
    float wave1;              // +0x60  (4 - v16*8) * v17
    float wave2;              // +0x64  (waveSpeed * deltaTime - 2) * v17
    float wave3;              // +0x68  (v17 * v16) * 2
    float staticDisplacement; // +0x6c  the bool as float
    float xSpeed;             // +0x70
    float ySpeed;             // +0x74
    float maxHeight;          // +0x78
};
JET_LAYOUT32(sizeof(PhysicsWaterGeometry) == 0x7c);

class WaterGeometryAnimation {
public:
    WaterGeometryAnimation();
    ~WaterGeometryAnimation();

    WaterGeometry* __cdecl GetGeometry();       // +0x00
    WaterGrid<float>* __cdecl GetFrame(int frame);
    bool __cdecl Create(RenderKernel* kernel, WaterMaterial* material, FrameAnimation* frameAnimation,
                        WaterAnimation* animation, const Size& size, const Vector3& scale, const Vector2& texScale);
    void __cdecl Update(FrameAnimation* frameAnimation);
    void __cdecl Destroy();

    WaterGeometry* geometry;                       // +0x00
    FastDynamicArray<WaterGrid<float>*, 13> frames;    // +0x04
    FastDynamicArray<WaterGrid<Vector2>*, 13> xyFrames;  // +0x08
    int currentFrame;                              // +0x0c
    int width;                                     // +0x10
    int height;                                    // +0x14
    float scale;                                   // +0x18  0.1
};
JET_LAYOUT32(sizeof(WaterGeometryAnimation) == 0x1c);

class WaterGeometryVS : public WaterGeometry {
public:
    WaterGeometryVS();
    virtual ~WaterGeometryVS();

    // the vertex-program variants are new virtuals (different signatures), after the inherited ones
    virtual bool __cdecl Initialize(RenderKernel* kernel, WaterMaterial* material, const Size& size,
                                    const PackedVertexPCT* corners, const Vector3& origin, float frequency,
                                    float amplitude);
    virtual bool __cdecl Initialize(RenderKernel* kernel, WaterMaterial* material, const Size& size,
                                    const Vector3& scale, const Vector2& texScale, const Vector3& origin,
                                    float frequency, float amplitude);
    virtual void __cdecl Render(Display* display, ViewPort& viewport, const DisplayLightArray* lights,
                                const Matrix4& world);

    float frequency;           // +0x40
    WaterMaterial* material;   // +0x44
};
JET_LAYOUT32(sizeof(WaterGeometryVS) == 0x48);

}  // namespace Jet
