// RenderSystem_7c, declared as far as the modules built so far use it (Render2DCore). The layer classes have their
// TS12 layout and vtable order (RenderSystem 0x1006f124 / 0x1007195c); Display, Texture and the kernel are opaque and
// reached through VCall wrappers by their vtable offsets.
#pragma once

#include <jet/FastDynamicArray.h>
#include <jet/KernelStandard.h>
#include <jet/Math.h>
#include <jet/PString.h>
#include <jet/RenderTypes.h>
#include <jet/SmallTypes.h>
#include <jet/VCall.h>

namespace Jet {

class Bitmap;
class DisplaySelector;
class DisplayTexture;
class LayerGroup;
class LayerManager;
class MaterialManager;
class RenderKernel;
class SceneGraph;
class SystemResourceManager;
class SystemResourceViewer;
class Texture;
class TextureManager;
class VertexBufferManager;
template <class T> class IList;

namespace Src {
enum Factor {
    SRC_ZERO = 0,
    SRC_ONE = 1,
    SRC_DST_COLOR = 2,
    SRC_ONE_MINUS_DST_COLOR = 3,
    SRC_SRC_ALPHA = 4,
    SRC_ONE_MINUS_SRC_ALPHA = 5,
    SRC_DST_ALPHA = 6,
    SRC_ONE_MINUS_DST_ALPHA = 7,
    SRC_ALPHA_SATURATE = 8
};
}
namespace Dst {
enum Factor {
    DST_ZERO = 0,
    DST_ONE = 1,
    DST_SRC_COLOR = 2,
    DST_ONE_MINUS_SRC_COLOR = 3,
    DST_SRC_ALPHA = 4,
    DST_ONE_MINUS_SRC_ALPHA = 5,
    DST_DST_ALPHA = 6,
    DST_ONE_MINUS_DST_ALPHA = 7
};
}
enum DFunction {
    DFUNCTION_NEVER = 0,
    DFUNCTION_LESS = 1,
    DFUNCTION_LESS_EQUAL = 2,
    DFUNCTION_EQUAL = 3,
    DFUNCTION_GREATER_EQUAL = 4,
    DFUNCTION_GREATER = 5,
    DFUNCTION_NOT_EQUAL = 6,
    DFUNCTION_ALWAYS = 7
};
enum DBlendEquation { DBLEND_ADD = 0, DBLEND_SUBTRACT = 1, DBLEND_REVERSE_SUBTRACT = 2, DBLEND_MIN = 3, DBLEND_MAX = 4 };
enum DFogMode { DFOG_NONE = 0, DFOG_LINEAR = 1, DFOG_EXP = 2, DFOG_EXP2 = 3 };
enum DFace : int;
enum DOperation : int;
enum DPolygonOffsetMode : int;
enum DTexGenMode : int;
enum DCull : int { DCULL_NONE = 0 };
enum DColorMaterialTypes : int;
enum PointPosition { POINT_CENTRE = 0, POINT_CORNER = 1 };

// the device-independent renderer (implemented by RenderDirectX9 / RenderOpenGL). Its virtual slots used so far:
//   +0x80 float GetTexelOffset()   +0x98 Texture* CreateTexture()   +0xa0 SetTexture(stage)
//   +0xe0 DrawUser(format id, primitive, ?, vertices, count)   +0xe4 Draw(vertex declaration, primitive, ?, vertices, count)
//   +0x10c vertex declaration GetDeclaration(format id)
class Display {
public:
    void __cdecl SetLightingMode(bool on);
    void __cdecl SetAlphaBlendFunction(Src::Factor src, Dst::Factor dst);
    void __cdecl GetAlphaBlendFunction(Src::Factor& src, Dst::Factor& dst) const;
    DBlendEquation __cdecl GetAlphaBlendEquation() const;
    void __cdecl SetAlphaBlendEquation(DBlendEquation equation);
    void __cdecl SetAlphaTestFunction(DFunction function, float reference);
    void __cdecl SetAlphaTestMode(bool on);
    bool __cdecl GetAlphaTestMode() const;
    float __cdecl GetAlphaTestReference() const;
    DFunction __cdecl GetAlphaTestFunction() const;
    bool __cdecl GetAlphaBlendMode() const;
    void __cdecl SetAlphaBlendMode(bool on);
    bool __cdecl GetLightingMode() const;
    bool __cdecl GetSeperateSpecularMode();
    void __cdecl SetSeperateSpecularMode(bool on);
    bool __cdecl GetLocalViewSpecularMode() const;
    void __cdecl SetLocalViewSpecularMode(bool on);
    DFogMode __cdecl GetFogMode() const;
    void __cdecl SetFogMode(DFogMode mode);
    float __cdecl GetFogDensity() const;
    void __cdecl SetFogDensity(float density);
    void __cdecl GetFogRange(float& start, float& end) const;
    void __cdecl SetFogRange(float start, float end);
    const Color& __cdecl GetFogColor() const;
    void __cdecl SetFogColor(const Color& color);
    unsigned long __cdecl GetColorWriteMask() const;
    void __cdecl SetColorWriteMask(unsigned long mask);
    bool __cdecl GetDepthWriteMode() const;
    void __cdecl SetDepthWriteMode(bool on);
    bool __cdecl GetDepthTestMode() const;
    void __cdecl SetDepthTestMode(bool on);
    DFunction __cdecl GetDepthFunction() const;
    void __cdecl SetDepthFunction(DFunction function);
    void __cdecl SetDepthClamp(bool on);
    bool __cdecl GetDepthClamp() const;
    bool __cdecl GetWireFrameMode() const;
    bool __cdecl GetOutlinesMode() const;
    void __cdecl SetOutlinesMode(bool on);
    bool __cdecl GetVertexNormalsMode() const;
    void __cdecl SetVertexNormalsMode(bool on);
    bool __cdecl GetTriangleNormalsMode() const;
    void __cdecl SetTriangleNormalsMode(bool on);
    float __cdecl GetNormalLength() const;
    void __cdecl SetNormalLength(float length);
    bool __cdecl GetLocalAxesMode() const;
    void __cdecl SetLocalAxesMode(bool on);
    bool __cdecl GetChunkColorMode() const;
    void __cdecl SetChunkColorMode(bool on);
    bool __cdecl GetChunkVertexCountMode() const;
    void __cdecl SetChunkVertexCountMode(bool on);
    bool __cdecl GetDynamicGeometryDisplayMode() const;
    void __cdecl SetDynamicGeometryDisplayMode(bool on);
    bool __cdecl GetStaticGeometryDisplayMode() const;
    void __cdecl SetStaticGeometryDisplayMode(bool on);
    bool __cdecl SetLightMode(int index, bool on);
    bool __cdecl GetLightMode(int index) const;
    int __cdecl GetNumberOfHardwareLights() const;
    void __cdecl DisableAllLights();
    bool __cdecl SetLightState(int index, const DisplayLight& light, bool on);
    const Color& __cdecl GetGlobalAmbient() const;
    unsigned long __cdecl GetGlobalAmbientConverted() const;
    void __cdecl SetGlobalAmbient(const Color& color);
    void __cdecl SetScissorMode(bool on);
    bool __cdecl GetScissorMode() const;
    void __cdecl SetCullMode(DCull mode);
    DCull __cdecl GetCullMode() const;
    void __cdecl SetReflectionCull(bool on);
    bool __cdecl GetReflectionCull() const;
    DColorMaterialTypes __cdecl GetColorMaterialMode();
    void __cdecl SetColorMaterialMode(DColorMaterialTypes mode);
    void __cdecl SetWireFrameMode(bool on);
    void __cdecl SetViewport(const Rect& rect);
    const Rect& __cdecl GetViewport() const;
    void __cdecl SetScissor(const Rect& rect);
    Rect __cdecl GetScissor();
    void __cdecl Set2DView(float left, float right, float bottom, float top, float nearPlane, float farPlane);
    void __cdecl Set2DViewNormalized(bool normalized);
    void __cdecl SetClearColor(const Color& color);
    const Color& __cdecl GetClearColor() const;
    void __cdecl SetClearDepth(float depth);
    float __cdecl GetClearDepth() const;
    void __cdecl SetClearStencil(long stencil);
    long __cdecl GetClearStencil() const;
    bool __cdecl GetStencilTestMode() const;
    void __cdecl SetStencilTestMode(bool on, bool twoSided);
    unsigned long __cdecl GetStencilWriteMask(DFace face) const;
    void __cdecl SetStencilWriteMask(unsigned long mask, DFace face);
    void __cdecl SetStencilFunction(DFunction function, int reference, unsigned long mask, DFace face);
    void __cdecl SetStencilOperation(DOperation fail, DOperation depthFail, DOperation pass, DFace face);
    void __cdecl SetStencilSetup(DFunction function, int reference, unsigned long mask,
                                 DOperation fail, DOperation depthFail, DOperation pass, DFace face);
    void __cdecl SetPolygonOffset(float factor, float units);
    void __cdecl EnablePolygonOffset(DPolygonOffsetMode mode, bool on);
    int __cdecl GetMaxTextureCombinerStages();
    int __cdecl GetMaxTextureImageUnits();
    int __cdecl GetMaxTextureCoordUnits();
    bool __cdecl GetAlphaToCoverage();
    bool __cdecl SupportsAntiAliasing() const;
    bool __cdecl GetAntiAliasing() const;
    void __cdecl SetAntiAliasing(bool on);
    bool __cdecl TextureCombinerSupport() const;
    bool __cdecl VertexProgramSupport() const;
    bool __cdecl HardwareAnimationSupport() const;
    VertexBufferManager* __cdecl GetVertexBufferManager() const;
    DisplaySelector* __cdecl GetDisplaySelector() const;
    OS* __cdecl GetOS() const;
    bool __cdecl IsRenderableState() const;
    unsigned int __cdecl GetCurrentFrame() const;
    float __cdecl GetPhysicalScreenRatio();
    void __cdecl Set3DTransitionalView(float a, float b, float c, float d, float e, float f, float g);
    void __cdecl Set3DView(float width, float height, float nearDistance, float farDistance);
    void __cdecl Set3DViewOffset(float width, float height, float nearDistance, float farDistance, float offset);
    bool __cdecl IsNormalized2DViewport() const;
    void __cdecl SetTextureCoordGen(int stage, DTexGenMode mode, unsigned coordinates);
    void __cdecl SetRenderTarget(DisplayTexture* texture, unsigned index);
    const Size& __cdecl GetTargetSize() const;
    const Size& __cdecl GetWindowSize() const;
    static unsigned long __cdecl ConvertColor(const Color& c);
    static void __cdecl ConvertColorArray(const Color* source, unsigned long* dest, int count);
    static void __cdecl ConvertColorArray(unsigned long* source, Color* dest, int count);
    static void __cdecl ConvertColorArray(unsigned long* colors, int count);
    static DFormatId __cdecl GetInternalColorFormat();

    // the render state block (+0x54): +0x68 src factor, +0x6c dst factor, +0xa0 flags (bit 0 wire frame, bit 5
    // alpha test), +0xcc alpha reference, +0xd0 alpha function
    const unsigned char* State() const { return *reinterpret_cast<unsigned char* const*>(Raw() + 0x54); }

    float TexelOffset() { return VCall<float>(this, 0x80); }
    Texture* CreateTexture() { return VCall<Texture*>(this, 0x98); }
    void SetTexture(int stage) { VCall<void>(this, 0xa0, stage); }
    void DrawUser(int format, int primitive, int kind, void* vertices, int count)
    {
        VCall<void>(this, 0xe0, format, primitive, kind, vertices, count);
    }
    void Draw(int declaration, int primitive, int kind, void* vertices, int count)
    {
        VCall<void>(this, 0xe4, declaration, primitive, kind, vertices, count);
    }
    int GetDeclaration(int format) { return VCall<int>(this, 0x10c, format); }

private:
    const char* Raw() const { return reinterpret_cast<const char*>(this); }
};

// the render kernel a layer belongs to: +0x7c its display, +0x94 its texture manager
class RenderKernel {
public:
    Display* GetDisplay() const { return *reinterpret_cast<Display* const*>(reinterpret_cast<const char*>(this) + 0x7c); }
    void* TextureManager() const { return *reinterpret_cast<void* const*>(reinterpret_cast<const char*>(this) + 0x94); }
    Display* __cdecl GetDisplay();
    OS* __cdecl GetOS();
    SceneGraph* __cdecl GetSceneGraph();
    void __cdecl SetSceneGraph(SceneGraph* graph);
    void __cdecl DestroySceneGraph();
    void __cdecl AddSystemResource(SystemResourceManager* resource);
    void __cdecl RemoveSystemResource(SystemResourceManager* resource);
    Jet::TextureManager* __cdecl GetTextureManager();
    void __cdecl SetTextureManager(Jet::TextureManager* manager);
    MaterialManager* __cdecl GetMaterialManager();
    void __cdecl SetMaterialManager(MaterialManager* manager);
};

class MaterialFileSupport {
public:
    MaterialFileSupport& operator=(const MaterialFileSupport&);
    static DFunction __cdecl ConvertDFunction(const PString& name);
    static Src::Factor __cdecl ConvertSrc(const PString& name);
    static Dst::Factor __cdecl ConvertDst(const PString& name);
private:
    MaterialFileSupport();
};

using SystemResourceViewerList = FastDynamicArray<SystemResourceViewer*, 13, Less<SystemResourceViewer*, SystemResourceViewer*>>;

class SystemResourceViewer {
public:
    const PString& __cdecl GetName() const;
    void __cdecl SetName(const PString& name);

private:
    friend struct LayerLifetimeAccess;
    void* vptr;
    RenderKernel* kernel;
    PString name;
};
JET_LAYOUT32(sizeof(SystemResourceViewer) == 0x0c);

namespace Detail {
struct LayerLink {
    LayerLink* next;
    LayerLink* prev;
};
}

class Layer {
public:
    Layer(LayerGroup* group, const PString& name, int priority, bool flag);
    Layer(const PString& name, int priority, bool flag);
    Layer(const Layer& o);
    Layer& operator=(const Layer& o);

    virtual ~Layer();                                                  // 0
    virtual void __cdecl RenderLayer(SystemResourceViewerList& list);  // 1
    virtual void __cdecl UpdateLayer(float dt);                        // 2
    virtual bool __cdecl Collides(const Vector2& p) const;             // 4 (overloads go in reverse)
    virtual bool __cdecl Collides(const Vector2& a, const Vector2& b) const;  // 3
    virtual void __cdecl Reset();                                      // 5

    int __cdecl GetPriority() const;
    bool __cdecl IsVisible() const;
    bool __cdecl IsGroup() const;
    void __cdecl SetVisible(bool visible);
    int __cdecl operator<(const Layer& other) const;
    const PString& __cdecl GetName() const;
    void __cdecl SetName(const PString& name);
    LayerGroup* __cdecl GetParent() const;
    LayerManager* __cdecl GetLayerManager();
    RenderKernel* __cdecl GetRenderKernel() const;
    void __cdecl SetRenderKernel(RenderKernel* value);
    void __cdecl SetParent(LayerGroup* parent);
    void __cdecl SetPriority(int value);
    void __cdecl BringToFront();
    void __cdecl BringToBack();
    void __cdecl ConvertToWindow(Point& point) const;
    void __cdecl ConvertToWindowAndClip(Rect& rect) const;

protected:
    virtual void __cdecl Render() = 0;        // 6
    virtual void __cdecl Update(float dt);    // 7
    virtual void __cdecl Begin();             // 8
    virtual void __cdecl End();               // 9
    virtual bool __cdecl CanRender();         // 10

    RenderKernel* Kernel() const { return kernel; }

public:
    JET_HEAP_OBJECT  // layers live in Jet's heap (their deleting destructors free with Alloc::Delete)

protected:
    LayerGroup* Group() const { return group; }

private:
    friend class LayerGroup;
    friend struct LayerLifetimeAccess;
    Detail::LayerLink link; // +0x04
    PString name;          // +0x0c
    int priority;          // +0x10
    unsigned char flags;   // +0x14
    RenderKernel* kernel;  // +0x18
    LayerGroup* group;     // +0x1c
};
JET_LAYOUT32(sizeof(Layer) == 0x20);

class Overlay : public Layer {
public:
    Overlay(LayerGroup* group, const PString& name, int priority, bool flag);
    Overlay(const PString& name, int priority, bool flag);
    Overlay(const Overlay& o);
    ~Overlay() override;
    Overlay& operator=(const Overlay& o);

    void __cdecl SetViewPortMode(bool enabled);
    void __cdecl SetClipping(const Rect& rect);
    Rect __cdecl GetClipping() const;

protected:
    void __cdecl Begin() override;
    void __cdecl End() override;

    unsigned char overlayFlags;  // +0x20 bit 1: viewport mode (enabled by sprite layers)
    Rect clipping;              // +0x24
    unsigned char savedDepthFlags;  // +0x34, depth enable/write
    int savedCullMode;              // +0x38
};
JET_LAYOUT32(sizeof(Overlay) == 0x3c);

class ClearLayer : public Layer {
public:
    Color __cdecl GetClearColor();
    void __cdecl SetClearColor(const Color& color);
    void __cdecl Render() override;

private:
    Color clearColor;  // +0x20
};
JET_LAYOUT32(sizeof(ClearLayer) == 0x30);

class LayerGroup : public Layer {
public:
    void __cdecl Add(Layer* layer);
    void __cdecl Remove(Layer* layer);
    void __cdecl BringToFront(Layer* layer);
    void __cdecl BringToBack(Layer* layer);
    Layer* __cdecl FindLayer(const PString& name);
    Layer* __cdecl GetLayerAt(const Vector2& point);
    Layer* __cdecl GetLayerUnder(const Vector2& a, const Vector2& b);
    const IList<Layer>& __cdecl GetLayerList() const;
    const Rect& __cdecl GetViewRect() const;
    void __cdecl SetViewRect(const Rect& rect);
    void __cdecl ConvertToWindow(Point& point) const;
    void __cdecl ConvertToWindowAndClip(Rect& rect) const;
    void __cdecl GetGroupViewRect(Rect& out);
    void __cdecl GetClippedGroupViewRect(Rect& out);

    void __cdecl UpdateLayer(float dt) override;
    void __cdecl RenderLayer(SystemResourceViewerList& list) override;
    void __cdecl Reset() override;

protected:
    void __cdecl Render() override;

private:
    friend struct LayerLifetimeAccess;
    Detail::LayerLink sentinel; // +0x20
    int count;                  // +0x28
    Rect view;                  // +0x2c

    static Layer* FromLink(Detail::LayerLink* link);
    void Insert(Layer* layer, bool beforeEqual);
    void Unlink(Layer* layer);
    Size DisplaySize();
};
JET_LAYOUT32(sizeof(LayerGroup) == 0x3c);

}  // namespace Jet
