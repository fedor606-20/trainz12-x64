// WaterSystem_7c U7b/U8a: the water material classes. The base class extends RenderSystem_7c's Material
// (declared in Render3DCoreMaterials.h with the TS12 layout and virtual order) and appends the water
// virtuals 23..47 exactly as the original vtable has them. The concrete materials append their own
// virtuals after slot 47.
//
// Slot map (from the original vtables at 0x1001e4b4 and the concrete tables):
//   0..22  Material (RenderSystem_7c)
//   23     Initialize(db, config, frameAnimation, textureAnimation, int)
//   24     SetReflectionType(int)
//   25     GetReflectionType() const
//   26     SetDetailAnim(bool)
//   27     GetDetailAnim() const
//   28     SetScaleNormXY(float)
//   29     GetScaleNormXY() const
//   30     Update(float)
//   31     SetRefractiveIndex(float)
//   32     SetWaterColor(const Color&)
//   33     GetWaterColor() const
//   34     SetAmbientColor(const Color&)
//   35     GetAmbientColor() const
//   36     SetLightDir(const Vector3&)
//   37     GetLightDir() const
//   38     SetLightColor(const Color&)
//   39     GetLightColor() const
//   40     SetUVScrollVelocity(Vector2)
//   41     GetUVScrollVelocity() const
//   42     EnableDetailAnimInterpolation(bool)
//   43     SetReflectEyeVector(bool)
//   44     IsReflectEyeVector() const
//   45     GetUseVertexProgram()
//   46     CreateGeometryChunk(numVertices, numPrimitives, numIndices, numFaceNormals, type, hint, bool)
//   47     InitInstance(WaterMaterial&) const
//   48..51 (WaterBumpMaterial / WaterOceanMaterial) CreateVertexType, SetupVertexProgramConstants,
//          SetDisplayState, ClearDisplayState
#pragma once

#include <jet/FastDynamicArray.h>
#include <jet/KernelStandard.h>
#include <jet/Math.h>
#include <jet/PString.h>
#include <jet/Render3DCoreMaterials.h>
#include <jet/RenderTypes.h>
#include <jet/UId.h>

namespace Jet {

class Camera;
class Display;
class FrameAnimation;
class GeometryChunk;
class GeometryChunkCreateHint;
class MaterialManager;
class Matrix4;
class ResourceDatabase;
class TextureCombiner;
class VertexProgram;
class VertexType;
class WaterManagerConfig;
class WaterReflectionCamera;
class WaterTextureAnimation;

using WaterDisplayLightArray = FastDynamicArray<DisplayLight*, 13, Less<DisplayLight*, DisplayLight*>>;

class WaterMaterial : public Material {
public:
    explicit WaterMaterial(MaterialManager* manager);
    virtual ~WaterMaterial();

    static const PString psWaterMatInterp;
    static const PString psWaterMatInterpFull;
    static const PString psWaterMatWaveAnim;
    static const PString psWaterMatWaveAnimRefl;
    static const PString psWaterMatWaveRefl;
    static const PString vsWaterMat;
    static const PString vsWaterMatAnimRefl;
    static const PString vsWaterMatRefl;
    static const PString vsWaterMatWave;
    static const PString vsWaterMatWaveAnim;
    static const PString vsWaterMatWaveAnimRefl;
    static const PString vsWaterMatWaveRefl;
    static const PString vsWaterTexInterp;

    // Material's pure virtuals, stubs in the original base vtable.
    virtual Material* __cdecl CreateInstance() const override;
    virtual bool __cdecl operator==(const Material& other) const override;
    virtual const PString& __cdecl GetType() const override;
    virtual void __cdecl Activate(int detail) override;
    virtual GeometryChunk* __cdecl CreateGeometryChunk(unsigned int numVertices, unsigned int numPrimitives,
                                                       unsigned int numIndices, unsigned int numFaceNormals,
                                                       GeometryType type,
                                                       GeometryChunkCreateHint* hint) const override;
    virtual Result __cdecl DuplicateTexture(int type) override;
    virtual Texture* __cdecl GetTexture(int type) override;
    virtual const Texture* __cdecl GetTexture(int type) const override;
    // slot 11 (GetNumPasses) is NOT overridden here: the original water vtables point at the base
    // Material::GetNumPasses imported from RenderSystem_7c (IAT thunk 0x1001799e), so the local
    // tables inherit the base entry the same way.
    virtual void __cdecl Bind(Display* display, ViewPort* view, unsigned int pass) override;
    virtual void __cdecl Unbind(Display* display, ViewPort* view, unsigned int pass) override;
    virtual void __cdecl CacheChunk(Display* display, ViewPort* view, GeometryChunk* chunk,
                                    const DisplayLightPtrList* lights, const Matrix4* modelView) const override;
    virtual void __cdecl RenderChunk(Display* display, ViewPort* view, GeometryChunk* chunk,
                                     const DisplayLightPtrList* lights, const Matrix4* modelView, int pass) const override;
    virtual void __cdecl RenderNumChunks(Display* display, ViewPort* view, GeometryChunk** chunks,
                                         const DisplayLightPtrList** lights, const Matrix4** modelViews,
                                         unsigned int numChunks, int pass) const override;
    virtual void __cdecl RenderPrimitives(Display* display, ViewPort* view, GeometryChunk* chunk,
                                          const DisplayLightPtrList* lights, const Matrix4* modelView,
                                          unsigned int numIndices, const unsigned short* indices,
                                          unsigned int numPrimitives, DPrimitive primitive, int pass) const override;

    // slots 23..47
    virtual bool Initialize(const ResourceDatabase* database, const WaterManagerConfig* config,
                            FrameAnimation* frameAnimation, WaterTextureAnimation* textureAnimation, int detail);
    virtual bool SetReflectionType(int type);
    virtual int GetReflectionType() const;
    virtual void SetDetailAnim(bool value);
    virtual bool GetDetailAnim() const;
    virtual void SetScaleNormXY(float value);
    virtual float GetScaleNormXY() const;
    virtual void Update(float time);
    virtual void SetRefractiveIndex(float value);
    virtual void SetWaterColor(const Color& color);
    virtual const Color& GetWaterColor() const;
    virtual void SetAmbientColor(const Color& color);
    virtual const Color& GetAmbientColor() const;
    virtual void SetLightDir(const Vector3& direction);
    virtual const Vector3& GetLightDir() const;
    virtual void SetLightColor(const Color& color);
    virtual const Color& GetLightColor() const;
    virtual void SetUVScrollVelocity(Vector2 velocity);
    virtual Vector2 GetUVScrollVelocity() const;
    virtual void EnableDetailAnimInterpolation(bool value);
    virtual void SetReflectEyeVector(bool value);
    virtual bool IsReflectEyeVector() const;
    virtual bool GetUseVertexProgram();
    virtual GeometryChunk* __cdecl CreateGeometryChunk(unsigned int numVertices, unsigned int numPrimitives,
                                               unsigned int numIndices, unsigned int numFaceNormals,
                                               GeometryType type, GeometryChunkCreateHint* hint,
                                               bool waveVertexProgram) const;
    virtual void InitInstance(WaterMaterial& instance) const;

    // public for the unit tests' layout checks
    void* renderCache;        // +0x9c  MaterialChunkCache
    void* alphaBlendCache;    // +0xa0  AlphaBlendCache
    Color waterColor;         // +0xa4
    Color lightColor;         // +0xb4
    Color ambientColor;       // +0xc4
    Vector3 lightDir;         // +0xd4
    bool reflectEyeVector;    // +0xe0
};

JET_LAYOUT32(sizeof(WaterMaterial) == 0xf0);
JET_LAYOUT32(offsetof(WaterMaterial, renderCache) == 0x9c && offsetof(WaterMaterial, waterColor) == 0xa4);
JET_LAYOUT32(offsetof(WaterMaterial, lightColor) == 0xb4 && offsetof(WaterMaterial, ambientColor) == 0xc4);
JET_LAYOUT32(offsetof(WaterMaterial, lightDir) == 0xd4 && offsetof(WaterMaterial, reflectEyeVector) == 0xe0);

// ---------------------------------------------------------------------------------------------------------------
// The two concrete materials. Both fill 0x138 / 0x174 bytes and append four virtuals after InitInstance.

class WaterBumpMaterial : public WaterMaterial {
public:
    explicit WaterBumpMaterial(MaterialManager* manager);
    ~WaterBumpMaterial() override;

    static const PString UNDECORATED_TYPE;  // "WaterBumpMaterial"
    static const PString DECORATED_TYPE;    // "m.WaterBumpMaterial"
    static const UId uid;                   // 5EA5C88C-A2F1-423A-B5AB-FFB935941E25

    virtual Material* __cdecl CreateInstance() const override;                          // 0
    virtual bool __cdecl operator==(const Material& other) const override;              // 1
    virtual void __cdecl SetResourcePriority(float priority) const override;            // 2
    virtual const PString& __cdecl GetType() const override;                            // 3
    virtual void __cdecl Activate(int detail) override;                                 // 4
    virtual GeometryChunk* __cdecl CreateGeometryChunk(unsigned int numVertices, unsigned int numPrimitives,
                                                       unsigned int numIndices, unsigned int numFaceNormals,
                                                       GeometryType type, GeometryChunkCreateHint* hint,
                                                       bool waveVertexProgram) const override;  // 46
    virtual Result __cdecl DuplicateTexture(int type) override;                         // 6
    virtual Texture* __cdecl GetTexture(int type) override;                             // 7
    virtual const Texture* __cdecl GetTexture(int type) const override;                 // 8
    virtual Result __cdecl SetTexture(int type, const ResourceDatabase* db, const PString& name, float amount,
                                      const String& request) override;                  // 9
    virtual Result __cdecl SetTexture(int type, const AutoReference<Texture>& texture, float amount) override;  // 10
    virtual void __cdecl Bind(Display* display, ViewPort* view, unsigned int pass) override;    // 12
    virtual void __cdecl Unbind(Display* display, ViewPort* view, unsigned int pass) override;  // 13
    virtual void __cdecl CacheChunk(Display* display, ViewPort* view, GeometryChunk* chunk,
                                    const DisplayLightPtrList* lights, const Matrix4* modelView) const override;  // 14
    virtual void __cdecl RenderChunk(Display* display, ViewPort* view, GeometryChunk* chunk,
                                     const DisplayLightPtrList* lights, const Matrix4* modelView, int pass) const override;  // 15
    virtual void __cdecl RenderNumChunks(Display* display, ViewPort* view, GeometryChunk** chunks,
                                         const DisplayLightPtrList** lights, const Matrix4** modelViews,
                                         unsigned int numChunks, int pass) const override;  // 16
    virtual void __cdecl RenderPrimitives(Display* display, ViewPort* view, GeometryChunk* chunk,
                                          const DisplayLightPtrList* lights, const Matrix4* modelView,
                                          unsigned int numIndices, const unsigned short* indices,
                                          unsigned int numPrimitives, DPrimitive primitive, int pass) const override;  // 17
    virtual Result __cdecl IsLoaded() const override;                                   // 22
    virtual bool Initialize(const ResourceDatabase* database, const WaterManagerConfig* config,
                            FrameAnimation* frameAnimation, WaterTextureAnimation* textureAnimation, int detail) override;  // 23
    virtual bool SetReflectionType(int type) override;                                  // 24
    virtual int GetReflectionType() const override;                                     // 25
    virtual void SetScaleNormXY(float value) override;                                  // 28
    virtual float GetScaleNormXY() const override;                                      // 29
    virtual void SetRefractiveIndex(float value) override;                              // 31

    void EnableTextureInterpolation(bool value);
    bool __cdecl Equals(const WaterBumpMaterial& other) const;

protected:
    // 48..51
    virtual void CreateVertexType();
    virtual void SetupVertexProgramConstants(Display* display, Camera* camera, const Matrix4* modelView) const;
    virtual void SetDisplayState(Display* display) const;
    virtual void ClearDisplayState(Display* display) const;
    void CalculateLerpTexture(Display* display) const;
    void RenderSquare(Display* display) const;
    void __cdecl InitInstance(WaterBumpMaterial& instance) const;

public:
    Texture* reflectionTexture;          // +0xe4
    VertexProgram* vertexProgram;        // +0xe8
    VertexProgram* interpProgram;        // +0xec
    TextureCombiner* combiner;           // +0xf0
    TextureCombiner* lerpCombiner;       // +0xf4
    void* renderTarget;                  // +0xf8 (DisplayTexture)
    float refractiveIndex;               // +0xfc (1.3333)
    Color color;                         // +0x100
    VertexType* vertexType;              // +0x110
    WaterTextureAnimation* textureAnimation;  // +0x114
    FrameAnimation* frameAnimation;      // +0x118
    bool enableTextureInterpolation;     // +0x11c
    bool enableRefraction;               // +0x11d
    float uvScrollX;                     // +0x120
    float uvScrollY;                     // +0x124
    float scaleNormXY;                   // +0x128 (1.0)
};

JET_LAYOUT32(sizeof(WaterBumpMaterial) == 0x138);
JET_LAYOUT32(offsetof(WaterBumpMaterial, reflectionTexture) == 0xe4 && offsetof(WaterBumpMaterial, vertexType) == 0x110);
JET_LAYOUT32(offsetof(WaterBumpMaterial, enableTextureInterpolation) == 0x11c);
JET_LAYOUT32(offsetof(WaterBumpMaterial, scaleNormXY) == 0x128);

class WaterOceanMaterial : public WaterMaterial {
public:
    explicit WaterOceanMaterial(MaterialManager* manager);
    ~WaterOceanMaterial() override;

    static const PString UNDECORATED_TYPE;  // "WaterOceanMaterial"
    static const PString DECORATED_TYPE;    // "m.WaterOceanMaterial"
    static const UId uid;                   // EF9795F6-F515-4AEE-A89F-6FBD9CF42FF6

    virtual Material* __cdecl CreateInstance() const override;
    virtual bool __cdecl operator==(const Material& other) const override;
    virtual void __cdecl SetResourcePriority(float priority) const override;
    virtual const PString& __cdecl GetType() const override;
    virtual void __cdecl Activate(int detail) override;
    virtual GeometryChunk* __cdecl CreateGeometryChunk(unsigned int numVertices, unsigned int numPrimitives,
                                                       unsigned int numIndices, unsigned int numFaceNormals,
                                                       GeometryType type, GeometryChunkCreateHint* hint,
                                                       bool waveVertexProgram) const override;
    virtual Result __cdecl DuplicateTexture(int type) override;
    virtual Texture* __cdecl GetTexture(int type) override;
    virtual const Texture* __cdecl GetTexture(int type) const override;
    virtual Result __cdecl SetTexture(int type, const ResourceDatabase* db, const PString& name, float amount,
                                      const String& request) override;
    virtual Result __cdecl SetTexture(int type, const AutoReference<Texture>& texture, float amount) override;
    virtual void __cdecl Bind(Display* display, ViewPort* view, unsigned int pass) override;
    virtual void __cdecl Unbind(Display* display, ViewPort* view, unsigned int pass) override;
    virtual void __cdecl CacheChunk(Display* display, ViewPort* view, GeometryChunk* chunk,
                                    const DisplayLightPtrList* lights, const Matrix4* modelView) const override;
    virtual void __cdecl RenderChunk(Display* display, ViewPort* view, GeometryChunk* chunk,
                                     const DisplayLightPtrList* lights, const Matrix4* modelView, int pass) const override;
    virtual void __cdecl RenderNumChunks(Display* display, ViewPort* view, GeometryChunk** chunks,
                                         const DisplayLightPtrList** lights, const Matrix4** modelViews,
                                         unsigned int numChunks, int pass) const override;
    virtual void __cdecl RenderPrimitives(Display* display, ViewPort* view, GeometryChunk* chunk,
                                          const DisplayLightPtrList* lights, const Matrix4* modelView,
                                          unsigned int numIndices, const unsigned short* indices,
                                          unsigned int numPrimitives, DPrimitive primitive, int pass) const override;
    virtual Result __cdecl IsLoaded() const override;
    virtual bool Initialize(const ResourceDatabase* database, const WaterManagerConfig* config,
                            FrameAnimation* frameAnimation, WaterTextureAnimation* textureAnimation, int detail) override;
    virtual bool SetReflectionType(int type) override;
    virtual int GetReflectionType() const override;
    virtual void SetScaleNormXY(float value) override;
    virtual float GetScaleNormXY() const override;
    virtual void Update(float time) override;
    virtual void SetRefractiveIndex(float value) override;

    void EnableTextureInterpolation(bool value);
    void SetWaveAttributes(float a, float b, float c);
    bool __cdecl Equals(const WaterOceanMaterial& other) const;

protected:
    // 48..51
    virtual void CreateVertexType();
    virtual void SetupVertexProgramConstants(Display* display, Camera* camera, const Matrix4* modelView) const;
    virtual void SetDisplayState(Display* display) const;
    virtual void ClearDisplayState(Display* display) const;
    void CalculateLerpTexture(Display* display) const;
    void RenderSquare(Display* display) const;
    void __cdecl InitInstance(WaterOceanMaterial& instance) const;

public:
    Texture* reflectionTexture;          // +0xe4
    VertexProgram* vertexProgram;        // +0xe8
    VertexProgram* interpProgram;        // +0xec
    TextureCombiner* combiner;           // +0xf0
    TextureCombiner* lerpCombiner;       // +0xf4
    VertexType* vertexType;              // +0xf8
    void* renderTarget;                  // +0xfc (DisplayTexture)
    float refractiveIndex;               // +0x100 (1.3333)
    Color color;                         // +0x104
    float colourScale[4];                // +0x114
    float depthRange;                    // +0x124
    float depthFalloff[3];               // +0x128
    float waveDir[3];                    // +0x134
    WaterTextureAnimation* textureAnimation;  // +0x140
    FrameAnimation* frameAnimation;      // +0x144
    bool enableTextureInterpolation;     // +0x148
    bool enableRefraction;               // +0x149
    float uvScrollX;                     // +0x14c
    float uvScrollY;                     // +0x150
    float elapsedTime;                   // +0x154
    float waveAttribute0;                // +0x158
    float waveAttribute1;                // +0x15c
    float waveAttribute2;                // +0x160
    float scaleNormXY;                   // +0x164
};

JET_LAYOUT32(sizeof(WaterOceanMaterial) == 0x174);
JET_LAYOUT32(offsetof(WaterOceanMaterial, reflectionTexture) == 0xe4);
JET_LAYOUT32(offsetof(WaterOceanMaterial, vertexType) == 0xf8 && offsetof(WaterOceanMaterial, renderTarget) == 0xfc);
JET_LAYOUT32(offsetof(WaterOceanMaterial, textureAnimation) == 0x140 && offsetof(WaterOceanMaterial, elapsedTime) == 0x154);
JET_LAYOUT32(offsetof(WaterOceanMaterial, scaleNormXY) == 0x164);

}  // namespace Jet