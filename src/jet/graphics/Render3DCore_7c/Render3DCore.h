#pragma once

// Wave 2 scaffold for Render3DCore_7c (Jet graphics, Trainz 12 / x64 port).
// Evidence sources (read-only):
// - trainz12-x64/notes/agents/Render3DCore_7c.md, CONSOLIDATED_PLAN.md
// - D:\trainz12-decomp\exports\Render3DCore_7c.tsv (2501 lines incl header => 2500 exports, ordinals 1..2500)
// - trainz12-x64/ghidra_maps/jet/graphics/Render3DCore_7c.dll.functions.tsv (3361 lines incl header => ~3360 funcs, ~802 Global)
// - trainz12-x64/ghidra_maps/jet/graphics/vtnames_Render3DCore_7c.dll.tsv (vftable/slot grouping evidence)
// STATUS: documentation/placeholders only. No guessed ABI, object layouts, vtable/slot order,
// calling conventions, MI/vbtable structure, RTTI, static ownership, or method behavior.
// All signatures/layouts/slots/order must be recovered from original decomp/maps before enabling exports.
//
// Module role (hypothesis, evidence-driven): core 3D runtime above RenderSystem_7c used by 3D scene,
// camera/viewport, lights, particles/cells/portals, materials/plugins, render caches, geometry chunks,
// math/primitives, splines, and render primitives/debug helpers. Strong dependency on RenderSystem_7c
// contracts (Display/Material/Texture/GeometryChunk/VertexType/RenderKernel etc.) and coordination with
// Render3DFoundation_7c, Render3DExtension_7c, RenderOpenGL_7c/RenderDirectX9_7c, WaterSystem_7c is required.
//
// Evidenced plugin/export entrypoint:
// - JetGetPluginsRender3DCore @ ordinal 2500, RVA 0x608e0 (exports TSV), function map: 100608e0, 80 bytes, Global.
//   Expected to register Render3DCore material/plugin/factory types with Jet plugin infrastructure
//   (exact registrations/IDs/order/side effects UNKNOWN; must be recovered from decomp). Declared below,
//   but not implemented/enabled in this scaffold.

namespace Jet {

// ---- Major class/system clusters to implement later (names evidenced in exports/functions/vtnames) ----
// 1) Plugin/material registration:
//    JetGetPluginsRender3DCore; StdMaterialPlugin/StdMaterialPluginTemplate; *MaterialPlugin types
//    (AutoTex, ColorOneTexture, GenericShader, Gloss, MVHSelect, MultiPassBump/Spec/NoTexture/OneTexture,
//    NoTexture, OneTexture/OneTextureNoAlpha, Reflective, etc.) and MaterialPluginX.
// 2) Materials and multipass/render-state helpers:
//    AutoTexMaterial, BillboardMaterial, ColorOneTextureMaterial, GenericShaderMaterial, GlossMaterial,
//    MVHSelectMaterial, NoTextureMaterial, OneTexture/NoTextureMaterial, ReflectiveMaterial,
//    MultiPass*Material families, MultiPassRenderState/MPLightRender/MultiPass*Chunk* helpers.
// 3) Geometry chunks and vertex specializations (large template surface):
//    GenericGeometryChunkSingleStream/MultiStream, GeometryChunkTemplate<PackedVertex*, ...>,
//    GeometryChunkNmlTemplate<PackedVertex*, ...> for many PackedVertex* layouts/stream kinds.
// 4) Rendering caches / sorting / singleton-like state:
//    AlphaBlendCache (+CacheBucket), UnsortedAlphaBlendCache, AlphaMaskCache,
//    MaterialChunkCache, GenericMaterialChunkCache, ParticleFXCache, ProjectorCache; exported statics/singletons.
// 5) Camera, viewport, and view/culling core:
//    Camera, ViewPort, DebugViewPort, ViewPoint/ViewPortParticle-related view traversal helpers.
// 6) Scene graph / particles / spatial / traversal:
//    Particle, ParticleStructure, ParticleAlias, AttachmentPoint/AttachmentObserver, ITree<Particle>,
//    ReferencePoint, Location/Transformation helpers, Cell, Portal/PhysicalPortal/VirtualPortal, Mirror,
//    bounding volumes (ParticleBoundingVolume, ParticleAxisAlignedBoundingBox, ParticleBoundingSphere,
//    ParticleOrientedBoundingBox, ParticleVisibleBoundingBox).
// 7) Lights:
//    Light, AmbientLight, OmniLight, ParallelLight, SpotLight, CubeMapLight.
// 8) Math/geometry primitives:
//    AxisAlignedBox, OrientedBox, Plane, Ray, Line/LineSegment(/2D), Sphere, Cylinder, Circle(/2D),
//    Ellipse, Ellipsoid, Polygon/Polygon2D, Primitive3D/Geometry, Function, Facility.
// 9) Splines/resources:
//    Spline/SplineSegment/SplineDefinition/SplineResource, BSpline/Bezier/Cardinal/Hermite/TCB variants.
// 10) Render/debug primitives:
//     RenderPrimitive/Render2DPrimitive/Render3DPrimitive and RenderBox/Sphere/Circle/Cone/Cylinder/Ellipse/
//     Ellipsoid/Line(s)/Polyline/Plane/Polygon/Spline/Tube, DebugRender.
// 11) Background loading / async:
//     BackgroundLoadingThread, BackgroundResource and associated state tables/observers.
// 12) Misc/exported statics and globals:
//     Corona/cache-related data, RenderPrimitive::lights, other exported singletons/tables; ownership ABI-sensitive.
//
// Implementation/deferred notes:
// - 802 mapped functions are Global/unattributed (thunks/helpers/template emission/unknown ownership) and must be
//   classified before ownership claims.
// - Many classes have shared/complex vftables and possible MI; vtnames show shared slot groups across families.
// - Exported data/singletons and thread/state-machine behavior must remain original-owned/forwarded until verified.

}  // namespace Jet

// Plugin entrypoint (evidenced export). ABI: unmangled extern "C" symbol; exact calling convention/side effects TBD.
// TODO: recover and implement registration before enabling in implemented.txt/build wiring.
extern "C" void JetGetPluginsRender3DCore();