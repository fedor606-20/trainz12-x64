#pragma once

// Wave 2 scaffold for RenderOpenGL_7c (Jet graphics, Trainz 12 / x64 port, live OpenGL backend).
// Evidence sources (read-only, 2026-11-19):
// - trainz12-x64/notes/agents/RenderOpenGL_7c.md, notes/agents/CONSOLIDATED_PLAN.md
// - D:\trainz12-decomp\exports\RenderOpenGL_7c.tsv (51 exports: ordinals 1..51)
// - trainz12-x64/ghidra_maps/jet/graphics/RenderOpenGL_7c.dll.functions.tsv (1052 mapped function rows, 108 thunks;
//   namespaces incl. Global 537 funcs / 80302 bytes, DisplayOpenGLXTerm 146, DisplayLibraryOpenGL 3, etc.)
// - trainz12-x64/ghidra_maps/jet/graphics/vtnames_RenderOpenGL_7c.dll.tsv (vftable/slot grouping evidence)
// - peer/reference context: notes/RenderSystem.md, MODLOG.md, PORT_PLAN.md
//
// STATUS: documentation/placeholders only. No guessed ABI, object layouts, vtable/slot order, calling conventions,
// RTTI, MI/vbtable structure, static ownership/init, GL/WGL/Cg state behavior, or RenderSystem contract behavior.
// All signatures/layouts/slots/order/semantics must be recovered from original decomp/maps/PE before enabling exports.
//
// Module role (evidenced): OpenGL backend driver for RenderSystem (live renderer at menu). Small exported API (51)
// dominated by DisplayTextureOpenGL, DisplayDescriptorOpenGL/ContextDescriptorOpenGL[WGL] types/vtables/statics and
// JetGetPluginsRenderOpenGL; most implementation is internal and registered via plugin factories/virtual interfaces.
//
// Exported/evidenced entrypoints and data (exports TSV):
// - JetGetPluginsRenderOpenGL @ ord 51, RVA 0x0f7a0 (~60 bytes, Global per function map): backend/plugin registration.
// - vftables: const Jet::DisplayDescriptorOpenGL::`vftable' @ ord 13 RVA 0x27820,
//   const Jet::DisplayTextureOpenGL::`vftable' @ ord 14 RVA 0x27644.
// - DisplayTextureOpenGL static data exports: anisotropySupport (ord 47, RVA 0x3c339), bound (ord 48, RVA 0x3c340),
//   canAutoMip (ord 49, RVA 0x3c33a), numObjects (ord 50, RVA 0x3c33c).
// - Exported methods (demangled in exports TSV; mixed __thiscall/__cdecl, some const getters, pointer/ref returns to
//   internal types OpenGLFormat/PBufferContext/DisplayOpenGL/DisplayInterfaceWGL):
//   ContextDescriptorOpenGL (protected ctor, PlatformType/get/copy), ContextDescriptorOpenGLWGL (ctor/copy,
//   SetContext/GetDeviceContext/GetRenderContext), DisplayDescriptorOpenGL (ctors/dtor/copy, GetContext/GetPlatform/
//   Initialize), DisplayTextureOpenGL (ctors/dtor/copy, Create/Destroy/CreateRenderContext/CreateInstance/Load/
//   SubLoad/SubloadFromFrame/Bind x2/BindTexture/UnbindRenderTexture/SetBasicState/CheckFormat/CopyMipToMip/
//   CopySamplerAttributesFrom/GetBitmap/GetTextureBitmap/GetDataLost/GetJetFormat/GetAPITexture/GetRenderContext/
//   GetDisplay/GetWinInterface/InitInstance/RefreshFromChangedSettings).
//
// Evidenced internal class/system clusters (functions/vtnames; counts/slots/behavior not recovered here):
// - Display core/platform: DisplayOpenGL (13 funcs), DisplayOpenGLXTerm (146), DisplayInterfaceOpenGL/WGL (+ nested
//   WinPBufferContext/SharedContextWGL/PBufferData vfuncs), DisplayLibraryOpenGL/WGL (DisplayLibraryOpenGL has very
//   large routine(s)), DisplaySaveBufferOpenGL, NonRenderContextWGL, GLDisplayContext, shared/PBuffer helpers.
// - Texture backend: DisplayTextureOpenGL (30 funcs) implementing RenderSystem DisplayTexture contract (lifecycle,
//   format mapping, upload/copy/bind/sampler/state, render-texture/PBuffer/context, bitmap readback) - exported.
// - Cg/CgFX shader pipeline: CGShaderLoaderOpenGL (vf0-vf5), CGShaderConstantOpenGL (vf0-vf13), CGShaderTechniqueOpenGL
//   (vf0-vf5). External thunks evidenced: CG.DLL (13: cgGetArrayParameter, cgCreateContext, cgCreateEffectFromFile,
//   cgDestroyEffect, cgDestroyContext, cgGetProfileString, cgGetNamedTechnique, cgValidateTechnique, cgGetTechniqueName,
//   cgGetNamedEffectParameter, cgGetFirstPass, cgSetPassState, cgResetPassState) and CGGL.DLL (15: cgGLSetParameter1f/
//   2f/3f/4f, cgGLSetParameter1fv/3fv/4fv, cgGLSetMatrixParameterfc/fr, cgGLSetMatrixParameterArrayfr, cgGLSetupSampler,
//   cgGLSetManageTextureParameters, cgGLRegisterStates, cgGLSetDebugMode, cgGLGetLatestProfile).
// - Capability/plugin families (with PluginFactoryOf<...> registrations evidenced): TextureCombinerOpenGL{NV,NVfp,ATI,
//   ARB,Plugin}, VertexProgramOpenGL{NV,ARB,Plugin} (+ ProgramId/VPSymbol helpers), VertexBufferManagerOpenGL{ARB,SW,
//   Plugin} (+ StaticBufferData/StaticBuffer helpers).
// - Large Global/unattributed surface (537 funcs) likely includes extension/capability probing, format/state tables and
//   converters, WGL helpers, draw/state utilities, registration glue; attribution required before ownership claims.

namespace Jet {

// TODO(contract recovery, no implementation here): align to RenderSystem_7c abstractions (Display, DisplayTexture,
// DisplayDescriptor/context types, vertex/program/combiner/buffer-manager interfaces, plugin factory/registration,
// RenderKernel/display init/state expectations). RenderSystem U7 is in-flight and U9 is last/contract-critical; do not
// fork headers or assume stable vslots/layouts/statics until coordinated with RS ownership.
//
// TODO(platform/backend): WGL/OpenGL context/device handling, PBuffer/shared/non-render/render-to-texture paths,
// extension selection/fallback, and CgFX effect/technique/pass/profile/parameter behavior must be reconstructed from
// original evidence. SpeedTree shader compilation path (bin/plugins/SpeedTreeShaders/*.fx via Cg) must remain compatible.

}  // namespace Jet

// Evidenced plugin/export entrypoint. ABI: unmangled extern "C" symbol (ord 51, RVA 0x0f7a0); exact calling
// convention/registration side effects/order and RenderSystem interaction are TBD and must be recovered from decomp.
// TODO: recover and implement exact registrations before enabling in implemented.txt/build wiring.
extern "C" void JetGetPluginsRenderOpenGL();