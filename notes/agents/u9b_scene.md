# U9b `scene`: report

Status: done for the scene state that does not need U3/U8/Display. **118 of the 209 slice exports are local**, 91
forward to `Render3DCore_7c_orig.dll`. The differential test reports 0 mismatches over 1000 rounds, all 118 fragment
names are local code/data (no forwarder), and the module still passes the export audit.

- `diff_r3d_u9b.exe <testdir> 1000` -> `2060 checks, 0 mismatches`
- `python tools/check_exports.py build32/Release/Render3DCore_7c.dll D:/trainz12-decomp/exports/Render3DCore_7c.tsv`
  -> `DLL 2500 exports, TSV 2500; missing/wrong 0, extra 0`
- fragment audit: `118 names, missing 0, forwarded 0` (all point inside our DLL)

## Files

- `src/jet/include/jet/Render3DCoreScene.h` - `AttachmentObserver`, `AttachmentPoint`, `Cell`, `Portal`,
  `PhysicalPortal`, `VirtualPortal`, `Mirror`, `Location`, `Corona`, `Function`; the U9a header's `ReferencePoint`
  is used as is. Layouts are static-asserted.
- `src/jet/graphics/Render3DCore_7c/AttachmentPoint.cpp` - observer list, attachment point constructors/copy/assign/
  dtor, notifications, `GetSubNames`, `CreateInstance`/`InitInstance`.
- `src/jet/graphics/Render3DCore_7c/Cell.cpp` - copy/assign/dtor, `GetExtents`, IDs; the ctor delegates the private
  `CellAxisAlignedBoundingBox` allocation to the original constructor (see Blockers).
- `src/jet/graphics/Render3DCore_7c/Portals.cpp` - `Portal`/`PhysicalPortal`/`VirtualPortal`/`Mirror` state,
  counters, stencil, sizes, destinations, warp matrix/reference; vtable stubs to the original for the camera/display
  bodies.
- `src/jet/graphics/Render3DCore_7c/SceneMisc.cpp` - `Location`, `Corona`, `Function`, and the out-of-line
  `ReferencePoint` members the U9a header keeps inline.
- `src/jet/tests/diff_r3d_u9b.cpp` - own vs original, same static object addresses for both sides, raw byte compare.
- `src/jet/graphics/Render3DCore_7c/implemented.d/u9b_scene.txt` - 118 mangled names.

No shared file was edited. The isolated copy's `CMakeLists.txt` test loop predates `u9b`, so for local verification
only the test target was added through
`-DCMAKE_PROJECT_trainz12_jet_INCLUDE=<tmp>/u9b_target.cmake` (a `cmake_language(DEFER)` that calls
`add_executable(diff_r3d_u9b ...)`). The main tree already has `u9b` in the loop, so the merge needs no CMake change.

## Implemented (118)

| class | local/total | names |
|---|---:|---|
| `AttachmentObserver` | 7/7 | ctors (default/copy/point), assign, dtor, `ShutDown`, vtable |
| `AttachmentPoint` | 27/27 | 4 ctors, assign, dtor, vtable, `Add` (2), `Push`, `InsertBefore/After`, `Remove` (3), `Pop`, `AddObserver`, `RemoveObserver`, `ClearObservers`, `NotifyObservers`, `CreateInstance`, `InitInstance`, `GetID`, `IsOfType`, `GetHostName`, `SetHostName`, `GetSubNames` |
| `ReferencePoint` | 9/12 | copy ctor, assign, dtor, `Get/SetName`, `Get/SetPosition`, `Get/SetOrientation` (the U9a header's inline definitions) |
| `Cell` | 9/21 | ctors (default/copy), assign, dtor, vtable, `GetExtents`, `GetID`, `IsOfType`, `RenderDebug` |
| `Portal` | 18/20 | ctors/copy/assign/dtor, vtable, `GetID`, `IsOfType`, `SetStencil`, `GetStencil`, `Get/SetEnableGlobalStencil`, `GetTotal`, `GetTraversed`, `Traverse`, `Reset`, the 3 exported statics |
| `PhysicalPortal` | 12/18 | ctors/copy/assign/dtor, vtable, `GetID`, `IsOfType`, `GetSize`, `Get/SetNonReenterant`, `GetDestination` (2) |
| `VirtualPortal` | 11/12 | ctors/copy/assign/dtor, vtable, `GetID`, `IsOfType`, `Get/SetReference`, `SetWarpMatrix` |
| `Mirror` | 10/12 | ctors/copy/assign/dtor, vtable, `GetID`, `IsOfType`, `Get/SetReference` (the original's shared stubs) |
| `Location` | 6/6 | 3 ctors, assign, `Set`, `GetGlobalLocation` |
| `Corona` | 5/6 | ctor, dtor, assign, `Cull`, `Render` |
| `Function` | 4/4 | 2 ctors, assign, vtable |

## Forwarded (91) and why

- **`BoneModifier` 18, `BoneBlender` 19, `SkeletonModifier` 27** - deferred. `BoneModifier`'s vtable has 23+ slots
  with unexported virtuals (slots 8/9 and 17-20 are the shared "return 0" stubs), `SkeletonModifier` is the
  ModifierList+Modifier multiple inheritance with `modifierListLock`, the RenderKernel/RTTI paths and the blender
  trees, and `BoneBlender` needs the priority-ordered insert variants and `UpdateBoneModifiers`. Their exports,
  vtables and the `BoneModifier::nullString` data stay the original's; nothing in the implemented scene code calls
  them.
- **`Cell` 12** - `Add`, `Remove`, `InsertParticle`, `RemoveParticle`, `ObjectMoved`, `AddAllToVisible`, `Cull`,
  `GetStat`, `GetStats`, `Reset`, `ResetAll`, `stats`. These need U3's `ParticleBoundingVolume` dirty-flag helpers,
  U8's `ViewPoint` traversal, and the CellNode tree helpers `0x1d760`/`0x1d850`/`0x1d1c0`. The stats block stays the
  original's because the delegated `Cell` constructor increments it there; implementing only the stats accessors
  locally would read a zeroed copy.
- **`Portal` 2** - `Enter`/`Leave`: display stencil setup and `RenderingCacheManager::Enter/Leave` through a ViewPort.
  The vtable slots are local stubs that call the original exports. Until these bodies move, the forwarded display
  path reads the original module's `enableStencilMode` copy, while the local `GetStencil`/`SetEnableGlobalStencil`
  accessors read ours; the test only exercises the local accessors.
- **`PhysicalPortal` 6** - `Traverse`, `Cull`, `Enter`, `Leave`, `CreateCamera`, `IsBackfacing`: Camera/ViewPoint/
  ViewPort/Display state. Same local vtable stubs. `PhysicalPortal::Traverse` is one of these, so a physical-portal
  traversal increments the original module's `traversed`, not the local counter returned by the implemented
  `Portal::GetTraversed`; a base `Portal::Traverse` does use the local counter.
- **`Mirror` 2** - `Cull`, `CreateCamera`: same U8/Display dependency.
- **`ReferencePoint` 3** - default ctor, `(PString,Vector3,Quaternion)` ctor, and `GetSubNames`. The class body is
  owned by the U9a header and cannot be extended here; the two ctors could be exported through `implemented.d`
  aliases later, and `GetSubNames` is the same static as `AttachmentPoint::GetSubNames` (already local).
- **`Corona` 1** - the `cache` static data (`ParticleFXCache*`) stays the original's.

## Layout and ABI facts

- `AttachmentObserver` 8 bytes: vtable +0, `AttachmentPoint*` +4. Slots 1..3 are the original's shared stubs
  (added/removed/shut-down callbacks); the exported `ShutDown` is non-virtual and calls slot 3 after clearing +4.
- `AttachmentPoint` 0x134: `Particle` 0x12c, host-name `PString` +0x12c, `FastDynamicArray<AttachmentObserver*,13>`
  +0x130. New virtual slots 48 `AddObserver`, 49 `RemoveObserver`, 50 `ClearObservers`, 51 `NotifyObservers`.
- `Cell` 0x158: own `CellNode*` +0x12c, particle count +0x130, copied-but-unused word +0x134, traversal ring
  `{count +0x138, capacity +0x13c, data +0x140, read +0x144, write +0x148}`, `Array<CellNode*>` `{count +0x14c,
  capacity +0x150, data +0x154}`. New virtual slot 48 `ObjectMoved`.
- `Portal` 0x134: enter count +0x12c (the base ctor leaves it uninitialised), stencil byte +0x130. New virtual slots
  48 `Traverse`, 49 `Enter`, 50 `Leave`, 51 `CreateCamera`. Statics `total`, `traversed`,
  `enableStencilMode` (the original's initial byte is **1**).
- `PhysicalPortal` 0x148: size +0x134, destination cell +0x13c, traversal stamp +0x140, non-reenterant +0x144.
  The constructor sets the stencil byte to 1.
- `VirtualPortal`/`Mirror` 0x18c: warp matrix +0x148..+0x188, reference particle +0x188. The ctor sets the identity
  matrix but leaves +0x188 **uninitialised**; `Mirror::GetReference` is the original's shared "xor eax,eax" stub and
  `SetReference` is a shared empty stub.
- `Location` 0x10 (Vector3 +0, Particle* +0xc), `Corona` 0x18 (six floats, no vtable), `Function` 4 bytes (one
  vtable slot, default returns 0).
- Calling conventions: ctors/assignment/dtors `__thiscall`; ordinary members and virtuals `__cdecl` (this on the
  stack). `Cell::GetExtents` and `Location::GetGlobalLocation` return by value through the hidden slot after `this`;
  `ITree<Particle>::Iterator` is the U9a non-POD 4-byte iterator with a hidden return slot.
- Odd original behaviours replicated: `AttachmentPoint::Remove(Iterator&)` removes the **tail**, returns the removed
  tail and notifies with the iterator's node (not the removed one). `GetSubNames` returns the post-`/` tail as the
  first name and `'b' + head-minus-first-char` as the second; without a slash the second is `'b' + name[1..]` (the
  decompiler showed the concat operands reversed). Copies/assignments of `Cell`/Portal objects share the source's
  bounding volume, so the test destroys only the owning objects.
- Blocked calls to our own forwarded names (the Cell constructor, the Cell/Portal/Mirror vtable stubs) resolve the
  original export through `GetProcAddress` on `Render3DCore_7c_orig.dll`, as U9a does for the embedded bounding
  volume. This keeps the static import table free of names we also export.

## NaN / float handling

`Location`'s ctors/`Set`, `AttachmentPoint`'s `(name,host,pos,quat)` ctor and `Cell::GetExtents` copy floats through
`fld`/`fstp`, which quiets a signalling NaN exactly like the original; `Corona` and the Portal fields are plain MOVs.
The test runs an awkward round every seventh iteration with `+0/-0/+inf/-inf/qNaN/qNaN-payload/sNaN` and compares the
raw object and return bytes.

## Test

```
cmake -S "D:\universal modder\r3d-agents\u9b\src\jet" -B "D:\universal modder\r3d-agents\u9b\build32" -A Win32 \
  -DCMAKE_PROJECT_trainz12_jet_INCLUDE=C:/Users/Fedor/AppData/Local/Temp/opencode/u9b_target.cmake
cmake --build "D:\universal modder\r3d-agents\u9b\build32" --config Release --target Render3DCore_7c diff_r3d_u9b
copy D:\universal modder\testbin\* testdir
ren testdir\Render3DCore_7c.dll Render3DCore_7c_orig.dll
copy build32\Release\Render3DCore_7c.dll testdir
copy (main build) Kernel_7c/KernelStandard_7c/KernelResource_7c/KernelResources_7c/Render2DCore_7c/RenderSystem_7c .dll testdir
set PATH=testdir;%PATH%
diff_r3d_u9b.exe testdir 1000   -> 2060 checks, 0 mismatches
```

The test covers: AttachmentPoint ctors/copy/assign/tree operations/named add/observers (with a test vtable counting
added/removed/shut-down callbacks)/InitInstance/CreateInstance/GetSubNames including slash names, ReferencePoint
members, Location with/without a particle frame, Corona/Function, Portal counters/stencil/traverse, PhysicalPortal/
VirtualPortal/Mirror state and references, and Cell node/volume construction, extents, copy/assign, plus vtable slot
checks (new slots and `GetID` slot, which also proves the delegated `Cell` constructor restored our vtable).

## Blockers / follow-ups

- `Cell`'s default constructor calls the original `??0Cell` (via `GetProcAddress`) because it allocates a private
  `Cell::CellAxisAlignedBoundingBox`, a U3 subclass with no exported constructor. Our vtable is saved and restored
  around that call; the destructor, copy and assignment are local, and the node-tree destructor (`0x1bc70`) is
  replicated. When U3 lands, replace the delegated ctor and implement the remaining Cell traversal/node methods.
- The Portal camera/display bodies need U8 (`Camera`, `ViewPoint`, `ViewPort`) and the Display API; `Portal::Enter`/
  `Leave` additionally need the `RenderingCacheManager` transition. The local vtable stubs keep dispatch correct
  until then.
- Bone modifiers need `BoneModifier`'s full vtable map (its unexported slots) and the SkeletonModifier/ModifierList
  lock path; defer to a later pass.
