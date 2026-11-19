#pragma once

// Wave 2/3 scaffold for Render3DFoundation_7c (Jet graphics, Trainz 12 / x64 port).
// Evidence sources (read-only):
// - trainz12-x64/notes/agents/Render3DFoundation_7c.md, CONSOLIDATED_PLAN.md
// - D:\trainz12-decomp\exports\Render3DFoundation_7c.tsv (1084 exports, ordinals 1..1084)
// - trainz12-x64/ghidra_maps/jet/graphics/Render3DFoundation_7c.dll.functions.tsv (1642 funcs; 623 Global / 1019 named; 200001 mapped bytes)
// - trainz12-x64/ghidra_maps/jet/graphics/vtnames_Render3DFoundation_7c.dll.tsv (220 slot entries; shared/MI slot evidence)
// STATUS: documentation/placeholders only. No guessed ABI, object layouts, vtable/vbtable/slot order,
// calling conventions, RTTI, adjustor/thunk structure, static/data ownership, or method behavior.
// All signatures/layouts/slots/order/registration semantics must be recovered from original decomp/maps before enabling exports.
//
// Module role (inventory-derived, evidence-driven): mid/high-level 3D foundation above RenderSystem_7c.
// Major evidenced clusters/subsystems (exports/functions/vtnames):
// - Animation/modifier runtime: AnimationEvent, AnimationModifier (+BoneData), AnimationObserver,
//   AnimationResource, AnimationTxtResource, ResourceType<AnimationResource/AnimationTxtResource>,
//   AnimationModifierLoader, Animator, AnimatorChannels, AnimationMaintenanceObserver (vt-named).
// - Movie subsystem: MoviePlayer (plugin-instance-style MI: for MoviePlayer/for PluginInstance + vbtable),
//   MovieStream, MovieJetFileReader (MEDIASUBTYPE_* statics), MovieTextureResource (ResourceObserver + TextureResource vftables),
//   MovieTextureResourceType, MovieTexture, MovieCanvas, RenderTexture.
// - Geometry/influence/particles/config-data: GeometryBaseResource, GeometryConfigData (+ GeometryConfigDataTokens::* exports),
//   GeometryDefinitionBase/Generic, GenericMeshAppData, InfluenceDefinition/Bone/Mesh, ParticleBone,
//   MaterialConfigData, TextureConfigData, MeshOverrides, MeshFileSupport, ObjectLOD, LightTarget, VolumeMesh.
// - Shadows and projection: Shadow, ShadowBlob, ShadowGenerator/BlobGenerator/VolumeGenerator, ShadowCaster,
//   ShadowVolume, ShadowVolumeGeom, ShadowInfluenceMesh, ShadowDarkLayer, ShadowBaseChunk/SWChunk/HWChunk,
//   ShadowProgramStore, ShadowCache (for ReferenceCount/for RenderingCache + vbtable), ShadowTypes,
//   TextureProjector/OccludedTextureProjector (for Particle/for Projector), ProjectorCache.
// - Large/complex vt families noted in maps: ShadowBaseChunk (51), ParticleBone (44), MovieTexture (16),
//   AnimationResource (15), AnimationModifier (13), etc.; slot sharing across chunk/projector/particle/texture/resource families observed.
//
// Evidenced plugin/export entrypoint:
// - JetGetPluginsRender3DFoundation @ ordinal 1084, RVA 0x218a0 (exports TSV), function map: 100218a0, 40 bytes, Global, IMPORTED false.
//   Likely registers resource types/factories (exact set/order/IDs/side effects UNKNOWN; must be recovered from decomp).
//   Declared below but not implemented/enabled in this scaffold.
//
// Dependency/coordination notes (evidence/hypotheses from reports): hard coupling to RenderSystem_7c and Kernel*
// (Display/RenderKernel/ResourceDatabase/Texture* /Material/GeometryChunk/VertexProgram/LayerGroup/Particle/SkeletonModifier/
// Light/Camera/ViewPort/math/PString/ConfigData/OS/ResultClass/File, plugin/resource bases). Relationships with Render3DCore/3DExtension,
// render backends, DSMoviePlayer, WaterSystem, and game consumers require import/xref/decomp confirmation. RenderSystem U7 is in-flight;
// align only to stable/exported contracts, do not modify shared/RS-owned files.

namespace Jet {

// Future implementation units (planning hypothesis from notes/agents/Render3DFoundation_7c.md):
// U0 proxy/bootstrap+ABI plumbing (separate claim), U1 tokens/value data, U2 config-data/parsers,
// U3 influence/mesh helpers, U4 animation core/resources, U5 animator/controllers, U6 movie I/O/playback,
// U7 movie textures/canvas/render target, U8 projectors/caches, U9 shadow primitives/programs/chunks,
// U10 shadow geometry/generation/cache/casters, U11 particles/remaining glue/Global attribution,
// U12 plugin registration/final statics. Refine after decomp/import/xref/layout recovery.

}  // namespace Jet

// Plugin entrypoint (evidenced export). ABI: unmangled extern "C" symbol; exact calling convention/side effects TBD.
// TODO: recover and implement registration before enabling in implemented.txt/build wiring.
extern "C" void JetGetPluginsRender3DFoundation();
