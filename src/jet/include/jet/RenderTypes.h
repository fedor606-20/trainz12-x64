// RenderSystem_7c unit U1 (notes/RenderSystem.md): the small value types of the display layer. Layouts and behaviour
// follow the TS12 assembly (addresses per member in RenderTypes.cpp). Statics that are module state stay the
// original's until U9 and are imported from RenderSystem_7c_orig.dll.
#pragma once

#include <jet/KernelStandard.h>
#include <jet/Math.h>
#include <jet/PString.h>
#include <jet/SmallTypes.h>

namespace Jet {

class Display;
class Material;
class SharedContext;
class ViewPoint;
class ViewPort;
class DebugViewPort;
class ProjectorCache;
class VIPConstHelper;

// a strided view of one vertex item; Base is the const flavour of the same view
template <class T, class Base>
class VertexItemPtr {
public:
    unsigned int stride;
    T* data;
};

enum DFormatId { DFORMAT_UNKNOWN = 0, DFORMAT_DEFAULT = 1, DFORMAT_CUSTOM = 20 };
enum DLightTypes { DLIGHT_DEFAULT = 3 };
enum TextureTarget {};
enum ModeType { MODE_COLOR = 0, MODE_ALPHA = 1, MODE_STAGE = 2, MODE_NONE = 3 };
enum DTexFuncType {};
enum Source {};

// a pixel format: its id and, per channel, the shift and the bit count
class DisplayFormat {
public:
    DisplayFormat();
    DisplayFormat(DFormatId id);
    DisplayFormat& operator=(const DisplayFormat& o);

    bool __cdecl operator==(const DisplayFormat& o) const;
    int __cdecl GetBytes() const;
    void __cdecl Set(DFormatId id);
    void __cdecl Classify() const;  // finds the id of a custom layout (FORMAT_CUSTOM when none matches)
    static void __cdecl GetMaskPlacement(unsigned long mask, unsigned char& shift, unsigned char& bits);
    static const char* __cdecl GetFormatIDString(DFormatId id);

protected:
    bool __cdecl Compare(const DisplayFormat& o) const;
    void __cdecl SetValues(unsigned char bits, unsigned char redShift, unsigned char redBits, unsigned char greenShift,
                           unsigned char greenBits, unsigned char blueShift, unsigned char blueBits,
                           unsigned char alphaShift, unsigned char alphaBits, unsigned char depthBits,
                           unsigned char stencilBits);
    void __cdecl SetDepthValues(unsigned char bits, unsigned char stencilBits, unsigned char depthBits);

public:
    mutable unsigned char id;  // +0 (a DFormatId)
    unsigned char bits;        // +1
    unsigned char redShift, redBits, greenShift, greenBits, blueShift, blueBits, alphaShift, alphaBits;
    unsigned char stencilBits;  // +0xa
    unsigned char depthBits;    // +0xb
};
JET_LAYOUT32(sizeof(DisplayFormat) == 0xc);

class RenderTextureConstruct {
public:
#ifdef JET_RS_BUILD
    RenderTextureConstruct();
#else
    // Render2DCore keeps the inline form it was verified with
    RenderTextureConstruct() : shared(nullptr), target(), useMainContext(false), createContext(false),
                               sharedPBuffer(false), activeDepthLevel(0) {}
#endif
    RenderTextureConstruct(TextureTarget target, bool useMainContext, bool createContext, bool sharedPBuffer,
                           const SharedContext* shared);
    RenderTextureConstruct(const RenderTextureConstruct& o);
    ~RenderTextureConstruct();
    RenderTextureConstruct& operator=(const RenderTextureConstruct& o);

    const SharedContext* __cdecl GetSharedContext() const;
    TextureTarget __cdecl GetRenderType() const;
    bool __cdecl GetUseMainContext() const;
    bool __cdecl GetCreateContext() const;
    bool __cdecl GetSharedPBuffer() const;
    int __cdecl GetActiveDepthLevel() const;
    void __cdecl SetActiveDepthLevel(int level);

private:
    const SharedContext* shared;  // +0, reference counted (SharedContext is a ReferenceCount)
    TextureTarget target;         // +4
    bool useMainContext;          // +8
    bool createContext;           // +9
    bool sharedPBuffer;           // +0xa
    int activeDepthLevel;         // +0xc
};
JET_LAYOUT32(sizeof(RenderTextureConstruct) == 0x10);

struct DisplayMaterial {
    DisplayMaterial();
    DisplayMaterial(const DisplayMaterial& o);
    DisplayMaterial& operator=(const DisplayMaterial& o);
    bool __cdecl operator!=(const DisplayMaterial& o);

    Color ambient, diffuse, specular, emissive;
    float power;
};
JET_LAYOUT32(sizeof(DisplayMaterial) == 0x44);

class DisplayLight {
public:
    DisplayLight();
    DisplayLight(DLightTypes type);
    DisplayLight(const DisplayLight& o);
    virtual ~DisplayLight();
    virtual void __cdecl BatchCalculateDiffuse(
        Display* display, int count, VertexItemPtr<Vector3, VertexItemPtr<const Vector3, VIPConstHelper>> positions,
        VertexItemPtr<Vector3, VertexItemPtr<const Vector3, VIPConstHelper>> normals,
        VertexItemPtr<unsigned long, VertexItemPtr<const unsigned long, VIPConstHelper>> colors,
        const Matrix4& world, const Matrix4& view, Material* material) const;
    // slots 2-5 have no exported name; the base answers 0
    virtual int __cdecl Slot2() const;
    virtual int __cdecl Slot3() const;
    virtual int __cdecl Slot4() const;
    virtual int __cdecl Slot5() const;
    virtual bool __cdecl operator==(const DisplayLight& o) const;
    virtual DisplayLight& __cdecl operator=(const DisplayLight& o);

    void __cdecl InitInstance(DisplayLight& o) const;  // copies everything but the vftable into o
    DLightTypes __cdecl GetLightType() const;
    const Color& __cdecl GetAmbient() const;
    const Color& __cdecl GetDiffuse() const;
    const Color& __cdecl GetSpecular() const;
    void __cdecl SetAmbient(const Color& c);
    void __cdecl SetDiffuse(const Color& c);
    void __cdecl SetSpecular(const Color& c);
    Vector3 __cdecl GetAttenuation() const;
    float __cdecl GetConstantAttenuation() const;
    float __cdecl GetLinearAttenuation() const;
    float __cdecl GetQuadraticAttenuation() const;
    void __cdecl SetConstantAttenuation(float v);
    void __cdecl SetLinearAttenuation(float v);
    void __cdecl SetQuadraticAttenuation(float v);
    float __cdecl GetSpotExponent() const;
    float __cdecl GetSpotCutoff() const;
    float __cdecl GetRange() const;
    void __cdecl SetSpotExponent(float v);
    void __cdecl SetSpotCutoff(float v);
    void __cdecl SetRange(float v);

    JET_HEAP_OBJECT

private:
    DLightTypes type;  // +4
    Color ambient;     // +8
    Color diffuse;     // +0x18
    Color specular;    // +0x28
    float constantAttenuation, linearAttenuation, quadraticAttenuation;  // +0x38
    float spotExponent;  // +0x44
    float spotCutoff;    // +0x48
    float range;         // +0x4c
};
JET_LAYOUT32(sizeof(DisplayLight) == 0x50);

// a scene graph node's culling and visit marks
class Element {
public:
    Element();
    Element(const Element& o);
    virtual ~Element();
    Element& operator=(const Element& o);
    virtual void __cdecl SetVisited() const;
    virtual void __cdecl SetLastCulledFrame(unsigned int frame);
    virtual void __cdecl Cull(ViewPoint& view, const Matrix4& m);
    virtual void __cdecl Render(Display* display, ViewPort& port, Matrix4& m);
    virtual void __cdecl RenderDebug(Display* display, DebugViewPort& port, Matrix4& m, unsigned long flags);

    bool __cdecl GetCulled(const Display* display) const;  // culled unless it was this frame
    bool __cdecl IsVisited() const;
    unsigned int __cdecl GetLastCulledFrame() const;
    static void __cdecl ResetVisited();

    JET_HEAP_OBJECT

private:
    mutable unsigned int visited;  // +4
    unsigned int lastCulledFrame;  // +8

    static unsigned int currentVisited;
};
JET_LAYOUT32(sizeof(Element) == 0xc);

class Projector {
public:
    Projector();
    Projector(const Projector& o);
    virtual ~Projector();
    Projector& operator=(const Projector& o);
    virtual int __cdecl Slot1() const;  // no exported name; the base answers 0

    JET_HEAP_OBJECT

protected:
    void __cdecl InitInstance(Projector& o) const;

    static __declspec(dllimport) ProjectorCache* projectorCache;  // the original's until U9
};
JET_LAYOUT32(sizeof(Projector) == 4);

// one argument of a texture combiner stage; the padding is copied with it
struct TexArg {
    bool minus;
    unsigned char pad[3];
    ModeType mode;
    Source loc;
};

// one texture stage function, as decoded from "COLOR MODULATE TEX COLOR PREV COLOR" style text
class TextureArgs {
public:
    TextureArgs();
    TextureArgs(int stage, unsigned int multiplier, ModeType mode, DTexFuncType fn, const TexArg& a1, const TexArg& a2,
                const TexArg& a3, const TexArg& a4);
    TextureArgs(const TextureArgs& o);  // the arguments' padding is not copied
    TextureArgs& operator=(const TextureArgs& o);

    bool __cdecl Decode(char* text);  // upper-cases text in place
    ModeType __cdecl GetMode() const;
    DTexFuncType __cdecl GetFn() const;
    int __cdecl GetNumArgs() const;
    int __cdecl GetStage() const;
    unsigned int __cdecl GetMultiplier() const;
    bool __cdecl IsArgMinus(int i) const;
    Source __cdecl GetLoc(int i) const;
    ModeType __cdecl GetArgMode(int i) const;

protected:
    int __cdecl GetNextToken(char*& text, bool keepUnderscore);

private:
    ModeType mode;            // +0
    DTexFuncType fn;          // +4
    int numArgs;              // +8
    TexArg args[4];           // +0xc
    int stage;                // +0x3c
    unsigned int multiplier;  // +0x40 (1, 2 or 4)
};
JET_LAYOUT32(sizeof(TextureArgs) == 0x44);

class TextureStage {
public:
    TextureStage(Display* display);
    TextureStage(const TextureStage& o);
    ~TextureStage();
    TextureStage& operator=(const TextureStage& o);

    void __cdecl CreateStageSegment(int stage, unsigned int multiplier, ModeType mode, DTexFuncType fn,
                                    const TexArg& a1, const TexArg& a2, const TexArg& a3, const TexArg& a4);
    void* __cdecl Build();  // the display's combiner for this stage (Display slot 0x11c)

private:
    TextureArgs color;     // +0
    TextureArgs alpha;     // +0x44
    int stage;             // +0x88 (left unset by the constructor)
    int unknown8C;         // +0x8c
    Display* display;      // +0x90
};
JET_LAYOUT32(sizeof(TextureStage) == 0x94);

// the renderer's name and version and the card's, as the display reports them
class DisplayDescriptor {
public:
    enum APIType {};

    DisplayDescriptor(const DisplayDescriptor& o);
    virtual ~DisplayDescriptor();
    DisplayDescriptor& operator=(const DisplayDescriptor& o);

    APIType __cdecl GetApiType() const;
    const PString& __cdecl GetApiName() const;
    const PString& __cdecl GetApiVersionString() const;
    float __cdecl GetApiVersionNumber() const;
    const PString& __cdecl GetCardVendorName() const;
    const PString& __cdecl GetCardName() const;

    JET_HEAP_OBJECT

protected:
    DisplayDescriptor(APIType type);
    void __cdecl SetData(const PString& apiName, const PString& apiVersion, const PString& vendor, const PString& card,
                         float versionNumber);

private:
    APIType apiType;          // +4
    PString apiName;          // +8
    PString apiVersion;       // +0xc
    float apiVersionNumber;   // +0x10
    PString cardVendorName;   // +0x14
    PString cardName;         // +0x18
};
JET_LAYOUT32(sizeof(DisplayDescriptor) == 0x1c);

}  // namespace Jet
