# U9a `Particle`: report

Status: done. `Jet::Particle`, `ViewPortParticle`, `ParticleAlias`, `ParticleStructure` and the
`ITree<Particle>` helpers are implemented in the isolated workspace, **143 of the 149 slice exports** are local and 6
forward. The differential test reports 0 mismatches:

- `diff_r3d_u9a.exe <testdir> 1000` -> `30080 checks, 0 mismatches`
- `tools/check_exports.py` on the built DLL: `2500 exports, TSV 2500; missing/wrong 0, extra 0`
- all 143 names in the fragment are real local code (no forwarded or import-thunk entries; checked from the PE export
  table, first two bytes `FF 25` would be an import thunk).

## Files

- `src/jet/include/jet/Render3DCoreParticle.h` — the four classes, the local `ITree<T>`/`Iterator`, `ReferencePoint`
  (0x20) and the list typedefs. `Render3DCore.h` and the shared headers were not edited.
- `src/jet/graphics/Render3DCore_7c/Particle.cpp` — ctor/copy/assign/dtor, `InitInstance`, `CreateInstance`, bounds,
  the local/world transform chain, the tree operations, traversal, cull/visibility forwarding, `GetModelView`.
- `src/jet/graphics/Render3DCore_7c/ViewPortParticle.cpp` — render flags and the alias pointer array
  `{count, capacity, data}` (0x10075af0/0x10075aa0/0x10075a40/0x1002ef90), ctor/copy/assign/dtor, `CreateInstance`.
- `src/jet/graphics/Render3DCore_7c/ParticleAlias.cpp` — target pointer (+0x13c), `AssumeIdentity`, `SetAliasTo`,
  `Unalias`, copy/assign/dtor, cull/visibility.
- `src/jet/graphics/Render3DCore_7c/ParticleStructure.cpp` — reference-point list at +0x13c, attachment points,
  `SetReferencePointList`, `InitInstance`/`CreateInstance`.
- `src/jet/tests/diff_r3d_u9a.cpp` — own-vs-original differential test, raw bytes/floats, same object addresses for
  both sides.
- `src/jet/graphics/Render3DCore_7c/implemented.d/u9a_particle.txt` — 143 mangled names.

## Implemented exports (143)

Fragment: `implemented.d/u9a_particle.txt`. `implemented.txt` was not touched; the merge with the current main
`implemented.txt` gives 168 implemented exports (25 U1a + 143 U9a). Group counts: `Particle` 101, `ParticleAlias` 15,
`ParticleStructure` 14, `ViewPortParticle` 13 (the four `??_7…` vftables included).

- all constructors, copy constructors, assignments and destructors of the four classes;
- `Particle`: `CreateInstance`, `GetID`/`IsOfType`, name, parent, position/orientation (raw and checked setters),
  reference frame, enabled/cull-size flags, opacity, bounds (`GetBoundingSphere`, AABB, OBB, global sphere and
  centre), `GetBoundingVolume`/`SetBoundingVolume`, model view, translate/rotate/self-rotate, point-at, all
  `LocalToGlobal`/`GlobalToLocal`/`LocalToLocal` overloads, the tree operations (`Add`/`Push`/`InsertBefore`/`After`/
  `Remove` x4/`Pop`/`RemoveAll`/`RemoveAllChildren`/`DeleteAll`), `FindByName`, `EnumerateParticles`,
  `FindParentContainer`, `IsWithin`, the four `GetAllWithin`, `CheckVisible`, `Cull`, `RenderDebug`, `GetCell`,
  `PushToCellRoot`;
- `ViewPortParticle`: `CreateInstance`, `CheckVisible`, aliases and render flags;
- `ParticleAlias`: constructors, `CreateInstance`, `GetID`/`IsOfType`, `CheckVisible`, `Cull`, `GetAliasTo`,
  `SetAliasTo`, `AssumeIdentity`, `Unalias`, `InitInstance`;
- `ParticleStructure`: ctors/assign/dtor, `CreateInstance`, `GetID`/`IsOfType`, reference-point list access/set/add,
  `AddAttachmentPoint`, `InitInstance`.

The four exported vftables (`??_7Particle`, `??_7ViewPortParticle`, `??_7ParticleAlias`, `??_7ParticleStructure`)
are the local ones. Slot-for-slot verification against the original passed for slots 6..47 (34/35 are the two
forwarded `TestCollision*` wrappers, which are local stubs in the vtable).

## Forwarded exports (6) and why

| export | reason |
|---|---|
| `?TestCollision@Particle@Jet@@UAA_NABVRay@2@AAVCollisionResult@2@PBV12@@Z` | needs `Ray`/`CollisionResult` internals (other units). The vtable entry is a local stub that calls the original export, so dispatch behaviour is the original's. |
| `?TestCollisionGeometry@Particle@Jet@@UBA_NABVRay@2@AAMAAVConstFaceDescriptor@2@@Z` | same: `Ray`/`ConstFaceDescriptor`. |
| `?SetCell@Particle@Jet@@QAAXPAVCell@2@@Z` | needs `Cell::RemoveParticle`/`Cell::InsertParticle`; `Cell`/`CellNode` are U9 scene. |
| `?EnumerateAttachmentPoints@ParticleStructure@Jet@@QAA_NAAV?$FastDynamicArray@PAVAttachmentPoint@Jet@@…` | needs `__RTDynamicCast` with the original `AttachmentPoint`/`Particle` RTTI descriptors (U9 scene). |
| `?EnumerateAttachmentPoints@ParticleStructure@Jet@@QBA_NAAV?$FastDynamicArray@PBVAttachmentPoint@Jet@@…` | same const overload. |
| `?SetCurrentReferencePoint@ParticleStructure@Jet@@QAA_NABVPString@2@@Z` | needs the reference-point pose maths (the 0x1001f5c0 helper and the inverse-frame composition); a scene-unit job. |

`AddAttachmentPoint` is local: an `AttachmentPoint` is a `Particle` whose host-name PString sits at +0x12c, which is
all the method reads. `PushToCellRoot` is local and calls the `Cell` vtable slot 0xc0 opaquely.

## Test

Build (isolated workspace; the copy's `CMakeLists.txt` predates the `u9a` entry in the main tree's test loop, so the
test was compiled directly here):

```
cmake -S "D:\universal modder\r3d-agents\u9a\src\jet" -B "D:\universal modder\r3d-agents\u9a\build32" -A Win32
cmake --build "D:\universal modder\r3d-agents\u9a\build32" --config Release --target Render3DCore_7c
cl /nologo /W3 /EHsc /GR /Zc:wchar_t- /Zp8 /O2 /MD /I src\jet\include ^
   src\jet\tests\diff_r3d_u9a.cpp /Fe:build32\Release\diff_r3d_u9a.exe /link /DYNAMICBASE:NO
copy D:\universal modder\testbin\* testdir
ren testdir\Render3DCore_7c.dll Render3DCore_7c_orig.dll
copy build32\Release\Render3DCore_7c.dll testdir
copy (main build) Kernel_7c/KernelStandard_7c/KernelResource_7c/KernelResources_7c/Render2DCore_7c/RenderSystem_7c .dll testdir
diff_r3d_u9a.exe testdir 1000
```

Result: `30080 checks, 0 mismatches` over 1000 rounds; every 7th round uses signed zero, infinities, quiet/signalling
NaNs and non-unit quaternions. The test compares raw object bytes (excluding the module-specific vptr), raw float bits
of all transform results, bounds, tree links, child order, clone fields, reference-point lists, alias arrays,
attachment parenting, `IsWithin`/`GetAllWithin` lists, and the vtable slot map.

The main tree already has `u9a` in the `foreach` that creates `diff_r3d_<stream>` targets, so the merge build will
create `diff_r3d_u9a` automatically.

## Layout and ABI facts

- `Particle` = 0x12c: `Element` (+0 vptr, +4 visited, +8 lastCulledFrame); `ITree<Particle>` at +0xc
  `{count,parent,head,tail,next,prev}` (0x18; `Iterator` is one node pointer); +0x24 cell node; +0x28 `PString name`;
  +0x2c `Vector3 position`; +0x38 `Quaternion orientation`; +0x48 flag byte (bit 0 cull-size check, bit 1 enabled,
  upper bits untouched); +0x4c bounding-volume pointer; +0x50 embedded `ParticleAxisAlignedBoundingBox` (0xa0);
  +0xf0 `OrientedBox` (0x3c).
- `ViewPortParticle` = 0x13c (+0x12c render flags, +0x130 count, +0x134 capacity, +0x138 alias block).
  `ParticleAlias` = 0x140 (+0x13c target). `ParticleStructure` = 0x140 (+0x13c `ReferencePointList`).
  `ReferencePoint` = 0x20 `{PString, Vector3, Quaternion}`.
- Virtuals are **`__cdecl`** (the mangled `U**` and every call site push `this`); constructors, assignment, the
  destructor and the private helpers are `__thiscall`. `SetRawPosition`/`SetRawOrientation` are **protected** in the
  original (`MAA`); declaring them public gives `UAA` and makes the `.def` export resolve to an import thunk from
  `_orig` instead of the local function. The scalar deleting destructor is `__thiscall` with the flags argument.
- Embedded volume layout (as seen through the pointer): sphere at +0x8 (vptr +0x8, centre +0xc, radius +0x18),
  sphere global centre +0x1c, AABB +0x28, OBB +0x40, global position +0x7c, global orientation +0x88, host Particle
  +0x98, instance marker +0x9c. Dirty bits: +4 bits 1 sphere, 2 AABB, 4 OBB, 0x10 global position, 0x20 global
  orientation.
- The constructor only ORs 3 into the flag byte, preserving whatever the upper bits held; the differential test
  therefore compares only the two defined bits on heap clones.
- The copy constructor and assignment copy the **bounding-volume pointer from the source** (+0x4c), as the original
  does; only the embedded storage is copied. Destroying such a copy makes `~Particle` delete the source's embedded
  volume through the scalar deleting destructor. The original behaves this way too; the test does not destroy
  copy-constructed objects (they are stack/static stand-ins). U3/U9 code that clones particles must keep this in mind.
- `Add`/`Push`/`InsertBefore`/`InsertAfter` detach the **incoming child** from its old parent first (not `this`);
  `Remove()` removes the tail, `Pop()` the head; `DeleteAll` uses the child's scalar deleting destructor, so particles
  must come from `Jet::Alloc` (a stack/static child would be passed to `Alloc::Delete`).
- `Cull` walks **child particles**, not view ports: for every child it calls `CheckVisible(view)`, then the child's
  `GetModelView(result, viewMatrix)` and `child->Cull(view, modelView)`. `RenderDebug` forwards to the object's own
  vtable slot 4 (`Element::Render`, with the extra flags argument present on the stack, as the original).
- `GetGlobalBoundingSphere` sets the returned `Sphere`'s vtable to the module's own `Sphere` vtable; only the fields
  can be compared across modules.

## NaN / float determinism (found the hard way)

- `LocalToGlobal(Vector3)` (0x1004be00) and `GlobalToLocal(Vector3)`/`GlobalToLocal(Plane)` use different operand
  groupings; the first version swapped `nqy`/`nqw` and the plane normal row, and a 1-ULP `plane.d` difference came
  from adding the position after the accumulated dot product instead of before. All are now transcribed in the
  assembly's order and match on 20,000 random awkward-input probes.
- `LocalToGlobal(Plane)`/`GlobalToLocal(Plane)` use `0 - x` negations and exact term order; the plane `d` grouping is
  `position + ((a+b)+c)`.
- The OBB matrix build (0x4eb70/0x4f320) is not the same expression as the 4x4 quaternion helper: it uses
  `q*(q*2)` products and a different add order. A dedicated `QuaternionToMatrix3` implements that order.
- **U1c gap**: the local `OrientedBox::Transform` (and its inverse) adds the translation as the second operand
  (`(sum) + t` instead of `t + sum`) and multiplies with the other operand as destination, which changes NaN
  payloads. The original particle code calls the original `OrientedBox::Transform`; our `LocalToGlobal(OrientedBox)`/
  `GlobalToLocal(OrientedBox)` therefore look up the original methods in `Render3DCore_7c_orig.dll` for now. Once U1c
  fixes its operand order this can switch back to `box.Transform`/`InverseTransform`. (Probe: 20k random
  awkward-value boxes matched only after this.)

## Dependencies on other units / what they must implement next

- **U3** (`ParticleBoundingVolume`, `ParticleAxisAlignedBoundingBox`): the embedded volume is built by the original
  `ParticleBoundingVolume(Particle*)`/copy constructor and assignment, the AABB vtable is the original data export,
  and `SetPositionChanged`/`SetOrientationChanged`/`SetPositionOrientationChanged`, `RecalculateGlobal*`,
  `RecalculateSphereGlobalCenter` and the `CreateInstance`/`OnChildAdded`/`OnChildRemoved` vtable slots are called
  through `Render3DCore_7c_orig.dll`. U3 should provide the full 0xa0-byte layout, the dirty-bit policy and the
  `ParticleAxisAlignedBoundingBox` vtable; then the `Bv*`/`OrigProc` block in `Particle.cpp` can be deleted. Keep
  the AABB vtable address identical or the ctor byte tests need re-basing.
- **U9 scene** (`Cell`, `CellNode`, `AttachmentPoint`, `ITree`): `SetCell` needs `Cell::RemoveParticle`/`InsertParticle`;
  `EnumerateAttachmentPoints` needs the two RTTI descriptors; `SetCurrentReferencePoint` needs the reference-point
  pose helper and the inverse frame; `ReferencePoint`/`AttachmentPoint` classes are declared minimally here and
  should move to the scene header (watch for a duplicate `ITree<T>` definition).
- **U8** (`Camera`, `ViewPort`, `ViewPoint`, `DebugViewPort`): `CheckVisible`, `Cull`, `ViewPortParticle::CheckVisible`
  and `RenderDebug` were transcribed from the assembly but cannot be exercised without a real `ViewPoint`/`Display`
  layout. `Cull`'s child traversal and `RenderDebug`'s own-vtable forwarding are confirmed from the assembly.
- **U7** (`Light`, `AmbientLight`, …): likely shares the `Element` base; no direct symbol dependency from U9a.
- **U10** (`RenderPrimitive` etc.): `ParticleBoundingVolume::CheckRenderableObjectPosition` and the renderable
  creation are U3/U10-owned; U9a only calls the volume's vtable.
