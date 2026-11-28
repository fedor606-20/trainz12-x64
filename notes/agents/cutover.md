# Render3DCore_7c cutover: removing the runtime dependency on the original
Status 2026-11-27. The export layer is complete (2500/2500, 0 forwarders). This note tracks the remaining
runtime links: every place that resolves a name, address or vtable from `Render3DCore_7c_orig.dll`.

Inventory: `python tools/r3d_cutover_inv.py` (names and RVAs) and `python tools/r3d_dyn_inv.py` (all lines that
mention the original module). The current DLL has **no static imports** from the original; everything left is a
dynamic lookup.

## Batch 1 (2026-11-27): cross-unit calls replaced with local calls

These call sites went through the original only because the owning unit was written later. They now call the
local implementation directly:

- `Particle.cpp`: `BvConstruct`, `BvConstructCopy`, `BvAssign`, `BvSetPositionOrientationChanged`,
  `TransformBox`, `InverseTransformBox`.
- `Camera.cpp`: the three bounding-volume notification helpers.
- `Light.cpp`: the bounding-volume notification helpers (the four-argument one was dead code).
- `ParticleBoundingVolume.cpp`: `TransformOrig`, `InitOrig`, `MaximizeOrig`.
- `ParticleAxisAlignedBoundingBox.cpp`, `ParticleOrientedBoundingBox.cpp`: aligned-box conversion and maximize.
- `ViewPort.cpp`: embedded `ViewPoint` assignment/destruction (includes `SimpleRemain.h`).

Two real argument-order bugs were found and fixed while doing this. `Camera::SetPosition`,
`Camera::SetOrientation` and `Camera::SetReferenceFrame` passed `(next, previous)` into helpers whose local
bodies expect `(old, new)`, so the embedded volume moved by the wrong delta (the camera fields themselves were
correct, which is why the byte comparison of the camera never saw it). The original resolves the callee as
`SetPositionOrientationChanged(bv, oldPos, newPos, oldQ, newQ)`; the callers now pass that order.

`OrientedBox::Init` is protected, so `Render3DCoreOrientedBox.h` gained a small `OrientedBoxAccess::Init`
adapter instead of another friend declaration.

`Cell::Cell` (0x1001bd20) is local now: the 0x4c node, the stats increments and the private
`CellAxisAlignedBoundingBox` (0xdc, only `RecalculateAxisAlignedBoxVolume` overridden). `SplineSegment::GetRatio`
stays forwarded: the local body is one ulp off in about 10% of `diff_r3d_tails2` cases, so the residual is still
open.

Batch 2 (same day) made eight `Camera` entries local: `Translate`, both `Rotate` overloads, both `SelfRotate`
overloads, `Perspective`, `UpdateCachedViewTransform` and `AlignToTarget`. `diff_r3d_u8` now compares the bounding
volume through the pointer at camera+0x4c, calls `UpdateCachedViewTransform` directly and reports NaN-payload-only
float differences separately (the local `AlignToTarget` matches the original except for NaN payloads). The
remaining six Camera entries are `GetClippingPlanes`, `Cull`, `Orbit`, `UpdateViewCone`,
`UpdateGlobalViewTransform` and `RenderDebug`.

`tools/r3d_mutate.py` is the mutation self-check: it patches one catalogued line, rebuilds the module, runs the
tests that should notice, and reports KILLED/SURVIVED after restoring the source and DLL. The first run found
three real test gaps (camera volume deltas, particle volume deltas, the cell extent copy); the tests and the u8/u9a/
u9b_rem fixtures were extended until all of them are killed.

Tests must now accept that objects built by our constructors carry our vtable words, and that the local box
math differs from the original only in NaN payloads. The affected tests now

- mask dwords that point into either module image (own vtable, the embedded volume's Sphere vtable),
- report NaN-payload-only float differences separately (u3, u3b),
- scan vtables over the module's `.text` range instead of `VirtualProtect`-based executability (u10).

Result: all 35 `diff_r3d_*` suites pass. u3 reports 1961 NaN-payload-only differences, u3b 117, everything else
0 mismatches.

## What is still forwarded (by symbol)

| File | Exports | Notes |
|---|---:|---|
| `Camera.cpp` | 6 | `GetClippingPlanes`, `Cull`, `Orbit`, `UpdateViewCone`, `UpdateGlobalViewTransform`, `RenderDebug` |
| `ViewPort.cpp` | 8 | `RenderLayer`, `Begin`, `End`, `Render`, `Update`, `GetElementAt`, `GetAllWithin` |
| `Bones.cpp` | 8 | `BoneBlender::UpdateBoneModifiers`, `SkeletonModifier::Add/InsertBefore x2/InsertAfter x2/SwitchParticles/AddToBoneBlenders` |
| `Particle.cpp` | 3 | `TestCollisionGeometry`, `TestCollision`, `SetCell` || `ParticleStructure.cpp` | 3 | `EnumerateAttachmentPoints` x2, `SetCurrentReferencePoint` — both blocked on cross-module dependencies: the enumeration is a `__RTDynamicCast` against the original's RTTI descriptors (0x100c12e0 AttachmentPoint / 0x100c127c Particle) and `SetCurrentReferencePoint` (0x10053a30) needs the scene unit's pose maths and the `ReferencePointList` chunk walk. They become local with a cross-module-RTTI story (or an RTTI-free `IsOfType` equivalence check) and a transcribed pose builder (2026-11-29) |
| `PointLights.cpp`, `Light.cpp` | 4 | the four `BatchCalculateDiffuse` bodies |
| `DebugViewPort.cpp` | 2 | `Update`, `Render` |
| `SimpleRemainCamera.cpp` | 3 | the three portal/warp `Camera` constructors |
| `U9bBridge.cpp`, `U9bRem.cpp` | 8 | six original RVAs for `Cell::Cull/AddAllToVisible`, `PhysicalPortal::Traverse/Enter/Leave`, `VirtualPortal::CreateCamera`; two `CellNode` helpers at 0x1d1c0/0x1d850 |

## What is still forwarded (by address or vtable)

- `Particle.cpp`: `AabbVftable`, `BvBaseVftable`, `BvTable` (vtable slot reads) and the `ParticleBoundingVolume`
  vtable words used by `BvCreateInstance/BvDelete`.
- `U5aFinal.cpp`: the static texture-stage descriptors at their original RVAs (4).
- `MultiPassState.cpp`: `Vtable(name)` resolves the original class vtables through `GetProcAddress`.
- `U5aRem.cpp`: the four cache `Add` bodies and the four cache singletons (`OriginalProc` + `OriginalData`).
  The local `Add` bodies exist; the differential test hooks the original's `Add`, so it has to be rewritten
  before this switches to the local singletons.
- `U11Plugins.cpp`: two constructor-less plugin helpers and the shared program-registration helper at 0x60050.
- `SplineSegment.cpp`: `GetRatio` still prefers the original body (local fallback is 1-2 ulp different).
- `RenderPrimitive.cpp`, `RenderPolyline.cpp`, `Polygon2D.cpp`: comments only; no code path left.

## Suggested order

1. `SplineSegment::GetRatio`: reconcile the one-ulp residual (test `diff_r3d_tails2`) and delete the forward.
2. `DebugViewPort::Update/Render`, the `ParticleStructure` three methods.
3. `Particle::TestCollision/TestCollisionGeometry/SetCell` (needs `Ray`/`CollisionResult` internals).
4. `Camera` six remaining; all are local-file methods and `diff_r3d_u8` already builds the fixtures (Cull needs
   the ViewPoint/RenderKernel layout, UpdateViewCone/GetClippingPlanes the frustum math).
5. `ViewPort` eight; `diff_r3d_u8` covers most traversal entry points.
6. `Bones` eight; `diff_r3d_simple_rem`/`u9b2` have the modifier fixtures.
7. The four `BatchCalculateDiffuse` bodies; `u7` has the fixture.
8. `U9b` octree/portal RVAs; the largest remaining piece (private helpers, display state save/restore).
9. Vtables/data: `MultiPassState`, `U5aFinal` descriptors, `U5aRem` caches (rewrite the cache test first),
   `U11Plugins`.
10. Re-run `python tools/compare_vtables.py <orig> <ours>` after every link change.
