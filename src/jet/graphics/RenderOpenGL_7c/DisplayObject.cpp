// RenderOpenGL_7c: local display object, 161-slot vtable and plugin registration.
//
// The primary table is the module's own Display table (RenderSystem_7c exports it) with the renderer's
// pure virtual slots filled in. The slot numbers are byte offsets / 4, exactly as RenderSystem calls them.
// The rendering model is the fixed-function OpenGL 1.1 pipeline plus the extension entry points the local
// texture class already uses: client vertex arrays, multitexture, stencil, fog, lights and materials.
#include "DisplayObject.h"

#include "DisplayInterfaceWGL.h"
#include "DisplayTextureOpenGL.h"
#include "OpenGLExtensions.h"
#include "RenderOpenGLTypes.h"

#include <jet/Geometry.h>
#include <jet/KernelStandard.h>
#include <jet/Math.h>
#include <jet/Plugin.h>
#include <jet/RenderSystem.h>
#include <jet/RenderTypes.h>
#include <jet/Window.h>

#include <windows.h>

#include <GL/gl.h>
#include <GL/glu.h>

#include <cstdint>
#include <cstring>
#include <new>

namespace GL = Jet::GL;

static void TraceLine(const char* text)
{
    (void)text;
}
static void TracePtr(const char* label, const void* value)
{
    (void)label;
    (void)value;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        TraceLine("DllMain attach");
    }
    return TRUE;
}

namespace {

constexpr int kDisplayObjectSize = 0xfac;
constexpr int kPluginBaseOffset = 0xfa4;  // ReferenceCount virtual base inside the display
constexpr int kDescriptorOffset = 0xf78;
constexpr int kContextSize = 0x2c8;

using Jet::DisplayInterfaceWGL;

// GL tokens that the Windows 1.1 headers do not define.
constexpr GLenum kFuncAdd = 0x8006;
constexpr GLenum kFuncSubtract = 0x800A;
constexpr GLenum kFuncReverseSubtract = 0x800B;
constexpr GLenum kMin = 0x8007;
constexpr GLenum kMax = 0x8008;
constexpr GLenum kMaxTextureUnits = 0x84E2;
constexpr GLenum kMaxTextureMaxAnisotropy = 0x84FF;
constexpr GLenum kTextureMaxAnisotropy = 0x84FE;
constexpr GLenum kDepthClampNv = 0x864F;
constexpr GLenum kLightModelColorControl = 0x81F8;
constexpr GLenum kSeparateSpecularColor = 0x81FA;
constexpr GLenum kSingleColor = 0x81F9;
constexpr GLenum kPolygonOffsetPoint = 0x2A01;
constexpr GLenum kPolygonOffsetLine = 0x2A02;
constexpr GLenum kPolygonOffsetFill = 0x8037;
constexpr GLenum kTextureCubeMap = 0x8513;
constexpr GLenum kTexture0 = 0x84C0;
constexpr GLenum kClampToEdge = 0x812F;
constexpr GLenum kFogCoordArray = 0x8457;
constexpr GLenum kGenerateMipmap = 0x8191;

int g_vbtableXTerm[2] = {0, 0xfa0};
int g_vbtableBase[2] = {0, 0xf9c};

template <class T>
T& Field(void* object, unsigned offset)
{
    return *reinterpret_cast<T*>(static_cast<unsigned char*>(object) + offset);
}

template <class T>
const T& Field(const void* object, unsigned offset)
{
    return *reinterpret_cast<const T*>(static_cast<const unsigned char*>(object) + offset);
}

unsigned char* Bytes(void* object) { return static_cast<unsigned char*>(object); }

HMODULE RsModule()
{
    static HMODULE module = GetModuleHandleA("RenderSystem_7c.dll");
    return module;
}

HMODULE KernelModule()
{
    static HMODULE module = GetModuleHandleA("Kernel_7c.dll");
    return module;
}

template <class T>
T Sym(HMODULE module, const char* name)
{
    return reinterpret_cast<T>(module ? GetProcAddress(module, name) : nullptr);
}

// ---- base Display entry points, called through RenderSystem_7c --------------------------------

using BaseBool = bool(__cdecl*)(void*);
using BaseInitFn = bool(__cdecl*)(void*, void*);
using BaseOpenFn = bool(__cdecl*)(void*, const void*, bool);
using BaseFrameFn = void(__cdecl*)(void*, float);
using BaseVoidFn = void(__cdecl*)(void*);
using BasePreOpenFn = bool(__cdecl*)(void*, const void*);
using BaseInitializeStateFn = bool(__cdecl*)(void*, void*, bool, bool, const void*);

BaseInitFn BaseInit() { static auto fn = Sym<BaseInitFn>(RsModule(), "?Init@Display@Jet@@UAA_NPAVOS@2@@Z"); return fn; }
BaseOpenFn BaseOpen() { static auto fn = Sym<BaseOpenFn>(RsModule(), "?Open@Display@Jet@@UAA_NPBVOptions@2@_N@Z"); return fn; }
BaseFrameFn BaseBeginFrame() { static auto fn = Sym<BaseFrameFn>(RsModule(), "?BeginFrame@Display@Jet@@UAAXM@Z"); return fn; }
BaseFrameFn BaseEndFrame() { static auto fn = Sym<BaseFrameFn>(RsModule(), "?EndFrame@Display@Jet@@UAAXM@Z"); return fn; }
BaseVoidFn BaseShutdown() { static auto fn = Sym<BaseVoidFn>(RsModule(), "?Shutdown@Display@Jet@@UAAXXZ"); return fn; }
BasePreOpenFn BasePreOpen() { static auto fn = Sym<BasePreOpenFn>(RsModule(), "?PreOpen@Display@Jet@@MAA_NPBVOptions@2@@Z"); return fn; }
BaseInitializeStateFn InitializeState()
{
    static auto fn = Sym<BaseInitializeStateFn>(RsModule(),
        "?InitializeState@Display@Jet@@MAA_NAAUDisplayContext@12@_N1PBVOptions@2@@Z");
    return fn;
}
BaseVoidFn DisplayDtor() { static auto fn = Sym<BaseVoidFn>(RsModule(), "??1Display@Jet@@MAE@XZ"); return fn; }

// ---- renderer state -------------------------------------------------------------------------

struct RendererState {
    void* self;
    DisplayInterfaceWGL* iface;
    void* bufferManager;
    const void* options;
    int maxTextureUnits;
    int currentUnit;
    int maxAnisotropy;
    bool initialized;
    bool shutdown;
    bool destroyed;
    bool inFrame;
    GLenum matrixMode[4];
    unsigned frame;
};

RendererState g_state;

void Trace(const char* text)
{
    TraceLine(text);
}

RendererState& State(void* self)
{
    g_state.self = self;
    return g_state;
}

DisplayInterfaceWGL* Interface(void* self)
{
    return self ? Field<DisplayInterfaceWGL*>(self, 0xf00) : nullptr;
}

// ---- small helpers --------------------------------------------------------------------------

GLenum BlendSrc(int f)
{
    switch (f) {
    case 0: return GL_ZERO;
    case 1: return GL_ONE;
    case 2: return GL_DST_COLOR;
    case 3: return GL_ONE_MINUS_DST_COLOR;
    case 4: return GL_SRC_ALPHA;
    case 5: return GL_ONE_MINUS_SRC_ALPHA;
    case 6: return GL_DST_ALPHA;
    case 7: return GL_ONE_MINUS_DST_ALPHA;
    case 8: return GL_SRC_ALPHA_SATURATE;
    default: return GL_ONE;
    }
}

GLenum BlendDst(int f)
{
    switch (f) {
    case 0: return GL_ZERO;
    case 1: return GL_ONE;
    case 2: return GL_SRC_COLOR;
    case 3: return GL_ONE_MINUS_SRC_COLOR;
    case 4: return GL_SRC_ALPHA;
    case 5: return GL_ONE_MINUS_SRC_ALPHA;
    case 6: return GL_DST_ALPHA;
    case 7: return GL_ONE_MINUS_DST_ALPHA;
    default: return GL_ZERO;
    }
}

GLenum BlendEquation(int e)
{
    switch (e) {
    case 1: return kFuncSubtract;
    case 2: return kFuncReverseSubtract;
    case 3: return kMin;
    case 4: return kMax;
    default: return kFuncAdd;
    }
}

GLenum DepthFunction(int f)
{
    switch (f) {
    case 0: return GL_NEVER;
    case 1: return GL_LESS;
    case 2: return GL_LEQUAL;
    case 3: return GL_EQUAL;
    case 4: return GL_GEQUAL;
    case 5: return GL_GREATER;
    case 6: return GL_NOTEQUAL;
    default: return GL_ALWAYS;
    }
}

GLenum FogMode(int m)
{
    switch (m) {
    case 1: return GL_LINEAR;
    case 2: return GL_EXP;
    case 3: return GL_EXP2;
    default: return GL_LINEAR;
    }
}

GLenum Primitive(int primitive)
{
    // GeometryChunk::primitiveLookup: 0 points, 1 lines, 2 line strip, 3 triangles, 4 triangle strip,
    // 5 triangle fan.
    switch (primitive) {
    case 0: return GL_POINTS;
    case 1: return GL_LINES;
    case 2: return GL_LINE_STRIP;
    case 3: return GL_TRIANGLES;
    case 4: return GL_TRIANGLE_STRIP;
    case 5: return GL_TRIANGLE_FAN;
    default: return GL_TRIANGLES;
    }
}

GLenum StencilOperation(int op)
{
    switch (op) {
    case 0: return GL_KEEP;
    case 1: return GL_ZERO;
    case 2: return GL_REPLACE;
    case 3: return GL_INCR;
    case 4: return GL_DECR;
    case 5: return GL_INVERT;
    case 6: return 0x8507;  // GL_INCR_WRAP
    case 7: return 0x8508;  // GL_DECR_WRAP
    default: return GL_KEEP;
    }
}

void SetMatrixMode(GLenum mode)
{
    glMatrixMode(mode);
}

void SetActiveUnit(int unit)
{
    if (unit < 0) unit = 0;
    if (GL::ext.ActiveTexture) GL::ext.ActiveTexture(kTexture0 + unit);
    if (GL::ext.ClientActiveTexture) GL::ext.ClientActiveTexture(kTexture0 + unit);
}

int MaxTextureUnits()
{
    GLint units = 1;
    glGetIntegerv(kMaxTextureUnits, &units);
    if (units < 1) units = 1;
    if (units > 8) units = 8;
    return units;
}

// ---- static buffers and their manager --------------------------------------------------------

class RoglStaticBuffer : public Jet::StaticBuffer {
public:
    RoglStaticBuffer(unsigned count_, unsigned stride_, bool index_, bool wide_)
        : count(count_), stride(stride_), index(index_), wide(wide_), data(nullptr)
    {
        const unsigned bytes = BytesNeeded();
        if (bytes) {
            data = static_cast<unsigned char*>(Jet::Alloc::New(bytes));
            if (data) std::memset(data, 0, bytes);
        }
    }

    ~RoglStaticBuffer() override
    {
        if (data) Jet::Alloc::Delete(data);
        data = nullptr;
    }

    unsigned BytesNeeded() const
    {
        const unsigned each = index ? (wide ? 4u : 2u) : stride;
        return count * each;
    }

    unsigned char* data;
    unsigned count;
    unsigned stride;
    bool index;
    bool wide;
};

RoglStaticBuffer* AsBuffer(const void* buffer)
{
    return const_cast<RoglStaticBuffer*>(static_cast<const RoglStaticBuffer*>(buffer));
}

void* NewStaticBuffer(unsigned count, unsigned stride, bool index, bool wide)
{
    void* memory = Jet::Alloc::New(sizeof(RoglStaticBuffer));
    if (!memory) return nullptr;
    return ::new (memory) RoglStaticBuffer(count, stride, index, wide);
}

// 23-slot VertexBufferManager-compatible table. Only the upload/initialise slots are used by
// RenderSystem's geometry chunks; the rest are the base class' no-ops.
void* g_bufferVtable[23];

// VertexBufferManager's slot 0 is a virtual destructor: RenderSystem_7c calls it thiscall.
void* __fastcall BufferDelete(void* self, void*, unsigned)
{
    if (g_state.bufferManager == self) g_state.bufferManager = nullptr;
    if (self) Jet::Alloc::Delete(self);
    return self;
}

bool __cdecl BufferInitialize(void* self, void* display, const void*)
{
    Field<void*>(self, 4) = display;
    return true;
}

bool __cdecl BufferUploadVertex(void* self, void* type, void* buffer, const void* data, unsigned count, int,
                                unsigned* out, void*)
{
    (void)self;
    auto* b = AsBuffer(buffer);
    if (!b || !type || !data) return false;
    const auto* streams = *reinterpret_cast<Jet::VertexType::Stream* const*>(Bytes(type) + 0x10);
    const unsigned vertexStride = streams ? streams[0].stride : b->stride;
    const unsigned bytes = count * vertexStride;
    if (!b->data || bytes > b->BytesNeeded()) {
        if (b->data) Jet::Alloc::Delete(b->data);
        b->data = static_cast<unsigned char*>(Jet::Alloc::New(bytes ? bytes : 1));
        b->count = count;
        b->stride = vertexStride;
        b->index = false;
    }
    if (!b->data) return false;
    std::memcpy(b->data, data, bytes);
    if (out) *out = 0;
    return true;
}

int __cdecl BufferUploadIndex(void* self, void* buffer, const void* data, unsigned count, int wide, unsigned* out)
{
    (void)self;
    auto* b = AsBuffer(buffer);
    if (!b || !data) return -1;
    const unsigned each = wide ? 4u : 2u;
    const unsigned bytes = count * each;
    if (!b->data || bytes > b->BytesNeeded()) {
        if (b->data) Jet::Alloc::Delete(b->data);
        b->data = static_cast<unsigned char*>(Jet::Alloc::New(bytes ? bytes : 1));
        b->count = count;
        b->wide = wide != 0;
        b->index = true;
    }
    if (!b->data) return -1;
    std::memcpy(b->data, data, bytes);
    if (out) *out = 0;
    return 0;
}

void __cdecl BufferVoid(void*) {}
int __cdecl BufferZero() { return 0; }

void BuildBufferVtable()
{
    static bool built = false;
    if (built) return;
    built = true;
    for (void*& slot : g_bufferVtable) slot = reinterpret_cast<void*>(&BufferZero);
    g_bufferVtable[0] = reinterpret_cast<void*>(&BufferDelete);
    g_bufferVtable[1] = reinterpret_cast<void*>(&BufferInitialize);
    g_bufferVtable[4] = reinterpret_cast<void*>(&BufferVoid);
    g_bufferVtable[8] = reinterpret_cast<void*>(&BufferUploadVertex);
    g_bufferVtable[11] = reinterpret_cast<void*>(&BufferUploadIndex);
    g_bufferVtable[14] = reinterpret_cast<void*>(&BufferVoid);
    g_bufferVtable[15] = reinterpret_cast<void*>(&BufferVoid);
    g_bufferVtable[16] = reinterpret_cast<void*>(&BufferVoid);
    g_bufferVtable[17] = reinterpret_cast<void*>(&BufferVoid);
    g_bufferVtable[18] = reinterpret_cast<void*>(&BufferVoid);
    g_bufferVtable[20] = reinterpret_cast<void*>(&BufferVoid);
}

void* CreateBufferManager(void* self)
{
    BuildBufferVtable();
    auto* manager = static_cast<unsigned char*>(Jet::Alloc::New(0x78));
    if (!manager) return nullptr;
    std::memset(manager, 0, 0x78);
    *reinterpret_cast<void**>(manager) = g_bufferVtable;
    Field<void*>(manager, 4) = self;
    // The base class' dynamic-buffer defaults (0x108000 / 0xffff) are not needed by the client-array
    // backend, but keeping them makes the layouts of the statistics structs sane.
    Field<unsigned>(manager, 8) = 0x108000;
    Field<unsigned>(manager, 0xc) = 0xffff;
    return manager;
}

// ---- vertex arrays --------------------------------------------------------------------------

struct StreamEntry {
    const void* data;
    const Jet::StaticBuffer* buffer;
    unsigned char dirty;
};

const void* StreamData(const StreamEntry& entry)
{
    if (entry.data) return entry.data;
    if (!entry.buffer) return nullptr;
    return AsBuffer(entry.buffer)->data;
}

void EnableItem(const Jet::VertexType::Item& item, const unsigned char* base, unsigned stride)
{
    const int type = item.type;
    const unsigned char* const at = base + item.offset;
    if (!base) return;
    if (type == 1) {  // Position
        glEnableClientState(GL_VERTEX_ARRAY);
        glVertexPointer(3, GL_FLOAT, static_cast<GLsizei>(stride), at);
    } else if (type == 4) {  // Normal
        glEnableClientState(GL_NORMAL_ARRAY);
        glNormalPointer(GL_FLOAT, static_cast<GLsizei>(stride), at);
    } else if (type == 2 || type == 3) {  // Color0/Color1, packed ARGB
        glEnableClientState(GL_COLOR_ARRAY);
        glColorPointer(4, GL_UNSIGNED_BYTE, static_cast<GLsizei>(stride), at);
    } else if (type == 14) {  // Fog
        if (GL::IsSupported("GL_EXT_fog_coord")) {
            glEnableClientState(kFogCoordArray);
            auto fogCoord = reinterpret_cast<void(APIENTRY*)(GLenum, GLsizei, const void*)>(
                wglGetProcAddress("glFogCoordPointerEXT"));
            if (fogCoord) fogCoord(GL_FLOAT, static_cast<GLsizei>(stride), at);
        }
    } else if (type >= 15 && type <= 22) {  // Texture0..Texture7
        const int unit = type - 15;
        if (unit >= g_state.maxTextureUnits) return;
        SetActiveUnit(unit);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glTexCoordPointer(2, GL_FLOAT, static_cast<GLsizei>(stride), at);
    }
}

void DisableItems(const Jet::VertexType::Item& item)
{
    switch (item.type) {
    case 1: glDisableClientState(GL_VERTEX_ARRAY); break;
    case 4: glDisableClientState(GL_NORMAL_ARRAY); break;
    case 2:
    case 3: glDisableClientState(GL_COLOR_ARRAY); break;
    case 14:
        if (GL::IsSupported("GL_EXT_fog_coord")) glDisableClientState(kFogCoordArray);
        break;
    default:
        if (item.type >= 15 && item.type <= 22) glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        break;
    }
}

unsigned StreamCount(const void* type)
{
    return type ? Field<unsigned>(type, 0xc) : 0;
}

const Jet::VertexType::Stream* StreamList(const void* type)
{
    return type ? *reinterpret_cast<const Jet::VertexType::Stream* const*>(static_cast<const unsigned char*>(type) + 0x10)
                : nullptr;
}

template <class Bind>
void ForEachItem(const void* type, Bind bind)
{
    const unsigned count = StreamCount(type);
    const auto* streams = StreamList(type);
    if (!streams) return;
    for (unsigned s = 0; s < count; ++s)
        for (unsigned i = 0; i < streams[s].count; ++i) bind(streams[s].items[i]);
}

void BindSingleStream(const void* type, const void* vertices, const Jet::StaticBuffer* buffer)
{
    const auto* streams = StreamList(type);
    if (!streams || !StreamCount(type)) return;
    const void* const data = vertices ? vertices : (buffer ? AsBuffer(buffer)->data : nullptr);
    for (unsigned i = 0; i < streams[0].count; ++i)
        EnableItem(streams[0].items[i], static_cast<const unsigned char*>(data), streams[0].stride);
}

void UnbindSingleStream(const void* type)
{
    ForEachItem(type, [](const Jet::VertexType::Item& item) { DisableItems(item); });
    SetActiveUnit(0);
}

const unsigned short* IndexData(const void* block)
{
    return reinterpret_cast<const unsigned short*>(reinterpret_cast<std::uintptr_t>(block) + 0x10);
}

void DrawElementBatch(const void* type, const void* vertices, const Jet::StaticBuffer* vertexBuffer,
                      const void* indices, const Jet::StaticBuffer* indexBuffer, unsigned count, int primitive)
{
    if (!type || !count) return;
    const auto* streams = StreamList(type);
    if (!streams) return;
    if (vertices && !vertexBuffer) {
        BindSingleStream(type, vertices, nullptr);
    } else {
        BindSingleStream(type, vertices, vertexBuffer);
    }

    if (!indices && indexBuffer) indices = AsBuffer(indexBuffer)->data;
    if (indices) {
        glDrawElements(Primitive(primitive), static_cast<GLsizei>(count), GL_UNSIGNED_SHORT, indices);
    } else {
        glDrawArrays(Primitive(primitive), 0, static_cast<GLsizei>(count));
    }
    UnbindSingleStream(type);
}

void DrawMultiStream(const void* type, const StreamEntry* entries, unsigned entryCount, const void* indices,
                     const Jet::StaticBuffer* indexBuffer, unsigned count, int primitive)
{
    if (!type || !entries || !count) return;
    const unsigned streams = StreamCount(type);
    const auto* list = StreamList(type);
    if (!list) return;
    const unsigned use = streams < entryCount ? streams : entryCount;
    for (unsigned s = 0; s < use; ++s) {
        const void* const data = StreamData(entries[s]);
        if (!data) continue;
        for (unsigned i = 0; i < list[s].count; ++i) {
            const auto& item = list[s].items[i];
            if (item.type >= 15 && item.type <= 22) SetActiveUnit(item.type - 15);
            EnableItem(item, static_cast<const unsigned char*>(data), list[s].stride);
        }
    }
    if (!indices && indexBuffer) indices = AsBuffer(indexBuffer)->data;
    if (indices) {
        glDrawElements(Primitive(primitive), static_cast<GLsizei>(count), GL_UNSIGNED_SHORT, indices);
    } else {
        glDrawArrays(Primitive(primitive), 0, static_cast<GLsizei>(count));
    }
    for (unsigned s = 0; s < use; ++s)
        for (unsigned i = 0; i < list[s].count; ++i) DisableItems(list[s].items[i]);
    SetActiveUnit(0);
}

// ---- display descriptor ---------------------------------------------------------------------

struct DescriptorAccess : Jet::DisplayDescriptorOpenGL {
    void SetFallback()
    {
        const Jet::PString api("OpenGL");
        const Jet::PString version("1.1");
        const Jet::PString vendor("OpenGL");
        const Jet::PString card("OpenGL");
        SetData(api, version, vendor, card, 1.1f);
    }
};

// ---- display interface ----------------------------------------------------------------------

void UpdateDescriptorContext(void* self, DisplayInterfaceWGL* iface)
{
    auto* descriptor = reinterpret_cast<Jet::DisplayDescriptorOpenGL*>(Bytes(self) + kDescriptorOffset);
    auto* context = new Jet::ContextDescriptorOpenGLWGL();
    context->SetContext(iface ? iface->DeviceContext() : nullptr, iface ? iface->RenderContext() : nullptr);
    Field<Jet::ContextDescriptorOpenGL*>(descriptor, 0x1c) = context;
}

DisplayInterfaceWGL* EnsureInterface(void* self)
{
    if (DisplayInterfaceWGL* existing = Interface(self)) return existing;
    auto* window = Field<Jet::Window*>(self, 0x40);
    if (!window) return nullptr;
    HWND hwnd = window->Handle();
    if (!hwnd) return nullptr;
    auto* iface = new DisplayInterfaceWGL();
    if (!iface->Create(hwnd, 32, 24, 8, 8, false)) {
        if (!iface->Create(hwnd, 16, 24, 8, 0, false)) {
            delete iface;
            return nullptr;
        }
    }
    Field<DisplayInterfaceWGL*>(self, 0xf00) = iface;
    g_state.iface = iface;
    UpdateDescriptorContext(self, iface);
    return iface;
}

void ApplyDefaults(void* self)
{
    auto* iface = Interface(self);
    const int width = Field<int>(self, 0x28c);
    const int height = Field<int>(self, 0x290);
    glViewport(0, 0, width > 0 ? width : (iface ? iface->width : 1),
               height > 0 ? height : (iface ? iface->height : 1));
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glDisable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDisable(GL_BLEND);
    if (GL::ext.BlendEquation) GL::ext.BlendEquation(kFuncAdd);
    glDisable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.0f);
    glDisable(GL_LIGHTING);
    glDisable(GL_FOG);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_STENCIL_TEST);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, 0);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    if (GL::ext.ActiveTexture) {
        for (int unit = 0; unit < g_state.maxTextureUnits; ++unit) {
            SetActiveUnit(unit);
            glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
        }
        SetActiveUnit(0);
    }
}

// ---- vtable slots ----------------------------------------------------------------------------

extern "C" int __cdecl SlotZero() { return 0; }
extern "C" bool __cdecl SlotTrue() { return true; }
extern "C" float __cdecl SlotOne() { return 1.0f; }
extern "C" void __cdecl SlotVoid(void*) {}

extern "C" bool __cdecl SlotInit(void* self, void* os)
{
    Trace("Init");
    if (auto fn = BaseInit()) {
        if (!fn(self, os)) return false;
    }
    return true;
}

extern "C" void __cdecl SlotShutdown(void* self)
{
    Trace("Shutdown");
    if (g_state.shutdown) return;
    g_state.shutdown = true;
    if (g_state.initialized) {
        if (auto fn = BaseShutdown()) fn(self);
    }
    if (auto* iface = Interface(self)) {
        iface->Destroy();
        delete iface;
        Field<void*>(self, 0xf00) = nullptr;
        g_state.iface = nullptr;
    }
}

extern "C" bool __cdecl SlotOpen(void* self, const void* options, bool select)
{
    Trace("Open enter");
    g_state.options = options;
    bool opened = false;
    if (auto fn = BaseOpen()) opened = fn(self, options, select);
    Trace(opened ? "Open base ok" : "Open base failed");
    if (!opened) return false;
    if (!EnsureInterface(self)) return false;
    // Display::Open creates the host window after PreOpen; initialize after it exists.
    if (!g_state.initialized) {
        Field<int>(self, 0x50) = 8;  // hardware lights the base class may use
        auto* context = Field<void*>(self, 0x58);
        if (auto init = InitializeState()) init(self, context, true, false, g_state.options);
        g_state.initialized = true;
    }
    ApplyDefaults(self);
    return true;
}

extern "C" void __cdecl SlotBeginFrame(void* self, float delta)
{
    if (!g_state.inFrame) Trace("BeginFrame");
    if (auto fn = BaseBeginFrame()) fn(self, delta);
    if (auto* iface = Interface(self)) iface->MakeCurrent();
    g_state.inFrame = true;
    ++g_state.frame;
}

extern "C" void __cdecl SlotEndFrame(void* self, float delta)
{
    Trace("EndFrame");
    if (auto fn = BaseEndFrame()) fn(self, delta);
    g_state.inFrame = false;
    if (auto* iface = Interface(self)) {
        iface->MakeCurrent();
        iface->SwapBuffers();
    }
}

extern "C" const char* __cdecl SlotName(void*) { Trace("SlotName"); return "OpenGL"; }
extern "C" const char* __cdecl SlotApi(void*) { return "OpenGL"; }

extern "C" unsigned char __cdecl SlotExtension(void*, int id)
{
    switch (id) {
    case 0: return 1;  // D_MULTI_TEXTURE
    case 1: case 2: case 3: case 4: return 0;  // combiners are not advertised
    case 5: return GL::ext.GenerateMipmap != nullptr;  // D_AUTOMIP
    case 6: case 7: return 0;  // alpha to coverage
    case 8: return 0;  // render to texture
    case 9: return 0;  // depth texture
    case 10: return 1;  // stencil buffer
    case 11: return 0;  // depth-stencil
    case 12: return GL::ext.StencilOpSeparate != nullptr;  // two-sided stencil
    case 13: return 0;
    case 14: return 1;  // object-space texgen
    case 15: return 0;  // save buffers
    case 16: return GL::IsSupported("GL_NV_depth_clamp") ? 1 : 0;
    case 17: case 18: case 19: case 20: case 21: case 22: case 23: case 24: case 25: return 0;
    case 26: return 1;  // sphere mapping
    case 27: return 1;  // cube mapping
    case 28: return 0;  // dot3 bump
    case 29: return 0;  // hardware vertex processing
    case 30: return 0;  // vertex buffers (client arrays are used)
    case 31: return 0;  // concurrent contexts
    default: return 0;
    }
}

extern "C" float __cdecl SlotMaxAnisotropy(void*)
{
    if (!GL::IsSupported("GL_EXT_texture_filter_anisotropic")) return 1.0f;
    GLfloat value = 1.0f;
    glGetFloatv(kMaxTextureMaxAnisotropy, &value);
    return value < 1.0f ? 1.0f : value;
}

extern "C" float __cdecl SlotTexelOffset(void*) { return 0.0f; }
extern "C" float __cdecl SlotGamma(void* self) { return Field<float>(self, 0xf20); }

extern "C" void __cdecl SlotSetGamma(void* self, float gamma)
{
    if (gamma < 1.0f) gamma = 1.0f;
    Field<float>(self, 0xf20) = gamma;
}

extern "C" void __cdecl SlotClear(void* self, unsigned mask)
{
    (void)self;
    GLbitfield bits = 0;
    if (mask & 1) bits |= GL_COLOR_BUFFER_BIT;
    if (mask & 2) bits |= GL_DEPTH_BUFFER_BIT;
    if (mask & 4) bits |= GL_STENCIL_BUFFER_BIT;
    if (bits) glClear(bits);
}

extern "C" void __cdecl SlotClearColor(void*, const Jet::Color* color)
{
    if (color) glClearColor(color->r, color->g, color->b, color->a);
}

extern "C" void __cdecl SlotClearDepth(void*, float depth) { glClearDepth(depth); }
extern "C" void __cdecl SlotClearStencil(void*, long stencil) { glClearStencil(stencil); }

extern "C" void __cdecl SlotViewport(void*, const Jet::Rect* rect)
{
    if (rect) glViewport(rect->x, rect->y, rect->w, rect->h);
}

extern "C" void __cdecl SlotScissor(void*, const Jet::Rect* rect)
{
    if (rect) glScissor(rect->x, rect->y, rect->w, rect->h);
}

extern "C" void __cdecl SlotScissorMode(void*, bool on)
{
    if (on) glEnable(GL_SCISSOR_TEST);
    else glDisable(GL_SCISSOR_TEST);
}

extern "C" void __cdecl SlotMask(void*, int kind, unsigned value)
{
    if (kind == 1) {
        glColorMask((value & 0x00ff0000u) != 0, (value & 0x0000ff00u) != 0, (value & 0x000000ffu) != 0,
                    (value & 0xff000000u) != 0);
    } else if (kind == 2) {
        glDepthMask(value != 0);
    }
}

extern "C" void __cdecl SlotDepthFunction(void*, int function) { glDepthFunc(DepthFunction(function)); }

extern "C" void __cdecl SlotDepthTest(void*, bool on)
{
    if (on) glEnable(GL_DEPTH_TEST);
    else glDisable(GL_DEPTH_TEST);
}

extern "C" void __cdecl SlotDepthClamp(void*, bool on)
{
    if (!GL::IsSupported("GL_NV_depth_clamp")) return;
    if (on) glEnable(kDepthClampNv);
    else glDisable(kDepthClampNv);
}

extern "C" bool __cdecl SlotStencilTest(void* self, bool on, bool twoSided)
{
    (void)self;
    (void)twoSided;
    if (on) glEnable(GL_STENCIL_TEST);
    else glDisable(GL_STENCIL_TEST);
    return true;
}

extern "C" void __cdecl SlotStencilMask(void*, int face, unsigned mask)
{
    if (face == 0) {
        glStencilMask(mask);
    } else if (GL::ext.StencilMaskSeparate) {
        GL::ext.StencilMaskSeparate(GL_BACK, mask);
    }
}

extern "C" void __cdecl SlotStencilFunction(void*, int face, int function, int reference, unsigned mask)
{
    const GLenum side = face == 0 ? GL_FRONT : GL_BACK;
    if (GL::ext.StencilFuncSeparate) GL::ext.StencilFuncSeparate(side, DepthFunction(function), reference, mask);
    else glStencilFunc(DepthFunction(function), reference, mask);
}

extern "C" void __cdecl SlotStencilOperation(void*, int face, int fail, int depthFail, int pass)
{
    const GLenum side = face == 0 ? GL_FRONT : GL_BACK;
    if (GL::ext.StencilOpSeparate) {
        GL::ext.StencilOpSeparate(side, StencilOperation(fail), StencilOperation(depthFail), StencilOperation(pass));
    } else {
        glStencilOp(StencilOperation(fail), StencilOperation(depthFail), StencilOperation(pass));
    }
}

extern "C" void __cdecl SlotStencilSetup(void* self, int face, int function, int reference, unsigned mask, int fail,
                                         int depthFail, int pass)
{
    SlotStencilFunction(self, face, function, reference, mask);
    SlotStencilOperation(self, face, fail, depthFail, pass);
}

extern "C" void __cdecl SlotCullMode(void*, int mode)
{
    if (mode == 0) {
        glDisable(GL_CULL_FACE);
        return;
    }
    glEnable(GL_CULL_FACE);
    if (mode == 2) {
        glCullFace(GL_FRONT);
        glFrontFace(GL_CW);
    } else {
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);
    }
}

extern "C" void __cdecl SlotBlendFunction(void*, int source, int destination)
{
    glBlendFunc(BlendSrc(source), BlendDst(destination));
}

extern "C" void __cdecl SlotBlendEquation(void*, int equation)
{
    if (GL::ext.BlendEquation) GL::ext.BlendEquation(BlendEquation(equation));
}

extern "C" void __cdecl SlotAlphaTest(void*, bool on)
{
    if (on) glEnable(GL_ALPHA_TEST);
    else glDisable(GL_ALPHA_TEST);
}

extern "C" void __cdecl SlotAlphaFunction(void*, int function, float reference)
{
    glAlphaFunc(DepthFunction(function), reference);
}

extern "C" void __cdecl SlotAlphaBlend(void*, bool on)
{
    if (on) glEnable(GL_BLEND);
    else glDisable(GL_BLEND);
}

extern "C" void __cdecl SlotLighting(void*, bool on)
{
    if (on) glEnable(GL_LIGHTING);
    else glDisable(GL_LIGHTING);
}

extern "C" void __cdecl SlotSeparateSpecular(void*, bool on)
{
    if (!GL::IsSupported("GL_EXT_separate_specular_color")) return;
    glLightModeli(kLightModelColorControl, on ? kSeparateSpecularColor : kSingleColor);
}

extern "C" void __cdecl SlotLocalViewer(void*, bool on) { glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, on ? 1 : 0); }

extern "C" void __cdecl SlotFogMode(void*, int mode)
{
    if (mode == 0) {
        glDisable(GL_FOG);
        return;
    }
    glEnable(GL_FOG);
    glFogi(GL_FOG_MODE, FogMode(mode));
}

extern "C" void __cdecl SlotFogDensity(void*, float density) { glFogf(GL_FOG_DENSITY, density); }

extern "C" void __cdecl SlotFogRange(void*, float start, float end)
{
    glFogf(GL_FOG_START, start);
    glFogf(GL_FOG_END, end);
}

extern "C" void __cdecl SlotFogColor(void*, const Jet::Color* color)
{
    if (!color) return;
    const GLfloat value[4]{color->r, color->g, color->b, color->a};
    glFogfv(GL_FOG_COLOR, value);
}

extern "C" void __cdecl SlotColorMaterial(void*, int mode)
{
    if (mode == 0) {
        glDisable(GL_COLOR_MATERIAL);
        return;
    }
    GLenum face = GL_AMBIENT_AND_DIFFUSE;
    switch (mode) {
    case 1: face = GL_AMBIENT; break;
    case 2: face = GL_DIFFUSE; break;
    case 3: face = GL_SPECULAR; break;
    case 4: face = GL_EMISSION; break;
    case 5: face = GL_AMBIENT_AND_DIFFUSE; break;
    default: break;
    }
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, face);
}

extern "C" void __cdecl SlotTextureMode(void*, bool on)
{
    if (on) {
        glEnable(GL_TEXTURE_2D);
    } else {
        glDisable(GL_TEXTURE_2D);
    }
}

extern "C" void __cdecl SlotSetTexture(void*, int stage)
{
    if (stage < 0) stage = 0;
    if (stage >= g_state.maxTextureUnits) stage = g_state.maxTextureUnits - 1;
    g_state.currentUnit = stage;
    SetActiveUnit(stage);
}

extern "C" void __cdecl SlotTextureCube(void*, int stage)
{
    if (stage < 0) stage = 0;
    if (stage >= g_state.maxTextureUnits) stage = g_state.maxTextureUnits - 1;
    SetActiveUnit(stage);
}

extern "C" void __cdecl SlotProjectedTexture(void* self, int stage, bool on, bool useAlpha)
{
    (void)self;
    (void)useAlpha;
    if (stage < 0 || stage >= g_state.maxTextureUnits) return;
    SetActiveUnit(stage);
    if (!on) {
        glDisable(GL_TEXTURE_GEN_S);
        glDisable(GL_TEXTURE_GEN_T);
        glDisable(GL_TEXTURE_GEN_R);
        glDisable(GL_TEXTURE_GEN_Q);
        return;
    }
    const GLfloat planeS[4]{1.0f, 0.0f, 0.0f, 0.0f};
    const GLfloat planeT[4]{0.0f, 1.0f, 0.0f, 0.0f};
    glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
    glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR);
    glTexGenfv(GL_S, GL_OBJECT_PLANE, planeS);
    glTexGenfv(GL_T, GL_OBJECT_PLANE, planeT);
    glEnable(GL_TEXTURE_GEN_S);
    glEnable(GL_TEXTURE_GEN_T);
}

extern "C" void __cdecl SlotTexCoordGen(void* self, int stage, int mode, unsigned coordinates)
{
    SlotProjectedTexture(self, stage, mode != 0 && coordinates != 0, false);
}

extern "C" void __cdecl SlotMaterial(void*, const Jet::DisplayMaterial* material)
{
    if (!material) return;
    const GLfloat ambient[4]{material->ambient.r, material->ambient.g, material->ambient.b, material->ambient.a};
    const GLfloat diffuse[4]{material->diffuse.r, material->diffuse.g, material->diffuse.b, material->diffuse.a};
    const GLfloat specular[4]{material->specular.r, material->specular.g, material->specular.b, material->specular.a};
    const GLfloat emissive[4]{material->emissive.r, material->emissive.g, material->emissive.b, material->emissive.a};
    glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, ambient);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffuse);
    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specular);
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, emissive);
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, material->power);
}

extern "C" void __cdecl SlotLightMode(void*, int index, bool on)
{
    if (index < 0 || index >= 8) return;
    const GLenum light = GL_LIGHT0 + index;
    if (on) glEnable(light);
    else glDisable(light);
}

extern "C" void __cdecl SlotLightState(void*, int index, const Jet::DisplayLight* light)
{
    if (!light || index < 0 || index >= 8) return;
    const GLenum id = GL_LIGHT0 + index;
    const GLfloat ambient[4]{light->GetAmbient().r, light->GetAmbient().g, light->GetAmbient().b, light->GetAmbient().a};
    const GLfloat diffuse[4]{light->GetDiffuse().r, light->GetDiffuse().g, light->GetDiffuse().b, light->GetDiffuse().a};
    const GLfloat specular[4]{light->GetSpecular().r, light->GetSpecular().g, light->GetSpecular().b,
                              light->GetSpecular().a};
    glLightfv(id, GL_AMBIENT, ambient);
    glLightfv(id, GL_DIFFUSE, diffuse);
    glLightfv(id, GL_SPECULAR, specular);
    glLightf(id, GL_CONSTANT_ATTENUATION, light->GetConstantAttenuation());
    glLightf(id, GL_LINEAR_ATTENUATION, light->GetLinearAttenuation());
    glLightf(id, GL_QUADRATIC_ATTENUATION, light->GetQuadraticAttenuation());
    glLightf(id, GL_SPOT_EXPONENT, light->GetSpotExponent());
    glLightf(id, GL_SPOT_CUTOFF, light->GetSpotCutoff());
    // DisplayLight carries no position: Render3DCore light subclasses provide one to the retail backend.
    // The local backend uses a camera-space head light, which keeps every lit surface visible.
    const GLfloat position[4]{0.0f, 0.0f, 1.0f, 0.0f};
    glLightfv(id, GL_POSITION, position);
}

extern "C" void __cdecl SlotLightingUpdate(void*, const Jet::Matrix4*)
{
    // Light positions are camera-space in this backend; nothing to transform.
}

extern "C" void __cdecl SlotGlobalAmbient(void*, const Jet::Color* color)
{
    if (!color) return;
    const GLfloat value[4]{color->r, color->g, color->b, color->a};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, value);
}

extern "C" void __cdecl SlotPolygonOffset(void*, float factor, float units) { glPolygonOffset(factor, units); }

extern "C" void __cdecl SlotPolygonOffsetMode(void*, int mode, bool on)
{
    auto toggle = [on](GLenum cap) {
        if (on) glEnable(cap);
        else glDisable(cap);
    };
    if (mode == 0) toggle(kPolygonOffsetPoint);
    else if (mode == 1) toggle(kPolygonOffsetLine);
    else if (mode == 2) toggle(kPolygonOffsetFill);
    else if (mode == 5) {
        toggle(kPolygonOffsetPoint);
        toggle(kPolygonOffsetLine);
        toggle(kPolygonOffsetFill);
    }
}

extern "C" void __cdecl SlotMatrixIdentity(void* self)
{
    SetMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    auto* context = Field<unsigned char*>(self, 0x54);
    if (context) {
        float* const matrix = reinterpret_cast<float*>(context + 0x1dc);
        for (int i = 0; i < 16; ++i) matrix[i] = (i % 5 == 0) ? 1.0f : 0.0f;
    }
}

extern "C" void __cdecl SlotMatrixMultiply(void*, const Jet::Matrix4* matrix)
{
    if (!matrix) return;
    SetMatrixMode(GL_MODELVIEW);
    glMultMatrixf(matrix->m);
}

extern "C" void __cdecl SlotMatrixLoadIdentity(void*, int mode)
{
    const GLenum target = mode == 1 ? GL_PROJECTION : mode >= 2 ? kTexture0 + (mode - 2) : GL_MODELVIEW;
    SetMatrixMode(target);
    glLoadIdentity();
}

extern "C" void __cdecl SlotMatrixLoad(void*, int mode, const Jet::Matrix4* matrix)
{
    if (!matrix) return;
    const GLenum target = mode == 1 ? GL_PROJECTION : mode >= 2 ? kTexture0 + (mode - 2) : GL_MODELVIEW;
    SetMatrixMode(target);
    glLoadMatrixf(matrix->m);
}

extern "C" void __cdecl SlotLoadMatrix(void*, const Jet::Matrix4* matrix)
{
    if (matrix) glLoadMatrixf(matrix->m);
}

extern "C" void __cdecl SlotLoadMatrixIndexed(void*, int, const Jet::Matrix4* matrix)
{
    if (matrix) glLoadMatrixf(matrix->m);
}

extern "C" void __cdecl SlotProjection(void*, float left, float right, float bottom, float top, float nearPlane,
                                       float farPlane)
{
    SetMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    if (right == left || top == bottom || farPlane == nearPlane) {
        glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);
    } else {
        glOrtho(left, right, bottom, top, nearPlane, farPlane);
    }
    SetMatrixMode(GL_MODELVIEW);
}

extern "C" void __cdecl SlotPerspective(void*, float width, float height, float nearPlane, float farPlane)
{
    if (height <= 0.0f || width <= 0.0f) return;
    if (farPlane <= nearPlane || nearPlane <= 0.0f) return;
    SetMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, static_cast<double>(width) / static_cast<double>(height), nearPlane, farPlane);
    SetMatrixMode(GL_MODELVIEW);
}

extern "C" void __cdecl SlotTransitional(void* self, float a, float b, float c, float d, float e, float f, float g)
{
    (void)e;
    (void)f;
    (void)g;
    if (a > 0.0f && b > 0.0f) {
        SlotPerspective(self, a, b, c > 0.0f ? c : 1.0f, d > c ? d : 1000.0f);
    }
}

extern "C" void __cdecl SlotLoadProjection(void*, const Jet::Matrix4* matrix)
{
    if (!matrix) return;
    SetMatrixMode(GL_PROJECTION);
    glLoadMatrixf(matrix->m);
    SetMatrixMode(GL_MODELVIEW);
}

extern "C" void __cdecl SlotMatrixMode(void* self, int mode)
{
    (void)self;
    (void)mode;
}

extern "C" void* __cdecl SlotCreateTexture(void* self)
{
    return new Jet::DisplayTextureOpenGL(static_cast<Jet::Display*>(self));
}

extern "C" void* __cdecl SlotAllocStaticVertex(void*, unsigned count, unsigned stride)
{
    return NewStaticBuffer(count, stride, false, false);
}

extern "C" void* __cdecl SlotAllocStaticIndex(void*, unsigned count, int wide)
{
    return NewStaticBuffer(count, 0, true, wide != 0);
}

extern "C" void __cdecl SlotDrawUser(void* self, void* type, int primitive, int, const void* vertices, int count)
{
    (void)self;
    if (!type || count <= 0) return;
    BindSingleStream(type, vertices, nullptr);
    glDrawArrays(Primitive(primitive), 0, count);
    UnbindSingleStream(type);
}

extern "C" void __cdecl SlotDrawStatic(void* self, void* type, int primitive, int, const void* vertices, int,
                                       const void* indices, int count, void* vertexBuffer, int, void* indexBuffer, int)
{
    (void)self;
    DrawElementBatch(type, vertices, static_cast<const Jet::StaticBuffer*>(vertexBuffer), indices,
                     static_cast<const Jet::StaticBuffer*>(indexBuffer), static_cast<unsigned>(count), primitive);
}

extern "C" void __cdecl SlotDrawDynamic(void* self, void* type, int primitive, int, const void* vertices, int,
                                        const void* indices, int count)
{
    (void)self;
    DrawElementBatch(type, vertices, nullptr, indices, nullptr, static_cast<unsigned>(count), primitive);
}

extern "C" void __cdecl SlotDrawMulti(void* self, void* type, int primitive, int, const StreamEntry* entries, int,
                                      const void* indices, const Jet::StaticBuffer* indexBuffer, int, int count, int)
{
    (void)self;
    DrawMultiStream(type, entries, StreamCount(type), indices, indexBuffer, static_cast<unsigned>(count), primitive);
}

extern "C" void __cdecl SlotDrawArrays(void* self, void* type, int primitive, int, const void* vertices, int count)
{
    SlotDrawUser(self, type, primitive, 0, vertices, count);
}

extern "C" bool __cdecl SlotReadColor(void*, const Jet::Rect* area, void* destination)
{
    if (!area || !destination) return false;
    glReadPixels(area->x, area->y, area->w, area->h, 0x80E1 /* GL_BGRA_EXT */, GL_UNSIGNED_BYTE, destination);
    return true;
}

extern "C" bool __cdecl SlotReadDepth(void*, const Jet::Rect* area, void* destination)
{
    if (!area || !destination) return false;
    glReadPixels(area->x, area->y, area->w, area->h, GL_DEPTH_COMPONENT, GL_FLOAT, destination);
    return true;
}

extern "C" void* __cdecl SlotDescriptor(void* self) { Trace("SlotDescriptor"); return Bytes(self) + kDescriptorOffset; }

extern "C" void* __cdecl SlotBufferManager(void* self)
{
    if (!g_state.bufferManager) g_state.bufferManager = CreateBufferManager(self);
    Field<void*>(self, 0x20c) = g_state.bufferManager;
    return g_state.bufferManager;
}

extern "C" void __cdecl SlotCreateVertexType(void* self, int id)
{
    using Fn = void(__cdecl*)(void*, int);
    if (auto fn = Sym<Fn>(RsModule(), "?CreateVertexTypeObject@Display@Jet@@MAAXW4VertexTypeID@2@@Z")) fn(self, id);
}

extern "C" bool __cdecl SlotPreOpen(void* self, const void* options)
{
    Trace("PreOpen enter");
    g_state.options = options;
    auto fn = BasePreOpen();
    bool ok = fn ? fn(self, options) : false;
    Trace(ok ? "PreOpen base ok" : "PreOpen base failed");
    if (ok) EnsureInterface(self);
    return ok;
}

extern "C" bool __cdecl SlotSetupVideoFormat(void*, const void*, void*) { return true; }

extern "C" bool __cdecl SlotResetVideoMode(void* self, const void*)
{
    auto* words = reinterpret_cast<unsigned*>(Bytes(self) + 0x24c);
    words[0] = words[1] = words[2] = words[3] = 0;
    return true;
}

extern "C" bool __cdecl SlotResetVideoFormat(void* self)
{
    auto* words = reinterpret_cast<unsigned*>(Bytes(self) + 0x25c);
    words[0] = words[1] = words[2] = words[3] = words[4] = words[5] = words[6] = words[7] = 0;
    return true;
}

extern "C" bool __cdecl SlotSelectVideoMode(void*, const void*) { return true; }
extern "C" void __cdecl SlotInitializeFormats(void*, const void*) {}

extern "C" bool __cdecl SlotFinishVideoMode(void* self, const void*)
{
    Trace("Finish enter");
    auto* iface = EnsureInterface(self);
    if (!iface) { Trace("Finish no iface"); return false; }
    GL::LoadExtensions();
    g_state.maxTextureUnits = MaxTextureUnits();
    g_state.maxAnisotropy = static_cast<int>(SlotMaxAnisotropy(self));
    if (!g_state.initialized) {
        Field<int>(self, 0x50) = 8;
        auto* context = Field<void*>(self, 0x58);
        if (auto init = InitializeState()) init(self, context, true, false, g_state.options);
        g_state.initialized = true;
    }
    SlotBufferManager(self);
    ApplyDefaults(self);
    Trace("Finish ok");
    return true;
}

extern "C" void __cdecl SlotFullscreen(bool*) {}
extern "C" unsigned __cdecl SlotCapability(void* self, int* value)
{
    (void)self;
    (void)value;
    return 0;
}

extern "C" bool __cdecl SlotCreateSaveBuffer(void*, unsigned) { return false; }

extern "C" void __cdecl SlotFlushState(void* self)
{
    if (g_state.bufferManager) {
        // Nothing to flush in the client-array backend.
    }
    glFlush();
    (void)self;
}

extern "C" void __cdecl SlotSwapHint(void*) {}

// ---- plugin instance table and display factory ----------------------------------------------

void* g_pluginStorageXTerm[3];
void* g_pluginStorageBase[3];
void** g_pluginVtableXTerm = g_pluginStorageXTerm + 1;
void** g_pluginVtableBase = g_pluginStorageBase + 1;

void __cdecl PluginDeleteThisXTerm(void* pluginBase)
{
    TracePtr("PluginDeleteThisXTerm", pluginBase);
    Jet::DestroyDisplayObject(static_cast<unsigned char*>(pluginBase) - 0xfa4);
}

void* __cdecl PluginDeletingDtorXTerm(void* pluginBase, unsigned)
{
    Jet::DestroyDisplayObject(static_cast<unsigned char*>(pluginBase) - 0xfa4);
    return pluginBase;
}

void __cdecl PluginDeleteThisBase(void* pluginBase)
{
    TracePtr("PluginDeleteThisBase", pluginBase);
    Jet::DestroyDisplayObject(static_cast<unsigned char*>(pluginBase) - 0xfa0);
}

void* __cdecl PluginDeletingDtorBase(void* pluginBase, unsigned)
{
    Jet::DestroyDisplayObject(static_cast<unsigned char*>(pluginBase) - 0xfa0);
    return pluginBase;
}

class DisplayFactory : public Jet::PluginFactoryBase<Jet::Display> {
public:
    DisplayFactory(const Jet::PString& name, const Jet::UId& id, const Jet::UId& parent, bool xterm)
        : PluginFactoryBase<Jet::Display>(name, id, parent), xterm(xterm)
    {
    }

    Jet::Display* __cdecl CreateInstance() const override
    {
        Trace("Factory create enter");
        auto* display = static_cast<Jet::Display*>(Jet::CreateDisplayObject(xterm));
        Trace("Factory object made");
        if (display) {
            // PluginInstance lives at +0xf98 (XTerm) or +0xf94 (base); Assign writes its factory at +4.
            this->AssignFactory(reinterpret_cast<Jet::PluginInstance*>(
                Bytes(display) + (xterm ? 0xf98 : 0xf94)));
        }
        Trace("Factory create done");
        return display;
    }

private:
    bool xterm;
};

}  // namespace

// The module's primary and plugin tables.
void* g_vtableStorage[162];
void** g_vtable = g_vtableStorage + 1;

namespace {

void BuildTables()
{
    static bool built = false;
    if (built) return;
    built = true;

    auto* base = reinterpret_cast<void**>(GetProcAddress(RsModule(), "??_7Display@Jet@@6B01@@"));
    for (int i = 0; i < 161; ++i) g_vtable[i] = reinterpret_cast<void*>(&SlotZero);
    if (base) {
        std::memcpy(g_vtable, base, 152 * sizeof(void*));
        g_vtable[-1] = base[-1];  // the local table's RTTI locator
    }

    g_vtable[0x00 / 4] = reinterpret_cast<void*>(&SlotInit);
    g_vtable[0x04 / 4] = reinterpret_cast<void*>(&SlotTrue);
    g_vtable[0x08 / 4] = reinterpret_cast<void*>(&SlotTrue);
    g_vtable[0x0c / 4] = reinterpret_cast<void*>(&SlotTrue);
    g_vtable[0x10 / 4] = reinterpret_cast<void*>(&SlotTrue);
    g_vtable[0x14 / 4] = reinterpret_cast<void*>(&SlotZero);
    g_vtable[0x18 / 4] = reinterpret_cast<void*>(&SlotName);
    g_vtable[0x1c / 4] = reinterpret_cast<void*>(&SlotExtension);
    g_vtable[0x28 / 4] = reinterpret_cast<void*>(&SlotMaxAnisotropy);
    g_vtable[0x30 / 4] = reinterpret_cast<void*>(&SlotShutdown);
    g_vtable[0x34 / 4] = reinterpret_cast<void*>(&SlotOpen);
    g_vtable[0x38 / 4] = reinterpret_cast<void*>(&SlotVoid);
    g_vtable[0x3c / 4] = reinterpret_cast<void*>(&SlotVoid);
    g_vtable[0x40 / 4] = reinterpret_cast<void*>(&SlotVoid);
    g_vtable[0x44 / 4] = reinterpret_cast<void*>(&SlotBeginFrame);
    g_vtable[0x48 / 4] = reinterpret_cast<void*>(&SlotEndFrame);
    g_vtable[0x50 / 4] = reinterpret_cast<void*>(&SlotMatrixIdentity);
    g_vtable[0x54 / 4] = reinterpret_cast<void*>(&SlotMatrixMultiply);
    g_vtable[0x58 / 4] = reinterpret_cast<void*>(&SlotMatrixLoadIdentity);
    g_vtable[0x5c / 4] = reinterpret_cast<void*>(&SlotMatrixLoad);
    g_vtable[0x68 / 4] = reinterpret_cast<void*>(&SlotLoadMatrixIndexed);
    g_vtable[0x6c / 4] = reinterpret_cast<void*>(&SlotLoadProjection);
    g_vtable[0x70 / 4] = reinterpret_cast<void*>(&SlotLoadMatrix);
    g_vtable[0x74 / 4] = reinterpret_cast<void*>(&SlotMatrixMode);
    g_vtable[0x80 / 4] = reinterpret_cast<void*>(&SlotTexelOffset);
    g_vtable[0x88 / 4] = reinterpret_cast<void*>(&SlotZero);
    g_vtable[0x8c / 4] = reinterpret_cast<void*>(&SlotCapability);
    g_vtable[0x98 / 4] = reinterpret_cast<void*>(&SlotCreateTexture);
    g_vtable[0xa0 / 4] = reinterpret_cast<void*>(&SlotSetTexture);
    g_vtable[0xa4 / 4] = reinterpret_cast<void*>(&SlotTextureCube);
    g_vtable[0xd0 / 4] = reinterpret_cast<void*>(&SlotClear);
    g_vtable[0xd4 / 4] = reinterpret_cast<void*>(&SlotReadColor);
    g_vtable[0xd8 / 4] = reinterpret_cast<void*>(&SlotReadDepth);
    g_vtable[0xdc / 4] = reinterpret_cast<void*>(&SlotCreateSaveBuffer);
    g_vtable[0xe4 / 4] = reinterpret_cast<void*>(&SlotDrawUser);
    g_vtable[0xec / 4] = reinterpret_cast<void*>(&SlotDrawStatic);
    g_vtable[0xf0 / 4] = reinterpret_cast<void*>(&SlotDrawDynamic);
    g_vtable[0xf4 / 4] = reinterpret_cast<void*>(&SlotDrawMulti);
    g_vtable[0x104 / 4] = reinterpret_cast<void*>(&SlotAllocStaticVertex);
    g_vtable[0x108 / 4] = reinterpret_cast<void*>(&SlotAllocStaticIndex);
    g_vtable[0x114 / 4] = reinterpret_cast<void*>(&SlotZero);
    g_vtable[0x144 / 4] = reinterpret_cast<void*>(&SlotZero);
    g_vtable[0x150 / 4] = reinterpret_cast<void*>(&SlotDescriptor);
    g_vtable[0x154 / 4] = reinterpret_cast<void*>(&SlotZero);
    g_vtable[0x158 / 4] = reinterpret_cast<void*>(&SlotZero);
    g_vtable[0x15c / 4] = reinterpret_cast<void*>(&SlotVoid);
    g_vtable[0x160 / 4] = reinterpret_cast<void*>(&SlotVoid);
    g_vtable[0x180 / 4] = reinterpret_cast<void*>(&SlotProjection);
    g_vtable[0x184 / 4] = reinterpret_cast<void*>(&SlotTransitional);
    g_vtable[0x188 / 4] = reinterpret_cast<void*>(&SlotPerspective);
    g_vtable[0x18c / 4] = reinterpret_cast<void*>(&SlotVoid);  // wire frame: fixed function path
    g_vtable[0x190 / 4] = reinterpret_cast<void*>(&SlotViewport);
    g_vtable[0x194 / 4] = reinterpret_cast<void*>(&SlotScissor);
    g_vtable[0x198 / 4] = reinterpret_cast<void*>(&SlotScissorMode);
    g_vtable[0x19c / 4] = reinterpret_cast<void*>(&SlotLoadProjection);
    g_vtable[0x1a0 / 4] = reinterpret_cast<void*>(&SlotClearColor);
    g_vtable[0x1a4 / 4] = reinterpret_cast<void*>(&SlotClearDepth);
    g_vtable[0x1a8 / 4] = reinterpret_cast<void*>(&SlotClearStencil);
    g_vtable[0x1ac / 4] = reinterpret_cast<void*>(&SlotMask);
    g_vtable[0x1b0 / 4] = reinterpret_cast<void*>(&SlotDepthFunction);
    g_vtable[0x1b4 / 4] = reinterpret_cast<void*>(&SlotDepthTest);
    g_vtable[0x1b8 / 4] = reinterpret_cast<void*>(&SlotDepthClamp);
    g_vtable[0x1bc / 4] = reinterpret_cast<void*>(&SlotStencilTest);
    g_vtable[0x1c0 / 4] = reinterpret_cast<void*>(&SlotStencilMask);
    g_vtable[0x1c4 / 4] = reinterpret_cast<void*>(&SlotStencilFunction);
    g_vtable[0x1c8 / 4] = reinterpret_cast<void*>(&SlotStencilOperation);
    g_vtable[0x1cc / 4] = reinterpret_cast<void*>(&SlotStencilSetup);
    g_vtable[0x1d0 / 4] = reinterpret_cast<void*>(&SlotVoid);
    g_vtable[0x1d4 / 4] = reinterpret_cast<void*>(&SlotVoid);
    g_vtable[0x1d8 / 4] = reinterpret_cast<void*>(&SlotCullMode);
    g_vtable[0x1dc / 4] = reinterpret_cast<void*>(&SlotBlendFunction);
    g_vtable[0x1e0 / 4] = reinterpret_cast<void*>(&SlotBlendEquation);
    g_vtable[0x1e4 / 4] = reinterpret_cast<void*>(&SlotAlphaTest);
    g_vtable[0x1e8 / 4] = reinterpret_cast<void*>(&SlotAlphaFunction);
    g_vtable[0x1ec / 4] = reinterpret_cast<void*>(&SlotAlphaBlend);
    g_vtable[0x1f0 / 4] = reinterpret_cast<void*>(&SlotLighting);
    g_vtable[0x1f4 / 4] = reinterpret_cast<void*>(&SlotSeparateSpecular);
    g_vtable[0x1f8 / 4] = reinterpret_cast<void*>(&SlotLocalViewer);
    g_vtable[0x1fc / 4] = reinterpret_cast<void*>(&SlotFogMode);
    g_vtable[0x200 / 4] = reinterpret_cast<void*>(&SlotFogDensity);
    g_vtable[0x204 / 4] = reinterpret_cast<void*>(&SlotFogRange);
    g_vtable[0x208 / 4] = reinterpret_cast<void*>(&SlotFogColor);
    g_vtable[0x20c / 4] = reinterpret_cast<void*>(&SlotColorMaterial);
    g_vtable[0x210 / 4] = reinterpret_cast<void*>(&SlotVoid);
    g_vtable[0x214 / 4] = reinterpret_cast<void*>(&SlotTextureMode);
    g_vtable[0x218 / 4] = reinterpret_cast<void*>(&SlotProjectedTexture);
    g_vtable[0x21c / 4] = reinterpret_cast<void*>(&SlotMaterial);
    g_vtable[0x220 / 4] = reinterpret_cast<void*>(&SlotCreateVertexType);
    g_vtable[0x224 / 4] = reinterpret_cast<void*>(&SlotLightMode);
    g_vtable[0x228 / 4] = reinterpret_cast<void*>(&SlotLightingUpdate);
    g_vtable[0x22c / 4] = reinterpret_cast<void*>(&SlotLightState);
    g_vtable[0x230 / 4] = reinterpret_cast<void*>(&SlotGlobalAmbient);
    g_vtable[0x234 / 4] = reinterpret_cast<void*>(&SlotPolygonOffset);
    g_vtable[0x238 / 4] = reinterpret_cast<void*>(&SlotPolygonOffsetMode);
    g_vtable[0x23c / 4] = reinterpret_cast<void*>(&SlotInitializeFormats);
    g_vtable[0x240 / 4] = reinterpret_cast<void*>(&SlotTexCoordGen);
    g_vtable[0x244 / 4] = reinterpret_cast<void*>(&SlotPreOpen);
    g_vtable[0x248 / 4] = reinterpret_cast<void*>(&SlotSetupVideoFormat);
    g_vtable[0x24c / 4] = reinterpret_cast<void*>(&SlotResetVideoMode);
    g_vtable[0x250 / 4] = reinterpret_cast<void*>(&SlotResetVideoFormat);
    g_vtable[0x254 / 4] = reinterpret_cast<void*>(&SlotInitializeFormats);
    g_vtable[0x258 / 4] = reinterpret_cast<void*>(&SlotSelectVideoMode);
    g_vtable[0x25c / 4] = reinterpret_cast<void*>(&SlotFinishVideoMode);
    g_vtable[0x260 / 4] = reinterpret_cast<void*>(&SlotVoid);
    g_vtable[0x264 / 4] = reinterpret_cast<void*>(&SlotTrue);
    g_vtable[0x268 / 4] = reinterpret_cast<void*>(&SlotVoid);
    g_vtable[0x26c / 4] = reinterpret_cast<void*>(&SlotVoid);
    g_vtable[0x270 / 4] = reinterpret_cast<void*>(&SlotTrue);
    g_vtable[0x278 / 4] = reinterpret_cast<void*>(&SlotZero);
    g_vtable[0x27c / 4] = reinterpret_cast<void*>(&SlotZero);
    g_vtable[0x280 / 4] = reinterpret_cast<void*>(&SlotZero);

    g_pluginVtableXTerm[0] = reinterpret_cast<void*>(&PluginDeleteThisXTerm);
    g_pluginVtableXTerm[1] = reinterpret_cast<void*>(&PluginDeletingDtorXTerm);
    g_pluginVtableBase[0] = reinterpret_cast<void*>(&PluginDeleteThisBase);
    g_pluginVtableBase[1] = reinterpret_cast<void*>(&PluginDeletingDtorBase);
    if (auto* pluginBase = reinterpret_cast<void**>(
            GetProcAddress(RsModule(), "??_7Display@Jet@@6BPluginInstance@1@@"))) {
        g_pluginVtableXTerm[-1] = pluginBase[-1];
        g_pluginVtableBase[-1] = pluginBase[-1];
    }
}

}  // namespace

namespace Jet {

void* __cdecl CreateDisplayObject(bool xterm)
{
    Trace("CreateDisplayObject");
    BuildTables();
    std::memset(&g_state, 0, sizeof g_state);
    g_state.maxTextureUnits = 1;
    Trace("  tables reset");

    const int size = xterm ? kDisplayObjectSize : 0xfa8;
    const int vbase = xterm ? 0xfa0 : 0xf9c;
    void** const pluginTable = xterm ? g_pluginVtableXTerm : g_pluginVtableBase;
    int* const vbtable = xterm ? g_vbtableXTerm : g_vbtableBase;

    auto* object = static_cast<unsigned char*>(Alloc::New(size));
    if (!object) return nullptr;
    std::memset(object, 0, size);
    TracePtr("  object", object);
    Trace("  object allocated");

    *reinterpret_cast<int**>(object + 4) = vbtable;

    // Display::Display(bool createContext = false, bool mostDerived = false)
    using Ctor = void*(__thiscall*)(void*, bool, int);
    if (auto ctor = Sym<Ctor>(RsModule(), "??0Display@Jet@@IAE@_N@Z")) ctor(object, false, 0);
    Trace("  display ctor done");

    *reinterpret_cast<void**>(object) = g_vtable;
    *reinterpret_cast<int**>(object + 4) = vbtable;
    Field<int>(object, vbase) = 0;
    Field<void*>(object, vbase + 4) = pluginTable;
    Field<unsigned>(object, vbase + 8) = 1;

    auto* context = static_cast<unsigned char*>(Alloc::New(kContextSize));
    if (context) {
        std::memset(context, 0, kContextSize);
        using CtxCtor = void*(__thiscall*)(void*);
        if (auto make = Sym<CtxCtor>(RsModule(), "??0DisplayContext@Display@Jet@@QAE@XZ")) make(context);
        Field<void*>(object, 0x54) = context;
        Field<void*>(object, 0x58) = context;
        Field<void*>(object, 0xf68) = context;
    }
    Trace("  context done");

    auto* descriptor = ::new (static_cast<void*>(object + kDescriptorOffset)) DisplayDescriptorOpenGL();
    auto* contextDescriptor = new ContextDescriptorOpenGLWGL();
    Field<ContextDescriptorOpenGL*>(descriptor, 0x1c) = contextDescriptor;
    static_cast<DescriptorAccess*>(descriptor)->SetFallback();
    Trace("  descriptor done");

    Field<float>(object, 0xf20) = 1.0f;
    Field<float>(object, 0xf24) = 1.0f;
    Field<int>(object, 0x4c) = 0;
    Field<int>(object, 0x50) = 8;
    Field<int>(object, 0x214) = 8;
    Field<int>(object, 0x218) = 4;
    Field<unsigned char>(object, 0x294) = 1;
    Field<unsigned char>(object, 0x4a) = 1;
    Field<unsigned char>(object, 0x7a) = 1;
    Field<unsigned char>(object, 0x7b) = 1;
    Field<unsigned char>(object, 0x213) = 1;
    Field<unsigned char>(object, 0xf1c) = 1;  // two-sided stencil
    Field<unsigned char>(object, 0xf1d) = 0;
    Field<unsigned char>(object, 0xf1e) = 1;  // separate specular
    if (void* probe = Alloc::New(16)) {
        Trace("  alloc probe ok");
        Alloc::Delete(probe);
    }
    Trace("CreateDisplayObject done");
    return object;
}

void __cdecl DestroyDisplayObject(void* display)
{
    TracePtr("DestroyDisplayObject", display);
    if (!display || g_state.destroyed) return;
    g_state.destroyed = true;
    auto* object = static_cast<unsigned char*>(display);

    if (!g_state.shutdown) {
        g_state.shutdown = true;
        if (g_state.initialized) {
            if (auto shutdown = BaseShutdown()) shutdown(display);
        }
    }
    Trace("  base shutdown done");
    if (auto* iface = Interface(display)) {
        iface->Destroy();
        delete iface;
        Field<void*>(display, 0xf00) = nullptr;
    }
    auto* descriptor = reinterpret_cast<DisplayDescriptorOpenGL*>(object + kDescriptorOffset);
    auto* contextDescriptor = Field<ContextDescriptorOpenGL*>(descriptor, 0x1c);
    Field<ContextDescriptorOpenGL*>(descriptor, 0x1c) = nullptr;
    delete contextDescriptor;
    Trace("  context descriptor deleted");
    descriptor->~DisplayDescriptorOpenGL();
    Trace("  descriptor destroyed");

    if (void* context = Field<void*>(object, 0x58)) {
        using CtxDtor = void(__thiscall*)(void*);
        if (auto fn = Sym<CtxDtor>(RsModule(), "??1DisplayContext@Display@Jet@@UAE@XZ")) fn(context);
        Alloc::Delete(context);
        Field<void*>(object, 0x54) = nullptr;
        Field<void*>(object, 0x58) = nullptr;
        Field<void*>(object, 0xf68) = nullptr;
    }
    Trace("  context destroyed");
    Alloc::Delete(object);
    Trace("DestroyDisplayObject done");
}

extern "C" void __cdecl JetGetPluginsRenderOpenGL()
{
    Trace("JetGetPlugins");
    UId const parent("6174F22A-5087-4ec6-B82F-1C98A354777A");
    UId const baseId("0C74D616-2160-4eda-8F08-4B9EE42A7C78");
    UId const xtermId("A17C47F6-56FA-42c0-AC20-284C75D848DB");
    PString const baseName("StdOpenGL");
    PString const xtermName("XTermOpenGL");
    new DisplayFactory(baseName, baseId, parent, false);
    new DisplayFactory(xtermName, xtermId, parent, true);
}

}  // namespace Jet
