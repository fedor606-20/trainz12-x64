// RenderSystem_7c, declared as far as the modules built so far use it (Render2DCore). The layer classes have their
// TS12 layout and vtable order (RenderSystem 0x1006f124 / 0x1007195c); Display, Texture and the kernel are opaque and
// reached through VCall wrappers by their vtable offsets.
#pragma once

#include <jet/DynamicArray.h>
#include <jet/FastDynamicArray.h>
#include <jet/KernelStandard.h>
#include <jet/Math.h>
#include <jet/PString.h>
#include <jet/Geometry.h>
#include <jet/RenderTypes.h>
#include <jet/SmallTypes.h>
#include <jet/VCall.h>

struct Matrix4;
class TextureCombiner;

namespace Jet {

class Bitmap;
class DisplaySelector;
class DisplayTexture;
class DisplayContext;

class Layer;
class LayerGroup;
class LayerManager;
class MaterialManager;
class ModifierUpdateTask;
class RenderKernel;
class SceneGraph;
class StaticBuffer;
class SystemResourceViewer;
class SystemResourceManager;
class Texture;
class TextureCombinerManager;
class TextureManager;
class VertexBufferManager;
class VertexProgramManager;
class VertexType;
enum VertexTypeID;

namespace Src {
enum Factor { SRC_ONE = 1 };
}
namespace Dst {
enum Factor { DST_ONE = 1 };
}
enum DFunction { DFUNCTION_ALWAYS = 7 };
enum DCull { DCULL_NONE = 0 };
enum DBlendEquation {};
enum DFogMode {};
enum DFace {};
enum DColorMaterialTypes {};
enum DOperation {};
enum DTexGenMode {};
enum DTexCompression {};
enum DExtension {};
enum PointPosition { POINT_CENTRE = 0, POINT_CORNER = 1 };
enum DPolygonOffsetMode {};

template <class T> class IList;

// the device-independent renderer (implemented by RenderDirectX9 / RenderOpenGL). Its virtual slots used so far:
//   +0x80 float GetTexelOffset()   +0x98 Texture* CreateTexture()   +0xa0 SetTexture(stage)
//   +0xe0 DrawUser(format id, primitive, ?, vertices, count)   +0xe4 Draw(vertex declaration, primitive, ?, vertices, count)
//   +0x10c vertex declaration GetDeclaration(format id)
class Display {
public:
    enum ScreenMode {};
    struct DisplayFrameStats {};
    struct DisplayContext { DisplayContext(); DisplayContext(const DisplayContext&); ~DisplayContext(); DisplayContext& operator=(const DisplayContext&); };
    struct VideoMode { unsigned int data[4]; };
    struct VideoFormat { unsigned int data[8]; };
    struct ViewModeParams { int data[9]; };

    void __cdecl SetLightingMode(bool on);  // ?SetLightingMode@Display@Jet@@QAAX_N@Z (the original's until U9)
    void __cdecl SetAlphaBlendFunction(Src::Factor src, Dst::Factor dst);
    void __cdecl SetAlphaTestFunction(DFunction function, float reference);
    void __cdecl SetAlphaTestMode(bool on);
    void __cdecl SetScissorMode(bool on);
    void __cdecl SetCullMode(DCull mode);
    void __cdecl SetWireFrameMode(bool on);
    const Size& GetTargetSize() const;
    static unsigned long __cdecl ConvertColor(const Color& c);
    static Color __cdecl ConvertColor(unsigned long v);
    static DFormatId __cdecl GetInternalColorFormat();

    float GetPhysicalScreenRatio();
    bool IsRenderableState() const;
    bool GetReflectionCull() const;
    float GetNormalLength() const;
    bool GetChunkColorMode() const;
    bool GetChunkVertexCountMode() const;
    bool GetDynamicGeometryDisplayMode() const;
    bool GetStaticGeometryDisplayMode() const;

    // simple/evidenced getters/setters (TS12)
    Window* GetWindow() const;
    OS* GetOS() const;
    const Size& GetWindowSize() const;
    DisplaySelector* GetDisplaySelector() const;
    ScreenMode GetCurrentScreenMode() const;
    bool GetScissorMode() const;
    const Rect& GetViewport() const;
    const Color& GetClearColor() const;
    float GetClearDepth() const;
    long GetClearStencil() const;
    DCull GetCullMode() const;
    DBlendEquation GetAlphaBlendEquation() const;
    DFogMode GetFogMode() const;
    const Color& GetFogColor() const;
    const Color& GetGlobalAmbient() const;
    unsigned long GetGlobalAmbientConverted() const;
    virtual bool GetTextureMode() const;
    virtual bool GetGlobalTextureMode() const;
    virtual int GetNumStencilBits() const;
    virtual TextureCombinerManager* GetTextureCombinerManager() const;
    bool TextureCombinerSupport() const;
    virtual VertexProgramManager* GetVertexProgramManager() const;
    bool VertexProgramSupport() const;
    bool HardwareAnimationSupport() const;
    VertexBufferManager* GetVertexBufferManager() const;
    virtual bool CheckPlatform(OS* os);
    unsigned int GetCurrentFrame() const;
    int GetNumberOfHardwareLights() const;
    virtual bool GetZBiasEnabled();
    virtual bool IsBetweenBeginAndEndFrame();
    const DisplayFrameStats& GetFrameStats() const;
    const DisplayFrameStats& GetCurrentFrameStats() const;
    int GetMaxTextureCombinerStages();
    int GetMaxTextureImageUnits();
    int GetMaxTextureCoordUnits();
    bool GetAlphaToCoverage();
    bool GetOutlinesMode() const;
    void SetOutlinesMode(bool on);
    bool GetVertexNormalsMode() const;
    void SetVertexNormalsMode(bool on);
    bool GetTriangleNormalsMode() const;
    void SetTriangleNormalsMode(bool on);
    void SetNormalLength(float length);
    bool GetLocalAxesMode() const;
    void SetLocalAxesMode(bool on);
    void SetChunkColorMode(bool on);
    void SetChunkVertexCountMode(bool on);
    void SetDynamicGeometryDisplayMode(bool on);
    void SetStaticGeometryDisplayMode(bool on);
    bool GetSeperateSpecularMode();
    bool GetLocalViewSpecularMode() const;

    bool SupportsAntiAliasing() const;
    bool GetAntiAliasing() const;
    void SetAntiAliasing(bool on);
    void GetAlphaBlendFunction(Src::Factor& src, Dst::Factor& dst) const;
    unsigned long GetColorWriteMask() const;
    bool GetDepthWriteMode() const;
    bool GetDepthTestMode() const;
    DFunction GetDepthFunction() const;
    bool GetDepthClamp() const;
    bool GetAlphaTestMode() const;
    float GetAlphaTestReference() const;
    DFunction GetAlphaTestFunction() const;
    bool GetAlphaBlendMode() const;
    bool GetLightingMode() const;
    float GetFogDensity() const;
    DColorMaterialTypes GetColorMaterialMode();
    bool GetVertexWeightMode() const;
    bool GetStencilTestMode() const;
    unsigned long GetStencilWriteMask(DFace face) const;
    bool GetWireFrameMode() const;

    void CheckResetDynamicBufferOffsets();
    void SetZBiasValues(float zBias, float slope);
    void LoadProjectionMatrix(const Matrix4& m);
    void BeginFrame(float dt);
    void EndFrame(float dt);
    const VertexType* GetBuiltInVertexType(VertexTypeID id) const;

    // simple/evidenced (batch)
    virtual void FlushState();
    void DisableAllLights();
    void SetPolygonOffset(float factor, float units);
    void EnablePolygonOffset(DPolygonOffsetMode mode, bool enable);
    bool GetAllVideoModes(const Array<VideoMode, 20>*& out) const;
    bool GetDefaultVideoMode(VideoMode& out) const;
    bool GetCurrentVideoMode(VideoMode& out) const;
    bool GetAllVideoFormats(const Array<VideoFormat, 20>*& out) const;
    bool GetCurrentVideoFormat(VideoFormat& out) const;
    bool IsNormalized2DViewport() const;
    void GetFogRange(float& start, float& end) const;
    Rect GetScissor();
    void SetScissor(const Rect& rect);
    void SetViewport(const Rect& rect);
    bool GetLightMode(int index) const;
    void SetTextureMode(bool on);
    void SetGlobalTextureMode(bool on);
    bool SetLightMode(int idx, bool on);

    // batch 2 setters
    void SetAlphaBlendEquation(DBlendEquation eq);
    void SetClearColor(const Color& c);
    void SetClearDepth(float v);
    void SetClearStencil(long v);
    void SetReflectionCull(bool on);
    void SetColorWriteMask(unsigned long v);
    void SetDepthWriteMode(bool on);
    void SetDepthTestMode(bool on);
    void SetDepthFunction(DFunction fn);
    void SetDepthClamp(bool on);
    void SetAlphaBlendMode(bool on);
    void SetSeperateSpecularMode(bool on);
    void SetLocalViewSpecularMode(bool on);
    void SetFogMode(DFogMode mode);
    void SetFogDensity(float v);
    void SetFogColor(const Color& c);
    void SetGlobalAmbient(const Color& c);
    void SetVertexWeightMode(bool on);
    void SetStencilWriteMask(unsigned long mask, DFace face);
    void SetFogRange(float start, float end);

    const char* GetExtensionString(DExtension idx) const;
    void SetRenderTarget(DisplayTexture* tex, unsigned int idx);
    void RestoreGamma();
    int GetTargetPPI() const;

    void Set3DView(float a, float b, float c, float d);
    void Set3DViewOffset(float a, float b, float c, float d, float e);
    void Set3DTransitionalView(float a, float b, float c, float d, float e, float f, float g);
    void Set2DView(float a, float b, float c, float d, float e, float f);
    void Set2DViewNormalized(bool on);
    void StartLightingUpdate(int idx, const Matrix4& m);
    void SetTextureCoordGen(int stage, DTexGenMode mode, unsigned int gen);
    void SetColorMaterialMode(DColorMaterialTypes mode);
    void SetStencilTestMode(bool test, bool write);
    void SetStencilFunction(DFunction fn, int ref, unsigned long mask, DFace face);
    void SetStencilOperation(DOperation op1, DOperation op2, DOperation op3, DFace face);
    void SetMaterialProperties(const DisplayMaterial& dm);
    void EnableProjectedTexture(int idx, bool a, bool b);
    void EndLightingUpdate();
    void SetViewModeParams(const ViewModeParams& p);
    void GetViewModeParams(ViewModeParams& p) const;
    bool CheckExtensionString(const PString& s) const;
    const char* ConvertTexCompressionString(DTexCompression idx) const;
    DTexCompression ConvertTexCompressionString(const char* s) const;

    void SetZBiasEnabled(bool on);
    void SetMaxAnisotropy(float v);
    void SetActiveTextureStage(int s);
    void SetGamma(float g, bool r);
    bool SetLightState(int idx, const DisplayLight& l, bool on);
    void SetDisplaySelector(DisplaySelector* s);
    void CriticalStartup(OS* os);
    void CriticalStartup(void* p);
    void CriticalShutdown(void* p);
    void CriticalShutdown();
    bool Init(OS* os);
    bool Open(const Options* o, bool b);
    bool ParseTextureStage(void* p, File& f);
    bool PreOpen(const Options* o);
    bool SetupVideoFormat(const Options* o, VideoFormat& f);
    void Shutdown();
    void UnreferenceResources();
    bool Update(float dt);
    void CreateNonRenderContext();
    SharedContext* CreateSharedContext() const;
    TextureCombiner* CreateTextureCombiner();
    void CreateVertexTypeObject(VertexTypeID id);
    void DrawDynamicArray(VertexTypeID id, DPrimitive prim, int a, const void* v, int c);
    StaticBuffer* AllocateStaticIndexBuffer(unsigned int n, bool b);
    StaticBuffer* AllocateStaticVertexBuffer(unsigned int a, unsigned int b);
    VertexType* CloneBuiltInVertexType(VertexTypeID id) const;
    Bitmap* Screenshot() const;
    void ConvertColorArray(unsigned int* a, int n);
    void ConvertColorArray(unsigned int* a, Color* c, int n);
    void ConvertColorArray(const Color* c, unsigned int* a, int n);
    void ConvertColorArray(VertexItemPtr<unsigned int, VertexItemPtr<const unsigned int, VIPConstHelper>> v, int n);
    void GetAntiAliasModes(Array<unsigned int, 10>& a) const;

    bool InitializeState(Display::DisplayContext& c, bool a, bool b, const Options* o);
    void InitializeVertexTypeForAPI(VertexType& t, const void* p, const char* s);

protected:
    void ResetDynamicBufferOffsets();

protected:
    Display(bool);
    virtual ~Display();
    static DFormatId internalColorFormat;

    // the render state block (+0x54): +0x68 src factor, +0x6c dst factor, +0xa0 flags (bit 0 wire frame, bit 5
    // alpha test), +0xcc alpha reference, +0xd0 alpha function
    const unsigned char* State() const { return *reinterpret_cast<unsigned char* const*>(Raw() + 0x54); }
    unsigned char* State() { return *reinterpret_cast<unsigned char**>(Raw() + 0x54); }

    float TexelOffset() { return VCall<float>(this, 0x80); }
    DisplayTexture* CreateTexture() { return VCall<DisplayTexture*>(this, 0x98); }
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
    char* Raw() { return reinterpret_cast<char*>(this); }
    const char* Raw() const { return reinterpret_cast<const char*>(this); }
};

class DisplaySelector {
public:
    DisplaySelector();
    virtual ~DisplaySelector();
    virtual void SelectResolution(const Options* o, unsigned int& a, unsigned int& b);
    virtual void SelectScreenMode(const Options* o, Window* w, const Display::VideoMode& m, Display::ScreenMode& s);
    virtual bool SelectVideoFormat(const Options* o, const Array<Display::VideoFormat, 20>& a, Display::VideoFormat& out);
    virtual bool SelectVideoMode(const Options* o, const Array<Display::VideoMode, 20>& a, Display::VideoMode& out);
    virtual void SelectWindowArea(const Options* o, Window* w, const Display::VideoMode& m, Rect& r);
};

// the render kernel a layer belongs to
class RenderKernel {
public:
    RenderKernel(SceneGraph*, float);
    ~RenderKernel();
    OS* GetOS();
    SceneGraph* GetSceneGraph();
    Display* GetDisplay();
    MaterialManager* GetMaterialManager();
    Jet::TextureManager* GetTextureManager();
    void DestroySceneGraph();
    void DumpScreen(unsigned int idx);
    bool Init(OS* os, DisplaySelector* sel);
    bool LoadRenderSystem(DisplaySelector* sel);
    void MultipleScreenDump(unsigned int a, unsigned int b, unsigned int c);
    void DumpScreenSave();
    void SetScreenDumpFilename(const String& s, bool a, bool b);
    void SetMaterialManager(MaterialManager* m);
    void SetSceneGraph(SceneGraph* s);
    void SetTextureManager(TextureManager* t);
    void AddSystemResource(SystemResourceManager* m);
    void RemoveSystemResource(SystemResourceManager* m);


    void* TextureManager() const { return *reinterpret_cast<void* const*>(reinterpret_cast<const char*>(this) + 0x94); }

protected:
    virtual RenderKernel* GetRenderKernel();
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

class SystemResourceManager {
public:
    SystemResourceManager();
    SystemResourceManager(const SystemResourceManager&);
    ~SystemResourceManager();
};

class SystemResourceViewer {
public:
    SystemResourceViewer();
    SystemResourceViewer(const SystemResourceViewer&);
    SystemResourceViewer(RenderKernel*, const PString&);
    ~SystemResourceViewer();
    SystemResourceViewer& operator=(const SystemResourceViewer&);
    const PString& GetName() const;
    void SetName(const PString& name);
};

class DisplaySaveBuffer {
public:
    DisplaySaveBuffer(unsigned int);
    virtual ~DisplaySaveBuffer();
    unsigned int GetType() const;
};

class NonRenderContext {
public:
    NonRenderContext();
    virtual ~NonRenderContext();
    virtual bool BindToThread();
    virtual bool UnbindFromThread();
    virtual bool Initialize(Display* display);
};

class SceneGraph {
public:
    SceneGraph();
    SceneGraph(const SceneGraph&);
    SceneGraph(ModifierUpdateTask*);
    ~SceneGraph();
    SceneGraph& operator=(const SceneGraph&);
    ModifierUpdateTask* GetModifierTask();
    void SetModifierTask(ModifierUpdateTask* task);
    bool IsEmpty() const;
    void Update(float dt);
    void AddScene(Element* e);
    void DelayedDelete(Element* e);
    void UnlinkScene(Element* e);
    void PurgeDeathRow();
};

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

    const PString& GetName() const;
    int GetPriority() const;
    bool IsVisible() const;
    bool IsGroup() const;
    int operator<(const Layer& other) const;
    void SetName(const PString& name);
    void SetVisible(bool visible);
    void SetRenderKernel(RenderKernel* renderKernel);
    RenderKernel* GetRenderKernel() const;
    LayerGroup* GetParent() const;
    LayerManager* GetLayerManager();
    void SetParent(LayerGroup* g);
    void SetPriority(int p);
    void BringToFront();
    void BringToBack();

    void ConvertToWindow(Point& point) const;
    void ConvertToWindowAndClip(Rect& rect) const;

protected:
    virtual void __cdecl Render() = 0;        // 6
    virtual void __cdecl Update(float dt);    // 7
    virtual void __cdecl Begin();             // 8
    virtual void __cdecl End();               // 9
    virtual bool __cdecl CanRender();         // 10

    RenderKernel* Kernel() const { return kernel; }
    static LayerManager* layerManager; // ?layerManager@Layer@Jet@@1PAVLayerManager@2@A

public:
    JET_HEAP_OBJECT  // layers live in Jet's heap (their deleting destructors free with Alloc::Delete)

protected:
    LayerGroup* Group() const { return group; }

private:
    void* unknown04;
    void* unknown08;
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

    void SetViewPortMode(bool on);
    void SetClipping(const Rect& r);
    Rect GetClipping() const;
    void __cdecl Begin() override;
    void __cdecl End() override;

protected:

    unsigned char overlayFlags;  // +0x20 bit 1: sprite layer
    unsigned char unknown21[0x1b];
};
JET_LAYOUT32(sizeof(Overlay) == 0x3c);

class LayerGroup {
public:
    LayerGroup();
    LayerGroup(const LayerGroup&);
    LayerGroup(LayerGroup*, const PString&, int, bool);
    ~LayerGroup();
    LayerGroup& operator=(const LayerGroup&);
    void __cdecl GetGroupViewRect(Rect& out);
    const IList<Layer>& GetLayerList() const;
    const Rect& GetViewRect() const;
    void ConvertToWindow(Point& point) const;
    void ConvertToWindowAndClip(Rect& rect) const;
    void SetViewRect(const Rect& rect);
    void Add(Layer* layer);
    void Remove(Layer* layer);
    void BringToFront(Layer* layer);
    void BringToBack(Layer* layer);
    Layer* FindLayer(const PString& name);
    void GetClippedGroupViewRect(Rect& out);
    Layer* GetLayerAt(const Vector2& v);
    Layer* GetLayerUnder(const Vector2& a, const Vector2& b);
    void Reset();
    void UpdateLayer(float dt);
    void RenderLayer(SystemResourceViewerList& list);

protected:
    virtual void __cdecl Render();
};

class LayerManager {
public:
    LayerManager();
    LayerManager(const LayerManager&);
    ~LayerManager();
    LayerManager& operator=(const LayerManager&);
    LayerGroup* GetLayerGroup();
    void Reset();
    void AddSystemResourceViewer(SystemResourceViewer* v);
    void RemoveSystemResourceViewer(SystemResourceViewer* v);
    SystemResourceViewer* FindSystemResourceViewer(const PString& s) const;
    Layer* FindLayer(const PString& name);
    Layer* GetLayerAt(const Vector2& v);
    Layer* GetLayerUnder(const Vector2& a, const Vector2& b);
    void BringToFront(Layer* layer);
    void BringToBack(Layer* layer);
    void Render();
    void Update(float dt);
};

class ClearLayer {
public:
    ClearLayer();
    ClearLayer(const ClearLayer&);
    ClearLayer(const PString&, int, bool);
    ClearLayer(LayerGroup*, const PString&, int, bool);
    ~ClearLayer();
    ClearLayer& operator=(const ClearLayer&);
    Color GetClearColor();
    void SetClearColor(const Color& c);
    void Render();
};

void __cdecl FlushScratchBuffer();


// Minimal stub class definitions for remaining forwarded exports.
class AsciiTextureResource { public: AsciiTextureResource(); AsciiTextureResource(const AsciiTextureResource&); ~AsciiTextureResource(); AsciiTextureResource& operator=(const AsciiTextureResource&); };
class DisplayTextureNull { public: DisplayTextureNull(); DisplayTextureNull(const DisplayTextureNull&); ~DisplayTextureNull(); DisplayTextureNull& operator=(const DisplayTextureNull&); };
class ElementLoader { public: ElementLoader(); ElementLoader(const ElementLoader&); ~ElementLoader(); ElementLoader& operator=(const ElementLoader&); };


class SharedContext { public: SharedContext(); SharedContext(const SharedContext&); ~SharedContext(); SharedContext& operator=(const SharedContext&); };
class SharedTextureCollection { public: SharedTextureCollection(); SharedTextureCollection(const SharedTextureCollection&); ~SharedTextureCollection(); SharedTextureCollection& operator=(const SharedTextureCollection&); };
class TextureRes { public: TextureRes(); TextureRes(const TextureRes&); ~TextureRes(); TextureRes& operator=(const TextureRes&); };
class TextureResource { public: TextureResource(); TextureResource(const TextureResource&); ~TextureResource(); TextureResource& operator=(const TextureResource&); };






}  // namespace Jet
