// Unit U5a2 of Render3DCore_7c: the material plugin factories' lifetime and assignments.
//
// The plugin objects are PluginFactoryBase<Material> (0x20 bytes) plus a run of rendering-cache pointers. Their
// exported constructors and assignments are __thiscall, so each body is a member of the small thunk object below and
// its decorated name is aliased to the original export in implemented.d/u5a2_materials.txt. The base
// MaterialPlugin constructors/assignment come from RenderSystem_7c; the derived virtual tables and UID objects are
// this module's own data (emitted by the U5a definitions), looked up by export name.
#include <jet/Render3DCoreMaterials.h>

#include <windows.h>

#include <cstring>

namespace Jet {
namespace U5a2 {

namespace {

using BaseNamedCtor = void(__thiscall*)(void*, const PString&, const UId&);
using BaseCopyCtor = void(__thiscall*)(void*, const MaterialPlugin&);
using BaseAssign = MaterialPlugin&(__thiscall*)(void*, const MaterialPlugin&);

HMODULE RenderSystem()
{
    static HMODULE module = [] {
        HMODULE m = GetModuleHandleA("RenderSystem_7c.dll");
        return m ? m : LoadLibraryA("RenderSystem_7c.dll");
    }();
    return module;
}

BaseNamedCtor NamedCtor()
{
    static BaseNamedCtor fn = reinterpret_cast<BaseNamedCtor>(
        GetProcAddress(RenderSystem(), "??0MaterialPlugin@Jet@@QAE@ABVPString@1@ABVUId@1@@Z"));
    return fn;
}

BaseCopyCtor CopyCtor()
{
    static BaseCopyCtor fn = reinterpret_cast<BaseCopyCtor>(
        GetProcAddress(RenderSystem(), "??0MaterialPlugin@Jet@@QAE@ABV01@@Z"));
    return fn;
}

BaseAssign Assign()
{
    static BaseAssign fn = reinterpret_cast<BaseAssign>(
        GetProcAddress(RenderSystem(), "??4MaterialPlugin@Jet@@QAEAAV01@ABV01@@Z"));
    return fn;
}

HMODULE ThisModule()
{
    static HMODULE module = [] {
        HMODULE h = nullptr;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCSTR>(&ThisModule), &h);
        return h;
    }();
    return module;
}

// since U5a wave 5 the plugin vftables and the material UID objects are this module's own data
void* LocalData(const char* name) { return reinterpret_cast<void*>(GetProcAddress(ThisModule(), name)); }

const UId& MaterialUid(const char* name) { return *static_cast<const UId*>(LocalData(name)); }

void* Vtable(const char* name) { return LocalData(name); }

void Zero(void* self, unsigned first, unsigned lastInclusive)
{
    for (unsigned offset = first; offset <= lastInclusive; offset += 4)
        *reinterpret_cast<void**>(static_cast<unsigned char*>(self) + offset) = nullptr;
}

void Copy(void* self, const void* source, unsigned first, unsigned lastInclusive)
{
    for (unsigned offset = first; offset <= lastInclusive; offset += 4)
        *reinterpret_cast<void**>(static_cast<unsigned char*>(self) + offset) =
            *reinterpret_cast<void* const*>(static_cast<const unsigned char*>(source) + offset);
}

// every class's prelude: call the base MaterialPlugin constructor, then install the derived vtable
void NamedPrelude(void* self, const PString& name, const UId& id, const char* vtable)
{
    NamedCtor()(self, name, id);
    *reinterpret_cast<void**>(self) = Vtable(vtable);
}
void CopyPrelude(void* self, const void* source, const char* vtable)
{
    CopyCtor()(self, *static_cast<const MaterialPlugin*>(source));
    *reinterpret_cast<void**>(self) = Vtable(vtable);
}

}  // namespace

struct PluginThunks {
    // default constructors (nine; Std is not exported by name)
    void* DefaultAutoTex();
    void* DefaultColorOneTexture();
    void* DefaultGenericShader();
    void* DefaultGloss();
    void* DefaultMVHSelect();
    void* DefaultNoTexture();
    void* DefaultOneTexture();
    void* DefaultOneTextureNoAlpha();
    void* DefaultReflective();

    // (name, id) constructors (ten; Std has none in the export table)
    void* NamedAutoTex(const PString& name, const UId& id);
    void* NamedColorOneTexture(const PString& name, const UId& id);
    void* NamedGenericShader(const PString& name, const UId& id);
    void* NamedGloss(const PString& name, const UId& id);
    void* NamedMVHSelect(const PString& name, const UId& id);
    void* NamedNoTexture(const PString& name, const UId& id);
    void* NamedOneTexture(const PString& name, const UId& id);
    void* NamedOneTextureNoAlpha(const PString& name, const UId& id);
    void* NamedReflective(const PString& name, const UId& id);
    void* NamedStd(const PString& name, const UId& id);
    void* NamedMaterialPluginX(const PString& name, const UId& id);

    // copy constructors (eleven)
    void* CopyAutoTex(const void* source);
    void* CopyColorOneTexture(const void* source);
    void* CopyGenericShader(const void* source);
    void* CopyGloss(const void* source);
    void* CopyMVHSelect(const void* source);
    void* CopyNoTexture(const void* source);
    void* CopyOneTexture(const void* source);
    void* CopyOneTextureNoAlpha(const void* source);
    void* CopyReflective(const void* source);
    void* CopyStd(const void* source);
    void* CopyMaterialPluginX(const void* source);

    // assignments (eleven)
    void* AssignAutoTex(const void* source);
    void* AssignColorOneTexture(const void* source);
    void* AssignGenericShader(const void* source);
    void* AssignGloss(const void* source);
    void* AssignMVHSelect(const void* source);
    void* AssignNoTexture(const void* source);
    void* AssignOneTexture(const void* source);
    void* AssignOneTextureNoAlpha(const void* source);
    void* AssignReflective(const void* source);
    void* AssignStd(const void* source);
    void* AssignMaterialPluginX(const void* source);
};

// ---- default constructors -------------------------------------------------------------------------------------

void* PluginThunks::DefaultAutoTex()
{
    NamedPrelude(this, AutoTexMaterial::UNDECORATED_TYPE, MaterialUid("?UID@AutoTexMaterial@Jet@@2VUId@2@B"),
                 "??_7AutoTexMaterialPlugin@Jet@@6B@");
    Zero(this, 0x20, 0x28);
    return this;
}

void* PluginThunks::DefaultColorOneTexture()
{
    NamedPrelude(this, ColorOneTextureMaterial::UNDECORATED_TYPE, MaterialUid("?UID@ColorOneTextureMaterial@Jet@@2VUId@2@B"),
                 "??_7ColorOneTextureMaterialPlugin@Jet@@6B@");
    Zero(this, 0x20, 0x30);
    return this;
}

void* PluginThunks::DefaultGenericShader()
{
    NamedPrelude(this, GenericShaderMaterial::UNDECORATED_TYPE, MaterialUid("?UID@GenericShaderMaterial@Jet@@2VUId@2@B"),
                 "??_7GenericShaderMaterialPlugin@Jet@@6B@");
    Zero(this, 0x20, 0x24);
    return this;
}

void* PluginThunks::DefaultGloss()
{
    NamedPrelude(this, GlossMaterial::UNDECORATED_TYPE, MaterialUid("?UID@GlossMaterial@Jet@@2VUId@2@B"), "??_7GlossMaterialPlugin@Jet@@6B@");
    Zero(this, 0x20, 0x34);
    Zero(this, 0x58, 0x58);
    return this;
}

void* PluginThunks::DefaultMVHSelect()
{
    NamedPrelude(this, MVHSelectMaterial::UNDECORATED_TYPE, MaterialUid("?UID@MVHSelectMaterial@Jet@@2VUId@2@B"),
                 "??_7MVHSelectMaterialPlugin@Jet@@6B@");
    Zero(this, 0x20, 0x34);
    Zero(this, 0x58, 0x58);
    return this;
}

void* PluginThunks::DefaultNoTexture()
{
    NamedPrelude(this, NoTextureMaterial::UNDECORATED_TYPE, MaterialUid("?UID@NoTextureMaterial@Jet@@2VUId@2@B"),
                 "??_7NoTextureMaterialPlugin@Jet@@6B@");
    Zero(this, 0x20, 0x2c);
    return this;
}

void* PluginThunks::DefaultOneTexture()
{
    NamedPrelude(this, OneTextureMaterial::UNDECORATED_TYPE, MaterialUid("?UID@OneTextureMaterial@Jet@@2VUId@2@B"),
                 "??_7OneTextureMaterialPlugin@Jet@@6B@");
    Zero(this, 0x20, 0x30);
    return this;
}

void* PluginThunks::DefaultOneTextureNoAlpha()
{
    NamedPrelude(this, OneTextureNoAlphaMaterial::UNDECORATED_TYPE, MaterialUid("?UID@OneTextureNoAlphaMaterial@Jet@@2VUId@2@B"),
                 "??_7OneTextureNoAlphaMaterialPlugin@Jet@@6B@");
    Zero(this, 0x20, 0x30);
    return this;
}

void* PluginThunks::DefaultReflective()
{
    NamedPrelude(this, ReflectiveMaterial::UNDECORATED_TYPE, MaterialUid("?UID@ReflectiveMaterial@Jet@@2VUId@2@B"),
                 "??_7ReflectiveMaterialPlugin@Jet@@6B@");
    Zero(this, 0x20, 0x34);
    Zero(this, 0x58, 0x58);
    return this;
}

// ---- (name, id) constructors ----------------------------------------------------------------------------------

void* PluginThunks::NamedAutoTex(const PString& name, const UId& id)
{
    NamedPrelude(this, name, id, "??_7AutoTexMaterialPlugin@Jet@@6B@");
    Zero(this, 0x20, 0x28);
    return this;
}

void* PluginThunks::NamedColorOneTexture(const PString& name, const UId& id)
{
    NamedPrelude(this, name, id, "??_7ColorOneTextureMaterialPlugin@Jet@@6B@");
    Zero(this, 0x20, 0x30);
    return this;
}

void* PluginThunks::NamedGenericShader(const PString& name, const UId& id)
{
    NamedPrelude(this, name, id, "??_7GenericShaderMaterialPlugin@Jet@@6B@");
    Zero(this, 0x20, 0x24);
    return this;
}

void* PluginThunks::NamedGloss(const PString& name, const UId& id)
{
    NamedPrelude(this, name, id, "??_7GlossMaterialPlugin@Jet@@6B@");
    Zero(this, 0x20, 0x34);
    Zero(this, 0x58, 0x58);
    return this;
}

void* PluginThunks::NamedMVHSelect(const PString& name, const UId& id)
{
    NamedPrelude(this, name, id, "??_7MVHSelectMaterialPlugin@Jet@@6B@");
    Zero(this, 0x20, 0x34);
    Zero(this, 0x58, 0x58);
    return this;
}

void* PluginThunks::NamedNoTexture(const PString& name, const UId& id)
{
    NamedPrelude(this, name, id, "??_7NoTextureMaterialPlugin@Jet@@6B@");
    Zero(this, 0x20, 0x2c);
    return this;
}

void* PluginThunks::NamedOneTexture(const PString& name, const UId& id)
{
    NamedPrelude(this, name, id, "??_7OneTextureMaterialPlugin@Jet@@6B@");
    Zero(this, 0x20, 0x30);
    return this;
}

void* PluginThunks::NamedOneTextureNoAlpha(const PString& name, const UId& id)
{
    NamedPrelude(this, name, id, "??_7OneTextureNoAlphaMaterialPlugin@Jet@@6B@");
    Zero(this, 0x20, 0x30);
    return this;
}

void* PluginThunks::NamedReflective(const PString& name, const UId& id)
{
    NamedPrelude(this, name, id, "??_7ReflectiveMaterialPlugin@Jet@@6B@");
    Zero(this, 0x20, 0x34);
    Zero(this, 0x58, 0x58);
    return this;
}

void* PluginThunks::NamedMaterialPluginX(const PString& name, const UId& id)
{
    NamedPrelude(this, name, id, "??_7MaterialPluginX@Jet@@6B@");
    return this;
}

void* PluginThunks::NamedStd(const PString& name, const UId& id)
{
    NamedPrelude(this, name, id, "??_7StdMaterialPlugin@Jet@@6B@");
    Zero(this, 0x20, 0x28);
    return this;
}

// ---- copy constructors ----------------------------------------------------------------------------------------

void* PluginThunks::CopyAutoTex(const void* source)
{
    CopyPrelude(this, source, "??_7AutoTexMaterialPlugin@Jet@@6B@");
    Copy(this, source, 0x20, 0x28);
    return this;
}

void* PluginThunks::CopyColorOneTexture(const void* source)
{
    CopyPrelude(this, source, "??_7ColorOneTextureMaterialPlugin@Jet@@6B@");
    Copy(this, source, 0x20, 0x30);
    return this;
}

void* PluginThunks::CopyGenericShader(const void* source)
{
    CopyPrelude(this, source, "??_7GenericShaderMaterialPlugin@Jet@@6B@");
    Copy(this, source, 0x20, 0x24);
    return this;
}

void* PluginThunks::CopyReflective(const void* source)
{
    CopyPrelude(this, source, "??_7ReflectiveMaterialPlugin@Jet@@6B@");
    Copy(this, source, 0x20, 0x58);
    return this;
}

void* PluginThunks::CopyGloss(const void* source)
{
    CopyReflective(source);
    *reinterpret_cast<void**>(this) = Vtable("??_7GlossMaterialPlugin@Jet@@6B@");
    return this;
}

void* PluginThunks::CopyMVHSelect(const void* source)
{
    CopyReflective(source);
    *reinterpret_cast<void**>(this) = Vtable("??_7MVHSelectMaterialPlugin@Jet@@6B@");
    return this;
}

void* PluginThunks::CopyNoTexture(const void* source)
{
    CopyPrelude(this, source, "??_7NoTextureMaterialPlugin@Jet@@6B@");
    Copy(this, source, 0x20, 0x2c);
    return this;
}

void* PluginThunks::CopyOneTexture(const void* source)
{
    CopyPrelude(this, source, "??_7OneTextureMaterialPlugin@Jet@@6B@");
    Copy(this, source, 0x20, 0x30);
    return this;
}

void* PluginThunks::CopyOneTextureNoAlpha(const void* source)
{
    CopyPrelude(this, source, "??_7OneTextureNoAlphaMaterialPlugin@Jet@@6B@");
    Copy(this, source, 0x20, 0x30);
    return this;
}

void* PluginThunks::CopyStd(const void* source)
{
    CopyPrelude(this, source, "??_7StdMaterialPlugin@Jet@@6B@");
    Copy(this, source, 0x20, 0x28);
    return this;
}

void* PluginThunks::CopyMaterialPluginX(const void* source)
{
    CopyPrelude(this, source, "??_7MaterialPluginX@Jet@@6B@");
    return this;
}

// ---- assignments ----------------------------------------------------------------------------------------------

void* PluginThunks::AssignAutoTex(const void* source)
{
    Assign()(this, *static_cast<const MaterialPlugin*>(source));
    Copy(this, source, 0x20, 0x28);
    return this;
}

void* PluginThunks::AssignColorOneTexture(const void* source)
{
    Assign()(this, *static_cast<const MaterialPlugin*>(source));
    Copy(this, source, 0x20, 0x30);
    return this;
}

void* PluginThunks::AssignGenericShader(const void* source)
{
    Assign()(this, *static_cast<const MaterialPlugin*>(source));
    Copy(this, source, 0x20, 0x24);
    return this;
}

void* PluginThunks::AssignReflective(const void* source)
{
    Assign()(this, *static_cast<const MaterialPlugin*>(source));
    Copy(this, source, 0x20, 0x58);
    return this;
}

void* PluginThunks::AssignGloss(const void* source)
{
    return AssignReflective(source);
}

void* PluginThunks::AssignMVHSelect(const void* source)
{
    return AssignReflective(source);
}

void* PluginThunks::AssignNoTexture(const void* source)
{
    Assign()(this, *static_cast<const MaterialPlugin*>(source));
    Copy(this, source, 0x20, 0x2c);
    return this;
}

void* PluginThunks::AssignOneTexture(const void* source)
{
    Assign()(this, *static_cast<const MaterialPlugin*>(source));
    Copy(this, source, 0x20, 0x30);
    return this;
}

void* PluginThunks::AssignOneTextureNoAlpha(const void* source)
{
    Assign()(this, *static_cast<const MaterialPlugin*>(source));
    Copy(this, source, 0x20, 0x30);
    return this;
}

void* PluginThunks::AssignStd(const void* source)
{
    return AssignAutoTex(source);
}

void* PluginThunks::AssignMaterialPluginX(const void* source)
{
    Assign()(this, *static_cast<const MaterialPlugin*>(source));
    return this;
}

}  // namespace U5a2
}  // namespace Jet
