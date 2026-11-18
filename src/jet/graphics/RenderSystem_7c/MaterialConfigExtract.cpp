#include <jet/Config.h>
#include <windows.h>

namespace Jet {
namespace {
void SetMaterialConfigVtable(void* object)
{
    static void* const table = reinterpret_cast<void*>(GetProcAddress(
        GetModuleHandleA("RenderSystem_7c_orig.dll"), "??_7MaterialConfig@Jet@@6B@"));
    *static_cast<void**>(object) = table;
}
const ConfigData* FindMaterialKey(const ConfigData& config, const char* name)
{
    const PString key(name);
    return &config.Find(key);
}
}
void* __fastcall RsMaterialConfigDefault(void* object, void*) { SetMaterialConfigVtable(object); return object; }
void* __fastcall RsMaterialConfigCopy(void* object, void*, const void*) { SetMaterialConfigVtable(object); return object; }
void __fastcall RsMaterialConfigDestroy(void* object, void*) { SetMaterialConfigVtable(object); }
void* __fastcall RsMaterialConfigAssign(void* object, void*, const void*) { return object; }
bool __cdecl RsExtractMaterialColor(void*, Color* out, const ConfigData* config, const char* name)
{
    const ConfigData* item = FindMaterialKey(*config, name);
    if (item == &ConfigData::unknownItem) return false;
    *out = static_cast<Color>(*item);
    return true;
}
bool __cdecl RsExtractMaterialFloat(void*, float* out, const ConfigData* config, const char* name)
{
    const ConfigData* item = FindMaterialKey(*config, name);
    if (item == &ConfigData::unknownItem) return false;
    *out = static_cast<float>(*item);
    return true;
}
bool __cdecl RsExtractMaterialBool(void*, bool* out, const ConfigData* config, const char* name)
{
    const ConfigData* item = FindMaterialKey(*config, name);
    if (item == &ConfigData::unknownItem) return false;
    *out = true;
    return true;
}
}
