# U9b wave 4 `scene remainder`: report

Status: done. All 34 scene exports that were still forwarded are now local definitions; none of this slice forwards
any more. U9b is 200/209 local; the remaining 9 are `SkeletonModifier`/`BoneBlender` methods of U9b2, not this
slice. The module builds and the export audit is clean.

- `diff_r3d_u9b_rem.exe <dir> 1000` -> `8000 checks, 0 mismatches`
- `diff_r3d_u9b.exe <dir> 1000` -> `2060 checks, 0 mismatches`
- `diff_r3d_u9b2.exe <dir> 1000` -> `82726 checks, 0 mismatches`
- `diff_r3d_u3.exe <dir> 1000` -> `282000 checks, 0 mismatches`
- `diff_r3d_u3b.exe <dir> 1000` -> `158000 checks, 0 mismatches`
- `diff_r3d_u8.exe <dir> 1000` -> `155000 checks, 0 mismatches`
- `python tools/check_exports.py build32-r3d-u9b/Release/Render3DCore_7c.dll D:/trainz12-decomp/exports/Render3DCore_7c.tsv`
  -> `DLL 2500 exports, TSV 2500; missing/wrong 0, extra 0`; the merged implemented list has 1794 names.

## Files

- `src/jet/graphics/Render3DCore_7c/U9bRem.cpp` - the new bodies: ReferencePoint, Cell statistics/tree operations,
  Portal `Enter`/`Leave`, PhysicalPortal `Cull`/`IsBackfacing`/`CreateCamera`, Mirror `Cull`, VirtualPortal/Mirror
  state, Corona's cache, and the seven RVA bridges.
- `src/jet/graphics/Render3DCore_7c/implemented.d/u9b_rem.txt` - 34 names (the ReferencePoint ones are aliases to
  the helper class; `stats` and `cache` are data).
- `src/jet/tests/diff_r3d_u9b_rem.cpp` - own vs original ReferencePoint, Cell stats/tree and PhysicalPortal camera
  scenarios on seeded buffers.
- `src/jet/include/jet/Render3DCoreScene.h` - edited: `Cell::Stats`, `GetStat`/`GetStats`/`Reset`/`ResetAll`,
  `AddAllToVisible`/`InsertParticle`/`RemoveParticle`, `static unsigned int* stats`; `Corona::cache`; the
  `ParticleFXCache` forward declaration. No layout changed.
- `src/jet/graphics/Render3DCore_7c/Cell.cpp` - edited: the delegated constructor increments our stats block; the
  old forwarding stubs were removed.
- `src/jet/graphics/Render3DCore_7c/Portals.cpp` - edited: only the state/ctor bodies remain; the camera/display
  bodies are gone from this file.
- `src/jet/graphics/Render3DCore_7c/implemented.d/u9b_scene.txt` - edited: the ReferencePoint copy constructor and
  destructor now alias the exact helper bodies.

No file outside U9b was edited.

## Implemented (27 real bodies + 7 local bridges)

| class | names |
|---|---|
| `ReferencePoint` | default/full/copy ctors, assign, dtor, `Get/SetName`, `Get/SetPosition`, `Get/SetOrientation`, `GetSubNames` (12/12) |
| `Cell` | `Add`, `Remove`, `ObjectMoved`, `InsertParticle`, `RemoveParticle`, `GetStat`, `GetStats`, `Reset`, `ResetAll`, `stats`; `Cull`/`AddAllToVisible` are RVA bridges (12/12) |
| `Portal` | `Enter`, `Leave` (20/20) |
| `PhysicalPortal` | `Cull`, `IsBackfacing`, `CreateCamera`; `Traverse`/`Enter`/`Leave` are RVA bridges (18/18) |
| `VirtualPortal` | `CreateCamera` (RVA bridge; the rest was already local) (12/12) |
| `Mirror` | `Cull`; `CreateCamera` RVA bridge (12/12) |
| `Corona` | `cache` data (6/6) |

The seven bridges are local exports (not `.def` forwarders) that call the original body at
`Render3DCore_7c_orig.dll + RVA`: `Cell::Cull` 0x1c040, `Cell::AddAllToVisible` 0x1c8e0,
`PhysicalPortal::Traverse` 0x53f30, `Enter` 0x542d0, `Leave` 0x54690, `VirtualPortal::CreateCamera` 0x75c10 and
`Mirror::CreateCamera` 0x2bcc0. Their behaviour is exact, but while they are bridges the original touches its own
copies of `Cell::stats`, `Portal::traversed` and `Portal::enableStencilMode`; the local accessors read our block.
That divergence disappears when the octree/display bodies themselves move.

## Facts established

### ReferencePoint

The U9a header's inline bodies are not the original's. The exports are aliased to `Jet::U9bRemRefPoint` members:

- default ctor writes only `name = 0`; position/orientation stay uninitialised (the inline body zero-initialises
  them, hence the separate definition).
- full ctor: sets `name = 0`, refcounts the source node (`PStringCache::Lock`, `node->refs++`,
  `cache->liveRefs++`, `Unlock`), then `fld/fstp` for all three position and four orientation floats.
- copy ctor: refcounts the name (a constructor must not destroy the destination), copies position with MOVs, then
  `fld/fstp` for the whole quaternion (`+0x10..+0x1c`).
- assignment: destroys the old name, refcounts the source, then `fld/fstp` for all seven floats.
- `SetName` destroys and refcounts; `SetPosition`/`SetOrientation` use `fld/fstp`, so a signalling NaN is quieted
  exactly as in the original.
- `GetName`/`GetPosition`/`GetOrientation` return `this`, `+4`, `+0x10`; `ReferencePoint::GetSubNames` is the same
  function as `AttachmentPoint::GetSubNames` (aliased, not duplicated).

### Cell statistics

The exported `stats` symbol is a 4-byte pointer and the original treats the following words as one array. Ours is
the head of an ordered `.data$r3dcstats` block of 19 words: `[0]` live particles, `[1]` cells constructed, `[2]`
particles ever inserted, `[3..18]` cull counters. `GetStat(i)` reads `(&stats)[i]`, `GetStats()` returns `&stats`,
`Reset()` clears 3..18, `ResetAll()` clears 0..18. The delegated `Cell` constructor counts `[1]`/`[2]` in our block
after the original constructor ran (the original incremented its own copy).

### Cell tree operations

`CellNode` is 0x4c bytes: parent +0, two children +4/+8, cell +0xc, split value +0x10, axis +0x14 (-1 leaf),
`FastDynamicArray<Particle*,13>` +0x18, depth +0x1c, subtree counter +0x20, count +0x24, cull state +0x28,
AABB +0x30..+0x44, flags +0x48.

- `InsertParticle`: bounding-volume vtable slot 6 (`+0x18`, OnChildAdded) with the particle, then (leaf only) the
  0x1d760 grow rule, list add, `node->count++`, `cell->particleCount++`, `stats[0]++`.
- 0x1d760 grow: recalculate the volume when `dirtyFlags & 8`, then min/max with the original's comparison
  direction (`COMISS`/`JBE`: a NaN keeps the node's value); an empty node copies the six floats with x87.
- `RemoveParticle`: 0x1d1c0 (remove from node/tree) through a naked ESI thunk. **The original helper clobbers ESI**,
  so the thunk saves/restores it; without that the C++ caller corrupts its own locals. Then slot 5 (`+0x14`,
  OnChildRemoved), 0x1d850 rebuild when the node is a leaf, `particleCount--`, `stats[0]--`.
- `Add`: reads `particle+0x24` (Particle's cell node), moves the particle out of the old cell when needed, then
  `InsertParticle` and stores the returned node. `Remove` returns `oldCell == this`, not "particle was found".
- `ObjectMoved`: slot 5, grow, and when the node changed 0x1d1c0 + list add + count++ (no cell/stats counter).
- Removing the **last** particle from a root makes the original's own `RemoveParticle` crash in this synthetic
  setup (root rebuild on an empty node); the differential test therefore keeps one particle in the cell.

### Portal Enter/Leave

`enterCount` (`+0x12c`) is incremented/decremented. The cache manager is `*(RenderKernel**)(ViewPort+0x18) + 0x6c`
(ViewPort's kernel is at Layer +0x18; the manager sits at kernel +0x6c). `RenderingCacheManager::Enter/Leave` gets
the caller's `Display*`, the `ViewPort*` and `stencil && enableStencilMode`. `Matrix4&` and the two counters are
unused.

### PhysicalPortal camera state

- `IsBackfacing`: identity view -> `Particle::GetModelView`; `f2 = (m8+m0)*0 - m4`, `f3 = (m9+m1)*0 - m5`,
  `f4 = (m10+m2)*0 - m6` (SSE operand order kept); dot = `(cam.z-port.z)*f4 + (cam.y-port.y)*f3 +
  (cam.x-port.x)*f2`, back-facing when the dot is negative.
- `CreateCamera`: writes the identity, adds the portal's cell position (`cellNode->cell`, +0x2c) as
  `m12=x+z*0+y*0`, `m13=y+x*0+z*0`, `m14=z+x*0+y*0`, then the negated destination position as
  `m12 = -dx + (-dz*0) + (-dy*0) + m12`, `m13 = -dy + (-dx*0) + (-dz*0) + m13`,
  `m14 = -dz + (-dx*0) + (-dy*0) + m14`. The `void* data` parameter is the output Camera memory; null returns null.
  The constructor is the still-forwarded export `??0Camera@Jet@@QAE@ABV01@PBVVector3@1@ABVMatrix4@1@@Z` (RVA
  0x182a0), resolved from the original with `GetProcAddress` so the import table stays free of names we export.
- `Cull`: the non-reenterant stamp uses `ViewPort::GetTraversalNumber()`; then `ViewPoint::AddPortal(this, m, 0)` and
  `Particle::Cull`. `Mirror::Cull` is the same with flag 1 and its own backface check.

### ViewPoint

Still a U9 forwarder. `U9bRem.cpp` declares the minimal layout it needs (`camera` at +4) and the two calls it makes
(`AddPortal`, `Update`), which link to the original's exports.

## Test

`diff_r3d_u9b_rem` runs four scenarios per round with both modules:

1. ReferencePoint on seeded buffers: default ctor, full ctor, copy, assign, getters/setters, `GetSubNames`, and
   awkward float rounds (signed zero, infinities, quiet/signalling NaNs) every seventh round. The record compares
   raw bytes including the untouched tail of the default-constructed object.
2. Cell statistics: `ResetAll`, two constructors, `Reset`, all 19 words.
3. Cell tree: two particles with random/awkward positions, Add (twice), Remove, re-Add, Remove the other, node
   count/AABB/list effects, `particleCount` and `stats[0]`.
4. PhysicalPortal camera: the portal is added to a source cell (so the source-position path is taken), then the
   backface bool, warp matrix and the whole constructed Camera are compared for both a data buffer and a null
   buffer, against the original constructor.

Build/run (the shared CMake loop does not list `u9b_rem`; the test is built with a temporary
`-DCMAKE_PROJECT_trainz12_jet_INCLUDE=<include that adds the target>`, or with `cl` directly):

```
cmake --build build32-r3d-u9b --config Release --target Render3DCore_7c
diff_r3d_u9b_rem.exe <testdir with our DLL + Render3DCore_7c_orig.dll> 1000
```

The main `src/jet/CMakeLists.txt` foreach list needs `u9b_rem` added at integration so the target exists in the
normal build; this unit did not edit it.

## Remaining

None for this slice. The only U9b forwards left are nine U9b2 methods (`SkeletonModifier::Add/InsertBefore/
InsertAfter/AddToBoneBlenders/SwitchParticles/Update` and `BoneBlender::UpdateBoneModifiers`); they need the bone
modifier RTTI/blender trees and are outside this task.
