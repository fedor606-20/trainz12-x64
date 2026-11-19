#pragma once

// RenderDirectX9_7c scaffold (Wave 2/4, proxy-first, evidence-only).
//
// This header is documentation/placeholder only: it must not define object
// layouts, field offsets, vtable layouts/slot semantics, or D3D9/D3DX/Cg
// behavior. No exports are enabled locally (see implemented.txt); when wired,
// all 41 exports should forward to RenderDirectX9_7c_orig.dll by default.
//
// Evidenced module surface:
// - Plugin entry: JetGetPluginsRenderDirectX9 (ordinal 41, rva 0xfb30).
// - Exported vftables/data:
//     ??_7DisplayDescriptorDirectX9@Jet@@6B@
//     ??_7DisplayTextureDirectX9@Jet@@6B@
//     ?canAutoMip@DisplayTextureDirectX9@Jet@@1_NA
//     ?mipCubeMapSupport@DisplayTextureDirectX9@Jet@@1HA
//     ?mipVolumeMapSupport@DisplayTextureDirectX9@Jet@@1HA
// - Exported classes/method families (demangled names in exports TSV):
//     Jet::DisplayDescriptorDirectX9: ctor/copy/assign/dtor, Initialize,
//       Destroy, vf0.
//     Jet::DisplayTextureDirectX9: ctor/copy/assign/dtor, Create/Destroy,
//       Load/SubLoad/SubloadFromFrame, Bind overloads, CheckFormat, Upload,
//       CreateInstance, GetBitmap/GetSurface/GetDepthSurface/GetDXCubeBuffer/
//       GetDXBuffer/GetAPITexture/GetDevice/GetDisplayDX9/GetJetFormat,
//       ReCreateD3DTextureBuffer/DestroyD3DTextureBuffer,
//       CopyMipToMip/CopySamplerAttributesFrom/RefreshFromChangedSettings,
//       InitInstance.
// - Evidenced internal/named implementation areas (functions/vtnames maps,
//   attribution incomplete): Jet::DisplayDirectX9, StateManagerD3D9,
//   Jet::VertexBufferManagerDirectX9 (+StaticBufferData),
//   Jet::TextureCombinerDirectX9, Jet::VertexProgramDirectX9,
//   Jet::CGShaderLoaderDirectX9, Jet::CGShaderTechniqueDirectX9,
//   Jet::CGShaderConstantDirectX9,
//   Jet::PluginFactoryOf<Jet::DisplayDirectX9,Jet::Display>,
//   Jet::VertexProgramBasic::VPSymbol, Jet::ReferenceCount.
// - Large unattributed Global surface (~260 mapped funcs / ~53 KB) likely holds
//   core DisplayDirectX9/D3D state, format translation, and helpers; must be
//   xref/vtable-attributed before local reimplementation.
// - Evidenced dependency thunks in function map: D3DX9_42.DLL
//   (D3DXCreateEffectFromFileA, D3DXLoadSurfaceFromMemory,
//   D3DXLoadSurfaceFromSurface, D3DXAssembleShader), MSVCR80 runtime/thunks.
//   Full PE imports (esp. d3d9.dll and any cg/cgD3D9 linkage) must be confirmed.
//
// Recovery required before enabling exports:
// - Exact class layouts/sizes/inheritance/MI/vbtable details and vtable slot
//   meanings (vtnames show aliases/shared slots; do not guess ordering).
// - RenderSystem Display/DisplayTexture/combiner/program/buffer-manager/shader
//   contracts and slot semantics (RS U7/U9 in-flight/contract-critical).
// - D3D9 device, COM lifetime/refcounting, device-lost/reset, render-target,
//   surface/texture, format/capability, and state-cache behavior.
// - D3DX and Cg/D3D shader/effect integration behavior and x64 dependency
//   strategy (d3dx9_42 vs d3dx9_43 vs removal).
//
// Namespace placeholders below carry no ABI/layout meaning.

namespace Jet {
// Evidence-backed class names only; no sizes/members/vtables declared here.
class DisplayDescriptorDirectX9;
class DisplayTextureDirectX9;
class DisplayDirectX9;
class VertexBufferManagerDirectX9;
class TextureCombinerDirectX9;
class VertexProgramDirectX9;
class CGShaderLoaderDirectX9;
class CGShaderTechniqueDirectX9;
class CGShaderConstantDirectX9;
class StateManagerD3D9;
} // namespace Jet
