// Render3DCore_7c U5b last wave (U5bLast): the multipass render-state property/scissor/pass bodies, the bump chunk
// tangent generator and the one-texture per-vertex path.
//
// The exported entries are plain cdecl functions whose first stack argument is `this`, exactly like the original
// (the mangled names are __cdecl members). Object layouts are read at the offsets established by wave 5 and by the
// original's assembly; only the RenderSystem_7c entry points are called by name.
#pragma once

#include "U5bFinal.h"

#include <jet/Render3DCoreLights.h>

namespace Jet {
namespace MultiPass {
namespace Last {

struct RectI {
    int x, y, w, h;
};
JET_LAYOUT32(sizeof(RectI) == 0x10);

// The three fast paths need a little arithmetic on the state block; the rest of the state is opaque.
constexpr unsigned int kStateSize = 0x120;

// MultiPassRenderState::StateSetup (0x39d30)
bool __cdecl StateSetup(void* self, const void* lights, const void* modelView);
// MultiPassRenderState::SetScissor (0x3c150)
void __cdecl StateSetScissor(void* self, const void* lights, const void* indices);
// MultiPassRenderState::LoadRenderProperties (0x3a870 / 0x3b4b0)
void __cdecl StateLoadPropertiesVP(void* self, void* light, const void* properties, void* program);
void __cdecl StateLoadPropertiesTC(void* self, void* light, const void* properties, void* combiner);
// MultiPassRenderState::RenderSingleLightPasses (0x3c3d0)
void __cdecl StateRenderSingleLightPasses(void* self, void* lightRender);

// MultiPassBumpChunkBase::CreateTangentVectors (0x2c250)
void __cdecl BumpChunkCreateTangentVectors(void* self, void* chunk);

// MultiPassOneTextureMaterial::RenderChunkPerVertex (0x37510)
void __cdecl OneTextureRenderChunkPerVertex(void* self, void* display, void* view, void* chunk, const void* lights,
                                            const void* modelView, int pass, unsigned int numIndices,
                                            const unsigned short* indices, unsigned int numPrimitives,
                                            int primitive);

// ---- plugin entries (U5bLastPlugins.cpp). All are __cdecl with self first.
void* __cdecl PluginCreateNoTexture(void* self, void* manager, void* kernel);
void* __cdecl PluginCreateOneTexture(void* self, void* manager, void* kernel);
void* __cdecl PluginCreateBump(void* self, void* manager, void* kernel);
void* __cdecl PluginCreateBump2(void* self, void* manager, void* kernel);
void* __cdecl PluginCreateBumpSpec(void* self, void* manager, void* kernel);
void* __cdecl PluginCreateBumpSpec2(void* self, void* manager, void* kernel);

bool __cdecl PluginInitializeCaches(void* self, void* kernel);
bool __cdecl PluginInitializeLightRenders(void* self, void* kernel);
void __cdecl PluginInitializeFixedVertexType(void* self, void* kernel);

bool __cdecl PluginInitAmbientNoTexture(void* self, void* kernel);
bool __cdecl PluginInitAmbient(void* self, void* kernel);
bool __cdecl PluginInitParallelNoTexture(void* self, void* kernel);
bool __cdecl PluginInitParallelOneTexture(void* self, void* kernel);
bool __cdecl PluginInitParallelBump(void* self, void* kernel);
bool __cdecl PluginInitParallelBumpSpec(void* self, void* kernel);
bool __cdecl PluginInitPointNoTexture(void* self, void* kernel);
bool __cdecl PluginInitPointOneTexture(void* self, void* kernel);
bool __cdecl PluginInitPointBump(void* self, void* kernel);
bool __cdecl PluginInitPointBumpSpec(void* self, void* kernel);
bool __cdecl PluginInitSpotNoTexture(void* self, void* kernel);
bool __cdecl PluginInitSpot(void* self, void* kernel);
bool __cdecl PluginInitCubeNoTexture(void* self, void* kernel);
bool __cdecl PluginInitCube(void* self, void* kernel);
bool __cdecl PluginInitCubeSpotNoTexture(void* self, void* kernel);
bool __cdecl PluginInitCubeSpot(void* self, void* kernel);

bool __cdecl PluginLoadLightPrograms(void* self, void* lightRender, void* kernel, const char* name,
                                     const char* combinerName);
void __cdecl PluginRegisterProgramsNoTexture(void* self);
void __cdecl PluginRegisterProgramsOneTexture(void* self);
void __cdecl PluginRegisterProgramsBump(void* self);
void __cdecl PluginRegisterProgramsBumpSpec(void* self);

// Per-family descriptions of the plugin bodies. The original has one body per family (plus shared leaves).
struct PluginFamily {
    unsigned int materialSize;       // 0xd0 / 0xdc / 0xe0
    const char* materialCtor;        // exported constructor of the material class
    bool materialHasVertexType;
};

// Built-in program registrations replayed by RegisterBuiltInPrograms.
struct BuiltInProgram {
    bool vertex;
    const char* name;
    const char* text;
};
extern const BuiltInProgram kBumpPrograms[];
extern const unsigned int kBumpProgramCount;
extern const BuiltInProgram kBumpspecPrograms[];
extern const unsigned int kBumpspecProgramCount;
extern const BuiltInProgram kNotexturePrograms[];
extern const unsigned int kNotextureProgramCount;
extern const BuiltInProgram kOnetexturePrograms[];
extern const unsigned int kOnetextureProgramCount;

enum PluginFamilyId {
    kNoTextureFamily = 0,
    kOneTextureFamily,
    kBumpFamily,
    kBumpSpecFamily,
    kBump2Family,
    kBumpSpec2Family,
    kFamilyCount
};

const PluginFamily& Family(PluginFamilyId id);

// The MPLightRender / MultiPassLightRender helpers shared with the plugin bodies.
void* NewMPLightRender();
void* NewMultiPassLightRender(unsigned int a, unsigned int b, unsigned int c, unsigned int d, unsigned int e);
void AddVPProperty(void* lightRender, unsigned int property, unsigned int value, unsigned int which);
void AddTCProperty(void* lightRender, unsigned int property, unsigned int value, unsigned int which);
void AppendLightRenderItem(void* arrayAddress, void* render);
void* VtableCall(void* object, unsigned int slot);

}  // namespace Last
}  // namespace MultiPass
}  // namespace Jet
