// WaterSystem_7c U8b: WaterMaterialOneTex, the one-texture water material. 0xf4 bytes: the one texture at +0xe4,
// the vtordisp at +0xe8 and the virtual ReferenceCount base at +0xec. Slots 48/49 are SetDisplayState and
// ClearDisplayState.
#pragma once

#include <jet/Texture.h>
#include <jet/WaterMaterial.h>

namespace Jet {

class WaterMaterialOneTex : public WaterMaterial {
public:
    explicit WaterMaterialOneTex(MaterialManager* manager);
    ~WaterMaterialOneTex() override;

    static const PString UNDECORATED_TYPE;  // "WaterMaterialOneTex"
    static const PString DECORATED_TYPE;    // "m.WaterMaterialOneTex"
    static const UId uid;                   // 4F7EA82E-1CB4-47B4-B1C2-2FCEA5A7D7CA

    Material* __cdecl CreateInstance() const override;
    bool __cdecl operator==(const Material& other) const override;
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
    float GetScaleNormXY() const override;
    void SetScaleNormXY(float value) override;

    // 48, 49
protected:
    virtual void __cdecl SetDisplayState(Display* display) const;
    virtual void __cdecl ClearDisplayState(Display* display) const;

public:

public:
    Texture* texture;  // +0xe4
};

JET_LAYOUT32(sizeof(WaterMaterialOneTex) == 0xf4);
JET_LAYOUT32(offsetof(WaterMaterialOneTex, texture) == 0xe4);

// The chunk OneTex hands out: GeometryChunkTemplate<PackedVertexPNT, GeometryChunkSingleStream> (0x60 bytes,
// 0x20-byte records, VertexTypeID 18).
GeometryChunk* CreateOneTexGeometryChunk(unsigned int numVertices, unsigned int numPrimitives,
                                         unsigned int numIndices, unsigned int numFaceNormals,
                                         Material::GeometryType type, GeometryChunkCreateHint* hint);

}  // namespace Jet




