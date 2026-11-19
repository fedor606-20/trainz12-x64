#pragma once

// Wave 3 scaffold for Render3DExtension_7c (Jet graphics, Trainz 12 / x64 port).
// Evidence sources (read-only):
// - trainz12-x64/notes/agents/Render3DExtension_7c.md, CONSOLIDATED_PLAN.md
// - D:\trainz12-decomp\exports\Render3DExtension_7c.tsv (186 exports)
// - trainz12-x64/ghidra_maps/jet/graphics/Render3DExtension_7c.dll.functions.tsv (508 mapped funcs, ~39,572 bytes)
// - trainz12-x64/ghidra_maps/jet/graphics/vtnames_Render3DExtension_7c.dll.tsv (shared/MI vftable slot evidence)
// STATUS: documentation/placeholders only. No guessed ABI, object layouts, field offsets, vtable/vbtable/slot order,
// calling conventions, MI/adjustor-thunk structure, RTTI, static ownership/init, or runtime behavior.
// All signatures/layouts/slots/registration semantics must be recovered from original decomp/maps/contracts before
// enabling any export.
//
// Module role (evidence-driven): render-side extension above 3D/render core providing decal, haze/chunk-cache material,
// and flythrough/spline/particle utility clusters, exposed partly via render/material plugin mechanisms.
//
// Evidenced unmangled plugin/export entrypoint:
// - JetGetPluginsRender3DExtension @ ordinal 186, RVA 0xa190 (exports TSV), 1000a190, size 5 (functions TSV), Global.
//   Expected to register Render3DExtension plugins/material-related factories with Jet infrastructure
//   (exact registrations, type strings/UID/cache-ID wiring, ordering, and side effects UNKNOWN; must be recovered).
//   Declared below but not implemented/enabled in this scaffold.
//
// Major clusters/classes to implement later (names evidenced in exports/functions/vtnames; layouts/behavior unknown):
// 1) Decal system:
//    - Jet::Decal: ctor/copy/dtor/op=, InitInstance, Get/SetMaterial, Get/SetGeometryChunk, Get/SetDecalLights,
//      Get/SetTargetTransparent, Cull, Render, AlignGeometryToPoints/Normals, MoveGeometry,
//      ClipGeometryToPolygon/Plane, SetupGeometryIndices, MergeDecalGeometry, NormalizeChunk.
//    - Jet::DecalCache (+ vbase dtor): ctor/dtor, AddDecal, Begin/End/Reset; exported static DecalCacheID.
//    - Jet::DecalParticle, Jet::PointDecal, Jet::ProjectedDecal: ctor/copy/dtor/op=, Init/InitInstance,
//      CreateInstance, Cull, Render, GetID/IsOfType/GetOpacity/SetOpacity (as exported/mapped).
// 2) Haze / atmospheric chunk-caching materials and plugins:
//    - Jet::HazeChunkCache (+ vbase dtor): ctor/dtor/Add/Begin/End/Enter/Leave/Reset/Update, GetExtents,
//      PointVisible/PointsExtents, InitOffsetTexture, SetHazeTexture/ReleaseHazeTexture,
//      SetOffsetScaling/SetOffsetTiling/SetScaleTime/SetScrollSpeed; exported static HazeChunkCacheID.
//    - Jet::PreHazeChunkCache (+ vbase dtor): ctor/dtor/Begin/End/Add/FlushPipe/Reset; exported PreHazeChunkCacheID.
//    - Jet::AlphaBlendCache: mapped vf0/vf1 only (minimal evidence).
//    - Jet::HazeMaterial, Jet::HazeOneTextureMaterial: ctor/dtor/vbase dtors, CreateInstance, Activate, Bind/Unbind,
//      CacheChunk/RenderChunk/RenderNumChunks/RenderPrimitives, CreateGeometryChunk, DuplicateTexture/SetTexture/
//      GetTexture, GetType/operator==/Equals/IsSupported/InitInstance, display state helpers (as exported/mapped);
//      exported DECORATED_TYPE/UNDECORATED_TYPE/UID statics.
//    - Jet::HazeMaterialPlugin, Jet::HazeOneTextureMaterialPlugin: ctor/copy/dtor/op=, IsSupported/CreateInstance
//      and plugin vtable entries (evidenced exports/maps).
// 3) Fly-through / spline / visibility helpers:
//    - Jet::FlyThrough: ctor/copy/dtor/op=, Load/SaveConfigData, Load/SaveFile, InitSplines, Reset,
//      ResetPointArrays, GetNextPoint, AddPoint, path/look-at spline accessors/definitions, speed/looping,
//      TestRender (MODLOG notes debug spline.txt / ConfigData::Write usage; reportedly hardly exercised).
//    - Jet::RenderSpline, Jet::ParticleVisibleBoundingBox: extensive mapped vf slots with small bodies/thunks
//      (exact ownership/semantics unknown).
// 4) Geometry chunk template specializations (evidenced in function/vtable maps):
//    - Jet::GeometryChunkTemplate<Jet::PackedVertexPT, Jet::GeometryChunkSingleStream>
//    - Jet::GeometryChunkNmlTemplate<Jet::PackedVertexPT, Jet::GeometryChunkSingleStream>
//    Determine export/template-instantiation/internal-helper role and required vtable/slot/layout compatibility with
//    RenderSystem/Render3DCore before implementation or ownership.
//
// Evidenced exported data/statics (ABI-sensitive, ownership/init unknown): DecalCacheID, HazeChunkCacheID,
// PreHazeChunkCacheID, HazeMaterial::{DECORATED_TYPE,UNDECORATED_TYPE,UID},
// HazeOneTextureMaterial::{DECORATED_TYPE,UNDECORATED_TYPE,UID}, plus many vtable/vbtable symbols for listed classes.
//
// Dependency/contract prerequisites (must be recovered, not assumed):
// - MODLOG: Render3DExtension imports Render3DCore (~134) and RenderSystem (~111).
// - Likely RenderSystem contracts/types: Display, ViewPort, RenderKernel, Material/MaterialManager/PluginInstance,
//   Texture, GeometryChunk/GeometryChunkCreateHint, DisplayLight, Matrix4/Vector/Quaternion/Plane/Line/Rect,
//   AutoReference/FastDynamicArray/PString/UId/String/ResourceDatabase/ConfigData/ResultClass, rendering cache/
//   material/particle base contracts.
// - Likely Render3DCore contracts/types: Particle base and 3D particle/visibility/spline/render-spline helpers used by
//   DecalParticle/PointDecal/ProjectedDecal/RenderSpline/ParticleVisibleBoundingBox/FlyThrough (exact split unknown).
// - Render3DCore/Render3DFoundation are not started and RenderSystem has in-flight U7 work; treat interfaces/layouts
//   as unstable until coordinated/stable contracts exist.
//
// Runtime/load/validation notes:
// - README/MODLOG: not loaded at menu (re-check in driving/session/editor/specific-content scenarios as applicable).
// - ConfigData::Write debug spline.txt path involving Render3DCore/Render3DExtension reported hardly exercised.

namespace Jet {

// Deferred: declare class APIs/layouts only after decomp-verified signatures, sizes, inheritance/MI, and
// vtable/vbtable slot order. Do not infer member layout or virtual dispatch from names alone.

}  // namespace Jet

// Plugin entrypoint (evidenced unmangled export). Exact calling convention/parameter/return semantics and registration
// side effects TBD from decomp. TODO: recover and implement before enabling in implemented.txt/build wiring.
extern "C" void JetGetPluginsRender3DExtension();
