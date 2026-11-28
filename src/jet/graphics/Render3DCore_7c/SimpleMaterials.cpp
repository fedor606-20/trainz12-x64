// Unit U5a of Render3DCore_7c: the material state machine of the simple material families. Transcribed from the TS12
// assembly (addresses per member). Objects are always constructed by the original module in this unit's lifetime, so
// the members here only read and write the class fields at their TS12 offsets and call the same RenderSystem_7c
// exports (Display setters, Material::InitInstance/Equals, Texture virtuals) the original calls.
//
// Implemented here: the type strings, the scalar accessors, IsLoaded/Activate/CacheAlphaHint, the texture getters,
// Equals/InitInstance, and the Bind/Unbind/SetDisplayState/ClearDisplayState state save-restore. Constructors,
// destructors, SetTexture/DuplicateTexture, CreateInstance, the geometry chunks and the render paths stay forwarded:
// they need the U4 caches, the U6 chunks or Texture copying, none of which are local yet.
#include <jet/Render3DCoreMaterials.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace Jet {

namespace {

// sNaN quieting of an x87 load/store pair (fld/fstp): only the quiet bit changes
float X87(float f)
{
    unsigned int u;
    std::memcpy(&u, &f, 4);
    if ((u & 0x7f800000u) == 0x7f800000u && (u & 0x007fffffu) && !(u & 0x00400000u)) u |= 0x00400000u;
    std::memcpy(&f, &u, 4);
    return f;
}

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

// a virtual slot of Display the project header does not name; the original calls it with this on the stack (cdecl)
template <class R, class... A>
R DisplaySlot(const Display* display, unsigned offset, A... args)
{
    using F = R(__cdecl*)(const Display*, A...);
    return (*reinterpret_cast<F* const*>(display))[offset / sizeof(void*)](display, args...);
}

// the original compares two texture references by their slot in the texture manager's 0x34-byte record block:
// (a - b) / 0x34 == 0. The integer division truncates toward zero exactly like the IMUL/SAR sequence.
bool SameTexture(const Texture* a, const Texture* b)
{
    const int delta = static_cast<int>(reinterpret_cast<std::intptr_t>(a) - reinterpret_cast<std::intptr_t>(b));
    return delta / 0x34 == 0;
}

void AddTexture(Texture* texture)
{
    if (texture) texture->AddReference();
}
void ReleaseTexture(Texture* texture)
{
    if (texture) texture->ReleaseReference();
}

// the codes the original stores in a ResultClass: 0 OK, 2 unsupported texture, 3 texture not found, 4 not loaded
Material::Result Code(int code)
{
    return Material::Result(code);
}

}  // namespace

// ---------------------------------------------------------------------------------------------------------------
// NoTextureMaterial

const PString& __cdecl NoTextureMaterial::GetType() const { return UNDECORATED_TYPE; }

Material::Result __cdecl NoTextureMaterial::DuplicateTexture(int)
{
    return Code(2);
}

void __cdecl NoTextureMaterial::Bind(Display* display, ViewPort*, unsigned int pass)
{
    if (pass == 0) SetDisplayState(display);
}

void __cdecl NoTextureMaterial::Unbind(Display* display, ViewPort*, unsigned int pass)
{
    if (pass == 0) ClearDisplayState(display);
}

void __cdecl NoTextureMaterial::Activate(int) {}

void __cdecl NoTextureMaterial::SetDefaultTextureStage(void* stage) { defaultTextureStage = stage; }

Texture* __cdecl NoTextureMaterial::GetTexture(int) { return nullptr; }
const Texture* __cdecl NoTextureMaterial::GetTexture(int) const { return nullptr; }

void __cdecl NoTextureMaterial::SetDisplayState(Display* display) const
{
    DisplaySlot<void>(display, 0xa0, 0);
    display->SetCullMode((flags & 1) ? DCULL_NONE : static_cast<DCull>(1));
    if (!(flags & 2)) {
        const unsigned char* state = display->State();
        Field<unsigned char>(const_cast<NoTextureMaterial*>(this), 0x1c) =
            static_cast<unsigned char>(state[0xa1] & 1);
        Field<unsigned int>(const_cast<NoTextureMaterial*>(this), 0x14) = Field<unsigned int>(state, 0x68);
        Field<unsigned int>(const_cast<NoTextureMaterial*>(this), 0x18) = Field<unsigned int>(state, 0x6c);
        display->SetAlphaBlendFunction(static_cast<Src::Factor>(srcBlend), static_cast<Dst::Factor>(dstBlend));
        display->SetAlphaBlendMode(true);
    }
    DisplaySlot<void>(display, 0x160, (flags >> 2) & 1);
    if (defaultTextureStage) DisplaySlot<void>(display, 0x128, defaultTextureStage);
}

void __cdecl NoTextureMaterial::ClearDisplayState(Display* display) const
{
    DisplaySlot<void>(display, 0xac, 1);
    display->SetCullMode(static_cast<DCull>(1));
    if (!(flags & 2)) {
        display->SetAlphaBlendMode(Field<unsigned char>(this, 0x1c) != 0);
        display->SetAlphaBlendFunction(static_cast<Src::Factor>(Field<unsigned int>(this, 0x14)),
                                       static_cast<Dst::Factor>(Field<unsigned int>(this, 0x18)));
    }
    if (defaultTextureStage) DisplaySlot<void>(display, 0x12c, 0);
    DisplaySlot<void>(display, 0x160, 0);
}

void __cdecl NoTextureMaterial::InitInstance(NoTextureMaterial&) const {}

bool __cdecl NoTextureMaterial::Equals(const NoTextureMaterial& other) const
{
    return Material::Equals(other);
}

// ---------------------------------------------------------------------------------------------------------------
// OneTextureMaterial

void __cdecl OneTextureMaterial::SetResourcePriority(float priority) const
{
    Field<float>(const_cast<OneTextureMaterial*>(this), 0x94) = priority;
    if (Texture* texture = const_cast<OneTextureMaterial*>(this)->texture.Get())
        texture->SetResourcePriority(priority);
}

const PString& __cdecl OneTextureMaterial::GetType() const { return UNDECORATED_TYPE; }

void __cdecl OneTextureMaterial::SetAlphaHint(TextureAlpha hint) { alphaHint = hint; }
TextureAlpha __cdecl OneTextureMaterial::GetAlphaHint() const { return alphaHint; }

void __cdecl OneTextureMaterial::SetDefaultTextureStage(void* stage) { defaultTextureStage = stage; }
void* __cdecl OneTextureMaterial::GetDefaultTextureStage() { return defaultTextureStage; }

void __cdecl OneTextureMaterial::Bind(Display* display, ViewPort*, unsigned int pass)
{
    if (pass == 0) SetDisplayState(display);
}

void __cdecl OneTextureMaterial::Unbind(Display* display, ViewPort*, unsigned int pass)
{
    if (pass == 0) ClearDisplayState(display);
}

Texture* __cdecl OneTextureMaterial::GetTexture(int type) { return type == 1 ? texture.Get() : nullptr; }
const Texture* __cdecl OneTextureMaterial::GetTexture(int type) const
{
    return type == 1 ? texture.Get() : nullptr;
}

void __cdecl OneTextureMaterial::Activate(int detail)
{
    if (Texture* texture = this->texture.Get()) texture->Activate(detail);
}

Material::Result __cdecl OneTextureMaterial::IsLoaded() const
{
    const Texture* texture = const_cast<OneTextureMaterial*>(this)->texture.Get();
    if (!texture) return Code(3);
    return texture->IsAvailable() ? Code(0) : Code(4);
}

void OneTextureMaterial::CacheAlphaHint() const
{
    if (alphaHint != static_cast<TextureAlpha>(-1)) return;
    if (IsLoaded().code != 0) return;
    Texture* texture = const_cast<OneTextureMaterial*>(this)->texture.Get();
    if (!texture) return;
    const_cast<OneTextureMaterial*>(this)->alphaHint = texture->GetAlphaHint();
}

void __cdecl OneTextureMaterial::SetDisplayState(Display* display) const
{
    display->SetCullMode((flags & 1) ? DCULL_NONE : static_cast<DCull>(1));
    Texture* texture = const_cast<OneTextureMaterial*>(this)->texture.Get();
    if (texture)
        texture->Bind(0);
    else
        DisplaySlot<void>(display, 0xa0, 0);
    if (!(flags & 2)) {
        const unsigned char* state = display->State();
        Field<unsigned char>(const_cast<OneTextureMaterial*>(this), 0x1c) =
            static_cast<unsigned char>(state[0xa1] & 1);
        Field<unsigned int>(const_cast<OneTextureMaterial*>(this), 0x14) = Field<unsigned int>(state, 0x68);
        Field<unsigned int>(const_cast<OneTextureMaterial*>(this), 0x18) = Field<unsigned int>(state, 0x6c);
        display->SetAlphaBlendFunction(static_cast<Src::Factor>(srcBlend), static_cast<Dst::Factor>(dstBlend));
        display->SetAlphaBlendMode(true);
    }
    if (defaultTextureStage) DisplaySlot<void>(display, 0x128, defaultTextureStage);
    DisplaySlot<void>(display, 0x160, (flags >> 2) & 1);
}

void __cdecl OneTextureMaterial::ClearDisplayState(Display* display) const
{
    display->SetCullMode(static_cast<DCull>(1));
    if (!(flags & 2)) {
        display->SetAlphaBlendMode(Field<unsigned char>(this, 0x1c) != 0);
        display->SetAlphaBlendFunction(static_cast<Src::Factor>(Field<unsigned int>(this, 0x14)),
                                       static_cast<Dst::Factor>(Field<unsigned int>(this, 0x18)));
    }
    if (defaultTextureStage) DisplaySlot<void>(display, 0x12c, 0);
    DisplaySlot<void>(display, 0x160, 0);
}

void __cdecl OneTextureMaterial::InitInstance(OneTextureMaterial& instance) const
{
    Material::InitInstance(instance);
    Texture* texture = const_cast<OneTextureMaterial*>(this)->texture.Get();
    AddTexture(texture);
    instance.texture = texture;  // AutoReference::operator=(T*) releases the old reference
}

bool __cdecl OneTextureMaterial::Equals(const OneTextureMaterial& other) const
{
    if (!SameTexture(const_cast<OneTextureMaterial*>(this)->texture.Get(),
                     const_cast<OneTextureMaterial&>(other).texture.Get()))
        return false;
    if (alphaHint != other.alphaHint) return false;
    return Material::Equals(other);
}

// ---------------------------------------------------------------------------------------------------------------
// OneTextureNoAlphaMaterial

void __cdecl OneTextureNoAlphaMaterial::SetResourcePriority(float priority) const
{
    Field<float>(const_cast<OneTextureNoAlphaMaterial*>(this), 0x94) = priority;
    if (Texture* texture = const_cast<OneTextureNoAlphaMaterial*>(this)->texture.Get())
        texture->SetResourcePriority(priority);
}

const PString& __cdecl OneTextureNoAlphaMaterial::GetType() const { return UNDECORATED_TYPE; }
void __cdecl OneTextureNoAlphaMaterial::SetAlphaHint(TextureAlpha hint) { alphaHint = hint; }
TextureAlpha __cdecl OneTextureNoAlphaMaterial::GetAlphaHint() const { return alphaHint; }
void __cdecl OneTextureNoAlphaMaterial::SetDefaultTextureStage(void* stage) { defaultTextureStage = stage; }
void* __cdecl OneTextureNoAlphaMaterial::GetDefaultTextureStage() { return defaultTextureStage; }

void __cdecl OneTextureNoAlphaMaterial::Bind(Display* display, ViewPort*, unsigned int pass)
{
    if (pass == 0) SetDisplayState(display);
}
void __cdecl OneTextureNoAlphaMaterial::Unbind(Display* display, ViewPort*, unsigned int pass)
{
    if (pass == 0) ClearDisplayState(display);
}

Texture* __cdecl OneTextureNoAlphaMaterial::GetTexture(int type) { return type == 1 ? texture.Get() : nullptr; }
const Texture* __cdecl OneTextureNoAlphaMaterial::GetTexture(int type) const
{
    return type == 1 ? texture.Get() : nullptr;
}

void __cdecl OneTextureNoAlphaMaterial::Activate(int detail)
{
    if (Texture* texture = this->texture.Get()) texture->Activate(detail);
}

Material::Result __cdecl OneTextureNoAlphaMaterial::IsLoaded() const
{
    const Texture* texture = const_cast<OneTextureNoAlphaMaterial*>(this)->texture.Get();
    if (!texture) return Code(3);
    return texture->IsAvailable() ? Code(0) : Code(4);
}

void OneTextureNoAlphaMaterial::CacheAlphaHint() const
{
    const_cast<OneTextureNoAlphaMaterial*>(this)->alphaHint = static_cast<TextureAlpha>(0);
}

void __cdecl OneTextureNoAlphaMaterial::SetDisplayState(Display* display) const
{
    display->SetCullMode((flags & 1) ? DCULL_NONE : static_cast<DCull>(1));
    Texture* texture = const_cast<OneTextureNoAlphaMaterial*>(this)->texture.Get();
    if (texture)
        texture->Bind(0);
    else
        DisplaySlot<void>(display, 0xa0, 0);
    const unsigned char* state = display->State();
    Field<unsigned char>(const_cast<OneTextureNoAlphaMaterial*>(this), 0x1c) =
        static_cast<unsigned char>(state[0xa1] & 1);
    display->SetAlphaBlendMode(false);
    if (defaultTextureStage) DisplaySlot<void>(display, 0x128, defaultTextureStage);
    DisplaySlot<void>(display, 0x160, (flags >> 2) & 1);
}

void __cdecl OneTextureNoAlphaMaterial::ClearDisplayState(Display* display) const
{
    display->SetCullMode(static_cast<DCull>(1));
    display->SetAlphaBlendMode(Field<unsigned char>(this, 0x1c) != 0);
    if (defaultTextureStage) DisplaySlot<void>(display, 0x12c, 0);
    DisplaySlot<void>(display, 0x160, 0);
}

void __cdecl OneTextureNoAlphaMaterial::InitInstance(OneTextureNoAlphaMaterial& instance) const
{
    Material::InitInstance(instance);
    Texture* texture = const_cast<OneTextureNoAlphaMaterial*>(this)->texture.Get();
    AddTexture(texture);
    instance.texture = texture;
}

bool __cdecl OneTextureNoAlphaMaterial::Equals(const OneTextureNoAlphaMaterial& other) const
{
    if (!SameTexture(const_cast<OneTextureNoAlphaMaterial*>(this)->texture.Get(),
                     const_cast<OneTextureNoAlphaMaterial&>(other).texture.Get()))
        return false;
    if (alphaHint != other.alphaHint) return false;
    return Material::Equals(other);
}

// ---------------------------------------------------------------------------------------------------------------
// BillboardMaterial and ColorOneTextureMaterial (both add no state of their own)

const PString& __cdecl BillboardMaterial::GetType() const { return UNDECORATED_TYPE; }

void __cdecl BillboardMaterial::InitInstance(BillboardMaterial& instance) const
{
    OneTextureMaterial::InitInstance(instance);
}

bool __cdecl BillboardMaterial::Equals(const BillboardMaterial& other) const
{
    return OneTextureMaterial::Equals(other);
}

const PString& __cdecl ColorOneTextureMaterial::GetType() const { return UNDECORATED_TYPE; }

void __cdecl ColorOneTextureMaterial::Bind(Display* display, ViewPort*, unsigned int pass)
{
    if (pass == 0) SetDisplayState(display);
}
void __cdecl ColorOneTextureMaterial::Unbind(Display* display, ViewPort*, unsigned int pass)
{
    if (pass == 0) ClearDisplayState(display);
}

void __cdecl ColorOneTextureMaterial::SetDisplayState(Display* display) const
{
    OneTextureMaterial::SetDisplayState(display);
    display->SetColorMaterialMode(static_cast<DColorMaterialTypes>(5));
}

void __cdecl ColorOneTextureMaterial::ClearDisplayState(Display* display) const
{
    OneTextureMaterial::ClearDisplayState(display);
    display->SetColorMaterialMode(static_cast<DColorMaterialTypes>(0));
}

void __cdecl ColorOneTextureMaterial::InitInstance(OneTextureMaterial& instance) const
{
    OneTextureMaterial::InitInstance(instance);
}

bool __cdecl ColorOneTextureMaterial::Equals(const ColorOneTextureMaterial& other) const
{
    return OneTextureMaterial::Equals(other);
}

// ---------------------------------------------------------------------------------------------------------------
// AutoTexMaterial

const PString& __cdecl AutoTexMaterial::GetType() const { return UNDECORATED_TYPE; }

void __cdecl AutoTexMaterial::Bind(Display* display, ViewPort*, unsigned int pass)
{
    if (pass == 0) SetDisplayState(display);
}
void __cdecl AutoTexMaterial::Unbind(Display* display, ViewPort*, unsigned int pass)
{
    if (pass == 0) ClearDisplayState(display);
}

void __cdecl AutoTexMaterial::SetTextureGenerationParameters(float a, float b, float c, float d)
{
    textureGeneration[0] = a;
    textureGeneration[1] = b;
    textureGeneration[2] = c;
    textureGeneration[3] = d;
}

void __cdecl AutoTexMaterial::GetTextureGenerationParameters(float& a, float& b, float& c, float& d) const
{
    a = X87(textureGeneration[0]);
    b = X87(textureGeneration[1]);
    c = X87(textureGeneration[2]);
    d = X87(textureGeneration[3]);
}

void __cdecl AutoTexMaterial::ClearDisplayState(Display* display) const
{
    display->SetTextureCoordGen(0, static_cast<DTexGenMode>(0), 2);
    DisplaySlot<void>(display, 0x78, 0);
    OneTextureMaterial::ClearDisplayState(display);
}

void __cdecl AutoTexMaterial::InitInstance(AutoTexMaterial& instance) const
{
    OneTextureMaterial::InitInstance(instance);
}

bool __cdecl AutoTexMaterial::Equals(const AutoTexMaterial& other) const
{
    return OneTextureMaterial::Equals(other);
}

// ---------------------------------------------------------------------------------------------------------------
// MVHSelectMaterial

const PString& __cdecl MVHSelectMaterial::GetType() const { return UNDECORATED_TYPE; }

void __cdecl MVHSelectMaterial::SetStageColor(Color& color)
{
    stageColor.r = X87(color.r);
    stageColor.g = X87(color.g);
    stageColor.b = X87(color.b);
    stageColor.a = X87(color.a);
}

const Color& __cdecl MVHSelectMaterial::GetStageColor() const { return stageColor; }

void __cdecl MVHSelectMaterial::SetDisplayState(Display* display) const
{
    const Color color{stageColor.r, stageColor.g, stageColor.b, Field<float>(this, 0x80)};
    DisplaySlot<void>(display, 0x130, 0, &color);
    DisplaySlot<void>(display, 0x128, context);
}

void __cdecl MVHSelectMaterial::ClearDisplayState(Display* display) const
{
    DisplaySlot<void>(display, 0x12c, 0);
}

void __cdecl MVHSelectMaterial::InitInstance(MVHSelectMaterial& instance) const
{
    OneTextureMaterial::InitInstance(instance);
    instance.stageColor.r = X87(stageColor.r);
    instance.stageColor.g = X87(stageColor.g);
    instance.stageColor.b = X87(stageColor.b);
    instance.stageColor.a = X87(stageColor.a);
}

bool __cdecl MVHSelectMaterial::Equals(const MVHSelectMaterial& other) const
{
    return OneTextureMaterial::Equals(other);
}

// ---------------------------------------------------------------------------------------------------------------
// ReflectiveMaterial

void __cdecl ReflectiveMaterial::SetResourcePriority(float priority) const
{
    Field<float>(const_cast<ReflectiveMaterial*>(this), 0x94) = priority;
    auto* self = const_cast<ReflectiveMaterial*>(this);
    if (Texture* texture = self->texture.Get()) texture->SetResourcePriority(priority);
    if (Texture* cube = self->cubeTexture.Get()) cube->SetResourcePriority(priority);
}

const PString& __cdecl ReflectiveMaterial::GetType() const { return UNDECORATED_TYPE; }

Material::Result __cdecl ReflectiveMaterial::IsLoaded() const
{
    auto* self = const_cast<ReflectiveMaterial*>(this);
    if (Texture* cube = self->cubeTexture.Get())
        if (!cube->IsAvailable()) return Code(4);
    Texture* const texture = self->texture.Get();
    if (!texture) return Code(3);
    if (!texture->IsAvailable()) return Code(4);
    return Code(0);
}

void __cdecl ReflectiveMaterial::Unbind(Display* display, ViewPort*, unsigned int pass)
{
    if (pass == 0) ClearDisplayState(display);
}

Texture* __cdecl ReflectiveMaterial::GetTexture(int type)
{
    if (type == 9) return cubeTexture.Get();
    if (type == 1) return texture.Get();
    return nullptr;
}
const Texture* __cdecl ReflectiveMaterial::GetTexture(int type) const
{
    if (type == 9) return const_cast<ReflectiveMaterial*>(this)->cubeTexture.Get();
    if (type == 1) return const_cast<ReflectiveMaterial*>(this)->texture.Get();
    return nullptr;
}

void __cdecl ReflectiveMaterial::Activate(int detail)
{
    if (Texture* cube = cubeTexture.Get()) cube->Activate(detail);
    if (Texture* texture = this->texture.Get()) texture->Activate(detail);
}

void __cdecl ReflectiveMaterial::ClearDisplayState(Display* display) const
{
    for (unsigned int i = contextCount; i > 0; --i) DisplaySlot<void>(display, 0x12c, i - 1);
    Texture* cube = const_cast<ReflectiveMaterial*>(this)->cubeTexture.Get();
    if (cube && cube->IsAvailable()) {
        const bool second = cube->GetDimension() == static_cast<TextureDimension>(1);
        const bool set = second ? (flags2 & 2) != 0 : (flags2 & 1) != 0;
        if (set) display->SetTextureCoordGen(1, static_cast<DTexGenMode>(0), 2);
    }
    display->SetLocalViewSpecularMode(((flags2 >> 2) & 1) != 0);
    display->SetCullMode(static_cast<DCull>(1));
}

void __cdecl ReflectiveMaterial::SetCubeMapMatrix(const Matrix4& matrix)
{
    const float* source = reinterpret_cast<const float*>(&matrix);
    float* target = reinterpret_cast<float*>(cubeMatrix);
    for (unsigned i = 0; i < 16; ++i) target[i] = X87(source[i]);
}

bool __cdecl ReflectiveMaterial::Equals(const ReflectiveMaterial& other) const
{
    auto* self = const_cast<ReflectiveMaterial*>(this);
    auto& that = const_cast<ReflectiveMaterial&>(other);
    if (!SameTexture(self->cubeTexture.Get(), that.cubeTexture.Get())) return false;
    if (!(reflectivity == other.reflectivity)) return false;
    if (contextCount != other.contextCount) return false;
    for (unsigned int i = 0; i < contextCount; ++i)
        if (contextData[i] != other.contextData[i]) return false;
    if (!SameTexture(self->texture.Get(), that.texture.Get())) return false;
    if (alphaHint != other.alphaHint) return false;
    return Material::Equals(other);
}

// ---------------------------------------------------------------------------------------------------------------
// GlossMaterial

const PString& __cdecl GlossMaterial::GetType() const { return UNDECORATED_TYPE; }

void __cdecl GlossMaterial::Unbind(Display* display, ViewPort*, unsigned int pass)
{
    if (pass == 0) ClearDisplayState(display);
}

Texture* __cdecl GlossMaterial::GetTexture(int type)
{
    if (type == 9) return cubeTexture.Get();
    if (type == 1) return texture.Get();
    return nullptr;
}
const Texture* __cdecl GlossMaterial::GetTexture(int type) const
{
    if (type == 9) return const_cast<GlossMaterial*>(this)->cubeTexture.Get();
    if (type == 1) return const_cast<GlossMaterial*>(this)->texture.Get();
    return nullptr;
}

void __cdecl GlossMaterial::Activate(int detail)
{
    if (Texture* cube = cubeTexture.Get()) cube->Activate(detail);
    if (Texture* texture = this->texture.Get()) texture->Activate(detail);
}

void GlossMaterial::CacheAlphaHint() const
{
    const_cast<GlossMaterial*>(this)->alphaHint = static_cast<TextureAlpha>(0);
}

bool __cdecl GlossMaterial::Equals(const GlossMaterial& other) const
{
    return ReflectiveMaterial::Equals(other);
}

// ---------------------------------------------------------------------------------------------------------------
// GenericShaderMaterial (the scalar part; its texture ring belongs with the cache unit)

const PString& __cdecl GenericShaderMaterial::GetType() const { return UNDECORATED_TYPE; }

TextureAlpha __cdecl GenericShaderMaterial::GetAlphaHint() { return alphaHint; }
void __cdecl GenericShaderMaterial::SetAlphaHint(TextureAlpha hint) { alphaHint = hint; }

}  // namespace Jet



