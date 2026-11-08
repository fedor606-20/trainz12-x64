// Unit U6c2 of RenderSystem_7c: Texture (0x10004aa0..0x10004ead, 0x1003f4f0..0x1003f9b8, folded getters). Behaviour
// follows the TS12 assembly: SetDisplayTexture frees the old API texture even when it is the new one; an instance
// is named "%08x%%name" from the original's uniqueTexId; the update flag goes through the original's
// TextureManager::SetTextureUpdate (U6d).
#include <jet/Texture.h>
#include <jet/VCall.h>

#include <cstdio>
#include <new>

namespace Jet {

namespace {

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

// the manager's slots used here
void Add(TextureManager* m, Texture* t) { VCall<void>(m, 0x10, t); }
void Remove(TextureManager* m, Texture* t) { VCall<void>(m, 0x18, t); }
const ReferenceCount* Ref(const TextureManager* m) { return reinterpret_cast<const ReferenceCount*>(m); }

// the API texture's (DisplayTexture slots 0 deleting destructor, 8 Bind, 10 CreateInstance)
void DeleteDisplayTexture(DisplayTexture* t) { VDelete(t); }

// an instance's name: "%08x%%" before the given one
bool HasUniquePrefix(const PString& n)
{
    return n.node && PString::Cache()->Find(n.node, reinterpret_cast<const unsigned char*>("%%"), 8) == 8;
}

}  // namespace

// 0x1003f550
Texture::Texture(const PString& n, TextureManager* m)
{
    unused08 = 0;
    flags &= 0xfe;
    ::new (&name) PString(n);
    activeFrame = 0;
    texture = nullptr;
    alpha = static_cast<TextureAlpha>(-1);
    averageColor.r = averageColor.g = averageColor.b = averageColor.a = 0.0f;
    manager = m;
    Ref(m)->AddReference();
    Add(m, this);
}

// 0x1003f630
Texture::Texture(const PString& n, TextureManager* m, DisplayTexture* t, TextureAlpha a)
{
    unused08 = 0;
    flags &= 0xfe;
    ::new (&name) PString(n);
    activeFrame = 0;
    alpha = a;
    texture = t;
    averageColor.r = averageColor.g = averageColor.b = averageColor.a = 0.0f;
    manager = m;
    Ref(m)->AddReference();
    Add(m, this);
}

// 0x1003f710
Texture::~Texture()
{
    if (manager) {
        Remove(manager, this);
        Ref(manager)->ReleaseReference();
        manager = nullptr;
    }
    if (texture) DeleteDisplayTexture(texture);
    texture = nullptr;
}

// 0x10004ab0: an instance with a name of its own and a copy of the API texture
Texture* __cdecl Texture::CreateInstance() const
{
    const PString unique = GenerateUniqueName(name);
    void* const mem = Alloc::New(sizeof(Texture));
    Texture* const t = mem ? ::new (mem) Texture(unique, manager) : nullptr;
    InitInstance(*t);
    return t;
}

bool __cdecl Texture::IsAvailable() const { return texture != nullptr; }                       // 0x10004ea0
bool __cdecl Texture::IsLoadFailure() const { return false; }                                  // 0x10009500
const PString& __cdecl Texture::GetResourceName() const { return name; }                       // 0x1000c990
const TextureResource* __cdecl Texture::GetResource() const { return nullptr; }                // 0x1001dec0
unsigned long __cdecl Texture::GetMemorySize() const { return texture ? texture->GetMemorySize() : 0; }  // 0x10004d90
TextureAlpha __cdecl Texture::GetAlphaHint() const { return alpha; }                            // 0x10010940
void __cdecl Texture::Update(float) {}                                                         // 0x10021870
void __cdecl Texture::SetResourcePriority(float) const {}                                      // 0x10021870
DisplayTexture* __cdecl Texture::GetDisplayTexture() const { return texture; }                 // 0x1000ae00
TextureManagerInfo* __cdecl Texture::GetTextureManagerInfo() { return nullptr; }               // 0x1001dec0
const PString& __cdecl Texture::GetName() const { return name; }                               // 0x1000c990
unsigned int __cdecl Texture::GetActiveFrame() const { return activeFrame; }                   // 0x10004aa0
void __cdecl Texture::SetActiveFrame(unsigned int frame) { activeFrame = frame; }              // 0x10004d80
bool __cdecl Texture::GetRequiresUpdate() const { return flags & 1; }                          // 0x10004d70
void Texture::SetAlphaHint(TextureAlpha a) { alpha = a; }                                      // 0x10004dd0
TextureManager* __cdecl Texture::GetTextureManager() const { return manager; }                 // 0x1000fd90

// 0x10004db0 (through the x87 stack)
Color __cdecl Texture::GetAverageColor() const
{
    Color c;
    X87Copy(&c.r, &averageColor.r);
    X87Copy(&c.g, &averageColor.g);
    X87Copy(&c.b, &averageColor.b);
    X87Copy(&c.a, &averageColor.a);
    return c;
}

TextureDimension __cdecl Texture::GetDimension() const  // 0x10004de0
{
    return texture ? texture->GetDimension() : static_cast<TextureDimension>(3);
}
unsigned int __cdecl Texture::GetDepth() const { return texture ? texture->GetDepthLevels() : 0; }  // 0x10004e00
Size __cdecl Texture::GetSize() const  // 0x10004e20
{
    if (texture) return texture->GetSize();
    const Size none = {0, 0};
    return none;
}
Size __cdecl Texture::GetAbsoluteSize() const { return GetSize(); }  // 0x10004e50 (through the slot)

// 0x1003f7d0
void __cdecl Texture::Activate(int how) { VCall<void>(manager, 0x24, this, how); }

// 0x1003f9a0
void __cdecl Texture::Bind(unsigned int unit) const
{
    if (texture) texture->Bind(unit);
}

// 0x1003f8b0: the bitmap at (x, y) of level 0 (and the smaller levels, see DisplayTexture::SubLoad)
bool __cdecl Texture::DynamicSubload(int x, int y, const Bitmap& bitmap, DisplayFormat& format)
{
    if (!texture) return false;
    const Rect area = {x, y, bitmap.width, bitmap.height};
    return texture->SubLoad(bitmap, area, format, 0);
}

// 0x10004e70 (the old one is freed even when it is the new one)
void __cdecl Texture::SetDisplayTexture(DisplayTexture* t)
{
    if (texture) DeleteDisplayTexture(texture);
    texture = t;
}

// 0x1003f920: renamed with the manager (out under the old name, in under the new)
void __cdecl Texture::SetName(PString& n)
{
    Remove(manager, this);
    name = n;
    Add(manager, this);
}

// 0x1003f900: used in the manager's current frame
bool __cdecl Texture::GetActive() const
{
    return *reinterpret_cast<const unsigned int*>(reinterpret_cast<const char*>(manager) + 0x28) == activeFrame;
}

// 0x1003f7f0
void __cdecl Texture::SetRequiresUpdate(bool on)
{
    if ((flags & 1) == static_cast<unsigned char>(on)) return;
    flags = static_cast<unsigned char>((flags & ~1) | (on & 1));
    manager->SetTextureUpdate(this, on);
}

// 0x1003f830: the API texture copied (when `to` has none), the alpha hint, the colour and the update flag
void __cdecl Texture::InitInstance(Texture& to) const
{
    if (texture && !to.texture) to.texture = texture->CreateInstance();
    to.alpha = alpha;
    X87Copy(&to.averageColor.r, &averageColor.r);
    X87Copy(&to.averageColor.g, &averageColor.g);
    X87Copy(&to.averageColor.b, &averageColor.b);
    X87Copy(&to.averageColor.a, &averageColor.a);
    const unsigned char on = flags & 1;
    if ((to.flags & 1) == on) return;
    to.flags = static_cast<unsigned char>((to.flags & ~1) | on);
    to.manager->SetTextureUpdate(&to, on != 0);
}

// 0x10004b70: "%08x%%name" with the next id (an instance's own prefix replaced)
PString __cdecl Texture::GenerateUniqueName(const PString& n) const
{
    String s;
    if (HasUniquePrefix(n)) {
        s = n;
        s.Del(0, 8);
        char id[16];
        std::sprintf(id, "%08x", static_cast<unsigned int>(uniqueTexId++));
        s.Ins(id, 0);
    } else {
        const unsigned int id = static_cast<unsigned int>(uniqueTexId++);
        s.Setf("%08x%%%%%s", id, n.Text());
    }
    return PString(static_cast<const char*>(s));
}

// 0x10004cd0: the name without an instance's prefix
PString __cdecl Texture::GetNonUniqueName() const
{
    if (HasUniquePrefix(name)) return PString(name.Text() + 10);
    return name;
}

}  // namespace Jet
