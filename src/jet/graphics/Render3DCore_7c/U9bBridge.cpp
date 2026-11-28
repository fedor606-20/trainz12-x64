// Unit U9b wave 5: the last seven scene exports. Cell::Cull / AddAllToVisible and the PhysicalPortal display bodies
// still run the original implementation at its RVA, but the module's own statistics, traversal counter and global
// stencil switch are synchronised after every call, so the local accessors agree with what just happened. Mirror's
// warp camera is a local transcription; VirtualPortal's camera composes two model views and stays on the original
// body until that composition is verified.
#include <jet/Render3DCoreScene.h>

#include <jet/Render3DCoreCamera.h>
#include <jet/Render3DCoreParticle.h>
#include <jet/RenderSystem.h>

#include <windows.h>

#include <cstring>

#pragma warning(disable : 4789)

namespace Jet {

#ifdef LIVEPATCH_BUILD
// the patch DLL carries only this file; the final module gets these from U9bRem.cpp / Portals.cpp
#pragma data_seg(".data$r3dcstats")
unsigned int* Cell::stats = nullptr;
unsigned int g_liveCellStats[18] = {};
#pragma data_seg()
unsigned int Portal::total = 0;
unsigned int Portal::traversed = 0;
bool Portal::enableStencilMode = true;
#endif

class CellNode;
class ViewPoint;

namespace {

HMODULE OrigModule()
{
    static HMODULE module = [] {
        HMODULE m = GetModuleHandleA("Render3DCore_7c_orig.dll");
        return m ? m : LoadLibraryA("Render3DCore_7c_orig.dll");
    }();
    return module;
}

template <class F>
F OrigRva(unsigned int rva)
{
    static void* base = static_cast<void*>(OrigModule());
    return reinterpret_cast<F>(static_cast<char*>(base) + rva);
}

template <class F>
F OrigProc(const char* name)
{
    HMODULE m = OrigModule();
    F fn{};
    FARPROC raw = m ? GetProcAddress(m, name) : nullptr;
    std::memcpy(&fn, &raw, sizeof fn);
    return fn;
}

// The original's portal counters are its own statics; copy them onto this module's after every call into it.
// The three statics are private, so the copy goes through the class body (a local-only friend).
void SyncPortalState(HMODULE orig)
{
    if (!orig) return;
    if (auto* p = reinterpret_cast<unsigned int*>(GetProcAddress(orig, "?traversed@Portal@Jet@@0IA")))
        PortalStateAccess::Traversed() = *p;
    if (auto* p = reinterpret_cast<unsigned int*>(GetProcAddress(orig, "?total@Portal@Jet@@0IA")))
        PortalStateAccess::Total() = *p;
    if (auto* p = reinterpret_cast<unsigned char*>(GetProcAddress(orig, "?enableStencilMode@Portal@Jet@@0_NA")))
        PortalStateAccess::EnableStencilMode() = *p != 0;
}

void CopyOrigCellStats(unsigned int* dst)
{
    HMODULE const orig = OrigModule();
    if (!orig) return;
    auto** symbol = static_cast<unsigned int**>(static_cast<void*>(GetProcAddress(orig, "?stats@Cell@Jet@@0PAIA")));
    if (!symbol || !*symbol) return;
    for (int i = 0; i < 19; ++i) dst[i] = (*symbol)[i];
}


Camera* BuildWarpedCamera(void* memory, Camera* source, const Vector3* position, const Matrix4& warp)
{
    using Fn = Camera*(__thiscall*)(void*, const Camera*, const Vector3*, const Matrix4*);
    static Fn fn = OrigProc<Fn>("??0Camera@Jet@@QAE@ABV01@PBVVector3@1@ABVMatrix4@1@@Z");
    return fn ? fn(memory, source, position, &warp) : nullptr;
}

void Identity(Matrix4& m)
{
    for (int i = 0; i < 16; ++i) m.m[i] = (i % 5 == 0) ? 1.0f : 0.0f;
}

}  // namespace

void __cdecl Cell::Cull(ViewPoint& view, const Matrix4& m)
{
    OrigRva<void(__cdecl*)(Cell*, ViewPoint*, const Matrix4*)>(0x1c040)(this, &view, &m);
    CopyOrigCellStats(reinterpret_cast<unsigned int*>(&stats));
}

void __cdecl Cell::AddAllToVisible(CellNode* node, ViewPoint& view, const Matrix4& m, int flags)
{
    OrigRva<void(__cdecl*)(Cell*, CellNode*, ViewPoint*, const Matrix4*, int)>(0x1c8e0)(
        this, node, &view, &m, flags);
    CopyOrigCellStats(reinterpret_cast<unsigned int*>(&stats));
}

void __cdecl PhysicalPortal::Traverse(ViewPoint& view, const Matrix4& m, Camera* camera)
{
    OrigRva<void(__cdecl*)(PhysicalPortal*, ViewPoint*, const Matrix4*, Camera*)>(0x53f30)(
        this, &view, &m, camera);
    SyncPortalState(OrigModule());
}

void __cdecl PhysicalPortal::Enter(Display* display, ViewPort& port, Matrix4& m, unsigned int a, unsigned int b)
{
    OrigRva<void(__cdecl*)(PhysicalPortal*, Display*, ViewPort*, Matrix4*, unsigned int, unsigned int)>(0x542d0)(
        this, display, &port, &m, a, b);
    SyncPortalState(OrigModule());
}

void __cdecl PhysicalPortal::Leave(Display* display, ViewPort& port, Matrix4& m, unsigned int a, unsigned int b)
{
    OrigRva<void(__cdecl*)(PhysicalPortal*, Display*, ViewPort*, Matrix4*, unsigned int, unsigned int)>(0x54690)(
        this, display, &port, &m, a, b);
    SyncPortalState(OrigModule());
}

Camera* __cdecl VirtualPortal::CreateCamera(Camera* camera, const Vector3* position, void* data,
                                            Matrix4& result) const
{
    return OrigRva<Camera*(__cdecl*)(const VirtualPortal*, Camera*, const Vector3*, void*, Matrix4*)>(0x75c10)(
        this, camera, position, data, &result);
}

// 0x2bcc0: the reflection matrix of the camera's model view, then the warped camera.
Camera* __cdecl Mirror::CreateCamera(Camera* camera, const Vector3* position, void* data, Matrix4& result) const
{
    Matrix4 view;
    Identity(view);
    Matrix4 model;
    const_cast<Mirror*>(this)->GetModelView(model, view);
    const float m0 = model.m[0], m1 = model.m[1], m2 = model.m[2];
    const float m4 = model.m[4], m5 = model.m[5], m6 = model.m[6];
    const float m8 = model.m[8], m9 = model.m[9], m10 = model.m[10];
    const float m12 = model.m[12], m13 = model.m[13], m14 = model.m[14];
    const float nx = (m8 + m0) * 0.0f - m4 * 1.0f;
    const float ny = (m9 + m1) * 0.0f - m5 * 1.0f;
    const float nz = (m10 + m2) * 0.0f - m6 * 1.0f;
    const float d = m14 * nz + m13 * ny + m12 * nx;
    result.m[0] = 1.0f - nx * nx * 2.0f;
    result.m[1] = ny * nx * -2.0f;
    result.m[2] = nz * nx * -2.0f;
    result.m[3] = 0.0f;
    result.m[4] = ny * nx * -2.0f;
    result.m[5] = 1.0f - ny * ny * 2.0f;
    result.m[6] = nz * ny * -2.0f;
    result.m[7] = 0.0f;
    result.m[8] = nz * nx * -2.0f;
    result.m[9] = nz * ny * -2.0f;
    result.m[10] = 1.0f - nz * nz * 2.0f;
    result.m[11] = 0.0f;
    result.m[12] = nx * d * 2.0f;
    result.m[13] = ny * d * 2.0f;
    result.m[14] = nz * d * 2.0f;
    result.m[15] = 1.0f;
    if (!data) return nullptr;
    return BuildWarpedCamera(data, camera, position, result);
}

}  // namespace Jet
