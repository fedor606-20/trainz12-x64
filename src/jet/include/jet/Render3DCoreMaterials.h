// Unit U5a of Render3DCore_7c: the material classes and their plugins.
//
// The classes keep the TS12 layout (offsets verified against the assembly) so that every member below reads and writes
// the same field the original does, including objects the original constructed. Only the members this unit actually
// implements are declared; the rest of each class's interface stays with Render3DCore_7c_orig.dll through the generated
// .def. The whole material hierarchy is defined here because the original has no public header: `Material` lives in
// RenderSystem_7c (its ctor/dtor/InitInstance/Equals/SetLightingState are exported there), and every concrete material
// is a Render3DCore_7c class.
//
// Object layout: Material derives from PluginInstance, whose virtual ReferenceCount base the compiler puts at the end
// of the most-derived object. Material's own state ends at +0x98 (materialManager); a concrete class's field starts at
// +0x9c, before the vtordisp and the ReferenceCount subobject. The virtual tables are not re-exported by this unit, so
// the vtables below only serve our own calls; the slot order matches the original (23 Material slots, then for the
// one-texture family CacheAlphaHint, SetDisplayState and ClearDisplayState at 23..25).
#pragma once

#include <jet/FastDynamicArray.h>
#include <jet/Geometry.h>
#include <jet/InputSystem.h>
#include <jet/KernelStandard.h>
#include <jet/Material.h>
#include <jet/Math.h>
#include <jet/PString.h>
#include <jet/Plugin.h>
#include <jet/RenderSystem.h>
#include <jet/RenderTypes.h>
#include <jet/Result.h>
#include <jet/Texture.h>
#include <jet/UId.h>

namespace Jet {

class GeometryChunkCreateHint;
class ResourceDatabase;
class VertexProgram;
class MaterialChunkCache;
class AlphaMaskCache;
class AlphaBlendCache;

using DisplayLightPtrList = FastDynamicArray<DisplayLight*, 13, Less<DisplayLight*, DisplayLight*>>;

// ---------------------------------------------------------------------------------------------------------------
// Material: the RenderSystem_7c base (0x9c of own state). Only the members the unit calls into are named, and the
// virtuals keep TS12's order.

class Material : public PluginInstance {
public:
    enum Results { RES_UNKNOWN = 1, RES_UNSUPPORTED_TEX, RES_TEX_NOT_FOUND, RES_NOT_LOADED, RES_MAX };
    static __declspec(dllimport) const ResultTable* const results;
    using Result = ResultClass<&results, Results>;

    enum GeometryType { GEOMETRY_TRIANGLES = 0, GEOMETRY_MAX };

    static Material* __cdecl CreateInstance(const PString& type, MaterialManager* manager);
    static Material* __cdecl CreateInstance(const UId& id, MaterialManager* manager);
    explicit Material(MaterialManager* manager);

    // slot 0
    virtual Material* __cdecl CreateInstance() const = 0;
    // slot 1
    virtual bool __cdecl operator==(const Material& other) const = 0;
    // slot 2
    virtual void __cdecl SetResourcePriority(float priority) const;
    // slot 3
    virtual const PString& __cdecl GetType() const = 0;
    // slot 4
    virtual void __cdecl Activate(int detail) = 0;
    // slot 5
    virtual GeometryChunk* __cdecl CreateGeometryChunk(unsigned int numVertices, unsigned int numPrimitives,
                                                       unsigned int numIndices, unsigned int numFaceNormals,
                                                       GeometryType type,
                                                       GeometryChunkCreateHint* hint) const = 0;
    // slot 6
    virtual Result __cdecl DuplicateTexture(int type) = 0;
    // slots 7, 8
    virtual Texture* __cdecl GetTexture(int type) = 0;
    virtual const Texture* __cdecl GetTexture(int type) const = 0;
    // slots 9, 10
    virtual Result __cdecl SetTexture(int type, const ResourceDatabase* db, const PString& name, float amount,
                                      const String& request);
    virtual Result __cdecl SetTexture(int type, const AutoReference<Texture>& texture, float amount);
    // slot 11: RenderSystem_7c's Material::GetNumPasses (a bare thunk in the original's tables)
    virtual unsigned int __cdecl GetNumPasses() const;
    // slots 12, 13
    virtual void __cdecl Bind(Display* display, ViewPort* view, unsigned int pass) = 0;
    virtual void __cdecl Unbind(Display* display, ViewPort* view, unsigned int pass) = 0;
    // slots 14, 15, 16, 17
    virtual void __cdecl CacheChunk(Display* display, ViewPort* view, GeometryChunk* chunk,
                                    const DisplayLightPtrList* lights, const Matrix4* modelView) const = 0;
    virtual void __cdecl RenderChunk(Display* display, ViewPort* view, GeometryChunk* chunk,
                                     const DisplayLightPtrList* lights, const Matrix4* modelView, int pass) const = 0;
    virtual void __cdecl RenderNumChunks(Display* display, ViewPort* view, GeometryChunk** chunks,
                                         const DisplayLightPtrList** lights, const Matrix4** modelViews,
                                         unsigned int numChunks, int pass) const = 0;
    virtual void __cdecl RenderPrimitives(Display* display, ViewPort* view, GeometryChunk* chunk,
                                          const DisplayLightPtrList* lights, const Matrix4* modelView,
                                          unsigned int numIndices, const unsigned short* indices,
                                          unsigned int numPrimitives, DPrimitive primitive, int pass) const = 0;
    // slot 18
    virtual void __cdecl AddProperty(const PString& name, const PString& value);
    // slots 19, 20
    virtual const PString& __cdecl GetPropertyString(const PString& name, bool& found) const;
    virtual const PString& __cdecl GetPropertyString(const PString& name) const;
    // slot 21
    virtual unsigned long __cdecl GetSortKey() const;
    // slot 22
    virtual Result __cdecl IsLoaded() const;

    // the chunk flags from the geometry type and the hint (RenderSystem_7c exports)
    static __declspec(dllimport) unsigned int __cdecl MakeGeometryChunkFlags(GeometryType type, const GeometryChunkCreateHint* hint);

protected:
    // RenderSystem_7c's protected virtual destructor: every concrete material's destructor chains into it.
    virtual ~Material();
    // RenderSystem_7c exports: 0x100331b0 and 0x10033010
    void __cdecl InitInstance(Material& instance) const;
    bool __cdecl Equals(const Material& other) const;
    __declspec(dllimport) void __cdecl SetTextureDefinition(int type, const PString& name, float weight);

public:
    // 0x1002f... (RenderSystem_7c): the lighting/pass state around a chunk render
    __declspec(dllimport) void __cdecl SetLightingState(Display* display, const Matrix4* modelView, const GeometryChunk* chunk,
                                  const VertexProgram* program, const DisplayLightPtrList* lights,
                                  const Matrix4* view, float opacity, bool unk) const;

public:
    unsigned char reserved0c[0x18];  // +0x0c .. +0x23
    PString decoratedName;           // +0x24
    PString undecoratedName;         // +0x28
    PString creationType;            // +0x2c
    PString requestName;             // +0x30
    DisplayMaterial properties;      // +0x34
    int srcBlend;                    // +0x78
    int dstBlend;                    // +0x7c
    float opacity;                   // +0x80
    unsigned char flags;             // +0x84 bit 0 two-sided, bit 1 standard blend
    unsigned char reserved85[3];     // +0x85
    void* propertyList;              // +0x88 (ring head)
    int numTextures;                 // +0x8c
    void* textureList;               // +0x90 (ring head)
    float resourcePriority;          // +0x94
    MaterialManager* materialManager;  // +0x98
};

JET_LAYOUT32(offsetof(Material, decoratedName) == 0x24);
JET_LAYOUT32(offsetof(Material, properties) == 0x34);
JET_LAYOUT32(offsetof(Material, srcBlend) == 0x78 && offsetof(Material, opacity) == 0x80);
JET_LAYOUT32(offsetof(Material, flags) == 0x84 && offsetof(Material, propertyList) == 0x88);
JET_LAYOUT32(offsetof(Material, textureList) == 0x90 && offsetof(Material, resourcePriority) == 0x94);
JET_LAYOUT32(offsetof(Material, materialManager) == 0x98);

// ---------------------------------------------------------------------------------------------------------------
// Concrete materials: the hierarchy and the fields each class adds.

class NoTextureMaterial : public Material {
public:
    static const PString DECORATED_TYPE;
    static const PString UNDECORATED_TYPE;
    static const UId UID;
    ~NoTextureMaterial() override;
    bool __cdecl operator==(const Material& other) const override;
    Material* __cdecl CreateInstance() const override;
    const PString& __cdecl GetType() const override;
    Result __cdecl DuplicateTexture(int type) override;
    void __cdecl Bind(Display* display, ViewPort* view, unsigned int pass) override;
    void __cdecl Unbind(Display* display, ViewPort* view, unsigned int pass) override;
    void __cdecl Activate(int detail) override;
    void __cdecl SetDefaultTextureStage(void* stage);
    Texture* __cdecl GetTexture(int type) override;
    const Texture* __cdecl GetTexture(int type) const override;
    GeometryChunk* __cdecl CreateGeometryChunk(unsigned int numVertices, unsigned int numPrimitives,
                                               unsigned int numIndices, unsigned int numFaceNormals,
                                               GeometryType type, GeometryChunkCreateHint* hint) const override;
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

protected:
    void __cdecl SetDisplayState(Display* display) const;
    void __cdecl ClearDisplayState(Display* display) const;
    void __cdecl InitInstance(NoTextureMaterial& instance) const;
    bool __cdecl Equals(const NoTextureMaterial& other) const;

public:
    void* defaultTextureStage;  // +0x9c

public:
    NoTextureMaterial(MaterialManager*);  // emission only: the compiler emits the class vbtable with it
};

JET_LAYOUT32(offsetof(NoTextureMaterial, defaultTextureStage) == 0x9c);

class OneTextureMaterial : public Material {
public:
    static const PString DECORATED_TYPE;
    static const PString UNDECORATED_TYPE;
    static const UId UID;
    ~OneTextureMaterial() override;
    bool __cdecl operator==(const Material& other) const override;
    Material* __cdecl CreateInstance() const override;
    void __cdecl SetResourcePriority(float priority) const override;
    const PString& __cdecl GetType() const override;
    void __cdecl SetAlphaHint(TextureAlpha hint);
    TextureAlpha __cdecl GetAlphaHint() const;
    void __cdecl SetDefaultTextureStage(void* stage);
    void* __cdecl GetDefaultTextureStage();
    void __cdecl Bind(Display* display, ViewPort* view, unsigned int pass) override;
    void __cdecl Unbind(Display* display, ViewPort* view, unsigned int pass) override;
    Texture* __cdecl GetTexture(int type) override;
    const Texture* __cdecl GetTexture(int type) const override;
    Material::Result __cdecl DuplicateTexture(int type) override;
    Material::Result __cdecl SetTexture(int type, const ResourceDatabase* db, const PString& name, float amount,
                                        const String& request) override;
    Material::Result __cdecl SetTexture(int type, const AutoReference<Texture>& texture, float amount) override;
    void __cdecl Activate(int detail) override;
    Result __cdecl IsLoaded() const override;
    GeometryChunk* __cdecl CreateGeometryChunk(unsigned int numVertices, unsigned int numPrimitives,
                                               unsigned int numIndices, unsigned int numFaceNormals,
                                               GeometryType type, GeometryChunkCreateHint* hint) const override;
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

    virtual void CacheAlphaHint() const;  // slot 23
protected:
    virtual void __cdecl SetDisplayState(Display* display) const;    // slot 24
    virtual void __cdecl ClearDisplayState(Display* display) const;  // slot 25
    void __cdecl InitInstance(OneTextureMaterial& instance) const;
    bool __cdecl Equals(const OneTextureMaterial& other) const;

public:
    OneTextureMaterial(MaterialManager*);  // emission only (vbtable)
public:
    AutoReference<Texture> texture;  // +0x9c
    TextureAlpha alphaHint;          // +0xa0 (-1 until cached)
    void* defaultTextureStage;       // +0xa4
};

JET_LAYOUT32(offsetof(OneTextureMaterial, texture) == 0x9c);
JET_LAYOUT32(offsetof(OneTextureMaterial, alphaHint) == 0xa0 && offsetof(OneTextureMaterial, defaultTextureStage) == 0xa4);

class OneTextureNoAlphaMaterial : public OneTextureMaterial {
public:
    static const PString DECORATED_TYPE;
    static const PString UNDECORATED_TYPE;
    static const UId UID;
    ~OneTextureNoAlphaMaterial() override;
    bool __cdecl operator==(const Material& other) const override;
    Material* __cdecl CreateInstance() const override;
    void __cdecl SetResourcePriority(float priority) const override;
    const PString& __cdecl GetType() const override;
    void __cdecl SetAlphaHint(TextureAlpha hint);
    TextureAlpha __cdecl GetAlphaHint() const;
    void __cdecl SetDefaultTextureStage(void* stage);
    void* __cdecl GetDefaultTextureStage();
    void __cdecl Bind(Display* display, ViewPort* view, unsigned int pass) override;
    void __cdecl Unbind(Display* display, ViewPort* view, unsigned int pass) override;
    Texture* __cdecl GetTexture(int type) override;
    const Texture* __cdecl GetTexture(int type) const override;
    void __cdecl Activate(int detail) override;
    Result __cdecl IsLoaded() const override;
    void CacheAlphaHint() const override;
    GeometryChunk* __cdecl CreateGeometryChunk(unsigned int numVertices, unsigned int numPrimitives,
                                               unsigned int numIndices, unsigned int numFaceNormals,
                                               GeometryType type, GeometryChunkCreateHint* hint) const override;
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

protected:
    void __cdecl SetDisplayState(Display* display) const override;
    void __cdecl ClearDisplayState(Display* display) const override;
    void __cdecl InitInstance(OneTextureNoAlphaMaterial& instance) const;
    bool __cdecl Equals(const OneTextureNoAlphaMaterial& other) const;

public:
    OneTextureNoAlphaMaterial(MaterialManager*);  // emission only (vbtable)
};

class BillboardMaterial : public OneTextureMaterial {
public:
    static const PString DECORATED_TYPE;
    static const PString UNDECORATED_TYPE;
    static const UId UID;
    ~BillboardMaterial() override;
    bool __cdecl operator==(const Material& other) const override;
    Material* __cdecl CreateInstance() const override;
    const PString& __cdecl GetType() const override;
    void __cdecl RenderChunk(Display* display, ViewPort* view, GeometryChunk* chunk,
                             const DisplayLightPtrList* lights, const Matrix4* modelView, int pass) const override;
    void __cdecl RenderNumChunks(Display* display, ViewPort* view, GeometryChunk** chunks,
                                 const DisplayLightPtrList** lights, const Matrix4** modelViews,
                                 unsigned int numChunks, int pass) const override;
    void __cdecl RenderPrimitives(Display* display, ViewPort* view, GeometryChunk* chunk,
                                  const DisplayLightPtrList* lights, const Matrix4* modelView,
                                  unsigned int numIndices, const unsigned short* indices,
                                  unsigned int numPrimitives, DPrimitive primitive, int pass) const override;

protected:
    void __cdecl InitInstance(BillboardMaterial& instance) const;
    bool __cdecl Equals(const BillboardMaterial& other) const;

public:
    BillboardMaterial(MaterialManager*);  // emission only (vbtable)
};

class ColorOneTextureMaterial : public OneTextureMaterial {
public:
    static const PString DECORATED_TYPE;
    static const PString UNDECORATED_TYPE;
    static const UId UID;
    ~ColorOneTextureMaterial() override;
    bool __cdecl operator==(const Material& other) const override;
    Material* __cdecl CreateInstance() const override;
    const PString& __cdecl GetType() const override;
    void __cdecl Bind(Display* display, ViewPort* view, unsigned int pass) override;
    void __cdecl Unbind(Display* display, ViewPort* view, unsigned int pass) override;
    GeometryChunk* __cdecl CreateGeometryChunk(unsigned int numVertices, unsigned int numPrimitives,
                                               unsigned int numIndices, unsigned int numFaceNormals,
                                               GeometryType type, GeometryChunkCreateHint* hint) const override;
    void __cdecl RenderChunk(Display* display, ViewPort* view, GeometryChunk* chunk,
                             const DisplayLightPtrList* lights, const Matrix4* modelView, int pass) const override;
    void __cdecl RenderNumChunks(Display* display, ViewPort* view, GeometryChunk** chunks,
                                 const DisplayLightPtrList** lights, const Matrix4** modelViews,
                                 unsigned int numChunks, int pass) const override;
    void __cdecl RenderPrimitives(Display* display, ViewPort* view, GeometryChunk* chunk,
                                  const DisplayLightPtrList* lights, const Matrix4* modelView,
                                  unsigned int numIndices, const unsigned short* indices,
                                  unsigned int numPrimitives, DPrimitive primitive, int pass) const override;

protected:
    void __cdecl SetDisplayState(Display* display) const override;
    void __cdecl ClearDisplayState(Display* display) const override;
    void __cdecl InitInstance(OneTextureMaterial& instance) const;
    bool __cdecl Equals(const ColorOneTextureMaterial& other) const;

public:
    ColorOneTextureMaterial(MaterialManager*);  // emission only (vbtable)
};
class AutoTexMaterial : public OneTextureMaterial {
public:
    // The five-argument constructor stays in the original module: StdMaterialPluginTemplate calls it by its exported
    // name (a declaration only; the class layout is unchanged).
    AutoTexMaterial(MaterialManager* manager, MaterialChunkCache* chunkCache, AlphaMaskCache* maskCache,
                    AlphaBlendCache* blendCache);
    Material* __cdecl CreateInstance() const override;
    static const PString DECORATED_TYPE;
    static const PString UNDECORATED_TYPE;
    static const UId UID;
    ~AutoTexMaterial() override;
    bool __cdecl operator==(const Material& other) const override;
    const PString& __cdecl GetType() const override;
    void __cdecl Bind(Display* display, ViewPort* view, unsigned int pass) override;
    void __cdecl Unbind(Display* display, ViewPort* view, unsigned int pass) override;
    void __cdecl SetTextureGenerationParameters(float a, float b, float c, float d);
    void __cdecl GetTextureGenerationParameters(float& a, float& b, float& c, float& d) const;
    GeometryChunk* __cdecl CreateGeometryChunk(unsigned int numVertices, unsigned int numPrimitives,
                                               unsigned int numIndices, unsigned int numFaceNormals,
                                               GeometryType type, GeometryChunkCreateHint* hint) const override;
    void __cdecl RenderChunk(Display* display, ViewPort* view, GeometryChunk* chunk,
                             const DisplayLightPtrList* lights, const Matrix4* modelView, int pass) const override;
    void __cdecl RenderNumChunks(Display* display, ViewPort* view, GeometryChunk** chunks,
                                 const DisplayLightPtrList** lights, const Matrix4** modelViews,
                                 unsigned int numChunks, int pass) const override;
    void __cdecl RenderPrimitives(Display* display, ViewPort* view, GeometryChunk* chunk,
                                  const DisplayLightPtrList* lights, const Matrix4* modelView,
                                  unsigned int numIndices, const unsigned short* indices,
                                  unsigned int numPrimitives, DPrimitive primitive, int pass) const override;

protected:
    void __cdecl SetDisplayState(Display* display) const override;
    void __cdecl ClearDisplayState(Display* display) const override;
    void __cdecl InitInstance(AutoTexMaterial& instance) const;
    bool __cdecl Equals(const AutoTexMaterial& other) const;

public:
    float textureGeneration[4];  // +0xa8 .. +0xb4

public:
    AutoTexMaterial(MaterialManager*);  // emission only (vbtable)
};

class MVHSelectMaterial : public OneTextureMaterial {
public:
    static const PString DECORATED_TYPE;
    static const PString UNDECORATED_TYPE;
    static const UId UID;
    ~MVHSelectMaterial() override;
    bool __cdecl operator==(const Material& other) const override;
    Material* __cdecl CreateInstance() const override;
    const PString& __cdecl GetType() const override;
    void __cdecl SetStageColor(Color& color);
    const Color& __cdecl GetStageColor() const;
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

protected:
    void __cdecl SetDisplayState(Display* display) const override;
    void __cdecl ClearDisplayState(Display* display) const override;
    void __cdecl InitInstance(MVHSelectMaterial& instance) const;
    bool __cdecl Equals(const MVHSelectMaterial& other) const;

public:
    Color stageColor;  // +0xa8
    void* context;     // +0xb8

public:
    MVHSelectMaterial(MaterialManager*);  // emission only (vbtable)
};

class ReflectiveMaterial : public OneTextureMaterial {
public:
    static const PString DECORATED_TYPE;
    static const PString UNDECORATED_TYPE;
    static const UId UID;
    ~ReflectiveMaterial() override;
    bool __cdecl operator==(const Material& other) const override;
    Material* __cdecl CreateInstance() const override;
    unsigned int __cdecl GetNumPasses() const override;
    void __cdecl SetResourcePriority(float priority) const override;
    const PString& __cdecl GetType() const override;
    Result __cdecl IsLoaded() const override;
    void __cdecl Bind(Display* display, ViewPort* view, unsigned int pass) override;
    void __cdecl Unbind(Display* display, ViewPort* view, unsigned int pass) override;
    Texture* __cdecl GetTexture(int type) override;
    const Texture* __cdecl GetTexture(int type) const override;
    Material::Result __cdecl DuplicateTexture(int type) override;
    Material::Result __cdecl SetTexture(int type, const ResourceDatabase* db, const PString& name, float amount,
                                        const String& request) override;
    Material::Result __cdecl SetTexture(int type, const AutoReference<Texture>& texture, float amount) override;
    void __cdecl Activate(int detail) override;
    void __cdecl SetCubeMapMatrix(const Matrix4& matrix);
    GeometryChunk* __cdecl CreateGeometryChunk(unsigned int numVertices, unsigned int numPrimitives,
                                               unsigned int numIndices, unsigned int numFaceNormals,
                                               GeometryType type, GeometryChunkCreateHint* hint) const override;
    void __cdecl RenderChunk(Display* display, ViewPort* view, GeometryChunk* chunk,
                             const DisplayLightPtrList* lights, const Matrix4* modelView, int pass) const override;
    void __cdecl RenderNumChunks(Display* display, ViewPort* view, GeometryChunk** chunks,
                                 const DisplayLightPtrList** lights, const Matrix4** modelViews,
                                 unsigned int numChunks, int pass) const override;
    void __cdecl RenderPrimitives(Display* display, ViewPort* view, GeometryChunk* chunk,
                                  const DisplayLightPtrList* lights, const Matrix4* modelView,
                                  unsigned int numIndices, const unsigned short* indices,
                                  unsigned int numPrimitives, DPrimitive primitive, int pass) const override;
    static void __cdecl SetSphericalTexureCoordinates(GeometryChunk* chunk, const Matrix4* matrix);

protected:
    void __cdecl SetDisplayState(Display* display, bool update) const;
    void __cdecl ClearDisplayState(Display* display) const override;
    void __cdecl InitInstance(ReflectiveMaterial& instance) const;
    bool __cdecl Equals(const ReflectiveMaterial& other) const;

public:
    ReflectiveMaterial(MaterialManager*);  // emission only (vbtable)
public:
    AutoReference<Texture> cubeTexture;  // +0xa8
    float reflectivity;                  // +0xac
    unsigned int contextCount;           // +0xb0
    void** contextData;                  // +0xb4
    unsigned char flags2;                // +0xb8
    unsigned char reservedB9[3];         // +0xb9
    unsigned char cubeMatrix[0x40];      // +0xbc .. +0xfb (the 0x40-byte cube map matrix)
};

JET_LAYOUT32(offsetof(ReflectiveMaterial, cubeTexture) == 0xa8);
JET_LAYOUT32(offsetof(ReflectiveMaterial, reflectivity) == 0xac && offsetof(ReflectiveMaterial, flags2) == 0xb8);
JET_LAYOUT32(offsetof(ReflectiveMaterial, cubeMatrix) == 0xbc);
JET_LAYOUT32(sizeof(ReflectiveMaterial) == 0x108);

class GlossMaterial : public ReflectiveMaterial {
public:
    static const PString DECORATED_TYPE;
    static const PString UNDECORATED_TYPE;
    static const UId UID;
    ~GlossMaterial() override;
    bool __cdecl operator==(const Material& other) const override;
    Material* __cdecl CreateInstance() const override;
    const PString& __cdecl GetType() const override;
    void __cdecl Bind(Display* display, ViewPort* view, unsigned int pass) override;
    void __cdecl Unbind(Display* display, ViewPort* view, unsigned int pass) override;
    void __cdecl CacheChunk(Display* display, ViewPort* view, GeometryChunk* chunk,
                            const DisplayLightPtrList* lights, const Matrix4* modelView) const override;
    Texture* __cdecl GetTexture(int type) override;
    const Texture* __cdecl GetTexture(int type) const override;
    void __cdecl Activate(int detail) override;
    void CacheAlphaHint() const override;

protected:
    void __cdecl InitInstance(GlossMaterial& instance) const;
    bool __cdecl Equals(const GlossMaterial& other) const;

public:
    GlossMaterial(MaterialManager*);  // emission only (vbtable)
};

class GenericShaderMaterial : public Material {
public:
    static const PString DECORATED_TYPE;
    static const PString UNDECORATED_TYPE;
    static const UId UID;
    ~GenericShaderMaterial() override;
    bool __cdecl operator==(const Material& other) const override;
    Material* __cdecl CreateInstance() const override;
    void __cdecl Activate(int detail) override;
    const PString& __cdecl GetType() const override;
    void __cdecl Bind(Display* display, ViewPort* view, unsigned int pass) override;
    void __cdecl Unbind(Display* display, ViewPort* view, unsigned int pass) override;
    TextureAlpha __cdecl GetAlphaHint();
    void __cdecl SetAlphaHint(TextureAlpha hint);
    void __cdecl SetResourcePriority(float priority) const override;
    Texture* __cdecl GetTexture(int type) override;
    const Texture* __cdecl GetTexture(int type) const override;
    Material::Result __cdecl DuplicateTexture(int type) override;
    GeometryChunk* __cdecl CreateGeometryChunk(unsigned int numVertices, unsigned int numPrimitives,
                                               unsigned int numIndices, unsigned int numFaceNormals,
                                               GeometryType type, GeometryChunkCreateHint* hint) const override;
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
    Result __cdecl SetTexture(int type, const ResourceDatabase* db, const PString& name, float amount,
                              const String& request) override;
    Result __cdecl SetTexture(int type, const AutoReference<Texture>& texture, float amount) override;

protected:
    void __cdecl SetDisplayState(Display* display) const;
    void __cdecl ClearDisplayState(Display* display) const;
    void __cdecl InitInstance(GenericShaderMaterial& instance) const;

public:
    GenericShaderMaterial(MaterialManager*);  // emission only (vbtable)
public:
    void* chunkCache;       // +0x9c
    void* alphaBlendCache;  // +0xa0
    void* textureRing;      // +0xa4
    unsigned int ringA;     // +0xa8
    unsigned int ringB;     // +0xac
    TextureAlpha alphaHint;  // +0xb0
};

JET_LAYOUT32(offsetof(GenericShaderMaterial, chunkCache) == 0x9c);
JET_LAYOUT32(offsetof(GenericShaderMaterial, alphaHint) == 0xb0);

// ---------------------------------------------------------------------------------------------------------------
// Plugins. TS12's MaterialPlugin has six virtual slots: the three PluginType ones, the factory template's
// CreateInstance() const, then IsSupported and CreateInstance(manager, kernel). StdMaterialPlugin adds a seventh,
// virtual CreatePluginResources; every other plugin's CreatePluginResources is a plain member (mangling IBA) that
// its CreateInstance calls. The objects carry the rendering-cache pointers at +0x20 and nothing else.

class MaterialPlugin : public PluginFactoryBase<Material> {
public:
    MaterialPlugin(const PString& name, const UId& id);
    MaterialPlugin(const MaterialPlugin& o);
    ~MaterialPlugin() override;
    MaterialPlugin& operator=(const MaterialPlugin& o);

    // slot 4
    virtual bool __cdecl IsSupported(Display* display, PString& reason);
    // slot 5
    virtual Material* __cdecl CreateInstance(MaterialManager* manager, RenderKernel* kernel) const = 0;

protected:
    // slot 3
    Material* __cdecl CreateInstance() const override;
};

JET_LAYOUT32(sizeof(MaterialPlugin) == 0x20);

// The default plugin: IsSupported false, no factory and no extra virtuals (the original's 0x100079e0 stubs).
class MaterialPluginX : public MaterialPlugin {
public:
    MaterialPluginX(const PString& name, const UId& id);
    MaterialPluginX(const MaterialPluginX& o);
    ~MaterialPluginX() override;
    MaterialPluginX& operator=(const MaterialPluginX& o);

    bool __cdecl IsSupported(Display* display, PString& reason) override;
    Material* __cdecl CreateInstance(MaterialManager* manager, RenderKernel* kernel) const override;
};

class StdMaterialPlugin : public MaterialPlugin {
public:
    StdMaterialPlugin();
    StdMaterialPlugin(const StdMaterialPlugin& o);
    ~StdMaterialPlugin() override;
    StdMaterialPlugin& operator=(const StdMaterialPlugin& o);

    bool __cdecl IsSupported(Display* display, PString& reason) override;
    Material* __cdecl CreateInstance(MaterialManager* manager, RenderKernel* kernel) const override;

protected:
    // slot 6; the original's exported symbol is protected (??CreatePluginResources...MBAX)
    virtual void __cdecl CreatePluginResources(RenderKernel* kernel) const;
    // 0x71120: the named constructor the template (and derived plugins) use
    StdMaterialPlugin(const PString& name, const UId& id);
};

class AutoTexMaterialPlugin : public StdMaterialPlugin {
public:
    AutoTexMaterialPlugin();
    AutoTexMaterialPlugin(const AutoTexMaterialPlugin& o);
    explicit AutoTexMaterialPlugin(const PString& name, const UId& id);
    ~AutoTexMaterialPlugin() override;
    AutoTexMaterialPlugin& operator=(const AutoTexMaterialPlugin& o);

    bool __cdecl IsSupported(Display* display, PString& reason) override;
    Material* __cdecl CreateInstance(MaterialManager* manager, RenderKernel* kernel) const override;

    RenderingCache* cache0;  // +0x20 MaterialChunkCache (the template's three cache slots)
    RenderingCache* cache1;  // +0x24 AlphaBlendCache
    RenderingCache* cache2;  // +0x28 AlphaMaskCache
};
JET_LAYOUT32(sizeof(AutoTexMaterialPlugin) == 0x2c);

class NoTextureMaterialPlugin : public MaterialPluginX {
public:
    NoTextureMaterialPlugin();
    NoTextureMaterialPlugin(const NoTextureMaterialPlugin& o);
    explicit NoTextureMaterialPlugin(const PString& name, const UId& id);
    ~NoTextureMaterialPlugin() override;
    NoTextureMaterialPlugin& operator=(const NoTextureMaterialPlugin& o);

    bool __cdecl IsSupported(Display* display, PString& reason) override;
    Material* __cdecl CreateInstance(MaterialManager* manager, RenderKernel* kernel) const override;
protected:
    void __cdecl CreatePluginResources(RenderKernel* kernel) const;
};

class GenericShaderMaterialPlugin : public MaterialPluginX {
public:
    GenericShaderMaterialPlugin();
    GenericShaderMaterialPlugin(const GenericShaderMaterialPlugin& o);
    explicit GenericShaderMaterialPlugin(const PString& name, const UId& id);
    ~GenericShaderMaterialPlugin() override;
    GenericShaderMaterialPlugin& operator=(const GenericShaderMaterialPlugin& o);

    bool __cdecl IsSupported(Display* display, PString& reason) override;
    Material* __cdecl CreateInstance(MaterialManager* manager, RenderKernel* kernel) const override;
protected:
    void __cdecl CreatePluginResources(RenderKernel* kernel) const;
};

class OneTextureMaterialPlugin : public MaterialPluginX {
public:
    OneTextureMaterialPlugin();
    OneTextureMaterialPlugin(const OneTextureMaterialPlugin& o);
    explicit OneTextureMaterialPlugin(const PString& name, const UId& id);
    ~OneTextureMaterialPlugin() override;
    OneTextureMaterialPlugin& operator=(const OneTextureMaterialPlugin& o);

    bool __cdecl IsSupported(Display* display, PString& reason) override;
    Material* __cdecl CreateInstance(MaterialManager* manager, RenderKernel* kernel) const override;
protected:
    void __cdecl CreatePluginResources(RenderKernel* kernel) const;
};

class OneTextureNoAlphaMaterialPlugin : public OneTextureMaterialPlugin {
public:
    OneTextureNoAlphaMaterialPlugin();
    OneTextureNoAlphaMaterialPlugin(const OneTextureNoAlphaMaterialPlugin& o);
    explicit OneTextureNoAlphaMaterialPlugin(const PString& name, const UId& id);
    ~OneTextureNoAlphaMaterialPlugin() override;
    OneTextureNoAlphaMaterialPlugin& operator=(const OneTextureNoAlphaMaterialPlugin& o);

    bool __cdecl IsSupported(Display* display, PString& reason) override;
    Material* __cdecl CreateInstance(MaterialManager* manager, RenderKernel* kernel) const override;
protected:
    void __cdecl CreatePluginResources(RenderKernel* kernel) const;
};

class ColorOneTextureMaterialPlugin : public OneTextureMaterialPlugin {
public:
    ColorOneTextureMaterialPlugin();
    ColorOneTextureMaterialPlugin(const ColorOneTextureMaterialPlugin& o);
    explicit ColorOneTextureMaterialPlugin(const PString& name, const UId& id);
    ~ColorOneTextureMaterialPlugin() override;
    ColorOneTextureMaterialPlugin& operator=(const ColorOneTextureMaterialPlugin& o);

    bool __cdecl IsSupported(Display* display, PString& reason) override;
    Material* __cdecl CreateInstance(MaterialManager* manager, RenderKernel* kernel) const override;
};

class MVHSelectMaterialPlugin : public OneTextureMaterialPlugin {
public:
    MVHSelectMaterialPlugin();
    MVHSelectMaterialPlugin(const MVHSelectMaterialPlugin& o);
    explicit MVHSelectMaterialPlugin(const PString& name, const UId& id);
    ~MVHSelectMaterialPlugin() override;
    MVHSelectMaterialPlugin& operator=(const MVHSelectMaterialPlugin& o);

    bool __cdecl IsSupported(Display* display, PString& reason) override;
    Material* __cdecl CreateInstance(MaterialManager* manager, RenderKernel* kernel) const override;
protected:
    void __cdecl CreatePluginResources(RenderKernel* kernel) const;
};

class ReflectiveMaterialPlugin : public OneTextureMaterialPlugin {
public:
    ReflectiveMaterialPlugin();
    ReflectiveMaterialPlugin(const ReflectiveMaterialPlugin& o);
    explicit ReflectiveMaterialPlugin(const PString& name, const UId& id);
    ~ReflectiveMaterialPlugin() override;
    ReflectiveMaterialPlugin& operator=(const ReflectiveMaterialPlugin& o);

    bool __cdecl IsSupported(Display* display, PString& reason) override;
    Material* __cdecl CreateInstance(MaterialManager* manager, RenderKernel* kernel) const override;
protected:
    void __cdecl CreatePluginResources(RenderKernel* kernel) const;
};

class GlossMaterialPlugin : public ReflectiveMaterialPlugin {
public:
    GlossMaterialPlugin();
    GlossMaterialPlugin(const GlossMaterialPlugin& o);
    explicit GlossMaterialPlugin(const PString& name, const UId& id);
    ~GlossMaterialPlugin() override;
    GlossMaterialPlugin& operator=(const GlossMaterialPlugin& o);

    bool __cdecl IsSupported(Display* display, PString& reason) override;
    Material* __cdecl CreateInstance(MaterialManager* manager, RenderKernel* kernel) const override;
protected:
    void __cdecl CreatePluginResources(RenderKernel* kernel) const;
};

// The template base the AutoTex plugin's factory export belongs to. Its three rendering-cache pointers are the
// +0x20/+0x24/+0x28 fields StdMaterialPlugin already owns (0xd6a0/0x71120); the destructor is StdMaterialPlugin's
// (the original's 0xd7a0 is a jump to it), so only the constructors/assignment and the factory are template code.
template <class T>
class StdMaterialPluginTemplate : public StdMaterialPlugin {
public:
    StdMaterialPluginTemplate();
    StdMaterialPluginTemplate(const PString& name, const UId& id);
    StdMaterialPluginTemplate(const StdMaterialPluginTemplate& o);
    ~StdMaterialPluginTemplate() override;
    StdMaterialPluginTemplate& operator=(const StdMaterialPluginTemplate& o);

    Material* __cdecl CreateInstance(MaterialManager* manager, RenderKernel* kernel) const override;
    void __cdecl CreatePluginResources(RenderKernel* kernel) const override;

    RenderingCache* cache0;  // +0x20 MaterialChunkCache
    RenderingCache* cache1;  // +0x24 AlphaBlendCache
    RenderingCache* cache2;  // +0x28 AlphaMaskCache
};
JET_LAYOUT32(sizeof(StdMaterialPluginTemplate<AutoTexMaterial>) == 0x2c);

}  // namespace Jet
