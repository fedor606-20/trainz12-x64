// RenderSystem_7c, declared as far as the modules built so far use it (Render2DCore). The layer classes have their
// TS12 layout and vtable order (RenderSystem 0x1006f124 / 0x1007195c); Display, Texture and the kernel are opaque and
// reached through VCall wrappers by their vtable offsets.
#pragma once

#include <jet/DynamicArray.h>
#include <jet/FastDynamicArray.h>
#include <jet/Geometry.h>
#include <jet/KernelStandard.h>
#include <jet/Math.h>
#include <jet/PString.h>
#include <jet/RenderTypes.h>
#include <jet/SmallTypes.h>
#include <jet/VCall.h>

namespace Jet {

class Bitmap;
class LayerGroup;
class RenderKernel;
class SystemResourceViewer;
class Texture;
class VertexType;
enum VertexTypeID;
class StaticBuffer;
class TextureCombiner;
class VertexBufferManager;
class DisplaySelector;
class DisplayTexture;
class MaterialManager;
class SceneGraph;
class TextureManager;
class VertexProgramManager;
class TextureCombinerManager;
class TextureManager;
class VertexProgramManager;
class TextureCombinerManager;

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
enum DPolygonOffsetMode {};
enum DTexGenMode {};
enum DTexCompression {};
enum DOperation {};
enum DExtension {};
enum ScreenMode {};

enum PointPosition { POINT_CENTRE = 0, POINT_CORNER = 1 };

// the device-independent renderer (implemented by RenderDirectX9 / RenderOpenGL). Its virtual slots used so far:
//   +0x80 float GetTexelOffset()   +0x98 Texture* CreateTexture()   +0xa0 SetTexture(stage)
//   +0xe0 DrawUser(format id, primitive, ?, vertices, count)   +0xe4 Draw(vertex declaration, primitive, ?, vertices, count)
//   +0x10c vertex declaration GetDeclaration(format id)
class Display {
public:
    struct DisplayFrameStats {};
    struct ViewModeParams { int data[9]; };
    struct VideoMode { unsigned int data[4]; };
    struct VideoFormat { unsigned int data[8]; };
    // DisplayBatch1
    void BeginFrame(float dt);
    void EndFrame(float dt);
    virtual void FlushState();
    void SetZBiasValues(float zBias, float slope);
    void DisableAllLights();
    void SetPolygonOffset(float factor, float units);
    void EnablePolygonOffset(DPolygonOffsetMode mode, bool enable);
    void ResetDynamicBufferOffsets();
    void CheckResetDynamicBufferOffsets();
    bool IsNormalized2DViewport() const;
    void GetFogRange(float& start, float& end) const;
    bool GetLightMode(int index) const;
    Rect GetScissor();
    void SetScissor(const Rect& rect);
    void SetViewport(const Rect& rect);
    bool GetAllVideoModes(const Array<VideoMode, 20>*& out) const;
    bool GetDefaultVideoMode(VideoMode& out) const;
    bool GetCurrentVideoMode(VideoMode& out) const;
    bool GetAllVideoFormats(const Array<VideoFormat, 20>*& out) const;
    bool GetCurrentVideoFormat(VideoFormat& out) const;
    // DisplayBatch2
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
    void SetLightingMode(bool on);
    void SetSeperateSpecularMode(bool on);
    void SetLocalViewSpecularMode(bool on);
    void SetFogMode(DFogMode mode);
    void SetFogDensity(float v);
    void SetFogColor(const Color& c);
    void SetGlobalAmbient(const Color& c);
    void SetVertexWeightMode(bool on);
    void SetStencilWriteMask(unsigned long mask, DFace face);
    void SetFogRange(float start, float end);
    void SetAlphaBlendFunction(Src::Factor src, Dst::Factor dst);

    void __cdecl SetAlphaTestFunction(DFunction function, float reference);
    void __cdecl SetAlphaTestMode(bool on);
    void __cdecl SetScissorMode(bool on);
    void __cdecl SetCullMode(DCull mode);
    void __cdecl SetWireFrameMode(bool on);
    const Size& __cdecl GetTargetSize() const;
    const char* GetExtensionString(DExtension idx) const;
    void SetRenderTarget(DisplayTexture* tex, unsigned int idx);
    void RestoreGamma();
    int GetTargetPPI() const;
    void SetAntiAliasing(bool on);
    static DFormatId __cdecl GetInternalColorFormat();
    float GetPhysicalScreenRatio();
    bool IsRenderableState() const;
    bool GetReflectionCull() const;
    float GetNormalLength() const;
    bool GetChunkColorMode() const;
    bool GetChunkVertexCountMode() const;
    bool GetDynamicGeometryDisplayMode() const;
    bool GetStaticGeometryDisplayMode() const;
    static unsigned long __cdecl ConvertColor(const Color& c);

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


    // DisplaySimpleMore
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
    bool GetTextureMode() const;
    bool GetGlobalTextureMode() const;
    int GetNumStencilBits() const;
    TextureCombinerManager* GetTextureCombinerManager() const;
    bool TextureCombinerSupport() const;
    VertexProgramManager* GetVertexProgramManager() const;
    bool VertexProgramSupport() const;
    bool HardwareAnimationSupport() const;
    VertexBufferManager* GetVertexBufferManager() const;
    bool CheckPlatform(OS* os);
    unsigned int GetCurrentFrame() const;
    int GetNumberOfHardwareLights() const;
    bool GetZBiasEnabled();
    bool IsBetweenBeginAndEndFrame();
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

protected:
    static DFormatId internalColorFormat;

    void SetTextureMode(bool on);
    void SetGlobalTextureMode(bool on);
    bool SetLightMode(int idx, bool on);
    void SetTextureCoordGen(int stage, DTexGenMode mode, unsigned int gen);
    void SetColorMaterialMode(DColorMaterialTypes mode);
    void SetStencilTestMode(bool test, bool write);
    void SetStencilFunction(DFunction fn, int ref, unsigned long mask, DFace face);
    void SetStencilOperation(DOperation op1, DOperation op2, DOperation op3, DFace face);
    void SetMaterialProperties(const DisplayMaterial& dm);
    void EnableProjectedTexture(int idx, bool a, bool b);
    void EndLightingUpdate();
    void SetActiveTextureStage(int s);
    void SetMaxAnisotropy(float v);
    void SetGamma(float g, bool r);
    bool SetLightState(int idx, const DisplayLight& l, bool on);
    void SetDisplaySelector(DisplaySelector* s);
    void CriticalStartup(OS* os);
    void CriticalShutdown(void* p);
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
    const VertexType* GetBuiltInVertexType(VertexTypeID id) const;
    Bitmap* Screenshot() const;
    void ConvertColorArray(unsigned int* a, int n);
    void GetAntiAliasModes(Array<unsigned int, 10>& a) const;
    bool CheckExtensionString(const PString& s) const;
    const char* ConvertTexCompressionString(DTexCompression idx) const;
    void SetViewModeParams(const ViewModeParams& p);
    void GetViewModeParams(ViewModeParams& p) const;
    void Set2DView(float a, float b, float c, float d, float e, float f);
    void Set2DViewNormalized(bool on);
    void Set3DView(float a, float b, float c, float d);
    void Set3DViewOffset(float a, float b, float c, float d, float e);
    void Set3DTransitionalView(float a, float b, float c, float d, float e, float f, float g);
    void LoadProjectionMatrix(const Matrix4& m);
    void StartLightingUpdate(int idx, const Matrix4& m);


private:
    const char* Raw() const { return reinterpret_cast<const char*>(this); }
};

// the render kernel a layer belongs to: +0x7c its display, +0x94 its texture manager
class RenderKernel {
public:
    Display* GetDisplay() const { return *reinterpret_cast<Display* const*>(reinterpret_cast<const char*>(this) + 0x7c); }
    void* TextureManager() const { return *reinterpret_cast<void* const*>(reinterpret_cast<const char*>(this) + 0x94); }
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

protected:
    void __cdecl Begin() override;
    void __cdecl End() override;

    unsigned char overlayFlags;  // +0x20 bit 1: sprite layer
    unsigned char unknown21[0x1b];
};
JET_LAYOUT32(sizeof(Overlay) == 0x3c);

class LayerGroup {
public:
    void __cdecl GetGroupViewRect(Rect& out);
};

}  // namespace Jet
