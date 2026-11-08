// Unit U6c1 of RenderSystem_7c: DisplayTexture (0x10004730..0x10004a9b, 0x1001f420..0x1001f6a5, folded getters)
// and DisplayTextureNull (0x1001dfd0..0x1001e129, 0x1001f6b0..0x10020417, 0x10008e40). Behaviour follows the TS12
// assembly, quirks included: Create refuses only volume textures (dimension 2); Load and SubLoad hand CheckFormat
// one int for two (three) of its outputs, and Load carries on with the level Convert may have written there;
// CreateInstance frees the source's kept bitmap after copying it (once per face for a cube map) and leaves the
// pointer. The null display's format table (0x100b8228) and the statics stay the original's (U9).
#include <jet/Texture.h>
#include <jet/VCall.h>

#include <windows.h>

#include <cstring>
#include <new>

namespace Jet {

bool __cdecl Convert(const DisplayFormat& from, const DisplayFormat& to, void*& data, int& pitch, int& a, int& b,
                     int width, int height);  // the original's (0x1000ed60, U9)

namespace {

// the floats the original copies through the x87 stack (a signalling NaN comes out quiet)
void X87Copy(float* to, const float* from)
{
#if defined(_M_IX86)
    __asm {
        mov eax, from
        mov edx, to
        fld dword ptr [eax]
        fstp dword ptr [edx]
    }
#else
    *to = *from;
#endif
}

// bits 0..2 of +0x2c are copied, the rest of the destination's byte stays
void CopyFlags(unsigned char& to, unsigned char from) { to = static_cast<unsigned char>((to & ~7) | (from & 7)); }

// the null display's format table (0x100b8228, 36 bytes per DFormatId, filled by DisplayNull)
struct NullFormat {
    unsigned int flags;  // bit 0 supported as is, bit 1 compressed
    int unused[3];
    int blockBits;       // +0x10 bits per 4x4 block (compressed)
    NullFormat* link;    // +0x14 the format it is converted to
    DisplayFormat format;  // +0x18
};
static_assert(sizeof(NullFormat) == 0x24 || sizeof(void*) != 4, "NULLFormat");

NullFormat* Entry(unsigned char id)
{
    static unsigned char* const base = reinterpret_cast<unsigned char*>(GetModuleHandleA("RenderSystem_7c_orig.dll"));
    return reinterpret_cast<NullFormat*>(base + 0xb8228) + id;
}

int Mul(int a, int b) { return static_cast<int>(static_cast<unsigned int>(a) * static_cast<unsigned int>(b)); }

// the display's slots used here
bool Query(Display* d, int what) { return VCall<bool>(d, 0x1c, what); }
float MaxAnisotropy(Display* d) { return VCall<float>(d, 0x28); }
DisplayTexture* CreateTexture(Display* d) { return VCall<DisplayTexture*>(d, 0x98); }
void SetTexture(Display* d, unsigned int unit) { VCall<void>(d, 0xa0, unit); }

void DeleteBitmap(Bitmap* b)
{
    b->~Bitmap();
    Alloc::Delete(b);
}

RenderTextureConstruct* CopyConstruct(const RenderTextureConstruct& c)
{
    void* const mem = Alloc::New(sizeof(RenderTextureConstruct));
    return mem ? ::new (mem) RenderTextureConstruct(c) : nullptr;
}

// a level's size, at least 1 on a side that is not 0 already (0x10004990, inlined in the null texture)
Size LevelSize(const Size& s, unsigned int level)
{
    Size r = {s.w >> level, s.h >> level};
    if (r.w > 0 && r.h == 0) r.h = 1;
    if (r.w == 0 && r.h > 0) r.w = 1;
    return r;
}

}  // namespace

// ---------------------------------------------------------------------------------------------------------------
// DisplayTexture

// 0x100048c0
DisplayTexture::DisplayTexture(Display* d)
{
    state = 0;
    memorySize = 0;
    display = d;
    size.w = size.h = 0;
    flags &= 0xf8;
    dimension = static_cast<TextureDimension>(0);
    depthLevels = 0;
    mipLevels = 0;
    requestedAnisotropy = 1.0f;
    anisotropy = 1.0f;
    autoMip = 0;
    construct = nullptr;
}

// 0x10004730 (the render target is shared, not copied)
DisplayTexture::DisplayTexture(const DisplayTexture& o)
{
    state = o.state;
    memorySize = o.memorySize;
    display = o.display;
    size = o.size;
    dimension = o.dimension;
    depthLevels = o.depthLevels;
    mipLevels = o.mipLevels;
    X87Copy(&requestedAnisotropy, &o.requestedAnisotropy);
    X87Copy(&anisotropy, &o.anisotropy);
    CopyFlags(flags, o.flags);
    autoMip = o.autoMip;
    construct = o.construct;
}

// 0x100047c0
DisplayTexture& DisplayTexture::operator=(const DisplayTexture& o)
{
    state = o.state;
    memorySize = o.memorySize;
    display = o.display;
    size = o.size;
    dimension = o.dimension;
    depthLevels = o.depthLevels;
    mipLevels = o.mipLevels;
    X87Copy(&requestedAnisotropy, &o.requestedAnisotropy);
    X87Copy(&anisotropy, &o.anisotropy);
    CopyFlags(flags, o.flags);
    autoMip = o.autoMip;
    construct = o.construct;
    return *this;
}

// 0x10004910
DisplayTexture::~DisplayTexture()
{
    if (RenderTextureConstruct* const c = construct) {
        c->~RenderTextureConstruct();
        Alloc::Delete(c);
    }
    construct = nullptr;
}

bool __cdecl DisplayTexture::SubloadFromFrame(unsigned int, const Point&, const Rect&, unsigned int) { return true; }  // 0x100015d0
bool __cdecl DisplayTexture::CreateRenderContext() { return true; }        // 0x100015d0
bool __cdecl DisplayTexture::GetDataLost() const { return (flags >> 2) & 1; }  // 0x100203c0
void __cdecl DisplayTexture::RefreshFromChangedSettings() {}               // 0x10021870
void* __cdecl DisplayTexture::GetAPITexture() const { return nullptr; }    // 0x1001dec0
void __cdecl DisplayTexture::LogInitFailure(const char*) const {}          // 0x10021870
void __cdecl DisplayTexture::EndFrameStats() {}                            // 0x10021870

Display* __cdecl DisplayTexture::GetDisplay() const { return display; }                       // 0x100106c0
const Size& __cdecl DisplayTexture::GetSize() const { return size; }                          // 0x1000c990
TextureDimension __cdecl DisplayTexture::GetDimension() const { return dimension; }           // 0x1000ae00
unsigned int __cdecl DisplayTexture::GetDepthLevels() const { return depthLevels; }           // 0x10010940
unsigned int __cdecl DisplayTexture::GetNumMipLevels() const { return mipLevels; }            // 0x10021b50
unsigned int __cdecl DisplayTexture::GetMemorySize() const { return memorySize; }             // 0x1000cab0
bool __cdecl DisplayTexture::GetCanMip() const { return (flags >> 1) & 1; }                  // 0x10004a50
bool __cdecl DisplayTexture::GetAutoMip() const { return autoMip != 0; }                      // 0x10004a60
TextureAutoMip __cdecl DisplayTexture::GetAutoMipHint() const { return static_cast<TextureAutoMip>(autoMip); }  // 0x10004a70
float __cdecl DisplayTexture::GetAnisotropy() const { return anisotropy; }                    // 0x10004a80
bool __cdecl DisplayTexture::CheckInitialized(const char*) const { return state == 1; }      // 0x10004a90
const DisplayTexture::DisplayTextureStats& __cdecl DisplayTexture::GetTextureStats() { return textureStats; }  // 0x1001f6a0

void __cdecl DisplayTexture::SetDataLost(bool lost)  // 0x10004a30
{
    flags = static_cast<unsigned char>((flags & ~4) | ((lost << 2) & 4));
}

void __cdecl DisplayTexture::SetActiveRenderLevel(unsigned int level)  // 0x100049d0
{
    if (construct) construct->SetActiveDepthLevel(static_cast<int>(level));
}
unsigned int __cdecl DisplayTexture::GetActiveRenderLevel() const  // 0x100049f0
{
    return construct ? static_cast<unsigned int>(construct->GetActiveDepthLevel()) : 0;
}
TextureTarget __cdecl DisplayTexture::GetRenderTargetType() const  // 0x10004a10
{
    return construct ? construct->GetRenderType() : static_cast<TextureTarget>(0);
}

// 0x10004950: the levels down to 1x1 of the larger side
int __cdecl DisplayTexture::GetNumMipLevels(const Size& s)
{
    int m = s.h;
    if (s.w >= m) m = s.w;
    if (m <= 1) return 1;
    int n = 0;
    for (int v = m / 2; v; v /= 2) ++n;
    return n + 1;
}

Size __cdecl DisplayTexture::GetMipLevelSize(const Size& s, unsigned int level) { return LevelSize(s, level); }

// 0x10004990 as TS12 exports it (its Size has constructors, so even this static returns through a pointer)
Size* __cdecl RsMipLevelSize(Size* out, const Size& s, unsigned int level)
{
    *out = LevelSize(s, level);
    return out;
}

// 0x1001f420: level 0 at `area`, then (for a texture with mips the API does not make) every smaller level from
// the bitmap scaled down, the area halved with it
bool __cdecl DisplayTexture::SubLoad(const Bitmap& bitmap, const Rect& area, DisplayFormat& format, unsigned int face)
{
    int x = area.x, y = area.y, w = area.w, h = area.h;
    if (!(flags & 2) || autoMip != 0) {
        const Rect r = {x, y, w, h};
        return SubLoad(0, r, format, bitmap, face);
    }
    const Size s = {w, h};
    const int levels = GetNumMipLevels(s);
    {
        const Rect r = {x, y, w, h};
        if (!SubLoad(0, r, format, bitmap, face)) return false;
    }
    for (int level = 1; level < levels; ++level) {
        x >>= 1;
        y >>= 1;
        if (w > 1) w >>= 1;
        if (h > 1) h >>= 1;
        Bitmap* const scaled = bitmap.Scale(w, h);
        const Rect r = {x, y, w, h};
        const bool ok = SubLoad(static_cast<unsigned int>(level), r, format, *scaled, face);
        if (scaled) DeleteBitmap(scaled);
        if (!ok) return false;
    }
    return true;
}

// 0x1001f5b0: the settings into `to`; a render target's construct is copied (to's own is not freed)
void __cdecl DisplayTexture::InitInstance(DisplayTexture& to) const
{
    to.memorySize = memorySize;
    to.dimension = dimension;
    to.depthLevels = depthLevels;
    to.size = size;
    to.mipLevels = mipLevels;
    to.display = display;
    X87Copy(&to.requestedAnisotropy, &requestedAnisotropy);
    X87Copy(&to.anisotropy, &anisotropy);
    CopyFlags(to.flags, flags);
    to.autoMip = autoMip;
    if (construct) to.construct = CopyConstruct(*construct);
}

// ---------------------------------------------------------------------------------------------------------------
// DisplayTextureNull

// 0x1001f6b0: the first one asks the display about automatic mips and makes the 1000 texture units
DisplayTextureNull::DisplayTextureNull(Display* d) : DisplayTexture(d)
{
    unused38 = -1;
    firstLevel = ~0u;
    renderContext = 0;
    minFilter = static_cast<TextureFilter>(0);
    magFilter = static_cast<TextureFilter>(0);
    unused58 = 0;
    dirty = false;
    bound61 = false;
    bindCount = 0;
    flag68 = true;
    if (numObjects++ == 0) {
        canAutoMip = Query(d, 5);
        bound = static_cast<const DisplayTextureNull**>(Alloc::New(1000 * sizeof(void*)));
        for (int i = 0; i < 1000; ++i) bound[i] = nullptr;
    }
    image = nullptr;
}

// 0x1001dfd0 (the kept bitmap is shared)
DisplayTextureNull::DisplayTextureNull(const DisplayTextureNull& o) : DisplayTexture(o)
{
    std::memcpy(&unused38, &o.unused38, 0x60 - 0x38);
    dirty = o.dirty;
    bound61 = o.bound61;
    bindCount = o.bindCount;
    flag68 = o.flag68;
    image = o.image;
}

// 0x1001e050
DisplayTextureNull& DisplayTextureNull::operator=(const DisplayTextureNull& o)
{
    DisplayTexture::operator=(o);
    std::memcpy(&unused38, &o.unused38, 0x60 - 0x38);
    dirty = o.dirty;
    bound61 = o.bound61;
    bindCount = o.bindCount;
    flag68 = o.flag68;
    image = o.image;
    return *this;
}

// 0x1001f7c0
DisplayTextureNull::~DisplayTextureNull()
{
    if (image) DeleteBitmap(image);
    image = nullptr;
    if (bindCount) {
        for (unsigned int i = 0; i < 1000; ++i) {
            if (bound[i] != this) continue;
            bound[i] = nullptr;
            if (--bindCount == 0) break;
        }
    }
    if (mipLevels) {
        mipLevels = 0;
        firstLevel = ~0u;
        unused38 = -1;
    }
    if (--numObjects == 0) {
        if (bound) Alloc::Delete(bound);
        bound = nullptr;
    }
}

// 0x1001f8c0: the settings only (no API object); refuses a volume texture, and a render target that is not 2D or
// a cube map
bool __cdecl DisplayTextureNull::Create(const Size& s, const DisplayFormat& format, TextureDimension dim,
                                        unsigned int mips, TextureAutoMip autoMipHint, unsigned int depth,
                                        float aniso, TextureHint hint, TextureFilter minF, TextureFilter magF,
                                        TextureFilter, TextureWrap wS, TextureWrap wT,
                                        const RenderTextureConstruct& c)
{
    if (dim != 1 && dim == 2) return false;
    if (minF == 2) minF = static_cast<TextureFilter>(1);
    if (magF == 2) magF = static_cast<TextureFilter>(1);
    dimension = dim;
    depthLevels = depth;
    requestedAnisotropy = aniso;
    if (aniso == TextureResource::ANISOTROPY_DEFAULT_TOKEN_VALUE)
        requestedAnisotropy = TextureResource::GetDefaultAnisotropyFilterLevel();
    X87Copy(&anisotropy, &requestedAnisotropy);
    if (anisotropy > MaxAnisotropy(display)) anisotropy = MaxAnisotropy(display);
    if (1.0f > anisotropy) anisotropy = 1.0f;
    size = s;
    dirty = true;
    formatId = format.id;
    format.Classify();
    if (format.depthBits > 0) flag68 = false;
    NullFormat* e = Entry(format.id);
    if (!(e->flags & 1)) e = e->link;
    if (e->flags & 2) {
        memorySize = static_cast<unsigned int>(Mul(Mul(e->blockBits, s.w), s.h) / 16);
        flags |= 1;
    } else {
        memorySize = static_cast<unsigned int>(Mul(Mul((format.bits + 7) / 8, s.w), s.h));
    }
    if (hint != 1 && mips > 1) {
        flags |= 2;
        minFilter = minF;
        mipLevels = mips;
    } else {
        flags &= ~2;
        minFilter = minF;
        mipLevels = 1;
    }
    magFilter = magF;
    wrapS = wS;
    wrapT = wT;
    autoMip = (flags & 2) && canAutoMip ? static_cast<char>(autoMipHint) : 0;
    if (c.GetRenderType() != 0) {
        if (dim != 0 && dim != 1) return false;
        construct = CopyConstruct(c);
        firstLevel = 0;
    }
    return true;
}

// 0x1001fae0
void __cdecl DisplayTextureNull::Destroy()
{
    if (!mipLevels) return;
    mipLevels = 0;
    firstLevel = ~0u;
    unused38 = -1;
}

bool __cdecl DisplayTextureNull::CreateRenderContext() { return renderContext != 0; }  // 0x1001fb00

// 0x10020030: the format as the table has it, else the bitmap's pixels converted to the format it links to
NULLFormat* __cdecl DisplayTextureNull::CheckFormat(DisplayFormat& format, void*& data, int& pitch, int& a, int& b,
                                                    int width, int height)
{
    format.Classify();
    NullFormat* const e = Entry(format.id);
    if (e->flags & 1) return reinterpret_cast<NULLFormat*>(e);
    if (!data) return nullptr;
    if (!e->link) return nullptr;
    if (!Convert(format, e->format, data, pitch, a, b, width, height)) return nullptr;
    return reinterpret_cast<NULLFormat*>(e->link);
}

// 0x1001fb10: keeps a copy of the first bitmap loaded; the level CheckFormat may have overwritten carries on
bool __cdecl DisplayTextureNull::Load(unsigned int level, DisplayFormat& format, const Bitmap& bitmap,
                                      unsigned int face)
{
    (void)face;
    if (level >= mipLevels) return true;
    const Bitmap* volatile given = &bitmap;  // (TS12 tests the reference for null)
    const Bitmap* const bmp = given;
    void* data;
    NullFormat* e;
    unsigned int lv = level;
    if (bmp) {
        data = bmp->GetPtr();
        int pitch = static_cast<int>(bmp->GetPitch() / (static_cast<unsigned int>(format.bits) >> 3));
        if (!pitch) pitch = bmp->width;
        int& l = reinterpret_cast<int&>(lv);
        e = reinterpret_cast<NullFormat*>(CheckFormat(format, data, pitch, l, l, bmp->width, bmp->height));
    } else {
        data = nullptr;
        int zero = 0;
        e = reinterpret_cast<NullFormat*>(CheckFormat(format, data, zero, zero, zero, 0, 0));
    }
    if (!e || dimension == 2) return false;
    if (dirty) dirty = false;
    if (e->flags & 2) {
        if (dimension == 2) return false;
    } else if (data && dimension == 2) {
        return false;
    }
    if (lv < firstLevel) firstLevel = lv;
    if (bmp && bmp->GetPtr() && !image) {
        void* const mem = Alloc::New(0x44);
        const Rect all = {0, 0, bmp->width, bmp->height};
        image = mem ? ::new (mem) Bitmap(*bmp, all) : nullptr;
    }
    flags &= ~4;
    return true;
}

// 0x1001fca0
bool __cdecl DisplayTextureNull::SubLoad(unsigned int level, const Rect& area, DisplayFormat& format,
                                         const Bitmap& bitmap, unsigned int)
{
    if (level >= mipLevels) return true;
    const Size l = LevelSize(size, level);
    if (l.w < area.x + area.w) return false;
    if (l.h < area.y + area.h) return false;
    void* data = bitmap.GetPtr();
    int pitch = static_cast<int>(bitmap.GetPitch() / (static_cast<unsigned int>(format.bits) >> 3));
    if (!pitch) pitch = bitmap.width;
    NullFormat* const e = reinterpret_cast<NullFormat*>(CheckFormat(format, data, pitch, pitch, pitch, area.w, area.h));
    if (!e || (e->flags & 2) || dimension == 2) return false;
    flags &= ~4;
    return true;
}

// 0x1001fd80
bool __cdecl DisplayTextureNull::SubloadFromFrame(unsigned int level, const Point&, const Rect& area, unsigned int)
{
    const Size l = LevelSize(size, level);
    if (l.w < area.x + area.w) return false;
    if (l.h < area.y + area.h) return false;
    if (dimension == 2) return false;
    if (static_cast<int>(level) < 0 && autoMip) level = 0;
    if (level < firstLevel) firstLevel = level;
    flags &= ~4;
    return true;
}

// 0x1001fe00
void __cdecl DisplayTextureNull::UnbindRenderTexture(unsigned int unit) const
{
    const DisplayTextureNull* const b = bound[unit];
    if (b && b->renderContext && b != this && b->bound61) b->bound61 = false;
}

// 0x1001fe30
void __cdecl DisplayTextureNull::BindTexture(unsigned int unit) const
{
    const DisplayTextureNull** const slot = &bound[unit];
    if (*slot) --(*slot)->bindCount;
    *slot = this;
    ++bindCount;
}

// 0x1001fe60: a texture with nothing loaded unbinds the unit through the display
bool __cdecl DisplayTextureNull::Bind(unsigned int unit) const
{
    if (firstLevel == ~0u) {
        SetTexture(display, unit);
        return true;
    }
    UnbindRenderTexture(unit);
    BindTexture(unit);
    if (dirty) dirty = false;
    return true;
}

// 0x1001fed0: binds with these sampler settings (kept, or put back afterwards); "default" filters become linear
// (the minifying one only for a texture with mips), a negative anisotropy keeps the current one
bool __cdecl DisplayTextureNull::Bind(unsigned int unit, bool keep, float aniso, TextureWrap wS, TextureWrap wT,
                                      TextureFilter minF, TextureFilter magF, TextureFilter)
{
    TextureFilter newMin = minFilter;
    TextureFilter newMag = magFilter;
    if (flags & 2) {
        newMin = minF == 2 ? static_cast<TextureFilter>(1) : minF;
    } else if (minF != 2) {
        newMin = minF;
    }
    if (magF != 2) newMag = magF;
    if (aniso == TextureResource::ANISOTROPY_DEFAULT_TOKEN_VALUE)
        aniso = TextureResource::GetDefaultAnisotropyFilterLevel();
    float a;
    if (0.0f > aniso) {
        a = anisotropy;
    } else {
        if (aniso > MaxAnisotropy(display)) aniso = MaxAnisotropy(display);
        a = aniso;
    }
    dirty = true;
    if (keep) {
        minFilter = newMin;
        wrapS = wS;
        magFilter = newMag;
        wrapT = wT;
        anisotropy = a;
        return Bind(unit);
    }
    const TextureWrap oldT = wrapT, oldS = wrapS;
    const TextureFilter oldMag = magFilter, oldMin = minFilter;
    const float oldA = anisotropy;
    minFilter = newMin;
    wrapS = wS;
    magFilter = newMag;
    wrapT = wT;
    anisotropy = a;
    const bool r = Bind(unit);
    minFilter = oldMin;
    magFilter = oldMag;
    wrapS = oldS;
    dirty = true;
    anisotropy = oldA;
    wrapT = oldT;
    return r;
}

void __cdecl DisplayTextureNull::CopyMipToMip(DisplayTexture*, int, DisplayTexture*, int) {}  // 0x10021870

// 0x100200a0: a new API texture with the same settings and the kept level 0 loaded into it (each level, and each
// face of a cube map, gets the same bitmap and frees it after: TS12 leaves the source pointing at it)
DisplayTexture* __cdecl DisplayTextureNull::CreateInstance() const
{
    DisplayTexture* const t = CreateTexture(display);
    DisplayFormat f;
    f.Set(static_cast<DFormatId>(formatId));
    float a;
    X87Copy(&a, &anisotropy);  // (passed through the x87 stack)
    bool failed;
    {
        const RenderTextureConstruct c = construct ? RenderTextureConstruct(*construct) : RenderTextureConstruct();
        failed = !t->Create(size, f, dimension, mipLevels, static_cast<TextureAutoMip>(autoMip), depthLevels,
                            a, static_cast<TextureHint>(0), static_cast<TextureFilter>(0),
                            static_cast<TextureFilter>(0), static_cast<TextureFilter>(0),
                            static_cast<TextureWrap>(2), static_cast<TextureWrap>(2), c);
    }
    if (failed) {
        VDelete(t);
        return nullptr;
    }
    DisplayTextureNull* const n = static_cast<DisplayTextureNull*>(t);
    n->dirty = true;
    X87Copy(&n->requestedAnisotropy, &requestedAnisotropy);
    X87Copy(&n->anisotropy, &anisotropy);
    n->minFilter = minFilter;
    n->magFilter = magFilter;
    n->wrapS = wrapS;
    n->wrapT = wrapT;
    if (construct && construct->GetRenderType() != 0) return t;
    const unsigned int levels = autoMip ? 1 : mipLevels;
    for (unsigned int level = 0; level < levels; ++level) {
        // (the original's format here has only its first two bytes set: ours is all zero)
        DisplayFormat zero;
        std::memset(&zero, 0, sizeof zero);
        switch (dimension) {
        case 0:
        case 3:
            if (Bitmap* const b = image) {
                t->Load(level, zero, *b, 0);
                DeleteBitmap(b);
            }
            break;
        case 1:
            for (unsigned int face = 0; face < 6; ++face) {
                if (Bitmap* const b = image) {
                    t->Load(level, zero, *b, face);
                    DeleteBitmap(b);
                }
            }
            break;
        default:
            break;
        }
    }
    return t;
}

// 0x10020370: the kept bitmap for a level and face that exist (2D, cube faces 0..5)
Bitmap* __thiscall DisplayTextureNull::GetBitmap(unsigned int level, unsigned int face, DisplayFormat&, DFormatId) const
{
    if (firstLevel == ~0u) return nullptr;
    if (level >= mipLevels) return nullptr;
    if (depthLevels && face >= depthLevels) return nullptr;
    if (construct && construct->GetRenderType() != 0) return nullptr;
    switch (dimension) {
    case 0:
    case 3:
        return image;
    case 1:
        return face < 6 ? image : nullptr;
    default:
        return nullptr;
    }
}

bool __cdecl DisplayTextureNull::GetDataLost() const { return (flags >> 2) & 1; }  // 0x100203c0
DFormatId __cdecl DisplayTextureNull::GetJetFormat() const { return static_cast<DFormatId>(0); }  // 0x1001dec0

// 0x100203d0
void __cdecl DisplayTextureNull::CopySamplerAttributesFrom(DisplayTexture* from)
{
    const DisplayTextureNull* const o = dynamic_cast<const DisplayTextureNull*>(from);
    if (!o) return;
    X87Copy(&requestedAnisotropy, &o->requestedAnisotropy);
    X87Copy(&anisotropy, &o->anisotropy);
    minFilter = o->minFilter;
    magFilter = o->magFilter;
    wrapS = o->wrapS;
    wrapT = o->wrapT;
}

PBufferContext* __cdecl DisplayTextureNull::GetRenderContext() const  // 0x100219b0
{
    return reinterpret_cast<PBufferContext*>(static_cast<intptr_t>(renderContext));
}
DisplayNull* __cdecl DisplayTextureNull::GetDisplay() const { return reinterpret_cast<DisplayNull*>(display); }  // 0x100106c0
OPENGL_INTERFACE_CLASS* __cdecl DisplayTextureNull::GetWinInterface() const { return nullptr; }  // 0x1001dec0
Bitmap* __cdecl DisplayTextureNull::GetTextureBitmap(int, unsigned int, DisplayFormat&, DFormatId) const  // 0x10008e40
{
    return image;
}
void __cdecl DisplayTextureNull::InitInstance(DisplayTextureNull&) const {}  // 0x10021870
void __cdecl DisplayTextureNull::SetBasicState() const {}                    // 0x10021870

}  // namespace Jet
