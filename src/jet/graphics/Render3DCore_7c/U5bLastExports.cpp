// Render3DCore_7c U5b last wave: the extern "C" entry points recorded in implemented.d/u5b_last.txt. Keeping them
// outside the class headers makes the export table independent of C++ mangling.
#include "U5bLast.h"

namespace Jet {
namespace MultiPass {
namespace Last {

using Final::Original;

// Per-class tails that still need the plugin's own name tables / vertex type descriptions: delegated to the
// original module's entry with the same ABI.
void FixedBump(void* self, void* kernel)
{
    using Fn = void(__cdecl*)(void*, void*);
    Fn fn = reinterpret_cast<Fn>(Original(
        "?InitializeFixedVertexType@MultiPassBumpMaterialPlugin@Jet@@MBAXPAVRenderKernel@2@@Z"));
    if (fn) fn(self, kernel);
}
void FixedBump2(void* self, void* kernel)
{
    using Fn = void(__cdecl*)(void*, void*);
    Fn fn = reinterpret_cast<Fn>(Original(
        "?InitializeFixedVertexType@MultiPassBump2MaterialPlugin@Jet@@MBAXPAVRenderKernel@2@@Z"));
    if (fn) fn(self, kernel);
}
void FixedBumpSpec2(void* self, void* kernel)
{
    using Fn = void(__cdecl*)(void*, void*);
    Fn fn = reinterpret_cast<Fn>(Original(
        "?InitializeFixedVertexType@MultiPassBumpSpec2MaterialPlugin@Jet@@MBAXPAVRenderKernel@2@@Z"));
    if (fn) fn(self, kernel);
}
void FixedOneTexture(void* self, void* kernel)
{
    using Fn = void(__cdecl*)(void*, void*);
    Fn fn = reinterpret_cast<Fn>(Original(
        "?InitializeFixedVertexType@MultiPassOneTextureMaterialPlugin@Jet@@MBAXPAVRenderKernel@2@@Z"));
    if (fn) fn(self, kernel);
}

bool LoadProgramsFor(void* self, void* lightRender, void* kernel, const char* name, const char* combiner,
                     const char* exportName)
{
    if (!name && !combiner) {
        reinterpret_cast<Final::Thunks*>(lightRender)->CreateNewLightPass(nullptr, nullptr);
        return true;
    }
    using Fn = bool(__cdecl*)(void*, void*, void*, const char*, const char*);
    Fn fn = reinterpret_cast<Fn>(Original(exportName));
    return fn ? fn(self, lightRender, kernel, name, combiner) : false;
}

}  // namespace Last
}  // namespace MultiPass
}  // namespace Jet

using namespace Jet::MultiPass;

// Two DebugAccess statics that DebugRender's new SimpleRemainScene unit imports but the module's link set does not
// provide yet (a neighbouring unit is mid-landing). The aliases keep this build linkable without touching its files.
extern "C" bool r3d_u5b_last_debugGlobalBoundingVolumesStorage = false;
extern "C" bool* r3d_u5b_last_debugGlobalBoundingVolumes = &r3d_u5b_last_debugGlobalBoundingVolumesStorage;
extern "C" unsigned long r3d_u5b_last_debugBoundingVolumeMaskStorage = 0;
extern "C" unsigned long* r3d_u5b_last_debugBoundingVolumeMask = &r3d_u5b_last_debugBoundingVolumeMaskStorage;
#if defined(_M_IX86)
#pragma comment(linker, "/alternatename:__imp_?globalBoundingVolumes@DebugAccess@Jet@@3_NA=_r3d_u5b_last_debugGlobalBoundingVolumes")
#pragma comment(linker, "/alternatename:__imp_?boundingVolumeMask@DebugAccess@Jet@@3KA=_r3d_u5b_last_debugBoundingVolumeMask")
#endif

namespace Jet { namespace MultiPass { namespace Last { namespace Leaves {

bool r3d_u5b_last_state_setup(void* self, const void* lights, const void* modelView)
{
    return Last::StateSetup(self, lights, modelView);
}
void r3d_u5b_last_state_scissor(void* self, const void* lights, const void* indices)
{
    Last::StateSetScissor(self, lights, indices);
}
void r3d_u5b_last_state_load_vp(void* self, void* light, const void* properties, void* program)
{
    Last::StateLoadPropertiesVP(self, light, properties, program);
}
void r3d_u5b_last_state_load_tc(void* self, void* light, const void* properties, void* combiner)
{
    Last::StateLoadPropertiesTC(self, light, properties, combiner);
}
void r3d_u5b_last_state_single_passes(void* self, void* lightRender)
{
    Last::StateRenderSingleLightPasses(self, lightRender);
}
void r3d_u5b_last_tangent(void* self, void* chunk) { Last::BumpChunkCreateTangentVectors(self, chunk); }
void r3d_u5b_last_per_vertex(void* self, void* display, void* view, void* chunk, const void* lights,
                             const void* modelView, int pass, unsigned int numIndices,
                             const unsigned short* indices, unsigned int numPrimitives, int primitive)
{
    Last::OneTextureRenderChunkPerVertex(self, display, view, chunk, lights, modelView, pass, numIndices, indices,
                                         numPrimitives, primitive);
}

void* r3d_u5b_last_create_notexture(void* self, void* manager, void* kernel)
{
    return Last::PluginCreateNoTexture(self, manager, kernel);
}
void* r3d_u5b_last_create_onetexture(void* self, void* manager, void* kernel)
{
    return Last::PluginCreateOneTexture(self, manager, kernel);
}
void* r3d_u5b_last_create_bump(void* self, void* manager, void* kernel)
{
    return Last::PluginCreateBump(self, manager, kernel);
}
void* r3d_u5b_last_create_bump2(void* self, void* manager, void* kernel)
{
    return Last::PluginCreateBump2(self, manager, kernel);
}
void* r3d_u5b_last_create_bumpspec(void* self, void* manager, void* kernel)
{
    return Last::PluginCreateBumpSpec(self, manager, kernel);
}
void* r3d_u5b_last_create_bumpspec2(void* self, void* manager, void* kernel)
{
    return Last::PluginCreateBumpSpec2(self, manager, kernel);
}

bool r3d_u5b_last_caches(void* self, void* kernel) { return Last::PluginInitializeCaches(self, kernel); }
bool r3d_u5b_last_light_renders(void* self, void* kernel)
{
    return Last::PluginInitializeLightRenders(self, kernel);
}
void r3d_u5b_last_fixed_bump(void* self, void* kernel) { Last::FixedBump(self, kernel); }
void r3d_u5b_last_fixed_bump2(void* self, void* kernel) { Last::FixedBump2(self, kernel); }
void r3d_u5b_last_fixed_bumpspec2(void* self, void* kernel) { Last::FixedBumpSpec2(self, kernel); }
void r3d_u5b_last_fixed_onetexture(void* self, void* kernel) { Last::FixedOneTexture(self, kernel); }

bool r3d_u5b_last_ambient_notexture(void* self, void* kernel)
{
    return Last::PluginInitAmbientNoTexture(self, kernel);
}
bool r3d_u5b_last_ambient(void* self, void* kernel) { return Last::PluginInitAmbient(self, kernel); }
bool r3d_u5b_last_parallel_notexture(void* self, void* kernel)
{
    return Last::PluginInitParallelNoTexture(self, kernel);
}
bool r3d_u5b_last_parallel_onetexture(void* self, void* kernel)
{
    return Last::PluginInitParallelOneTexture(self, kernel);
}
bool r3d_u5b_last_parallel_bump(void* self, void* kernel) { return Last::PluginInitParallelBump(self, kernel); }
bool r3d_u5b_last_parallel_bumpspec(void* self, void* kernel)
{
    return Last::PluginInitParallelBumpSpec(self, kernel);
}
bool r3d_u5b_last_point_notexture(void* self, void* kernel)
{
    return Last::PluginInitPointNoTexture(self, kernel);
}
bool r3d_u5b_last_point_onetexture(void* self, void* kernel)
{
    return Last::PluginInitPointOneTexture(self, kernel);
}
bool r3d_u5b_last_point_bump(void* self, void* kernel) { return Last::PluginInitPointBump(self, kernel); }
bool r3d_u5b_last_point_bumpspec(void* self, void* kernel) { return Last::PluginInitPointBumpSpec(self, kernel); }
bool r3d_u5b_last_spot_notexture(void* self, void* kernel) { return Last::PluginInitSpotNoTexture(self, kernel); }
bool r3d_u5b_last_spot(void* self, void* kernel) { return Last::PluginInitSpot(self, kernel); }
bool r3d_u5b_last_cube_notexture(void* self, void* kernel) { return Last::PluginInitCubeNoTexture(self, kernel); }
bool r3d_u5b_last_cube(void* self, void* kernel) { return Last::PluginInitCube(self, kernel); }
bool r3d_u5b_last_cubespot_notexture(void* self, void* kernel)
{
    return Last::PluginInitCubeSpotNoTexture(self, kernel);
}
bool r3d_u5b_last_cubespot(void* self, void* kernel) { return Last::PluginInitCubeSpot(self, kernel); }

bool r3d_u5b_last_load_bump(void* self, void* lightRender, void* kernel, const char* name, const char* combiner)
{
    return Last::LoadProgramsFor(self, lightRender, kernel, name, combiner,
                                 "?LoadLightPrograms@MultiPassBumpMaterialPlugin@Jet@@MBA_NPAVMultiPassLightRender@2@"
                                 "PAVRenderKernel@2@PBD2@Z");
}
bool r3d_u5b_last_load_notexture(void* self, void* lightRender, void* kernel, const char* name,
                                 const char* combiner)
{
    return Last::LoadProgramsFor(self, lightRender, kernel, name, combiner,
                                 "?LoadLightPrograms@MultiPassNoTextureMaterialPlugin@Jet@@IBA_NPAVMultiPassLightRender@2@"
                                 "PAVRenderKernel@2@PBD2@Z");
}
bool r3d_u5b_last_load_onetexture(void* self, void* lightRender, void* kernel, const char* name,
                                  const char* combiner)
{
    return Last::LoadProgramsFor(self, lightRender, kernel, name, combiner,
                                 "?LoadLightPrograms@MultiPassOneTextureMaterialPlugin@Jet@@IBA_NPAVMultiPassLightRender@2@"
                                 "PAVRenderKernel@2@PBD2@Z");
}

void r3d_u5b_last_register_notexture(void* self) { Last::PluginRegisterProgramsNoTexture(self); }
void r3d_u5b_last_register_onetexture(void* self) { Last::PluginRegisterProgramsOneTexture(self); }
void r3d_u5b_last_register_bump(void* self) { Last::PluginRegisterProgramsBump(self); }
void r3d_u5b_last_register_bumpspec(void* self) { Last::PluginRegisterProgramsBumpSpec(self); }

} } } }  // namespace Jet::MultiPass::Last::Leaves
