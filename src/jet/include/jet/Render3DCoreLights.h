// Render3DCore_7c unit U7: the light scene nodes. Layout and behaviour follow the TS12 assembly
// (addresses per member in Light.cpp, PointLights.cpp, CubeMapLight.cpp).
//
//   DisplayLight           +0x000 (0x50: vftable, type, ambient, diffuse, specular, attenuation, spot, range)
//   Particle               +0x050 (0x12c)
//   Light::lightingBegin   +0x17c (byte, 1 between SetLightingBegin and BatchCalculateDiffuse; 0 after InitInstance)
//
// Sizes: Light 0x180, AmbientLight 0x180, OmniLight 0x18c, ParallelLight 0x1a8, SpotLight 0x1b4,
// CubeMapLight 0x1ac. The vtables have 10 primary slots (Light: DisplayLight's 8 + SetLightingBegin,
// operator==(Light)); OmniLight adds operator==(OmniLight) as slot 10 and CubeMapLight adds
// Rotate(Quaternion), Rotate(Matrix3), SelfRotate(Quaternion), SelfRotate(Matrix3) as slots 10..13.
//
// DisplayLight's slots 2..5 are the light position/direction accessors. RenderSystem's reconstruction names
// them Slot2..Slot5, so the overrides here are Slot2..Slot5 as well, and the exported Set/GetLightPosition /
// Set/GetLightDirection names are produced as aliases of the plain methods (implemented.d `name=symbol`).
// All virtuals are __cdecl (this on the stack); constructors and the derived assignments are __thiscall, while
// Light's own assignment is __cdecl, as the mangled names say.
//
// The BatchCalculateDiffuse overrides call the original method through Render3DCore_7c_orig.dll; they need the
// display, material and vertex-item units. CubeMapLight's is the shared empty function.
#pragma once

#include <jet/InputSystem.h>
#include <jet/Render3DCoreParticle.h>
#include <jet/RenderTypes.h>
#include <jet/Texture.h>

namespace Jet {

class Material;
class VIPConstHelper;

using LightVertexPtr = VertexItemPtr<Vector3, VertexItemPtr<const Vector3, VIPConstHelper>>;
using LightColorPtr = VertexItemPtr<unsigned long, VertexItemPtr<const unsigned long, VIPConstHelper>>;

class Light : public DisplayLight, public Particle {
public:
    Light(DLightTypes type);
    Light(DLightTypes type, const Vector3& position, float radius);
    Light(const Light& o);
    virtual ~Light();
    Light& __cdecl operator=(const Light& o);

    virtual int __cdecl GetID() const;
    virtual bool __cdecl IsOfType(int id) const;
    virtual void __cdecl SetLightingBegin();
    virtual bool __cdecl operator==(const Light& o) const;

    // the plain accessors; the virtual exports (UAA/UBA) alias these (QAA/QBA)
    void __cdecl SetLightPosition(const Vector3& position);
    const Vector3& __cdecl GetLightPosition() const;

    // DisplayLight slots 2 (SetLightPosition) and 3 (GetLightPosition)
    virtual int __cdecl Slot2() const override;
    virtual int __cdecl Slot3() const override;
    // slots 4/5 are the shared empty stubs in the original Light vtable
    virtual int __cdecl Slot4() const override;
    virtual int __cdecl Slot5() const override;

    // DisplayLight and Element each declare operator new/delete; name them once here
    JET_HEAP_OBJECT

protected:
    void __cdecl InitInstance(Light& instance) const;
    void __cdecl AssignLightState(const Light& o);

    unsigned char lightingBegin;  // +0x17c
    unsigned char unused17d[3];
};
JET_LAYOUT32(sizeof(Light) == 0x180);

class AmbientLight : public Light {
public:
    AmbientLight();
    AmbientLight(const Vector3& position, float radius, const Color& color);
    AmbientLight(const AmbientLight& o);
    virtual ~AmbientLight();
    AmbientLight& operator=(const AmbientLight& o);

    virtual Particle* __cdecl CreateInstance() const;
    virtual int __cdecl GetID() const;
    virtual bool __cdecl IsOfType(int id) const;
    virtual void __cdecl BatchCalculateDiffuse(Display* display, int count, LightVertexPtr positions,
                                               LightVertexPtr normals, LightColorPtr colors, const Matrix4& world,
                                               const Matrix4& view, Material* material) const;

    // slot 4 (SetLightDirection) is the shared empty function, slot 5 (GetLightDirection) the static zero vector
    virtual int __cdecl Slot4() const override;
    virtual int __cdecl Slot5() const override;

    void __cdecl SetLightDirection(const Vector3& direction);  // empty, the virtual export aliases this
    const Vector3& __cdecl GetLightDirection() const;

    void __cdecl InitInstance(AmbientLight& instance) const;
};
JET_LAYOUT32(sizeof(AmbientLight) == 0x180);

class OmniLight : public Light {
public:
    OmniLight();
    OmniLight(const Vector3& position, float radius, const Color& color);
    OmniLight(const OmniLight& o);
    virtual ~OmniLight();
    OmniLight& operator=(const OmniLight& o);

    virtual Particle* __cdecl CreateInstance() const;
    virtual int __cdecl GetID() const;
    virtual bool __cdecl IsOfType(int id) const;
    virtual void __cdecl BatchCalculateDiffuse(Display* display, int count, LightVertexPtr positions,
                                               LightVertexPtr normals, LightColorPtr colors, const Matrix4& world,
                                               const Matrix4& view, Material* material) const;

    virtual int __cdecl Slot4() const override;
    virtual int __cdecl Slot5() const override;

    virtual bool __cdecl operator==(const OmniLight& o) const;  // primary slot 10

    void __cdecl SetLightDirection(const Vector3& direction);  // empty, the virtual export aliases this
    const Vector3& __cdecl GetLightDirection() const;

protected:
    void __cdecl InitInstance(OmniLight& instance) const;

    float unknown180;  // +0x180 the constant/linear/quadratic attenuation copies
    float unknown184;  // +0x184
    float unknown188;  // +0x188
};
JET_LAYOUT32(sizeof(OmniLight) == 0x18c);

class ParallelLight : public Light {
public:
    ParallelLight(DLightTypes type);
    ParallelLight(const Vector3& position, const Quaternion& orientation, float radius, const Color& color,
                  DLightTypes type);
    ParallelLight(const ParallelLight& o);
    virtual ~ParallelLight();
    ParallelLight& operator=(const ParallelLight& o);

    virtual Particle* __cdecl CreateInstance() const;
    virtual int __cdecl GetID() const;
    virtual bool __cdecl IsOfType(int id) const;
    virtual void __cdecl BatchCalculateDiffuse(Display* display, int count, LightVertexPtr positions,
                                               LightVertexPtr normals, LightColorPtr colors, const Matrix4& world,
                                               const Matrix4& view, Material* material) const;

    virtual int __cdecl Slot4() const override;
    virtual int __cdecl Slot5() const override;

    void __cdecl SetLightDirection(const Vector3& direction);
    const Vector3& __cdecl GetLightDirection() const;

protected:
    void __cdecl InitInstance(ParallelLight& instance) const;

    float unknown180;             // +0x180
    float unknown184;             // +0x184
    float unknown188;             // +0x188
    mutable Vector3 lightDirection;        // +0x18c
    mutable Quaternion globalOrientation;  // +0x198
};
JET_LAYOUT32(sizeof(ParallelLight) == 0x1a8);

class SpotLight : public ParallelLight {
public:
    SpotLight();
    SpotLight(const Vector3& position, const Quaternion& orientation, float radius, float cutoff, float exponent,
              const Color& color);
    SpotLight(const SpotLight& o);
    virtual ~SpotLight();
    SpotLight& operator=(const SpotLight& o);

    virtual Particle* __cdecl CreateInstance() const;
    virtual int __cdecl GetID() const;
    virtual bool __cdecl IsOfType(int id) const;
    virtual void __cdecl BatchCalculateDiffuse(Display* display, int count, LightVertexPtr positions,
                                               LightVertexPtr normals, LightColorPtr colors, const Matrix4& world,
                                               const Matrix4& view, Material* material) const;

protected:
    void __cdecl InitInstance(SpotLight& instance) const;

    float unknown1a8;  // +0x1a8
    float unknown1ac;  // +0x1ac
    float unknown1b0;  // +0x1b0
};
JET_LAYOUT32(sizeof(SpotLight) == 0x1b4);

class CubeMapLight : public Light {
public:
    CubeMapLight(const Vector3& position, const Quaternion& orientation, const AutoReference<Texture>& texture,
                 float radius, const Color& color);
    CubeMapLight(const CubeMapLight& o);
    virtual ~CubeMapLight();
    CubeMapLight& operator=(const CubeMapLight& o);

    virtual Particle* __cdecl CreateInstance() const;
    virtual int __cdecl GetID() const;
    virtual bool __cdecl IsOfType(int id) const;
    virtual void __cdecl BatchCalculateDiffuse(Display* display, int count, LightVertexPtr positions,
                                               LightVertexPtr normals, LightColorPtr colors, const Matrix4& world,
                                               const Matrix4& view, Material* material) const;

    // Particle overrides
    virtual void __cdecl SetOrientation(const Quaternion& orientation) override;
    virtual void __cdecl SetReferenceFrame(const Vector3& position, const Quaternion& orientation) override;

    // new virtuals, primary slots 10..13 in this order after MSVC's overload reversal
    virtual void __cdecl Rotate(const Matrix3& rotation);
    virtual void __cdecl Rotate(const Quaternion& rotation);
    virtual void __cdecl SelfRotate(const Matrix3& rotation);
    virtual void __cdecl SelfRotate(const Quaternion& rotation);

    virtual int __cdecl Slot4() const override;
    virtual int __cdecl Slot5() const override;

    void __cdecl SetLightDirection(const Vector3& direction);  // empty, the virtual export aliases this
    const Vector3& __cdecl GetLightDirection() const;

    void __cdecl SetTexture(const AutoReference<Texture>& texture);
    const AutoReference<Texture>& __cdecl GetTexture() const;
    void __cdecl SetLightMatrix(const Matrix3& matrix);
    const Matrix3& __cdecl GetLightMatrix() const;
    void __cdecl SetFrontFaceMode(bool frontFace);
    bool __cdecl GetFrontFaceMode();

    void __cdecl UpdateLightMatrix();

private:
    // the storage is an AutoReference (one pointer); kept raw so the destructor releases it exactly once
    Texture* texture;    // +0x180
    Matrix3 lightMatrix;  // +0x184 (0x24)
    bool frontFaceMode;   // +0x1a8
    unsigned char unused1a9[3];
};
JET_LAYOUT32(sizeof(CubeMapLight) == 0x1ac);

}  // namespace Jet
