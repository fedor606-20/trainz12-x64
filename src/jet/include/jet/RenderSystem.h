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
class LayerGroup;
class RenderKernel;
class SystemResourceViewer;
class Texture;

namespace Src {
enum Factor { SRC_ONE = 1 };
}
namespace Dst {
enum Factor { DST_ONE = 1 };
}
enum DFunction { DFUNCTION_ALWAYS = 7 };
enum DCull { DCULL_NONE = 0 };
enum PointPosition { POINT_CENTRE = 0, POINT_CORNER = 1 };

// the device-independent renderer (implemented by RenderDirectX9 / RenderOpenGL). Its virtual slots used so far:
//   +0x80 float GetTexelOffset()   +0x98 Texture* CreateTexture()   +0xa0 SetTexture(stage)
//   +0xe0 DrawUser(format id, primitive, ?, vertices, count)   +0xe4 Draw(vertex declaration, primitive, ?, vertices, count)
//   +0x10c vertex declaration GetDeclaration(format id)
class Display {
public:
    void __cdecl SetLightingMode(bool on);  // ?SetLightingMode@Display@Jet@@QAAX_N@Z (the original's until U9)
    void __cdecl SetAlphaBlendFunction(Src::Factor src, Dst::Factor dst);
    void __cdecl SetAlphaTestFunction(DFunction function, float reference);
    void __cdecl SetAlphaTestMode(bool on);
    void __cdecl SetScissorMode(bool on);
    void __cdecl SetCullMode(DCull mode);
    void __cdecl SetWireFrameMode(bool on);
    const Size& __cdecl GetTargetSize() const;
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
