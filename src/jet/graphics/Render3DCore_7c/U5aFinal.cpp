// Unit U5a (wave 5, final): the material exports that wave 4 left forwarded.
//
// The data exports are the compiler's own tables and objects now: every concrete class is declared completely in
// Render3DCoreMaterials.h, so this file's destructor and operator== definitions make the compiler emit the class's
// two vftables, its vbtable, its RTTI and the two static strings plus the UID. The constructors in
// MaterialLifetime.cpp/MaterialPlugins.cpp install those same local tables (looked up in this module by export name,
// which is also what GetProcAddress returns to the outside).
//
// The behaviour below follows the U5a assembly: the same member offsets, the same U4 cache APIs, the same RenderSystem
// calls and the same x87/SSE orders. The plugin factories are the wave-4 blocker made possible by U4's local cache
// constructors and cache IDs; the only thing they still take from Render3DCore_7c_orig.dll is the address of the
// texture-stage descriptor statics (0x100dcd60 etc.) that Display slot 0x11c builds a stage from - they are anonymous
// .bss objects built by the original's static init and are not exported.
#include <jet/Geometry.h>
#include <jet/Render3DCoreCamera.h>
#include <jet/Render3DCoreCaches.h>
#include <jet/Render3DCoreMaterials.h>
#include <jet/RenderSystem.h>
#include <jet/VCall.h>

#include <windows.h>

#include <cstddef>
#include <cstring>
#include <xmmintrin.h>

namespace Jet {

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

HMODULE OriginalModule()
{
    static HMODULE module = [] {
        HMODULE m = GetModuleHandleA("Render3DCore_7c_orig.dll");
        return m ? m : LoadLibraryA("Render3DCore_7c_orig.dll");
    }();
    return module;
}

HMODULE ThisModule()
{
    static HMODULE module = [] {
        HMODULE h = nullptr;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCSTR>(&OriginalModule), &h);
        return h;
    }();
    return module;
}

template <class F>
F Proc(HMODULE module, const char* name)
{
    return module ? reinterpret_cast<F>(GetProcAddress(module, name)) : nullptr;
}

// The original's static texture-stage descriptors, addressed by their RVA in Render3DCore_7c_orig.dll.
void* OriginalRva(unsigned int rva)
{
    HMODULE const module = OriginalModule();
    return module ? reinterpret_cast<void*>(reinterpret_cast<unsigned char*>(module) + (rva - 0x10000000u))
                  : nullptr;
}

void ReleaseCache(void* cache)
{
    if (!cache) return;
    void* const vbptr = Field<void*>(cache, 0xc);
    const int delta = *reinterpret_cast<int*>(static_cast<unsigned char*>(vbptr) + 4);
    reinterpret_cast<ReferenceCount*>(static_cast<unsigned char*>(cache) + 0xc + delta)->ReleaseReference();
}

void ReferencelessNoop() {}

// Display/Texture virtual slots the project headers do not name.
template <class R, class... A>
R Slot(const void* object, unsigned offset, A... args)
{
    using F = R(__cdecl*)(const void*, A...);
    return (*reinterpret_cast<F* const*>(object))[offset / sizeof(void*)](object, args...);
}

using ResultPtr = Material::Result*;

// The 0x14-byte ring blocks of GenericShaderMaterial: {next, prev, count, items[]} with {int type, Texture*} items.
void FreeTextureRing(void* headSlot)
{
    unsigned char* const head = static_cast<unsigned char*>(headSlot);
    void* const first = *reinterpret_cast<void**>(head);
    if (!first) return;
    *reinterpret_cast<void**>(head) = nullptr;
    unsigned char* block = static_cast<unsigned char*>(first);
    for (;;) {
        unsigned char* const next = *reinterpret_cast<unsigned char**>(block);
        const unsigned int count = *reinterpret_cast<unsigned int*>(block + 8);
        for (unsigned int i = 0; i < count; ++i) {
            Texture* const texture = *reinterpret_cast<Texture**>(block + 0x10 + i * 8);
            if (texture) texture->ReleaseReference();
        }
        Alloc::Delete(block);
        block = next;
        if (!block || block == first) break;
    }
}

// 0x1001ab30: out = a * b with the assembly's SSE operand order (the first product of every ADDSS survives a NaN).
void MultiplyMatrices(Matrix4& out, const Matrix4& a, const Matrix4& b)
{
    float result[16];
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            __m128 value = _mm_mul_ss(_mm_load_ss(&a.m[row * 4 + 1]), _mm_load_ss(&b.m[4 + column]));
            value = _mm_add_ss(value, _mm_mul_ss(_mm_load_ss(&a.m[row * 4 + 3]), _mm_load_ss(&b.m[12 + column])));
            value = _mm_add_ss(value, _mm_mul_ss(_mm_load_ss(&a.m[row * 4 + 2]), _mm_load_ss(&b.m[8 + column])));
            value = _mm_add_ss(value, _mm_mul_ss(_mm_load_ss(&a.m[row * 4 + 0]), _mm_load_ss(&b.m[column])));
            _mm_store_ss(&result[row * 4 + column], value);
        }
    }
    std::memcpy(out.m, result, sizeof result);
}

// 0x1001aaf0: out = cubeMatrix * viewTransform.
void MultiplyCubeMatrix(Matrix4& out, const Matrix4& cubeMatrix, const Matrix4& viewTransform)
{
    Matrix4 copy = viewTransform;
    MultiplyMatrices(out, cubeMatrix, copy);
}

// The camera's cell-space view transform (0x10009f80): a 4x3 transform the original transposes into a Matrix4 with
// the last row (0,0,0,1) before the cube multiply.
const Matrix4* CellSpaceViewTransform(Camera* camera)
{
    using F = const Matrix4*(__cdecl*)(const Camera*);
    static F fn = Proc<F>(OriginalModule(), "?GetCellSpaceViewTransform@Camera@Jet@@QBAABVMatrix4@2@XZ");
    if (!fn) return nullptr;
    return fn(camera);
}

void BuildCellSpaceMatrix(Matrix4& matrix, const float* source)
{
    std::memset(&matrix, 0, sizeof matrix);
    matrix.m[0] = source[0];
    matrix.m[1] = source[4];
    matrix.m[2] = source[8];
    matrix.m[4] = source[1];
    matrix.m[5] = source[5];
    matrix.m[6] = source[9];
    matrix.m[8] = source[2];
    matrix.m[9] = source[6];
    matrix.m[10] = source[10];
    matrix.m[12] = source[3];
    matrix.m[13] = source[7];
    matrix.m[14] = source[11];
    matrix.m[15] = 1.0f;
}

}  // namespace

// ---------------------------------------------------------------------------------------------------------------
// Static data. The strings and GUIDs are the ones the original's module initialisers pass to PStringCache::Create
// and UId::UId (0x1007a000..), so every PString points at the same interned node the original's static would.

const PString NoTextureMaterial::DECORATED_TYPE("m.notex");
const PString NoTextureMaterial::UNDECORATED_TYPE("notex");
const UId NoTextureMaterial::UID("F9DE4A98-FDF1-459b-BBD4-F4BD2FD034EB");

const PString OneTextureMaterial::DECORATED_TYPE("m.onetex");
const PString OneTextureMaterial::UNDECORATED_TYPE("onetex");
const UId OneTextureMaterial::UID("45DBFE90-6DE6-46dd-B37F-F3F0A471E068");

const PString OneTextureNoAlphaMaterial::DECORATED_TYPE("m.onetexnoalpha");
const PString OneTextureNoAlphaMaterial::UNDECORATED_TYPE("onetexnoalpha");
const UId OneTextureNoAlphaMaterial::UID("45DBFE90-6DE6-46dd-B37F-F3F0A471E178");

const PString BillboardMaterial::DECORATED_TYPE("m.billboard");
const PString BillboardMaterial::UNDECORATED_TYPE("billboard");
const UId BillboardMaterial::UID("822E6580-52B3-11d5-9AEF-0001025364B8");

const PString ColorOneTextureMaterial::DECORATED_TYPE("m.coloronetex");
const PString ColorOneTextureMaterial::UNDECORATED_TYPE("coloronetex");
const UId ColorOneTextureMaterial::UID("5831FE90-AFE6-7DEE-B37F-1561A476E049");

const PString AutoTexMaterial::DECORATED_TYPE("m.autotex");
const PString AutoTexMaterial::UNDECORATED_TYPE("autotex");
const UId AutoTexMaterial::UID("F6178437-1CFB-4212-82C2-912C37D23D20");

const PString MVHSelectMaterial::DECORATED_TYPE("m.mvhselect");
const PString MVHSelectMaterial::UNDECORATED_TYPE("mvhselect");
const UId MVHSelectMaterial::UID("A5DB7890-34E6-46EE-AA7F-13F9A471E065");

const PString ReflectiveMaterial::DECORATED_TYPE("m.reflect");
const PString ReflectiveMaterial::UNDECORATED_TYPE("reflect");
const UId ReflectiveMaterial::UID("F1AA4E1B-761A-40dd-89E4-B2B13F9B679B");

const PString GlossMaterial::DECORATED_TYPE("m.gloss");
const PString GlossMaterial::UNDECORATED_TYPE("gloss");
const UId GlossMaterial::UID("2F5C2951-4808-493b-ACF5-2E9D26963B05");

const PString GenericShaderMaterial::DECORATED_TYPE("m.shader");
const PString GenericShaderMaterial::UNDECORATED_TYPE("shader");
const UId GenericShaderMaterial::UID("54FB9F73-172E-4d01-852E-20D3D96D93C7");

// ---------------------------------------------------------------------------------------------------------------
// operator==: the exported bodies live in MaterialOps.cpp; the members exist so the compiler emits the class vtable
// with a real address in slot 1 (the .def still exports the free function under the original name).

namespace U5a2 {
bool eq_NoTexture(const void*, const Material*);
bool eq_OneTexture(const void*, const Material*);
bool eq_OneTextureNoAlpha(const void*, const Material*);
bool eq_Billboard(const void*, const Material*);
bool eq_ColorOneTexture(const void*, const Material*);
bool eq_AutoTex(const void*, const Material*);
bool eq_MVHSelect(const void*, const Material*);
bool eq_Reflective(const void*, const Material*);
bool eq_Gloss(const void*, const Material*);
bool eq_GenericShader(const void*, const Material*);
void OneTexture_set_texture_db(void*, Material::Result*, int, const ResourceDatabase*, const PString*, float,
                               const void*);
void OneTexture_set_texture_ref(void*, Material::Result*, int, const AutoReference<Texture>*, float);
void OneTextureNoAlpha_set_texture_db(void*, Material::Result*, int, const ResourceDatabase*, const PString*, float,
                                      const void*);
void OneTextureNoAlpha_set_texture_ref(void*, Material::Result*, int, const AutoReference<Texture>*, float);
void OneTexture_duplicate_texture(void*, Material::Result*, int);
void Reflective_set_texture_db(void*, Material::Result*, int, const ResourceDatabase*, const PString*, float,
                               const void*);
void Reflective_set_texture_ref(void*, Material::Result*, int, const AutoReference<Texture>*, float);
void Reflective_duplicate_texture(void*, Material::Result*, int);
void GenericShader_duplicate_texture(void*, Material::Result*, int);
Texture* GenericShader_get_texture(void*, int);
void GenericShader_activate(void*, int);
void GenericShader_set_resource_priority(void*, float);
void GenericShader_set_display_state(const void*, Display*);
void GenericShader_clear_display_state(const void*, Display*);
void GenericShader_bind(void*, Display*, ViewPort*, unsigned int);
void GenericShader_unbind(void*, Display*, ViewPort*, unsigned int);
bool supported_AutoTex(const void*, Display*, PString*);
void InitOneTextureFields(void*, MaterialManager*);
unsigned int Reflective_get_num_passes(const void*);
void Reflective_change_tex_gen(const void*, Display*, bool);
Material* create_NoTexture(const void*);
Material* create_OneTexture(const void*);
Material* create_OneTextureNoAlpha(const void*);
Material* create_Billboard(const void*);
Material* create_ColorOneTexture(const void*);
Material* create_AutoTex(const void*);
Material* create_MVHSelect(const void*);
Material* create_Reflective(const void*);
Material* create_Gloss(const void*);
Material* create_GenericShader(const void*);
}  // namespace U5a2

Material* __cdecl NoTextureMaterial::CreateInstance() const
{
    return U5a2::create_NoTexture(this);
}
Material* __cdecl OneTextureMaterial::CreateInstance() const
{
    return U5a2::create_OneTexture(this);
}
Material* __cdecl OneTextureNoAlphaMaterial::CreateInstance() const
{
    return U5a2::create_OneTextureNoAlpha(this);
}
Material* __cdecl BillboardMaterial::CreateInstance() const
{
    return U5a2::create_Billboard(this);
}
Material* __cdecl ColorOneTextureMaterial::CreateInstance() const
{
    return U5a2::create_ColorOneTexture(this);
}
Material* __cdecl AutoTexMaterial::CreateInstance() const
{
    return U5a2::create_AutoTex(this);
}
Material* __cdecl MVHSelectMaterial::CreateInstance() const
{
    return U5a2::create_MVHSelect(this);
}
Material* __cdecl ReflectiveMaterial::CreateInstance() const
{
    return U5a2::create_Reflective(this);
}
Material* __cdecl GlossMaterial::CreateInstance() const
{
    return U5a2::create_Gloss(this);
}
Material* __cdecl GenericShaderMaterial::CreateInstance() const
{
    return U5a2::create_GenericShader(this);
}

bool __cdecl NoTextureMaterial::operator==(const Material& other) const
{
    return U5a2::eq_NoTexture(this, &other);
}
bool __cdecl OneTextureMaterial::operator==(const Material& other) const
{
    return U5a2::eq_OneTexture(this, &other);
}
bool __cdecl OneTextureNoAlphaMaterial::operator==(const Material& other) const
{
    return U5a2::eq_OneTextureNoAlpha(this, &other);
}
bool __cdecl BillboardMaterial::operator==(const Material& other) const
{
    return U5a2::eq_Billboard(this, &other);
}
bool __cdecl ColorOneTextureMaterial::operator==(const Material& other) const
{
    return U5a2::eq_ColorOneTexture(this, &other);
}
bool __cdecl AutoTexMaterial::operator==(const Material& other) const
{
    return U5a2::eq_AutoTex(this, &other);
}
bool __cdecl MVHSelectMaterial::operator==(const Material& other) const
{
    return U5a2::eq_MVHSelect(this, &other);
}
bool __cdecl ReflectiveMaterial::operator==(const Material& other) const
{
    return U5a2::eq_Reflective(this, &other);
}
bool __cdecl GlossMaterial::operator==(const Material& other) const
{
    return U5a2::eq_Gloss(this, &other);
}
bool __cdecl GenericShaderMaterial::operator==(const Material& other) const
{
    return U5a2::eq_GenericShader(this, &other);
}

// The texture leaves the class vtable needs (the .def exports the MaterialOps free functions).
Material::Result __cdecl OneTextureMaterial::DuplicateTexture(int type)
{
    Material::Result result(0);
    U5a2::OneTexture_duplicate_texture(this, &result, type);
    return result;
}
Material::Result __cdecl OneTextureMaterial::SetTexture(int type, const ResourceDatabase* db, const PString& name,
                                                       float amount, const String& request)
{
    Material::Result result(0);
    U5a2::OneTexture_set_texture_db(this, &result, type, db, &name, amount, &request);
    return result;
}
Material::Result __cdecl OneTextureMaterial::SetTexture(int type, const AutoReference<Texture>& texture, float amount)
{
    Material::Result result(0);
    U5a2::OneTexture_set_texture_ref(this, &result, type, &texture, amount);
    return result;
}
Material::Result __cdecl ReflectiveMaterial::DuplicateTexture(int type)
{
    Material::Result result(0);
    U5a2::Reflective_duplicate_texture(this, &result, type);
    return result;
}
Material::Result __cdecl ReflectiveMaterial::SetTexture(int type, const ResourceDatabase* db, const PString& name,
                                                       float amount, const String& request)
{
    Material::Result result(0);
    U5a2::Reflective_set_texture_db(this, &result, type, db, &name, amount, &request);
    return result;
}
Material::Result __cdecl ReflectiveMaterial::SetTexture(int type, const AutoReference<Texture>& texture, float amount)
{
    Material::Result result(0);
    U5a2::Reflective_set_texture_ref(this, &result, type, &texture, amount);
    return result;
}
Material::Result __cdecl GenericShaderMaterial::DuplicateTexture(int type)
{
    Material::Result result(0);
    U5a2::GenericShader_duplicate_texture(this, &result, type);
    return result;
}
void __cdecl GenericShaderMaterial::Activate(int detail)
{
    U5a2::GenericShader_activate(this, detail);
}
Texture* __cdecl GenericShaderMaterial::GetTexture(int type)
{
    return U5a2::GenericShader_get_texture(this, type);
}
const Texture* __cdecl GenericShaderMaterial::GetTexture(int type) const
{
    return U5a2::GenericShader_get_texture(const_cast<GenericShaderMaterial*>(this), type);
}
unsigned int __cdecl ReflectiveMaterial::GetNumPasses() const
{
    return U5a2::Reflective_get_num_passes(this);
}

// The five GenericShader virtuals whose exports were the U5a2 free functions. Defining the members makes the
// class vtable's slots local; the exported names in implemented.d now bind to these same members.
void __cdecl GenericShaderMaterial::Bind(Display* display, ViewPort* view, unsigned int pass)
{
    U5a2::GenericShader_bind(this, display, view, pass);
}
void __cdecl GenericShaderMaterial::Unbind(Display* display, ViewPort* view, unsigned int pass)
{
    U5a2::GenericShader_unbind(this, display, view, pass);
}
void __cdecl GenericShaderMaterial::SetResourcePriority(float priority) const
{
    U5a2::GenericShader_set_resource_priority(const_cast<GenericShaderMaterial*>(this), priority);
}
void __cdecl GenericShaderMaterial::SetDisplayState(Display* display) const
{
    U5a2::GenericShader_set_display_state(this, display);
}
void __cdecl GenericShaderMaterial::ClearDisplayState(Display* display) const
{
    U5a2::GenericShader_clear_display_state(this, display);
}

// Private constructors that exist only so the compiler emits each class's vbtable in this module (the exported
// constructors are the hand-written thunks in MaterialLifetime.cpp; they never call these).
NoTextureMaterial::NoTextureMaterial(MaterialManager* manager) : Material(manager) {}
OneTextureMaterial::OneTextureMaterial(MaterialManager* manager) : Material(manager) {}
OneTextureNoAlphaMaterial::OneTextureNoAlphaMaterial(MaterialManager* manager) : OneTextureMaterial(manager) {}
BillboardMaterial::BillboardMaterial(MaterialManager* manager) : OneTextureMaterial(manager) {}
ColorOneTextureMaterial::ColorOneTextureMaterial(MaterialManager* manager) : OneTextureMaterial(manager) {}
AutoTexMaterial::AutoTexMaterial(MaterialManager* manager) : OneTextureMaterial(manager) {}

// 0xc5d0: the five-argument constructor StdMaterialPluginTemplate<AutoTexMaterial> calls. The three cache
// arguments are unused by the original (OneTextureMaterial's constructor ignores them); the shared texture and the
// generation parameters are what the body writes.
AutoTexMaterial::AutoTexMaterial(MaterialManager* manager, MaterialChunkCache*, AlphaMaskCache*, AlphaBlendCache*)
    : OneTextureMaterial(manager)
{
    U5a2::InitOneTextureFields(this, manager);
    textureGeneration[0] = 1.0f;
    textureGeneration[1] = 1.0f;
    textureGeneration[2] = 0.0f;
    textureGeneration[3] = 0.0f;
}
MVHSelectMaterial::MVHSelectMaterial(MaterialManager* manager) : OneTextureMaterial(manager) {}
ReflectiveMaterial::ReflectiveMaterial(MaterialManager* manager) : OneTextureMaterial(manager) {}
GlossMaterial::GlossMaterial(MaterialManager* manager) : ReflectiveMaterial(manager) {}
GenericShaderMaterial::GenericShaderMaterial(MaterialManager* manager) : Material(manager) {}

// The vbtable of every material class is a COMDAT emitted only where the class is constructed. These classes are
// built by the hand-written thunks, so this never-called function is the ODR use that makes the compiler emit them.
namespace {
volatile int g_emitMaterialTables = 0;
alignas(16) unsigned char g_emitBuffer[0x120];
}  // namespace

void EmitMaterialTables()
{
    if (!g_emitMaterialTables) return;
    MaterialManager* const manager = nullptr;
    ::new (g_emitBuffer) NoTextureMaterial(manager);
    ::new (g_emitBuffer) OneTextureMaterial(manager);
    ::new (g_emitBuffer) OneTextureNoAlphaMaterial(manager);
    ::new (g_emitBuffer) BillboardMaterial(manager);
    ::new (g_emitBuffer) ColorOneTextureMaterial(manager);
    ::new (g_emitBuffer) AutoTexMaterial(manager);
    ::new (g_emitBuffer) MVHSelectMaterial(manager);
    ::new (g_emitBuffer) ReflectiveMaterial(manager);
    ::new (g_emitBuffer) GlossMaterial(manager);
    ::new (g_emitBuffer) GenericShaderMaterial(manager);
}

// ---------------------------------------------------------------------------------------------------------------
// The material destructors. The compiler writes the two vftables, runs the member destructors (the AutoReference
// texture and cube map, exactly the ReleaseReference calls of the original) and chains into the imported
// Material::~Material. Only GenericShaderMaterial's ring and the two extra cache references need explicit code.

NoTextureMaterial::~NoTextureMaterial() = default;
OneTextureMaterial::~OneTextureMaterial() = default;
OneTextureNoAlphaMaterial::~OneTextureNoAlphaMaterial() = default;
BillboardMaterial::~BillboardMaterial() = default;
ColorOneTextureMaterial::~ColorOneTextureMaterial() = default;
AutoTexMaterial::~AutoTexMaterial() = default;
MVHSelectMaterial::~MVHSelectMaterial() = default;
ReflectiveMaterial::~ReflectiveMaterial() = default;
GlossMaterial::~GlossMaterial() = default;

GenericShaderMaterial::~GenericShaderMaterial()
{
    // 0x10026730: the two cache references, the two extra cache-like references at +0xa8/+0xac, the texture ring,
    // then Material::~Material (the compiler appends that call).
    ReleaseCache(chunkCache);
    ReleaseCache(alphaBlendCache);
    ReleaseCache(reinterpret_cast<void*>(static_cast<std::uintptr_t>(ringA)));
    ReleaseCache(reinterpret_cast<void*>(static_cast<std::uintptr_t>(ringB)));
    FreeTextureRing(&textureRing);
}

// ---------------------------------------------------------------------------------------------------------------
// Plugin destructors: release the cache references, free the display's texture-stage nodes, then the imported
// MaterialPlugin destructor (the compiler appends that base call).

namespace {

void FreePluginStage(void* plugin, unsigned stageOffset, unsigned displayOffset)
{
    void* const stage = Field<void*>(plugin, stageOffset);
    void* const display = Field<void*>(plugin, displayOffset);
    if (!stage || !display) return;
    Slot<void>(display, 0x124, stage);
    Field<void*>(plugin, stageOffset) = nullptr;
}

void FreePluginCaches2(void* plugin)  // {+0x20, +0x24}
{
    ReleaseCache(Field<void*>(plugin, 0x20));
    ReleaseCache(Field<void*>(plugin, 0x24));
}

void FreePluginCaches3(void* plugin)  // {+0x20, +0x24, +0x28}
{
    ReleaseCache(Field<void*>(plugin, 0x20));
    ReleaseCache(Field<void*>(plugin, 0x24));
    ReleaseCache(Field<void*>(plugin, 0x28));
}

}  // namespace

NoTextureMaterialPlugin::~NoTextureMaterialPlugin()
{
    FreePluginCaches2(this);
    FreePluginStage(this, 0x28, 0x2c);
}

OneTextureMaterialPlugin::~OneTextureMaterialPlugin()
{
    FreePluginCaches3(this);
    FreePluginStage(this, 0x2c, 0x30);
}

OneTextureNoAlphaMaterialPlugin::~OneTextureNoAlphaMaterialPlugin() = default;
ColorOneTextureMaterialPlugin::~ColorOneTextureMaterialPlugin() = default;

GenericShaderMaterialPlugin::~GenericShaderMaterialPlugin()
{
    FreePluginCaches2(this);
}

ReflectiveMaterialPlugin::~ReflectiveMaterialPlugin()
{
    void* const display = Field<void*>(this, 0x58);
    const unsigned int count = Field<unsigned int>(this, 0x34);
    if (display && count) {
        for (unsigned int i = 0; i < count; ++i) Slot<void>(display, 0x124, Field<void*>(this, 0x38 + i * 4));
    }
}

GlossMaterialPlugin::~GlossMaterialPlugin() = default;

MVHSelectMaterialPlugin::~MVHSelectMaterialPlugin()
{
    void* const display = Field<void*>(this, 0x58);
    const unsigned int count = Field<unsigned int>(this, 0x34);
    if (display && count) {
        for (unsigned int i = 0; i < count; ++i) Slot<void>(display, 0x124, Field<void*>(this, 0x38 + i * 4));
    }
}

StdMaterialPlugin::~StdMaterialPlugin()
{
    FreePluginCaches3(this);
}

MaterialPluginX::~MaterialPluginX() = default;

bool __cdecl MaterialPluginX::IsSupported(Display*, PString&) { return false; }
Material* __cdecl MaterialPluginX::CreateInstance(MaterialManager*, RenderKernel*) const { return nullptr; }
bool __cdecl StdMaterialPlugin::IsSupported(Display*, PString&) { return true; }
Material* __cdecl StdMaterialPlugin::CreateInstance(MaterialManager*, RenderKernel*) const { return nullptr; }

// 0x71120 / 0xd6a0: the two StdMaterialPlugin constructors. The original's +0x20..+0x28 cache pointers are the
// derived classes' fields (StdMaterialPluginTemplate writes those in its own initialiser list), so the base bodies
// only run the RenderSystem_7c MaterialPlugin base and let the compiler install the vtable.
StdMaterialPlugin::StdMaterialPlugin(const PString& name, const UId& id) : MaterialPlugin(name, id) {}
StdMaterialPlugin::StdMaterialPlugin(const StdMaterialPlugin& o) : MaterialPlugin(o) {}

bool __cdecl AutoTexMaterialPlugin::IsSupported(Display* display, PString& reason)
{
    return U5a2::supported_AutoTex(this, display, &reason);
}

// ---------------------------------------------------------------------------------------------------------------
// Plugin factories and resource construction.

namespace {

template <class Cache>
Cache* Construct(void* memory) { return ::new (memory) Cache(); }

void AddCacheReference(void* cache)
{
    if (!cache) return;
    void* const vbptr = Field<void*>(cache, 0xc);
    const int delta = *reinterpret_cast<int*>(static_cast<unsigned char*>(vbptr) + 4);
    reinterpret_cast<ReferenceCount*>(static_cast<unsigned char*>(cache) + 0xc + delta)->AddReference();
}

// One plugin cache slot: reuse the kernel manager's cache, else build a new one and register it.
template <class Cache>
void* EnsureCache(void* self, void* kernel, unsigned fieldOffset, const PString& id)
{
    void*& slot = Field<void*>(self, fieldOffset);
    if (slot) return slot;
    RenderingCacheManager& manager = Field<RenderingCacheManager>(kernel, 0x6c);
    RenderingCache* const found = manager.FindCache(id);
    if (found) {
        AddCacheReference(found);
        slot = found;
        return slot;
    }
    void* const memory = Alloc::New(sizeof(Cache));
    Cache* const cache = memory ? Construct<Cache>(memory) : nullptr;
    slot = cache;
    if (cache) manager.AddCache(cache);
    return slot;
}

// The display's texture-stage node from the original's two static descriptors (Display slot 0x11c).
void* BuildStage(Display* display, unsigned int firstRva, unsigned int secondRva)
{
    if (!display) return nullptr;
    return Slot<void*>(display, 0x11c, 0, OriginalRva(firstRva), OriginalRva(secondRva));
}

void AssignPlugin(void* material, void* plugin)
{
    using F = void(__cdecl*)(void*, const void*);
    static F fn = Proc<F>(GetModuleHandleA("Kernel_7c.dll"), "?Assign@PluginInstance@Jet@@AAAXPBVPluginFactory@2@@Z");
    if (!fn) return;
    fn(material ? static_cast<unsigned char*>(material) + 4 : nullptr, plugin);
}

using MaterialCtor3 = void*(__thiscall*)(void*, MaterialManager*, MaterialChunkCache*, AlphaBlendCache*, bool);
using MaterialCtor4 = void*(__thiscall*)(void*, MaterialManager*, MaterialChunkCache*, AlphaMaskCache*,
                                         AlphaBlendCache*, bool);
using MaterialCtorMVH = void*(__thiscall*)(void*, MaterialManager*, MaterialChunkCache*, AlphaMaskCache*,
                                           AlphaBlendCache*, void*, bool);
using MaterialCtorReflective = void*(__thiscall*)(void*, MaterialManager*, MaterialChunkCache*, AlphaBlendCache*,
                                                  unsigned int, void**, bool);

template <class F>
F LocalProc(const char* name) { return Proc<F>(ThisModule(), name); }

}  // namespace

// NoTexture: caches {+0x20 blend, +0x24 chunk} and one stage {+0x28 stage, +0x2c display}.
Material* __cdecl NoTextureMaterialPlugin::CreateInstance(MaterialManager* manager, RenderKernel* kernel) const
{
    void* const self = const_cast<NoTextureMaterialPlugin*>(this);
    EnsureCache<AlphaBlendCache>(self, kernel, 0x20, AlphaBlendCache::AlphaBlendCacheID);
    EnsureCache<MaterialChunkCache>(self, kernel, 0x24, MaterialChunkCache::MaterialChunkCacheID);
    void*& stage = Field<void*>(self, 0x28);
    if (!stage) {
        Display* const display = Field<Display*>(kernel, 0x7c);
        Field<void*>(self, 0x2c) = static_cast<void*>(display);
        stage = BuildStage(display, 0xDCD60, 0xDCDA8);
    }
    static MaterialCtor3 ctor = LocalProc<MaterialCtor3>(
        "??0NoTextureMaterial@Jet@@QAE@PAVMaterialManager@1@PAVMaterialChunkCache@1@PAVAlphaBlendCache@1@@Z");
    void* const memory = Alloc::New(0xac);
    if (!memory) return nullptr;
    void* const material = ctor(memory, manager, static_cast<MaterialChunkCache*>(Field<void*>(self, 0x24)),
                                static_cast<AlphaBlendCache*>(Field<void*>(self, 0x20)), true);
    Field<void*>(material, 0x9c) = Field<void*>(self, 0x28);
    AssignPlugin(material, self);
    return static_cast<Material*>(material);
}

void __cdecl NoTextureMaterialPlugin::CreatePluginResources(RenderKernel*) const {}
bool __cdecl NoTextureMaterialPlugin::IsSupported(Display*, PString&) { return true; }

// OneTexture family: caches {+0x20 chunk, +0x24 mask, +0x28 blend} and one stage {+0x2c stage, +0x30 display}.
void __cdecl OneTextureMaterialPlugin::CreatePluginResources(RenderKernel* kernel) const
{
    void* const self = const_cast<OneTextureMaterialPlugin*>(this);
    EnsureCache<MaterialChunkCache>(self, kernel, 0x20, MaterialChunkCache::MaterialChunkCacheID);
    EnsureCache<AlphaMaskCache>(self, kernel, 0x24, AlphaMaskCache::AlphaMaskCacheID);
    EnsureCache<AlphaBlendCache>(self, kernel, 0x28, AlphaBlendCache::AlphaBlendCacheID);
    void*& stage = Field<void*>(self, 0x2c);
    if (!stage) {
        Display* const display = Field<Display*>(kernel, 0x7c);
        Field<void*>(self, 0x30) = static_cast<void*>(display);
        stage = BuildStage(display, 0xDCE08, 0xDCE50);
    }
}

Material* __cdecl OneTextureMaterialPlugin::CreateInstance(MaterialManager* manager, RenderKernel* kernel) const
{
    void* const self = const_cast<OneTextureMaterialPlugin*>(this);
    CreatePluginResources(kernel);
    static MaterialCtor4 ctor = LocalProc<MaterialCtor4>(
        "??0OneTextureMaterial@Jet@@QAE@PAVMaterialManager@1@PAVMaterialChunkCache@1@PAVAlphaMaskCache@1@PAV"
        "AlphaBlendCache@1@@Z");
    void* const memory = Alloc::New(0xb4);
    if (!memory) return nullptr;
    void* const material = ctor(memory, manager, static_cast<MaterialChunkCache*>(Field<void*>(self, 0x20)),
                                static_cast<AlphaMaskCache*>(Field<void*>(self, 0x24)),
                                static_cast<AlphaBlendCache*>(Field<void*>(self, 0x28)), true);
    Field<void*>(material, 0xa4) = Field<void*>(self, 0x2c);
    AssignPlugin(material, self);
    return static_cast<Material*>(material);
}

bool __cdecl OneTextureMaterialPlugin::IsSupported(Display*, PString&) { return true; }

void __cdecl OneTextureNoAlphaMaterialPlugin::CreatePluginResources(RenderKernel* kernel) const
{
    void* const self = const_cast<OneTextureNoAlphaMaterialPlugin*>(this);
    EnsureCache<MaterialChunkCache>(self, kernel, 0x20, MaterialChunkCache::MaterialChunkCacheID);
    EnsureCache<AlphaMaskCache>(self, kernel, 0x24, AlphaMaskCache::AlphaMaskCacheID);
    EnsureCache<AlphaBlendCache>(self, kernel, 0x28, AlphaBlendCache::AlphaBlendCacheID);
    void*& stage = Field<void*>(self, 0x2c);
    if (!stage) {
        Display* const display = Field<Display*>(kernel, 0x7c);
        Field<void*>(self, 0x30) = static_cast<void*>(display);
        stage = BuildStage(display, 0xDCEB0, 0xDCEF8);
    }
}

Material* __cdecl OneTextureNoAlphaMaterialPlugin::CreateInstance(MaterialManager* manager,
                                                                 RenderKernel* kernel) const
{
    void* const self = const_cast<OneTextureNoAlphaMaterialPlugin*>(this);
    CreatePluginResources(kernel);
    static MaterialCtor4 ctor = LocalProc<MaterialCtor4>(
        "??0OneTextureNoAlphaMaterial@Jet@@QAE@PAVMaterialManager@1@PAVMaterialChunkCache@1@PAVAlphaMaskCache@1@"
        "PAVAlphaBlendCache@1@@Z");
    void* const memory = Alloc::New(0xb4);
    if (!memory) return nullptr;
    void* const material = ctor(memory, manager, static_cast<MaterialChunkCache*>(Field<void*>(self, 0x20)),
                                static_cast<AlphaMaskCache*>(Field<void*>(self, 0x24)),
                                static_cast<AlphaBlendCache*>(Field<void*>(self, 0x28)), true);
    Field<void*>(material, 0xa4) = Field<void*>(self, 0x2c);
    AssignPlugin(material, self);
    return static_cast<Material*>(material);
}

bool __cdecl OneTextureNoAlphaMaterialPlugin::IsSupported(Display*, PString&) { return true; }

Material* __cdecl ColorOneTextureMaterialPlugin::CreateInstance(MaterialManager* manager, RenderKernel* kernel) const
{
    void* const self = const_cast<ColorOneTextureMaterialPlugin*>(this);
    OneTextureMaterialPlugin::CreatePluginResources(kernel);
    static MaterialCtor3 ctor = LocalProc<MaterialCtor3>(
        "??0ColorOneTextureMaterial@Jet@@QAE@PAVMaterialManager@1@PAVMaterialChunkCache@1@PAVAlphaBlendCache@1@@Z");
    void* const memory = Alloc::New(0xb4);
    if (!memory) return nullptr;
    void* const material = ctor(memory, manager, static_cast<MaterialChunkCache*>(Field<void*>(self, 0x20)),
                                static_cast<AlphaBlendCache*>(Field<void*>(self, 0x28)), true);
    AssignPlugin(material, self);
    return static_cast<Material*>(material);
}

bool __cdecl ColorOneTextureMaterialPlugin::IsSupported(Display*, PString&) { return true; }

// GenericShader: caches {+0x20 chunk, +0x24 blend}.
void __cdecl GenericShaderMaterialPlugin::CreatePluginResources(RenderKernel* kernel) const
{
    void* const self = const_cast<GenericShaderMaterialPlugin*>(this);
    EnsureCache<MaterialChunkCache>(self, kernel, 0x20, MaterialChunkCache::MaterialChunkCacheID);
    EnsureCache<AlphaBlendCache>(self, kernel, 0x24, AlphaBlendCache::AlphaBlendCacheID);
}

Material* __cdecl GenericShaderMaterialPlugin::CreateInstance(MaterialManager* manager, RenderKernel* kernel) const
{
    void* const self = const_cast<GenericShaderMaterialPlugin*>(this);
    CreatePluginResources(kernel);
    static MaterialCtor3 ctor = LocalProc<MaterialCtor3>(
        "??0GenericShaderMaterial@Jet@@QAE@PAVMaterialManager@1@PAVMaterialChunkCache@1@PAVAlphaBlendCache@1@@Z");
    void* const memory = Alloc::New(0xc0);
    if (!memory) return nullptr;
    void* const material = ctor(memory, manager, static_cast<MaterialChunkCache*>(Field<void*>(self, 0x20)),
                                static_cast<AlphaBlendCache*>(Field<void*>(self, 0x24)), true);
    AssignPlugin(material, self);
    return static_cast<Material*>(material);
}

bool __cdecl GenericShaderMaterialPlugin::IsSupported(Display*, PString&) { return true; }

// MVHSelect: the one-texture caches plus one context stage {+0x38, count +0x34, display +0x58}.
void __cdecl MVHSelectMaterialPlugin::CreatePluginResources(RenderKernel* kernel) const
{
    void* const self = const_cast<MVHSelectMaterialPlugin*>(this);
    OneTextureMaterialPlugin::CreatePluginResources(kernel);
    if (!kernel || Field<unsigned int>(self, 0x34)) return;
    Display* const display = Field<Display*>(kernel, 0x7c);
    Field<void*>(self, 0x58) = static_cast<void*>(display);
    Field<void*>(self, 0x38) = BuildStage(display, 0xDCCB8, 0xDCD00);
    Field<unsigned int>(self, 0x34) = 1;
}

Material* __cdecl MVHSelectMaterialPlugin::CreateInstance(MaterialManager* manager, RenderKernel* kernel) const
{
    void* const self = const_cast<MVHSelectMaterialPlugin*>(this);
    CreatePluginResources(kernel);
    static MaterialCtorMVH ctor = LocalProc<MaterialCtorMVH>(
        "??0MVHSelectMaterial@Jet@@QAE@PAVMaterialManager@1@PAVMaterialChunkCache@1@PAVAlphaMaskCache@1@PAV"
        "AlphaBlendCache@1@PAX@Z");
    void* const memory = Alloc::New(0xc8);
    if (!memory) return nullptr;
    void* const material = ctor(memory, manager, static_cast<MaterialChunkCache*>(Field<void*>(self, 0x20)),
                                static_cast<AlphaMaskCache*>(Field<void*>(self, 0x24)),
                                static_cast<AlphaBlendCache*>(Field<void*>(self, 0x28)), Field<void*>(self, 0x38),
                                true);
    AssignPlugin(material, self);
    return static_cast<Material*>(material);
}

bool __cdecl MVHSelectMaterialPlugin::IsSupported(Display*, PString&) { return true; }

// Reflective/Gloss: the one-texture caches plus two context stages {+0x38/+0x3c, count +0x34, display +0x58}.
void __cdecl ReflectiveMaterialPlugin::CreatePluginResources(RenderKernel* kernel) const
{
    void* const self = const_cast<ReflectiveMaterialPlugin*>(this);
    OneTextureMaterialPlugin::CreatePluginResources(kernel);
    if (!kernel || Field<unsigned int>(self, 0x34)) return;
    Display* const display = Field<Display*>(kernel, 0x7c);
    Field<void*>(self, 0x58) = static_cast<void*>(display);
    void* const first = BuildStage(display, 0xDCF60, 0xDCFA8);
    Field<void*>(self, 0x38) = first;
    if (!first) return;
    void* const second = BuildStage(display, 0xDCFF0, 0xDD038);
    Field<void*>(self, 0x3c) = second;
    if (!second) Slot<void>(display, 0x124, first);
    Field<unsigned int>(self, 0x34) = 2;
}

Material* __cdecl ReflectiveMaterialPlugin::CreateInstance(MaterialManager* manager, RenderKernel* kernel) const
{
    void* const self = const_cast<ReflectiveMaterialPlugin*>(this);
    CreatePluginResources(kernel);
    static MaterialCtorReflective ctor = LocalProc<MaterialCtorReflective>(
        "??0ReflectiveMaterial@Jet@@QAE@PAVMaterialManager@1@PAVMaterialChunkCache@1@PAVAlphaBlendCache@1@IPAPAX@Z");
    void* const memory = Alloc::New(0x108);
    if (!memory) return nullptr;
    void* const material = ctor(memory, manager, static_cast<MaterialChunkCache*>(Field<void*>(self, 0x20)),
                                static_cast<AlphaBlendCache*>(Field<void*>(self, 0x28)),
                                Field<unsigned int>(self, 0x34), &Field<void*>(self, 0x38), true);
    AssignPlugin(material, self);
    return static_cast<Material*>(material);
}

bool __cdecl ReflectiveMaterialPlugin::IsSupported(Display* display, PString& reason)
{
    if (Slot<bool>(display, 0x1c, 1)) return true;
    reason = OneTextureMaterial::UNDECORATED_TYPE;
    return false;
}

void __cdecl GlossMaterialPlugin::CreatePluginResources(RenderKernel* kernel) const
{
    void* const self = const_cast<GlossMaterialPlugin*>(this);
    OneTextureMaterialPlugin::CreatePluginResources(kernel);
    if (!kernel || Field<unsigned int>(self, 0x34)) return;
    Display* const display = Field<Display*>(kernel, 0x7c);
    Field<void*>(self, 0x58) = static_cast<void*>(display);
    void* const first = BuildStage(display, 0xDCAC8, 0xDCB10);
    Field<void*>(self, 0x38) = first;
    if (!first) return;
    void* const second = BuildStage(display, 0xDCB58, 0xDCBA0);
    Field<void*>(self, 0x3c) = second;
    if (!second) Slot<void>(display, 0x124, first);
    Field<unsigned int>(self, 0x34) = 2;
}

Material* __cdecl GlossMaterialPlugin::CreateInstance(MaterialManager* manager, RenderKernel* kernel) const
{
    void* const self = const_cast<GlossMaterialPlugin*>(this);
    CreatePluginResources(kernel);
    static MaterialCtorReflective ctor = LocalProc<MaterialCtorReflective>(
        "??0GlossMaterial@Jet@@QAE@PAVMaterialManager@1@PAVMaterialChunkCache@1@PAVAlphaBlendCache@1@IPAPAX@Z");
    void* const memory = Alloc::New(0x108);
    if (!memory) return nullptr;
    void* const material = ctor(memory, manager, static_cast<MaterialChunkCache*>(Field<void*>(self, 0x20)),
                                static_cast<AlphaBlendCache*>(Field<void*>(self, 0x28)),
                                Field<unsigned int>(self, 0x34), &Field<void*>(self, 0x38), true);
    AssignPlugin(material, self);
    return static_cast<Material*>(material);
}

bool __cdecl GlossMaterialPlugin::IsSupported(Display* display, PString& reason)
{
    if (Slot<bool>(display, 0x1c, 1)) return true;
    reason = OneTextureNoAlphaMaterial::UNDECORATED_TYPE;
    return false;
}

// Std: the caches only (no material factory; the original's slot 5 returns null).
void __cdecl StdMaterialPlugin::CreatePluginResources(RenderKernel* kernel) const
{
    void* const self = const_cast<StdMaterialPlugin*>(this);
    EnsureCache<MaterialChunkCache>(self, kernel, 0x20, MaterialChunkCache::MaterialChunkCacheID);
    EnsureCache<AlphaBlendCache>(self, kernel, 0x24, AlphaBlendCache::AlphaBlendCacheID);
    EnsureCache<AlphaMaskCache>(self, kernel, 0x28, AlphaMaskCache::AlphaMaskCacheID);
}

// AutoTex's plugin stays forwarded: its vtable has a key-function problem under this compiler (the class defines no
// constructor of its own and MSVC emits no vftable), and forcing one would import StdMaterialPlugin's default ctor,
// which the original does not export. See notes/agents/u5a_final.md.


// ---------------------------------------------------------------------------------------------------------------
// ReflectiveMaterial's render paths (wave 4 left them forwarded: they need the cell-space cube matrix and the
// ChangeTexGen path below). The flow is the one-texture body with the cube-map state in the middle.

namespace {

float ChunkOpacity(const GeometryChunk* chunk) { return Field<float>(chunk, 0x3c); }
float MaterialOpacity(const Material* material) { return Field<float>(material, 0x80); }

const VertexProgram* ChunkProgram(GeometryChunk* chunk)
{
    return chunk ? chunk->GetVertexProgram(0) : nullptr;
}

// Camera::cachedViewTransform (+0x1b8), updated when the dirty byte at +0x25c is set.
const Matrix4* ViewMatrix(ViewPort* view)
{
    if (!view) return nullptr;
    Camera* const camera = Field<Camera*>(view, 0x20);
    if (!camera) return nullptr;
    if (Field<unsigned char>(camera, 0x25c)) {
        struct CameraAccess : Camera {
            void Update() const { UpdateCachedViewTransform(); }
        };
        reinterpret_cast<const CameraAccess*>(camera)->Update();
    }
    return &Field<Matrix4>(camera, 0x1b8);
}

// The cube-map branch of RenderChunk/RenderPrimitives: build the transposed cell-space transform, multiply it by
// the material's cube matrix and hand the result to Display slots 0x64 and 0x74.
void ApplyCubeMatrix(const ReflectiveMaterial* material, Display* display, ViewPort* view)
{
    if (!display || !view) return;
    Camera* const camera = Field<Camera*>(view, 0x20);
    if (!camera) return;
    const Matrix4* const cell = CellSpaceViewTransform(camera);
    if (!cell) return;
    Matrix4 base;
    BuildCellSpaceMatrix(base, cell->m);
    Matrix4 result;
    MultiplyCubeMatrix(result, Field<Matrix4>(material, 0xbc), base);
    Slot<void>(display, 0x64, &result);
    Slot<void>(display, 0x74, 1, &result);
}

}  // namespace

void __cdecl ReflectiveMaterial::RenderChunk(Display* display, ViewPort* view, GeometryChunk* chunk,
                                            const DisplayLightPtrList* lights, const Matrix4* modelView,
                                            int pass) const
{
    (void)pass;
    const VertexProgram* const program = ChunkProgram(chunk);
    SetDisplayState(display, program == nullptr);
    float opacity;
    if (chunk) {
        opacity = ChunkOpacity(chunk) * MaterialOpacity(this);
        const Matrix4* const cameraView = ViewMatrix(view);
        SetLightingState(display, cameraView, chunk, program, lights, modelView, opacity, false);
    } else {
        opacity = MaterialOpacity(this) * ChunkOpacity(chunk);
        SetLightingState(display, nullptr, chunk, program, lights, modelView, opacity, false);
    }

    const int combiners = Slot<int>(display, 0x2c);
    if ((flags2 & 1) || combiners == 1) {
        if (chunk && (flags2 & 2) && Slot<bool>(display, 0xc) && combiners == 1)
            ApplyCubeMatrix(this, display, view);
    } else {
        SetSphericalTexureCoordinates(chunk, modelView);
    }

    Slot<void>(display, 0x54, modelView);
    if (chunk) chunk->RenderGeometry(display, view, program, true, 0);
    Slot<void>(display, 0x78, 1);
    ClearDisplayState(display);
}

void __cdecl ReflectiveMaterial::RenderPrimitives(Display* display, ViewPort* view, GeometryChunk* chunk,
                                                 const DisplayLightPtrList* lights, const Matrix4* modelView,
                                                 unsigned int numIndices, const unsigned short* indices,
                                                 unsigned int numPrimitives, DPrimitive primitive, int pass) const
{
    (void)pass;
    const VertexProgram* const program = ChunkProgram(chunk);
    SetDisplayState(display, program == nullptr);
    float opacity;
    if (chunk) {
        opacity = ChunkOpacity(chunk) * MaterialOpacity(this);
        const Matrix4* const cameraView = ViewMatrix(view);
        SetLightingState(display, cameraView, chunk, program, lights, modelView, opacity, false);
    } else {
        opacity = MaterialOpacity(this) * ChunkOpacity(chunk);
        SetLightingState(display, nullptr, chunk, program, lights, modelView, opacity, false);
    }

    const int combiners = Slot<int>(display, 0x2c);
    if ((flags2 & 1) || combiners == 1) {
        if (chunk && (flags2 & 2) && Slot<bool>(display, 0xc) && combiners == 1)
            ApplyCubeMatrix(this, display, view);
    } else {
        SetSphericalTexureCoordinates(chunk, modelView);
    }

    Slot<void>(display, 0x54, modelView);
    if (chunk)
        chunk->RenderPrimitives(display, view, program, numIndices, indices, numPrimitives, primitive, true, 0);
    Slot<void>(display, 0x78, 1);
    ClearDisplayState(display);
}

void __cdecl ReflectiveMaterial::RenderNumChunks(Display* display, ViewPort* view, GeometryChunk** chunks,
                                                const DisplayLightPtrList** lights, const Matrix4** modelViews,
                                                unsigned int numChunks, int pass) const
{
    (void)pass;
    VertexProgram const* first = numChunks ? ChunkProgram(chunks[0]) : nullptr;
    SetDisplayState(display, first == nullptr);
    const int combiners = Slot<int>(display, 0x2c);
    const bool cubeStages = Slot<bool>(display, 0xc) != 0;
    if (numChunks && (flags2 & 2) && cubeStages && combiners == 1)
        ApplyCubeMatrix(this, display, view);

    bool wasNull = first == nullptr;
    for (unsigned int i = 0; i < numChunks; ++i) {
        GeometryChunk* const chunk = chunks[i];
        const VertexProgram* const program = ChunkProgram(chunk);
        const Matrix4* const modelView = modelViews[i];
        const bool isNull = program == nullptr;
        if (isNull != wasNull) {
            wasNull = isNull;
            U5a2::Reflective_change_tex_gen(this, display, isNull);
        }

        if ((flags2 & 1) || combiners == 1) {
            if (chunk && cubeStages && combiners == 1) ApplyCubeMatrix(this, display, view);
        } else {
            SetSphericalTexureCoordinates(chunk, modelView);
        }

        float opacity;
        if (chunk) {
            opacity = ChunkOpacity(chunk) * MaterialOpacity(this);
            const Matrix4* const cameraView = ViewMatrix(view);
            SetLightingState(display, cameraView, chunk, program, lights[i], modelView, opacity, false);
        } else {
            opacity = MaterialOpacity(this) * ChunkOpacity(chunk);
            SetLightingState(display, nullptr, chunk, program, lights[i], modelView, opacity, false);
        }
        Slot<void>(display, 0x54, modelView);
        if (chunk) chunk->RenderGeometry(display, view, program, true, 0);
    }
    Slot<void>(display, 0x78, 1);
    ClearDisplayState(display);
}

}  // namespace Jet
