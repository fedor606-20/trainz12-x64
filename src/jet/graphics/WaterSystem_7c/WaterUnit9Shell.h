// WaterSystem_7c unit U9's private view of the Render3DCore_7c types this unit reaches by name:
// Camera::Clip3D and ViewPoint::Add, plus the ParticleVisibleBoundingBox constructor WaterTiled uses to build
// its particle's visible volume. The methods are imported from Render3DCore_7c.dll; only the fields this unit
// reads are declared. Move these to the Render3DCore headers when those classes are reconstructed.
#pragma once

#include <jet/Math.h>
#include <jet/Render3DCorePlane.h>  // ClipResult
#include <jet/RenderTypes.h>        // Element

namespace Jet {

class Particle;

class Camera {
public:
    ClipResult __cdecl Clip3D(const Vector3& position, float radius) const;
    ClipResult __cdecl Clip3D(const Vector3* positions, int count) const;
};

class ViewPoint {
public:
    void __cdecl Add(Element* element, const Matrix4& matrix);

    unsigned char unknown0[4];  // +0x00
    Camera* camera;             // +0x04
};

class ParticleVisibleBoundingBox {
public:
    ParticleVisibleBoundingBox(Particle* particle);
};

}  // namespace Jet
