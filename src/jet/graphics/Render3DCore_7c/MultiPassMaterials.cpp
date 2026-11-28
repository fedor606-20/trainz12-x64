// Unit U5b of Render3DCore_7c: the cdecl leaves of the multipass materials, transcribed from the TS12 assembly.
//
// Every recorded method here is __cdecl in the original (ordinary and virtual alike), so `this` arrives on the stack
// and a plain `extern "C"` function has the exact entry ABI; implemented.d/u5b_multipass.txt exports these symbols
// under the original mangled names. Constructors, destructors and every method that needs Jet::Material (U5a), the
// U4 caches or the material/geometry contract keep forwarding.
//
// The virtual leaves (GetType, GetTexture, Activate, UpdateTextureArray) are called directly by their exported name,
// which is what the differential test compares; the class vftables stay the original's, so an object made by the
// original still dispatches to the original slot until the owning unit moves.
#include <jet/Render3DCoreMultiPass.h>

#include <jet/DynamicArray.h>
#include <jet/PString.h>

#include <cstring>

namespace Jet {
namespace MultiPass {

namespace {

// GetType and the IsSupported fallback names are the module's exported static PString objects; returning the
// original's address keeps the pointer, the interned node and its reference counts identical.
const void* OriginalType(const char* name) { return OriginalExport(name); }

constexpr const char* kTypeNoTexture = "?UNDECORATED_TYPE@MultiPassNoTextureMaterial@Jet@@2VPString@2@B";
constexpr const char* kTypeOneTexture = "?UNDECORATED_TYPE@MultiPassOneTextureMaterial@Jet@@2VPString@2@B";
constexpr const char* kTypeBump = "?UNDECORATED_TYPE@MultiPassBumpMaterial@Jet@@2VPString@2@B";
constexpr const char* kTypeBump2 = "?UNDECORATED_TYPE@MultiPassBump2Material@Jet@@2VPString@2@B";
constexpr const char* kTypeBumpSpec = "?UNDECORATED_TYPE@MultiPassBumpSpecMaterial@Jet@@2VPString@2@B";
constexpr const char* kTypeBumpSpec2 = "?UNDECORATED_TYPE@MultiPassBumpSpec2Material@Jet@@2VPString@2@B";

// A virtual texture slot called with (this, int) on the stack (Activate uses slot 0x3c).
void CallTextureActivate(void* texture, int value)
{
    if (!texture) return;
    void** const vtable = *reinterpret_cast<void***>(texture);
    reinterpret_cast<void(__cdecl*)(void*, int)>(vtable[0x3c / 4])(texture, value);
}

// One Texture pointer from the material tail.
void* TextureAt(void* self, unsigned int offset)
{
    return *reinterpret_cast<void**>(static_cast<unsigned char*>(self) + offset);
}

// UpdateTextureArray: the Array<const Texture*, 10> at +0xb8 is emptied and refilled one Reserve(count + 1) at a
// time (the original funnels every append through the same inlined Reserve, so the capacity steps by ten).
void ResetTextureArray(void* self)
{
    Array<const void*, 10>* const array =
        reinterpret_cast<Array<const void*, 10>*>(static_cast<unsigned char*>(self) + MaterialLayout::textureArray);
    array->count = 0;
}

void AppendTexture(void* self, const void* texture)
{
    Array<const void*, 10>* const array =
        reinterpret_cast<Array<const void*, 10>*>(static_cast<unsigned char*>(self) + MaterialLayout::textureArray);
    array->Reserve(array->count + 1);
    array->data[array->count] = texture;
    ++array->count;
}

}  // namespace

}  // namespace MultiPass
}  // namespace Jet

// The original mangled entries; this translation unit only needs the flat ABI. Each GetType/GetTexture pair of
// implementations (non-const and const) shares one function because the generated code is identical.
namespace Jet {
namespace MultiPass {
namespace Leaves {

// ---- MultiPassNoTextureMaterial mode flags (0xac..0xaf) ------------------------------------------------------
unsigned char r3d_u5b_notexture_get_smooth(const void* self)
{
    return *reinterpret_cast<const unsigned char*>(static_cast<const unsigned char*>(self) + 0xac);
}
void r3d_u5b_notexture_set_smooth(void* self, unsigned char value)
{
    *reinterpret_cast<unsigned char*>(static_cast<unsigned char*>(self) + 0xac) = value;
}
unsigned char r3d_u5b_notexture_get_single(const void* self)
{
    return *reinterpret_cast<const unsigned char*>(static_cast<const unsigned char*>(self) + 0xad);
}
void r3d_u5b_notexture_set_single(void* self, unsigned char value)
{
    *reinterpret_cast<unsigned char*>(static_cast<unsigned char*>(self) + 0xad) = value;
}
unsigned char r3d_u5b_notexture_get_intensity(const void* self)
{
    return *reinterpret_cast<const unsigned char*>(static_cast<const unsigned char*>(self) + 0xae);
}
void r3d_u5b_notexture_set_intensity(void* self, unsigned char value)
{
    *reinterpret_cast<unsigned char*>(static_cast<unsigned char*>(self) + 0xae) = value;
}
unsigned char r3d_u5b_notexture_get_debug(const void* self)
{
    return *reinterpret_cast<const unsigned char*>(static_cast<const unsigned char*>(self) + 0xaf);
}
void r3d_u5b_notexture_set_debug(void* self, unsigned char value)
{
    *reinterpret_cast<unsigned char*>(static_cast<unsigned char*>(self) + 0xaf) = value;
}

// ---- GetType of the six multipass material types (0x2d840, 0x2dd90, 0x2e290, 0x2e6a0, 0x31b10, 0x31df0) -------
const void* r3d_u5b_type_notexture(const void*)
{
    return Jet::MultiPass::OriginalType(Jet::MultiPass::kTypeNoTexture);
}
const void* r3d_u5b_type_onetexture(const void*)
{
    return Jet::MultiPass::OriginalType(Jet::MultiPass::kTypeOneTexture);
}
const void* r3d_u5b_type_bump(const void*)
{
    return Jet::MultiPass::OriginalType(Jet::MultiPass::kTypeBump);
}
const void* r3d_u5b_type_bump2(const void*)
{
    return Jet::MultiPass::OriginalType(Jet::MultiPass::kTypeBump2);
}
const void* r3d_u5b_type_bumpspec(const void*)
{
    return Jet::MultiPass::OriginalType(Jet::MultiPass::kTypeBumpSpec);
}
const void* r3d_u5b_type_bumpspec2(const void*)
{
    return Jet::MultiPass::OriginalType(Jet::MultiPass::kTypeBumpSpec2);
}

// ---- GetTexture (0x2d850, 0x2dda0, 0x2e2a0) ----------------------------------------------------------------
void* r3d_u5b_texture_notexture(void* self, int slot)
{
    if (slot == 0xc) return Jet::MultiPass::TextureAt(self, 0xa4);
    if (slot == 0xd) return Jet::MultiPass::TextureAt(self, 0xa8);
    return nullptr;
}
void* r3d_u5b_texture_onetexture(void* self, int slot)
{
    if (slot == 1) return Jet::MultiPass::TextureAt(self, 0xc4);
    if (slot == 0xc) return Jet::MultiPass::TextureAt(self, 0xa4);
    if (slot == 0xd) return Jet::MultiPass::TextureAt(self, 0xa8);
    return nullptr;
}
void* r3d_u5b_texture_bump(void* self, int slot)
{
    if (slot == 8) return Jet::MultiPass::TextureAt(self, 0xd0);
    if (slot == 1) return Jet::MultiPass::TextureAt(self, 0xc4);
    if (slot == 0xc) return Jet::MultiPass::TextureAt(self, 0xa4);
    if (slot == 0xd) return Jet::MultiPass::TextureAt(self, 0xa8);
    return nullptr;
}

// ---- OneTexture alpha hint (0x2e0f0 / 0x2e100): a whole dword, -1 until set -----------------------------------
void r3d_u5b_onetexture_set_alpha(void* self, unsigned int value)
{
    *reinterpret_cast<unsigned int*>(static_cast<unsigned char*>(self) + 0xc8) = value;
}
unsigned int r3d_u5b_onetexture_get_alpha(const void* self)
{
    return *reinterpret_cast<const unsigned int*>(static_cast<const unsigned char*>(self) + 0xc8);
}

// ---- Activate (0x2d930, 0x2de50, 0x2e360): each non-null texture's slot 0x3c with the argument ---------------
void r3d_u5b_activate_notexture(void* self, int value)
{
    Jet::MultiPass::CallTextureActivate(Jet::MultiPass::TextureAt(self, 0xa4), value);
    Jet::MultiPass::CallTextureActivate(Jet::MultiPass::TextureAt(self, 0xa8), value);
}
void r3d_u5b_activate_onetexture(void* self, int value)
{
    Jet::MultiPass::CallTextureActivate(Jet::MultiPass::TextureAt(self, 0xc4), value);
    Jet::MultiPass::CallTextureActivate(Jet::MultiPass::TextureAt(self, 0xa4), value);
    Jet::MultiPass::CallTextureActivate(Jet::MultiPass::TextureAt(self, 0xa8), value);
}
void r3d_u5b_activate_bump(void* self, int value)
{
    Jet::MultiPass::CallTextureActivate(Jet::MultiPass::TextureAt(self, 0xd0), value);
    Jet::MultiPass::CallTextureActivate(Jet::MultiPass::TextureAt(self, 0xc4), value);
    Jet::MultiPass::CallTextureActivate(Jet::MultiPass::TextureAt(self, 0xa4), value);
    Jet::MultiPass::CallTextureActivate(Jet::MultiPass::TextureAt(self, 0xa8), value);
}

// ---- UpdateTextureArray (0x2dc60, 0x2dfb0, 0x2e420) ----------------------------------------------------------
void r3d_u5b_update_notexture(void* self)
{
    Jet::MultiPass::ResetTextureArray(self);
    Jet::MultiPass::AppendTexture(self, Jet::MultiPass::TextureAt(self, 0xa4));
    Jet::MultiPass::AppendTexture(self, Jet::MultiPass::TextureAt(self, 0xa8));
}
void r3d_u5b_update_onetexture(void* self)
{
    Jet::MultiPass::ResetTextureArray(self);
    Jet::MultiPass::AppendTexture(self, Jet::MultiPass::TextureAt(self, 0xc4));
    Jet::MultiPass::AppendTexture(self, Jet::MultiPass::TextureAt(self, 0xa4));
    Jet::MultiPass::AppendTexture(self, Jet::MultiPass::TextureAt(self, 0xa8));
}
void r3d_u5b_update_bump(void* self)
{
    Jet::MultiPass::ResetTextureArray(self);
    Jet::MultiPass::AppendTexture(self, Jet::MultiPass::TextureAt(self, 0xc4));
    Jet::MultiPass::AppendTexture(self, Jet::MultiPass::TextureAt(self, 0xd0));
    Jet::MultiPass::AppendTexture(self, Jet::MultiPass::TextureAt(self, 0xa4));
    Jet::MultiPass::AppendTexture(self, Jet::MultiPass::TextureAt(self, 0xa8));
}
}  // namespace Leaves
}  // namespace MultiPass
}  // namespace Jet
