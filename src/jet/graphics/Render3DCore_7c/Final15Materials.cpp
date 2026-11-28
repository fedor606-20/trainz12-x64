// Render3DCore_7c Final15: the multipass material texture virtuals left by U5a/U5b. Transcribed from the TS12
// assembly at the addresses in the comments. The bodies only touch fields at their TS12 offsets and call the same
// RenderSystem/Texture entries the original calls; objects are always constructed by the original module.
#include "Final15.h"

#include <cstring>

namespace Jet {
namespace Final15 {

namespace {

template <class T>
T& Field(void* self, unsigned int offset)
{
    return *reinterpret_cast<T*>(static_cast<unsigned char*>(self) + offset);
}
template <class T>
T Field(const void* self, unsigned int offset)
{
    return *reinterpret_cast<const T*>(static_cast<const unsigned char*>(self) + offset);
}

// fld dword / fstp dword: only the quiet bit of a signalling NaN changes.
float X87(float f)
{
    unsigned int u;
    std::memcpy(&u, &f, 4);
    if ((u & 0x7f800000u) == 0x7f800000u && (u & 0x007fffffu) && !(u & 0x00400000u)) u |= 0x00400000u;
    std::memcpy(&f, &u, 4);
    return f;
}

// Material's protected SetTextureDefinition, reached through a view with the real class layout.
struct MaterialView : Material {
    void Definition(int type, const PString& name, float amount) const
    {
        const_cast<MaterialView*>(this)->SetTextureDefinition(type, name, amount);
    }
    void UpdateTextureArray() const
    {
        using Fn = void(__cdecl*)(const Material*);
        reinterpret_cast<Fn>((*reinterpret_cast<void***>(const_cast<MaterialView*>(this)))[0x5c / 4])(this);
    }
};

void SetDefinition(void* self, int type, const PString& name, float amount)
{
    reinterpret_cast<const MaterialView*>(self)->Definition(type, name, X87(amount));
}

// texture virtual slot 2 (CreateInstance); the returned reference is transferred to the caller
Texture* DuplicateTexture(Texture* texture)
{
    if (!texture) return nullptr;
    using Fn = Texture*(__cdecl*)(Texture*);
    return reinterpret_cast<Fn>((*reinterpret_cast<void***>(texture))[2])(texture);
}

void SetPriority(Texture* texture, float priority)
{
    using Fn = void(__cdecl*)(Texture*, float);
    reinterpret_cast<Fn>((*reinterpret_cast<void***>(texture))[0x48 / 4])(texture, X87(priority));
}

bool IsAvailable(Texture* texture)
{
    using Fn = bool(__cdecl*)(const Texture*);
    return reinterpret_cast<Fn>((*reinterpret_cast<void***>(texture))[0xc / 4])(texture);
}

// TextureManager::Create: the manager hangs off materialManager (+0x98) at +0xc.
Texture* CreateTexture(void* self, const ResourceDatabase* db, const PString& name, const String& request)
{
    void* const manager = Field<void*>(Field<void*>(self, 0x98), 0xc);
    using Fn = Texture*(__cdecl*)(void*, const ResourceDatabase*, const PString*, const String*);
    return reinterpret_cast<Fn>((*reinterpret_cast<void***>(manager))[3])(manager, db, &name, &request);
}

// The common SetTexture(db) tail: result 0 on success, 3 when the manager gave no texture (old restored).
void SetTextureDbCommon(void* self, Material::Result* out, int type, unsigned int slot, const ResourceDatabase* db,
                        const PString& name, float amount, const String& request)
{
    Texture* const old = Field<Texture*>(self, slot);
    if (old) old->AddReference();
    Texture* const texture = CreateTexture(self, db, name, request);
    if (Field<Texture*>(self, slot)) Field<Texture*>(self, slot)->ReleaseReference();
    Field<Texture*>(self, slot) = texture;
    if (!texture) {
        Field<Texture*>(self, slot) = old;
        if (old) old->AddReference();
        *out = Material::Result(3);
    } else {
        SetPriority(texture, Field<float>(self, 0x94));
        SetDefinition(self, type, name, amount);
        reinterpret_cast<const MaterialView*>(self)->UpdateTextureArray();
        *out = Material::Result(0);
    }
    if (old) old->ReleaseReference();
}

// The common SetTexture(AutoReference) body: wrong type is rejected by the caller. Result 3 when empty.
void SetTextureRefCommon(void* self, Material::Result* out, int type, unsigned int slot,
                         const AutoReference<Texture>& texture, float amount)
{
    Texture* const value = const_cast<AutoReference<Texture>&>(texture).Get();
    if (!value) {
        *out = Material::Result(3);
        return;
    }
    Texture* const old = Field<Texture*>(self, slot);
    Field<Texture*>(self, slot) = value;
    value->AddReference();
    if (old) old->ReleaseReference();
    SetPriority(value, Field<float>(self, 0x94));
    SetDefinition(self, type, Field<PString>(value, 0x10), amount);
    reinterpret_cast<const MaterialView*>(self)->UpdateTextureArray();
    *out = Material::Result(0);
}

void DuplicateTextureCommon(void* self, Material::Result* out, int type, unsigned int slot)
{
    Texture* const old = Field<Texture*>(self, slot);
    if (!old) {
        *out = Material::Result(3);
        return;
    }
    Texture* const copy = DuplicateTexture(old);
    if (Field<Texture*>(self, slot)) Field<Texture*>(self, slot)->ReleaseReference();
    Field<Texture*>(self, slot) = copy;
    *out = Material::Result(0);
}

}  // namespace

// ---------------------------------------------------------------------------------------------------------------
// MultiPassNoTextureMaterial (0x1002d880, 0x10034080, 0x10034260)

void NoTextureDuplicateTexture(void* self, Material::Result* out, int type)
{
    if (type == 0xc) {
        DuplicateTextureCommon(self, out, type, 0xa4);
    } else if (type == 0xd) {
        DuplicateTextureCommon(self, out, type, 0xa8);
    } else {
        *out = Material::Result(2);
    }
}

void NoTextureSetTextureDb(void* self, Material::Result* out, int type, const ResourceDatabase* db,
                           const PString& name, float amount, const String& request)
{
    if (type == 0xc) {
        SetTextureDbCommon(self, out, 0xc, 0xa4, db, name, amount, request);
    } else if (type == 0xd) {
        SetTextureDbCommon(self, out, 0xd, 0xa8, db, name, amount, request);
    } else {
        *out = Material::Result(2);
    }
}

void NoTextureSetTextureRef(void* self, Material::Result* out, int type, const AutoReference<Texture>& texture,
                            float amount)
{
    if (type == 0xc) {
        SetTextureRefCommon(self, out, 0xc, 0xa4, texture, amount);
    } else if (type == 0xd) {
        SetTextureRefCommon(self, out, 0xd, 0xa8, texture, amount);
    } else {
        *out = Material::Result(2);
    }
}

// ---------------------------------------------------------------------------------------------------------------
// MultiPassOneTextureMaterial (0x1002dde0, 0x1002e030, 0x10036c80, 0x10036de0)

void OneTextureDuplicateTexture(void* self, Material::Result* out, int type)
{
    if (type == 1) {
        DuplicateTextureCommon(self, out, 1, 0xc4);
    } else {
        NoTextureDuplicateTexture(self, out, type);
    }
}

void OneTextureSetTextureDb(void* self, Material::Result* out, int type, const ResourceDatabase* db,
                            const PString& name, float amount, const String& request)
{
    if (type != 1) {
        NoTextureSetTextureDb(self, out, type, db, name, amount, request);
        return;
    }
    SetTextureDbCommon(self, out, 1, 0xc4, db, name, amount, request);
    if (out->code == 0) Field<unsigned int>(self, 0xc8) = 0xffffffffu;
}

void OneTextureSetTextureRef(void* self, Material::Result* out, int type, const AutoReference<Texture>& texture,
                             float amount)
{
    if (type != 1) {
        NoTextureSetTextureRef(self, out, type, texture, amount);
        return;
    }
    SetTextureRefCommon(self, out, 1, 0xc4, texture, amount);
    if (out->code == 0) Field<unsigned int>(self, 0xc8) = 0xffffffffu;
}

void OneTextureIsLoaded(const void* self, Material::Result* out)
{
    Texture* const texture = Field<Texture*>(self, 0xc4);
    if (texture && IsAvailable(texture)) {
        *out = Material::Result(0);
        return;
    }
    *out = Material::Result(4);
}

// ---------------------------------------------------------------------------------------------------------------
// MultiPassBumpMaterial (0x1002e2f0, 0x1002e4c0, 0x1002e850, 0x1002e9b0)

void BumpDuplicateTexture(void* self, Material::Result* out, int type)
{
    if (type == 8) {
        DuplicateTextureCommon(self, out, 8, 0xd0);
    } else {
        OneTextureDuplicateTexture(self, out, type);
    }
}

void BumpSetTextureDb(void* self, Material::Result* out, int type, const ResourceDatabase* db, const PString& name,
                      float amount, const String& request)
{
    if (type != 8) {
        OneTextureSetTextureDb(self, out, type, db, name, amount, request);
        return;
    }
    SetTextureDbCommon(self, out, 8, 0xd0, db, name, amount, request);
    if (out->code == 0) Field<unsigned int>(self, 0xc8) = 0xffffffffu;
}

void BumpSetTextureRef(void* self, Material::Result* out, int type, const AutoReference<Texture>& texture,
                       float amount)
{
    if (type != 8) {
        OneTextureSetTextureRef(self, out, type, texture, amount);
        return;
    }
    SetTextureRefCommon(self, out, 8, 0xd0, texture, amount);
}

void BumpIsLoaded(const void* self, Material::Result* out)
{
    Texture* const bump = Field<Texture*>(self, 0xd0);
    if (bump && IsAvailable(bump)) {
        Texture* const texture = Field<Texture*>(self, 0xc4);
        if (texture && IsAvailable(texture)) {
            *out = Material::Result(0);
            return;
        }
    }
    *out = Material::Result(4);
}

}  // namespace Final15
}  // namespace Jet
