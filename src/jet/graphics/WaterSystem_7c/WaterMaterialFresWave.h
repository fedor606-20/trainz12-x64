// WaterSystem_7c U8b: WaterMaterialFresWave (the vertex-program water material) and the chunk template the
// water materials hand out. Layout follows the TS12 assembly: the derived fields start at +0xe4, the vtordisp sits
// at +0x210 and the virtual ReferenceCount base at +0x214 (size 0x21c). Slots 48/49 are the two virtuals this class
// appends to WaterMaterial's 48 (SetReflectionCamera, SetupVertexProgramConstants).
#pragma once

#include <jet/Geometry.h>
#include <jet/GfxLanguage.h>
#include <jet/Texture.h>
#include <jet/WaterMaterial.h>

namespace Jet {

class DisplayTexture;
class FrameAnimation;
class MaterialManager;
class RenderKernel;
class WaterReflectionCamera;
class WaterTextureAnimation;

// One texture-stage definition the factory builds from two static TextureArgs (Display slot 0x11c). The 12-byte
// node belongs to the Display; the material only keeps the pointers.
struct FresWaveTextureStage;

class WaterMaterialFresWave : public WaterMaterial {
public:
    WaterMaterialFresWave(MaterialManager* manager, void** textureStages);
    ~WaterMaterialFresWave() override;

    static const UId uid;
    static const PString UNDECORATED_TYPE;
    static const PString DECORATED_TYPE;
    static const PString vertexName;
    static const PString vertexNamePTT;

    Material* __cdecl CreateInstance() const override;
    bool __cdecl operator==(const Material& other) const override;
    void __cdecl SetResourcePriority(float priority) const override;
    const PString& __cdecl GetType() const override;
    void __cdecl Activate(int detail) override;
    GeometryChunk* __cdecl CreateGeometryChunk(unsigned int numVertices, unsigned int numPrimitives,
                                               unsigned int numIndices, unsigned int numFaceNormals,
                                               GeometryType type, GeometryChunkCreateHint* hint) const override;
    GeometryChunk* __cdecl CreateGeometryChunk(unsigned int numVertices, unsigned int numPrimitives,
                                               unsigned int numIndices, unsigned int numFaceNormals,
                                               GeometryType type, GeometryChunkCreateHint* hint,
                                               bool waveVertexProgram) const override;
    Result __cdecl DuplicateTexture(int type) override;
    Texture* __cdecl GetTexture(int type) override;
    const Texture* __cdecl GetTexture(int type) const override;
    Result __cdecl SetTexture(int type, const ResourceDatabase* database, const PString& name, float amount,
                              const String& request) override;
    Result __cdecl SetTexture(int type, const AutoReference<Texture>& texture, float amount) override;
    void __cdecl Bind(Display* display, ViewPort* view, unsigned int pass) override;
    void __cdecl Unbind(Display* display, ViewPort* view, unsigned int pass) override;
    void __cdecl CacheChunk(Display* display, ViewPort* view, GeometryChunk* chunk,
                            const DisplayLightPtrList* lights, const Matrix4* modelView) const override;
    void __cdecl RenderChunk(Display* display, ViewPort* view, GeometryChunk* chunk,
                             const DisplayLightPtrList* lights, const Matrix4* modelView, int pass) const override;
    void __cdecl RenderNumChunks(Display* display, ViewPort* view, GeometryChunk** chunks,
                                 const DisplayLightPtrList** lights, const Matrix4** modelViews,
                                 unsigned int numChunks, int pass) const override;
    void __cdecl RenderPrimitives(Display* display, ViewPort* view, GeometryChunk* chunk,
                                  const DisplayLightPtrList* lights, const Matrix4* modelView,
                                  unsigned int numIndices, const unsigned short* indices,
                                  unsigned int numPrimitives, DPrimitive primitive, int pass) const override;
    Result __cdecl IsLoaded() const override;

    bool Initialize(const ResourceDatabase* database, const WaterManagerConfig* config,
                    FrameAnimation* frameAnimation, WaterTextureAnimation* textureAnimation, int detail) override;
    bool SetReflectionType(int type) override;
    int GetReflectionType() const override;
    void SetDetailAnim(bool value) override;
    bool GetDetailAnim() const override;
    void SetScaleNormXY(float value) override;
    float GetScaleNormXY() const override;
    void Update(float time) override;
    void SetRefractiveIndex(float value) override;
    void SetWaterColor(const Color& color) override;
    void SetUVScrollVelocity(Vector2 velocity) override;
    Vector2 GetUVScrollVelocity() const override;
    void EnableDetailAnimInterpolation(bool value) override;
    void SetGridSpacing(float value);
    virtual void __cdecl SetReflectionCamera(WaterReflectionCamera* camera);
    bool __cdecl Equals(const WaterMaterialFresWave& other) const;

protected:
    virtual void __cdecl SetupVertexProgramConstants(VertexProgram* vertexProgram, TextureCombiner* combiner,
                                                     Display* display, Camera* camera,
                                                     const DisplayLightPtrList* lights, const Matrix4* modelView,
                                                     const Matrix4& world, const Matrix4& view, bool reflection) const;

protected:
    void __cdecl InitVertexProgramms();
    void __cdecl InitInstance(WaterMaterialFresWave& instance) const;
    void CalculateDetailTexture(Display* display) const;
    void CalculateWaterColorTexture(Display* display) const;
    void CalculateLightAnimTexture(Display* display) const;
    void RenderSquare(Display* display) const;

public:
    VertexProgram* vertexProgramWave;             // +0xe4
    VertexProgram* vertexProgramWaveAnim;         // +0xe8
    VertexProgram* vertexProgramWaveRefl;         // +0xec
    VertexProgram* vertexProgramWaveAnimRefl;     // +0xf0
    VertexProgram* vertexProgramAnimRefl;         // +0xf4
    VertexProgram* vertexProgramRefl;             // +0xf8
    VertexProgram* currentVertexProgram;          // +0xfc
    TextureCombiner* currentCombiner;             // +0x100
    VertexProgram* currentReflectionProgram;      // +0x104
    int vertexProgramConstants[15];               // +0x108
    int reflectionProgramConstants[12];           // +0x144
    TextureCombiner* combiners[3];                // +0x174
    void** textureStages;                        // +0x180
    Texture* detailTexture;                      // +0x184
    Texture* reflectionTexture;                  // +0x188
    WaterReflectionCamera* reflectionCamera;      // +0x18c
    DisplayTexture* detailTarget;                // +0x190
    DisplayTexture* waterColorTarget;            // +0x194
    DisplayTexture* lightAnimTarget;             // +0x198
    VertexType* vertexTypePA2A4T;                // +0x19c
    VertexType* vertexTypePTT;                   // +0x1a0
    WaterTextureAnimation* textureAnimation;     // +0x1a4
    FrameAnimation* frameAnimation;              // +0x1a8
    int lastFrame;                               // +0x1ac
    unsigned char flags;                         // +0x1b0
    float time;                                  // +0x1b4
    float gridSpacing;                           // +0x1b8
    float refractiveTerm;                        // +0x1bc
    float uvOffsetX;                             // +0x1c0
    float uvOffsetY;                             // +0x1c4
    float uvVelocityX;                           // +0x1c8
    float uvVelocityY;                           // +0x1cc
    int reflectionType;                          // +0x1d0
    float scaleNormXY;                           // +0x1d4
    float reflectionAmount;                      // +0x1d8
    bool waterColorDirty;                        // +0x1dc
    Color lightAnimColor;                        // +0x1e0
    Color lightAnimColor2;                       // +0x1f0
    Vector3 lightAnimDirection;                  // +0x200
    int lightAnimFrame;                          // +0x20c
};

JET_LAYOUT32(sizeof(WaterMaterialFresWave) == 0x21c);
JET_LAYOUT32(offsetof(WaterMaterialFresWave, vertexProgramWave) == 0xe4);
JET_LAYOUT32(offsetof(WaterMaterialFresWave, textureStages) == 0x180);
JET_LAYOUT32(offsetof(WaterMaterialFresWave, flags) == 0x1b0);
JET_LAYOUT32(offsetof(WaterMaterialFresWave, reflectionType) == 0x1d0);
JET_LAYOUT32(offsetof(WaterMaterialFresWave, lightAnimColor) == 0x1e0);
JET_LAYOUT32(offsetof(WaterMaterialFresWave, lightAnimDirection) == 0x200);
JET_LAYOUT32(offsetof(WaterMaterialFresWave, lightAnimFrame) == 0x20c);

// 0x10013800 / 0x1000eb90 helper: the chunk the water materials create (shared by FresWave and OneTex).
GeometryChunk* CreateWaterGeometryChunk(bool waveVertexProgram, unsigned int numVertices,
                                        unsigned int numPrimitives, unsigned int numIndices,
                                        unsigned int numFaceNormals, Material::GeometryType type,
                                        GeometryChunkCreateHint* hint);
GeometryChunk* CreateOneTexGeometryChunk(unsigned int numVertices, unsigned int numPrimitives,
                                         unsigned int numIndices, unsigned int numFaceNormals,
                                         Material::GeometryType type, GeometryChunkCreateHint* hint);

// the Render3DCore camera accessor the render paths pass to Material::SetLightingState
const Matrix4* GetWaterViewTransform(const Camera* camera);

// the RenderSystem Material entry points the shared header declares with the wrong access/constness: called
// through their exported names so our units link against the original ABI
void SetWaterMaterialLightingState(const Material* material, Display* display, const Matrix4* modelView,
                                   const GeometryChunk* chunk, const VertexProgram* program,
                                   const DisplayLightPtrList* lights, const Matrix4* view, float opacity,
                                   bool twoSided);
void SetWaterMaterialTextureDefinition(const Material* material, int type, const PString& name, float weight);

// Display slot 0x11c: a texture-stage node over two of the factory's static TextureArgs (index order is the
// original's static table order).
void* CreateWaterStageNode(Display* display, int flag, int firstArg, int secondArg);

}  // namespace Jet








