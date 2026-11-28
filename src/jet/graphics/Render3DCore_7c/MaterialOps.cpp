// Unit U5a2 of Render3DCore_7c: the second pass over the material exports left forwarded by U5a. The class objects
// are the original modules's; this file only adds members that need no geometry chunk: the RTTI operator== set, the
// material factories, the plugin capability queries, the texture set/duplicate paths of the one-texture family and
// the GenericShader ring leaves. Everything that still needs U6 chunks stays in that unit and is forwarded.
//
// Ordinary and virtual entries are __cdecl with `this` on the stack (as the TS12 mangling spells them), so the bodies
// are plain namespace functions exported under the original names by implemented.d/u5a2_materials.txt. The only
// thiscall entries here (constructors, assignments) live in MaterialLifetime.cpp/MaterialPlugins.cpp with a matching
// thunk object.
#include <jet/Render3DCoreCaches.h>
#include <jet/Render3DCoreMaterials.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <typeinfo>

namespace Jet {

// Not named by any reconstructed RenderSystem header: the shared default textures are fetched through it.
enum SharedTextureID {};
class SharedTextureCollection {
public:
    static bool __cdecl GetTexture(SharedTextureID id, TextureManager* manager, AutoReference<Texture>& out);
};

namespace U5a2 {

namespace {

template <class T>
T& Field(void* p, unsigned offset)
{
    return *reinterpret_cast<T*>(static_cast<unsigned char*>(p) + offset);
}
template <class T>
const T& Field(const void* p, unsigned offset)
{
    return *reinterpret_cast<const T*>(static_cast<const unsigned char*>(p) + offset);
}

// a virtual slot of Display the project header does not name (cdecl, this first)
template <class R, class... A>
R DisplaySlot(const Display* display, unsigned offset, A... args)
{
    using F = R(__cdecl*)(const Display*, A...);
    return (*reinterpret_cast<F* const*>(display))[offset / sizeof(void*)](display, args...);
}

// the original compares two texture references by their distance inside the texture-manager record block:
// (a - b) / 0x34 == 0. Integer division truncates toward zero like the IMUL/SAR sequence the compiler made.
bool SameTexture34(const Texture* a, const Texture* b)
{
    const int delta = static_cast<int>(reinterpret_cast<std::intptr_t>(a) - reinterpret_cast<std::intptr_t>(b));
    return delta / 0x34 == 0;
}

float X87(float f)
{
    unsigned int u;
    std::memcpy(&u, &f, 4);
    if ((u & 0x7f800000u) == 0x7f800000u && (u & 0x007fffffu) && !(u & 0x00400000u)) u |= 0x00400000u;
    std::memcpy(&f, &u, 4);
    return f;
}

Texture* TextureOf(const AutoReference<Texture>& ref) { return const_cast<AutoReference<Texture>&>(ref).Get(); }
Texture* TextureOf(const OneTextureMaterial& m)
{
    return const_cast<OneTextureMaterial&>(m).texture.Get();
}

// The virtual sets are reachable from a derived class only through a protected accessor. These helpers have the exact
// class layout; no object of theirs is ever made, they are only used as a typed view of the original object.
struct NoTextureView : NoTextureMaterial {
    bool BaseEquals(const Material& o) const { return Material::Equals(o); }
};
struct OneTextureView : OneTextureMaterial {
    bool BaseEquals(const Material& o) const { return Material::Equals(o); }
};
struct ReflectiveView : ReflectiveMaterial {
    bool BaseEquals(const Material& o) const { return Material::Equals(o); }
};
struct GenericShaderView : GenericShaderMaterial {
    bool BaseEquals(const Material& o) const { return Material::Equals(o); }
    bool SelfEquals(const GenericShaderMaterial& o) const { return GenericShaderMaterial::Equals(o); }
};
struct MaterialView : Material {
    void Definition(int type, const PString& name, float amount) const
    {
        const_cast<MaterialView*>(this)->SetTextureDefinition(type, name, amount);
    }
    void Init(Material& instance) const { const_cast<MaterialView*>(this)->Material::InitInstance(instance); }
};
struct OneTextureFactoryView : OneTextureMaterial {
    void CopyTexture(OneTextureMaterial& target) const
    {
        Texture* const texture = const_cast<OneTextureFactoryView*>(this)->texture.Get();
        if (texture) texture->AddReference();
        Texture* const old = target.texture.Get();
        target.texture = texture;  // AutoReference takes over the reference added above
        if (old) old->ReleaseReference();
    }
};
struct MVHFactoryView : MVHSelectMaterial {
    void Init(MVHSelectMaterial& instance) const { MVHSelectMaterial::InitInstance(instance); }
};
struct ReflectiveFactoryView : ReflectiveMaterial {
    void Init(ReflectiveMaterial& instance) const { ReflectiveMaterial::InitInstance(instance); }
};
struct GenericShaderFactoryView : GenericShaderMaterial {
    void Init(GenericShaderMaterial& instance) const { GenericShaderMaterial::InitInstance(instance); }
};

bool OneTextureEquals(const OneTextureMaterial* self, const Material* other)
{
    const OneTextureMaterial* const o = static_cast<const OneTextureMaterial*>(other);
    if (!SameTexture34(TextureOf(*self), TextureOf(*o))) return false;
    if (self->alphaHint != o->alphaHint) return false;
    return reinterpret_cast<const OneTextureView*>(self)->BaseEquals(*other);
}

bool ReflectiveEquals(const ReflectiveMaterial* self, const Material* other)
{
    const ReflectiveMaterial* const o = dynamic_cast<const ReflectiveMaterial*>(other);
    if (!o) return false;
    if (!SameTexture34(TextureOf(*static_cast<const OneTextureMaterial*>(self)),
                       TextureOf(*static_cast<const OneTextureMaterial*>(o))))
        return false;
    if (!SameTexture34(TextureOf(self->cubeTexture), TextureOf(o->cubeTexture))) return false;
    // the original compares reflectivity with ucomiss: unordered (NaN) is never equal
    if (!(self->reflectivity == o->reflectivity)) return false;
    if (self->contextCount != o->contextCount) return false;
    for (unsigned int i = 0; i < self->contextCount; ++i)
        if (self->contextData[i] != o->contextData[i]) return false;
    return reinterpret_cast<const ReflectiveView*>(self)->BaseEquals(*other);
}

// GenericShader's texture ring: a circular list of blocks {next, prev, count, items[8 bytes]}; every item is
// {int type, Texture*}. The list is shared, so its identity (the head pointer) is what Equals compares.
struct RingBlock {
    RingBlock* next;   // +0x00
    RingBlock* prev;   // +0x04
    unsigned int count; // +0x08
};
struct RingItem {
    int type;         // +0x00
    Texture* texture; // +0x04
};

template <class F>
void ForEachRingItem(const void* head, F f)
{
    RingBlock* const first = const_cast<RingBlock*>(static_cast<const RingBlock*>(head));
    if (!first) return;
    RingBlock* block = first;
    for (;;) {
        RingItem* const items = reinterpret_cast<RingItem*>(reinterpret_cast<unsigned char*>(block) + 0x0c);
        for (unsigned int i = 0; i < block->count; ++i) f(items[i]);
        block = block->next;
        if (block == first || !block) return;
    }
}

RingItem* FindRingItem(void* head, int type)
{
    RingItem* found = nullptr;
    ForEachRingItem(head, [&](RingItem& item) {
        if (!found && item.type == type) found = &item;
    });
    return found;
}

Texture* GenericShaderGetTexture(const GenericShaderMaterial* self, int type)
{
    RingItem* const item = FindRingItem(const_cast<GenericShaderMaterial*>(self)->textureRing, type);
    return item ? item->texture : nullptr;
}

// the same ring walk the other GenericShader leaves use
template <class F>
void ForEachTexture(const GenericShaderMaterial* self, F f)
{
    ForEachRingItem(const_cast<GenericShaderMaterial*>(self)->textureRing, [&](RingItem& item) {
        if (item.texture) f(item.texture);
    });
}

bool GenericShaderEquals(const GenericShaderMaterial* self, const GenericShaderMaterial* other)
{
    if (self->alphaHint != other->alphaHint) return false;
    if (self->textureRing && other->textureRing && self->textureRing != other->textureRing) return false;
    return reinterpret_cast<const GenericShaderView*>(self)->BaseEquals(*other);
}

// A texture's virtual slot 2 (CreateInstance) duplicates it; the original takes over the returned reference.
Texture* DuplicateTextureObject(Texture* texture)
{
    if (!texture) return nullptr;
    void** const vtable = *reinterpret_cast<void***>(texture);
    return reinterpret_cast<Texture* (__cdecl*)(Texture*)>(vtable[2])(texture);
}

void SetDefinition(void* self, int type, const PString& name, float amount)
{
    reinterpret_cast<const MaterialView*>(self)->Definition(type, name, amount);
}

// one-texture SetTexture result: 0 set, 2 wrong type, 3 no texture, 6 accepted but no texture type
void SetOneTexture(void* self, Material::Result* out, int type, const AutoReference<Texture>* ref, float amount)
{
    OneTextureMaterial& material = *static_cast<OneTextureMaterial*>(self);
    if (type != 1) {
        *out = Material::Result(type == 6 ? 0 : 2);
        return;
    }
    Texture* const texture = TextureOf(*ref);
    if (!texture) {
        *out = Material::Result(3);
        return;
    }
    Texture* const old = material.texture.Get();
    material.texture = texture;
    texture->AddReference();
    if (old) old->ReleaseReference();
    texture->SetResourcePriority(Field<float>(self, 0x94));
    Field<TextureAlpha>(self, 0xa0) = static_cast<TextureAlpha>(-1);
    SetDefinition(self, 1, Field<PString>(texture, 0x10), X87(amount));
    *out = Material::Result(0);
}

}  // namespace

// ---------------------------------------------------------------------------------------------------------------
// operator==: __RTDynamicCast to the class, then the class's own field comparison and Material::Equals.

bool eq_NoTexture(const void* self, const Material* other)
{
    if (!dynamic_cast<const NoTextureMaterial*>(other)) return false;
    return reinterpret_cast<const NoTextureView*>(self)->BaseEquals(*other);
}

bool eq_OneTexture(const void* self, const Material* other)
{
    // The original's RTTI for OneTextureNoAlphaMaterial does not answer the cast to OneTextureMaterial (the other
    // derived one-texture classes do), so its OneTextureMaterial::operator== rejects that class. Reproduced by name.
    if (typeid(*other) == typeid(OneTextureNoAlphaMaterial)) return false;
    return OneTextureEquals(static_cast<const OneTextureMaterial*>(self), other);
}

bool eq_OneTextureNoAlpha(const void* self, const Material* other)
{
    if (!dynamic_cast<const OneTextureNoAlphaMaterial*>(other)) return false;
    return OneTextureEquals(static_cast<const OneTextureMaterial*>(self), other);
}

bool eq_Billboard(const void* self, const Material* other)
{
    if (!dynamic_cast<const BillboardMaterial*>(other)) return false;
    return OneTextureEquals(static_cast<const OneTextureMaterial*>(self), other);
}

bool eq_ColorOneTexture(const void* self, const Material* other)
{
    if (!dynamic_cast<const ColorOneTextureMaterial*>(other)) return false;
    return OneTextureEquals(static_cast<const OneTextureMaterial*>(self), other);
}

bool eq_AutoTex(const void* self, const Material* other)
{
    if (!dynamic_cast<const AutoTexMaterial*>(other)) return false;
    return OneTextureEquals(static_cast<const OneTextureMaterial*>(self), other);
}

bool eq_MVHSelect(const void* self, const Material* other)
{
    if (!dynamic_cast<const MVHSelectMaterial*>(other)) return false;
    return OneTextureEquals(static_cast<const OneTextureMaterial*>(self), other);
}

bool eq_Reflective(const void* self, const Material* other)
{
    return ReflectiveEquals(static_cast<const ReflectiveMaterial*>(self), other);
}

bool eq_Gloss(const void* self, const Material* other)
{
    if (!dynamic_cast<const GlossMaterial*>(other)) return false;
    return ReflectiveEquals(static_cast<const ReflectiveMaterial*>(self), other);
}

bool eq_GenericShader(const void* self, const Material* other)
{
    const GenericShaderMaterial* const o = dynamic_cast<const GenericShaderMaterial*>(other);
    if (!o) return false;
    return GenericShaderEquals(static_cast<const GenericShaderMaterial*>(self), o);
}

bool GenericShader_equals(const void* self, const GenericShaderMaterial* other)
{
    return GenericShaderEquals(static_cast<const GenericShaderMaterial*>(self), other);
}

// ---------------------------------------------------------------------------------------------------------------
// CreateInstance: Material::CreateInstance(UNDECORATED_TYPE, this->materialManager)

Material* create_NoTexture(const void* self)
{
    return Material::CreateInstance(NoTextureMaterial::UNDECORATED_TYPE, Field<MaterialManager*>(self, 0x98));
}
Material* create_OneTexture(const void* self)
{
    Material* const instance = Material::CreateInstance(OneTextureMaterial::UNDECORATED_TYPE,
                                                        Field<MaterialManager*>(self, 0x98));
    if (instance) {
        reinterpret_cast<const MaterialView*>(self)->Init(*instance);
        reinterpret_cast<const OneTextureFactoryView*>(self)->CopyTexture(*static_cast<OneTextureMaterial*>(instance));
    }
    return instance;
}
Material* create_OneTextureNoAlpha(const void* self)
{
    Material* const instance = Material::CreateInstance(OneTextureNoAlphaMaterial::UNDECORATED_TYPE,
                                                        Field<MaterialManager*>(self, 0x98));
    if (instance) {
        reinterpret_cast<const MaterialView*>(self)->Init(*instance);
        reinterpret_cast<const OneTextureFactoryView*>(self)->CopyTexture(*static_cast<OneTextureMaterial*>(instance));
    }
    return instance;
}
Material* create_Billboard(const void* self)
{
    Material* const instance = Material::CreateInstance(BillboardMaterial::UNDECORATED_TYPE,
                                                        Field<MaterialManager*>(self, 0x98));
    if (instance) {
        reinterpret_cast<const MaterialView*>(self)->Init(*instance);
        reinterpret_cast<const OneTextureFactoryView*>(self)->CopyTexture(*static_cast<OneTextureMaterial*>(instance));
    }
    return instance;
}
Material* create_ColorOneTexture(const void* self)
{
    Material* const instance = Material::CreateInstance(ColorOneTextureMaterial::UNDECORATED_TYPE,
                                                        Field<MaterialManager*>(self, 0x98));
    if (instance) {
        reinterpret_cast<const MaterialView*>(self)->Init(*instance);
        reinterpret_cast<const OneTextureFactoryView*>(self)->CopyTexture(*static_cast<OneTextureMaterial*>(instance));
    }
    return instance;
}
Material* create_AutoTex(const void* self)
{
    Material* const instance = Material::CreateInstance(AutoTexMaterial::UNDECORATED_TYPE,
                                                        Field<MaterialManager*>(self, 0x98));
    if (instance) {
        reinterpret_cast<const MaterialView*>(self)->Init(*instance);
        reinterpret_cast<const OneTextureFactoryView*>(self)->CopyTexture(*static_cast<OneTextureMaterial*>(instance));
    }
    return instance;
}
Material* create_MVHSelect(const void* self)
{
    Material* const instance = Material::CreateInstance(MVHSelectMaterial::UNDECORATED_TYPE,
                                                        Field<MaterialManager*>(self, 0x98));
    if (instance)
        reinterpret_cast<const MVHFactoryView*>(self)->Init(*static_cast<MVHSelectMaterial*>(instance));
    return instance;
}
Material* create_Reflective(const void* self)
{
    Material* const instance = Material::CreateInstance(ReflectiveMaterial::UNDECORATED_TYPE,
                                                        Field<MaterialManager*>(self, 0x98));
    if (instance)
        reinterpret_cast<const ReflectiveFactoryView*>(self)->Init(*static_cast<ReflectiveMaterial*>(instance));
    return instance;
}
Material* create_Gloss(const void* self)
{
    Material* const instance = Material::CreateInstance(GlossMaterial::UNDECORATED_TYPE,
                                                        Field<MaterialManager*>(self, 0x98));
    if (instance)
        reinterpret_cast<const ReflectiveFactoryView*>(self)->Init(*static_cast<ReflectiveMaterial*>(instance));
    return instance;
}
Material* create_GenericShader(const void* self)
{
    Material* const instance = Material::CreateInstance(GenericShaderMaterial::UNDECORATED_TYPE,
                                                        Field<MaterialManager*>(self, 0x98));
    if (instance)
        reinterpret_cast<const GenericShaderFactoryView*>(self)->Init(
            *static_cast<GenericShaderMaterial*>(instance));
    return instance;
}

// ---------------------------------------------------------------------------------------------------------------
// Plugin IsSupported. The five plugins that share the original's 0x277f0 body just return true; the other three
// ask the display for one texture format (Display slot 7), then name their fallback material in the reason string.

bool supported_always(const void*, Display*, PString*) { return true; }

bool supported_AutoTex(const void*, Display* display, PString* reason)
{
    if (DisplaySlot<bool>(display, 0x1c, 0xe)) return true;
    *reason = NoTextureMaterial::UNDECORATED_TYPE;
    return false;
}

bool supported_Gloss(const void*, Display* display, PString* reason)
{
    if (DisplaySlot<bool>(display, 0x1c, 1)) return true;
    *reason = OneTextureNoAlphaMaterial::UNDECORATED_TYPE;
    return false;
}

bool supported_Reflective(const void*, Display* display, PString* reason)
{
    if (DisplaySlot<bool>(display, 0x1c, 1)) return true;
    *reason = OneTextureMaterial::UNDECORATED_TYPE;
    return false;
}

// ---------------------------------------------------------------------------------------------------------------
// GenericShader leaves (0x26220..0x26c1f)

Texture* GenericShader_get_texture(void* self, int type)
{
    return GenericShaderGetTexture(static_cast<GenericShaderMaterial*>(self), type);
}

void GenericShader_set_resource_priority(void* self, float priority)
{
    GenericShaderMaterial& material = *static_cast<GenericShaderMaterial*>(self);
    Field<float>(self, 0x94) = priority;
    const float quiet = X87(priority);
    ForEachTexture(&material, [&](Texture* texture) { texture->SetResourcePriority(quiet); });
}

void GenericShader_is_loaded(const void* self, Material::Result* out)
{
    bool available = true;
    ForEachTexture(static_cast<const GenericShaderMaterial*>(self),
                   [&](Texture* texture) { if (!texture->IsAvailable()) available = false; });
    *out = Material::Result(available ? 0 : 4);
}

void GenericShader_activate(void* self, int detail)
{
    ForEachTexture(static_cast<GenericShaderMaterial*>(self),
                   [&](Texture* texture) { texture->Activate(detail); });
}

void GenericShader_set_display_state(const void* self, Display* display)
{
    const GenericShaderMaterial& material = *static_cast<const GenericShaderMaterial*>(self);
    display->SetCullMode((material.flags & 1) ? DCULL_NONE : static_cast<DCull>(1));
    ForEachTexture(&material, [&](Texture* texture) { texture->Bind(0); });
    if (void* const extra = Field<void*>(self, 0xac)) {
        void** const vtable = *reinterpret_cast<void***>(extra);
        reinterpret_cast<void(__cdecl*)(void*)>(vtable[0x14 / 4])(extra);
    }
    if (!(material.flags & 2)) {
        const unsigned char* const state = display->State();
        Field<unsigned char>(const_cast<GenericShaderMaterial*>(&material), 0x1c) =
            static_cast<unsigned char>(state[0xa1] & 1);
        Field<unsigned int>(const_cast<GenericShaderMaterial*>(&material), 0x14) = Field<unsigned int>(state, 0x68);
        Field<unsigned int>(const_cast<GenericShaderMaterial*>(&material), 0x18) = Field<unsigned int>(state, 0x6c);
        display->SetAlphaBlendFunction(static_cast<Src::Factor>(material.srcBlend),
                                       static_cast<Dst::Factor>(material.dstBlend));
        display->SetAlphaBlendMode(true);
    }
}

void GenericShader_clear_display_state(const void* self, Display* display)
{
    display->SetCullMode(static_cast<DCull>(1));
    if (void* const extra = Field<void*>(self, 0xac)) {
        void** const vtable = *reinterpret_cast<void***>(extra);
        reinterpret_cast<void(__cdecl*)(void*)>(vtable[0x18 / 4])(extra);
    }
    if (!(Field<unsigned char>(self, 0x84) & 2)) {
        display->SetAlphaBlendMode(Field<unsigned char>(self, 0x1c) != 0);
        display->SetAlphaBlendFunction(static_cast<Src::Factor>(Field<unsigned int>(self, 0x14)),
                                       static_cast<Dst::Factor>(Field<unsigned int>(self, 0x18)));
    }
}

void GenericShader_bind(void* self, Display* display, ViewPort*, unsigned int pass)
{
    if (pass == 0) GenericShader_set_display_state(self, display);
}

void GenericShader_unbind(void* self, Display* display, ViewPort*, unsigned int pass)
{
    if (pass == 0) GenericShader_clear_display_state(self, display);
}

void GenericShader_duplicate_texture(void* self, Material::Result* out, int type)
{
    GenericShaderMaterial& material = *static_cast<GenericShaderMaterial*>(self);
    RingItem* const item = FindRingItem(material.textureRing, type);
    if (item) {
        Texture* const old = item->texture;
        Texture* const copy = DuplicateTextureObject(old);
        if (old) old->ReleaseReference();
        item->texture = copy;
        *out = Material::Result(0);
        return;
    }
    *out = Material::Result(type <= 0xf ? 3 : 2);
}

// ---------------------------------------------------------------------------------------------------------------
// The one-texture family: SetTexture (both overloads) and DuplicateTexture on the texture slot at +0x9c.

void OneTexture_set_texture_db(void* self, Material::Result* out, int type, const ResourceDatabase* db,
                               const PString* name, float amount, const void* request)
{
    OneTextureMaterial& material = *static_cast<OneTextureMaterial*>(self);
    if (type != 1) {
        *out = Material::Result(type == 6 ? 0 : 2);
        return;
    }
    TextureManager* const textures = Field<TextureManager*>(Field<void*>(self, 0x98), 0xc);
    void** const vtable = *reinterpret_cast<void***>(textures);
    Texture* const texture = reinterpret_cast<Texture* (__cdecl*)(TextureManager*, const ResourceDatabase*,
                                                                  const PString*, const void*)>(vtable[3])(textures, db, name,
                                                                                                            request);
    Texture* const old = material.texture.Get();
    if (old) old->ReleaseReference();
    material.texture = texture;
    if (!texture) {
        *out = Material::Result(3);
        return;
    }
    texture->SetResourcePriority(Field<float>(self, 0x94));
    Field<TextureAlpha>(self, 0xa0) = static_cast<TextureAlpha>(-1);
    SetDefinition(self, 1, *name, X87(amount));
    *out = Material::Result(0);
}

void OneTexture_set_texture_ref(void* self, Material::Result* out, int type, const AutoReference<Texture>* ref,
                                float amount)
{
    SetOneTexture(self, out, type, ref, amount);
}

void OneTextureNoAlpha_set_texture_db(void* self, Material::Result* out, int type, const ResourceDatabase* db,
                                      const PString* name, float amount, const void* request)
{
    OneTextureMaterial& material = *static_cast<OneTextureMaterial*>(self);
    if (type != 1) {
        *out = Material::Result(type == 6 ? 0 : 2);
        return;
    }
    TextureManager* const textures = Field<TextureManager*>(Field<void*>(self, 0x98), 0xc);
    void** const vtable = *reinterpret_cast<void***>(textures);
    Texture* const texture = reinterpret_cast<Texture* (__cdecl*)(TextureManager*, const ResourceDatabase*,
                                                                  const PString*, const void*)>(vtable[3])(textures, db, name,
                                                                                                            request);
    Texture* const old = material.texture.Get();
    if (old) old->ReleaseReference();
    material.texture = texture;
    if (!texture) {
        *out = Material::Result(3);
        return;
    }
    texture->SetResourcePriority(Field<float>(self, 0x94));
    Field<TextureAlpha>(self, 0xa0) = static_cast<TextureAlpha>(0);
    SetDefinition(self, 1, *name, X87(amount));
    *out = Material::Result(0);
}

void OneTextureNoAlpha_set_texture_ref(void* self, Material::Result* out, int type, const AutoReference<Texture>* ref,
                                       float amount)
{
    SetOneTexture(self, out, type, ref, amount);
    if (type == 1 && out->code == 0) Field<TextureAlpha>(self, 0xa0) = static_cast<TextureAlpha>(0);
}

void OneTexture_duplicate_texture(void* self, Material::Result* out, int type)
{
    if (type != 1) {
        *out = Material::Result(2);
        return;
    }
    AutoReference<Texture>& ref = static_cast<OneTextureMaterial*>(self)->texture;
    Texture* const old = ref.Get();
    if (!old) {
        *out = Material::Result(3);
        return;
    }
    Texture* const copy = DuplicateTextureObject(old);
    old->ReleaseReference();
    ref = copy;
    *out = Material::Result(0);
}

// Reflective/Gloss add the cube texture (type 9); other types go to the one-texture body.

void Reflective_set_texture_db(void* self, Material::Result* out, int type, const ResourceDatabase* db,
                               const PString* name, float amount, const void* request)
{
    if (type != 9) {
        OneTexture_set_texture_db(self, out, type, db, name, amount, request);
        return;
    }
    ReflectiveMaterial& material = *static_cast<ReflectiveMaterial*>(self);
    TextureManager* const textures = Field<TextureManager*>(Field<void*>(self, 0x98), 0xc);
    void** const vtable = *reinterpret_cast<void***>(textures);
    Texture* const texture = reinterpret_cast<Texture* (__cdecl*)(TextureManager*, const ResourceDatabase*,
                                                                  const PString*, const void*)>(vtable[3])(textures, db, name,
                                                                                                            request);
    texture->AddReference();
    Texture* const old = material.cubeTexture.Get();
    if (old) old->ReleaseReference();
    material.cubeTexture = texture;
    if (!texture) {
        *out = Material::Result(3);
        return;
    }
    texture->SetResourcePriority(Field<float>(self, 0x94));
    material.reflectivity = amount;
    SetDefinition(self, 9, *name, X87(amount));
    *out = Material::Result(0);
}

void Reflective_set_texture_ref(void* self, Material::Result* out, int type, const AutoReference<Texture>* ref,
                                float amount)
{
    if (type != 9) {
        SetOneTexture(self, out, type, ref, amount);
        return;
    }
    ReflectiveMaterial& material = *static_cast<ReflectiveMaterial*>(self);
    Texture* const texture = TextureOf(*ref);
    if (!texture) {
        *out = Material::Result(3);
        return;
    }
    Texture* const old = material.cubeTexture.Get();
    material.cubeTexture = texture;
    texture->AddReference();
    if (old) old->ReleaseReference();
    texture->SetResourcePriority(Field<float>(self, 0x94));
    material.reflectivity = amount;
    SetDefinition(self, 9, Field<PString>(texture, 0x10), X87(amount));
    *out = Material::Result(0);
}

void Reflective_duplicate_texture(void* self, Material::Result* out, int type)
{
    ReflectiveMaterial& material = *static_cast<ReflectiveMaterial*>(self);
    if (type != 9) {
        OneTexture_duplicate_texture(self, out, type);
        return;
    }
    Texture* const old = material.cubeTexture.Get();
    if (!old) {
        *out = Material::Result(3);
        return;
    }
    Texture* const copy = DuplicateTextureObject(old);
    old->ReleaseReference();
    material.cubeTexture = copy;
    *out = Material::Result(0);
}

// Gloss's four entries are tail calls into Reflective's bodies.

void Gloss_set_texture_db(void* self, Material::Result* out, int type, const ResourceDatabase* db,
                          const PString* name, float amount, const void* request)
{
    Reflective_set_texture_db(self, out, type, db, name, amount, request);
}
void Gloss_set_texture_ref(void* self, Material::Result* out, int type, const AutoReference<Texture>* ref, float amount)
{
    Reflective_set_texture_ref(self, out, type, ref, amount);
}
void Gloss_duplicate_texture(void* self, Material::Result* out, int type)
{
    Reflective_duplicate_texture(self, out, type);
}

// ---------------------------------------------------------------------------------------------------------------
// Reflective's small leaves

unsigned int Reflective_get_num_passes(const void*) { return 2; }

void Reflective_change_tex_gen(const void* self, Display* display, bool update)
{
    const ReflectiveMaterial& material = *static_cast<const ReflectiveMaterial*>(self);
    if (!update) {
        display->SetTextureCoordGen(1, static_cast<DTexGenMode>(0), 2);
        return;
    }
    Texture* const cube = const_cast<ReflectiveMaterial&>(material).cubeTexture.Get();
    if (!cube) return;
    const int dimension = cube->GetDimension();
    if (dimension == 1) {
        if (material.flags2 & 2) display->SetTextureCoordGen(1, static_cast<DTexGenMode>(5), 3);
        return;
    }
    if (material.flags2 & 1) display->SetTextureCoordGen(1, static_cast<DTexGenMode>(3), 2);
}

}  // namespace U5a2
}  // namespace Jet
