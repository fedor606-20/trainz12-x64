# U9: WaterTile, Water, WaterTiled, WaterSurface

Status 2026-11-24: **43/43 exports local**, isolated module build clean, differential test 35 checks /
0 mismatches. The shared CMake build of the whole module is currently blocked by other units (see Build).

Owned files:

- `src/jet/include/jet/WaterTile.h`, `Water.h`, `WaterTiled.h`, `WaterSurface.h` (project convention: public
  class headers live in `include/jet`; the brief's `graphics/.../*.h` paths read as the same files)
- `src/jet/graphics/WaterSystem_7c/WaterTile.cpp`, `Water.cpp`, `WaterTiled.cpp`, `WaterSurface.cpp`
- `src/jet/graphics/WaterSystem_7c/WaterUnit9Shell.h` (private view of Render3DCore's Camera, ViewPoint and
  ParticleVisibleBoundingBox; move it when Render3DCore's headers exist)
- `src/jet/graphics/WaterSystem_7c/implemented.d/u9.txt` (43 names)
- `src/jet/tests/diff_ws_u9.cpp` (differential harness, compiled by hand; not in CMakeLists)

## Class shapes (RTTI and assembly)

| Class | Size | Base | vtable |
|---|---:|---|---|
| `WaterTile` | 0x24 | `Element` (RenderSystem) | 10 slots |
| `Water` | 0x134 | `Particle` (Render3DCore) | 53 slots |
| `WaterTiled` | 0x138 | `Particle` (Render3DCore) | 48 slots |
| `WaterSurface` | 0x18 | `WaterGrid<float>` | 4 slots |

**WaterTiled derives Particle, not Water.** The RTTI base array is WaterTiled, Particle, Element, ITree;
its vtable ends at slot 47 (slot 48 is the RTTI/adjacent data, not AddTile), its constructor calls
`Particle::Particle`, and its layout (`tile` at +0x12c, `Size` at +0x130) is not Water's (two arrays at
+0x12c/+0x130). The work brief's "WaterTiled derives Water" is wrong for this binary; the implementation and
the vtable follow the binary.

### WaterTile (0x24)

    +0x00 vftable (Element part: visited +4, lastCulledFrame +8)
    +0x0c Vector3 position
    +0x18 WaterGeometry*
    +0x1c WaterMaterial*
    +0x20 const FastDynamicArray<DisplayLight*,13>*

Vtable: 0 deleting dtor, 1 SetVisited, 2 SetLastCulledFrame, 3 Cull, 4 Render, 5 RenderDebug,
6 CullTile, 7 SetGeometry, 8 SetMaterial, 9 SetPosition. The base slots are the compiler's thunks to
RenderSystem_7c's Element. `GetPosition` is `const` (`QBE`); `GetGeometry`/`GetMaterial`/`SetLight` are
non-const (`QAE`); the virtuals are `UAEX` (thiscall) except `Render` (`UAAX`, cdecl).

- Ctor zeroes position/geometry/material/lights. Copy ctor and assignment addref the geometry directly and the
  material through its `PluginInstance` vbptr.
- Dtor releases material first, then geometry, and leaves the Element vtable (the original does the same).
- SetGeometry/SetMaterial keep the original's four-call reference sequence: AddRef(arg), store, AddRef(new),
  Release(old), Release(arg). Net is +1 on the new object.
- `Render`: `if (geometry) geometry->Render(display, viewport, lights, matrix)` (WaterGeometry slot 4).
- `SetLight` is a bare store of the array pointer at +0x20.

### Water (0x134)

    +0x12c FastDynamicArray<WaterTile*,13> tiles
    +0x130 FastDynamicArray<DisplayLight*,13> lightList

Vtable slots 48..52: AddTile, RemoveTile, SetLight, GetLight, GetAxisAlignedBoundingBox(Vector3&,Vector3&).
Slot 3 Cull is overridden.

- Ctor/copy/assignment: `Particle` first, then the two arrays. The original copy uses the chunk-level helper
  at 0x10004670 (`AppendChunks`, preserves chunk counts); assignment uses 0x10004460 (clear + append, no
  self-assignment guard).
- Dtor order matters: delete every tile through its own vtable slot 0 with flag 1, then free the **light**
  chunks, then the **tile** chunks, then `~Particle`. Verified by allocation/free order.
- AddTile: `tiles.Add(tile)`, then this water's own `GetAxisAlignedBoundingBox`, then
  `boundingVolume->vt[8](AxisAlignedBox(min,max))`. RemoveTile: Find/Remove, then the same box refresh.
- SetLight: null clears the list and returns true; a light whose `+4` type is not `DLIGHT_DEFAULT` (3) clears
  and returns false; a valid light replaces `lightList[0]` when the list is non-empty, otherwise Add. GetLight
  returns `lightList[0]` or null.
- IsEmpty tests the tile-array head.
- Cull: for each tile, `tile->SetLight(&lightList)` (raw store), then
  `tile->CullTile(view, matrix, GetGlobalPosition())`.
- GetAxisAlignedBoundingBox: min/max over each tile's `position + geometry bounds` (position + boundsMax for
  max, boundsMin + position for min), then rotates the eight min/max corners by the particle's quaternion and
  adds its position. The nine rotation terms and the three sums keep the assembly's SSE order; the diff test
  matches every bit. An empty list writes (0,0,0) to both outputs.

### WaterTiled (0x138)

    +0x12c WaterTile* tile
    +0x130 Size size

Vtable 48 slots; Cull at 3, Render at 4.

- Ctor: stores `size`, allocates 0x24 for a WaterTile (Alloc::New, in-place ctor), `SetMaterial(material)` then
  `SetGeometry(geometry)`, allocates 0xdc for a `ParticleVisibleBoundingBox` (Render3DCore ctor with this),
  `SetBoundingVolume`; if it is not adopted, the volume is deleted through `vt[0](volume,1)`.
- Dtor deletes the tile (vt[0] flag 1), then `~Particle` (which frees the volume).
- Copy/assignment are shallow (tile pointer + size), calling the Particle copy/assignment.
- GetTile is **cdecl** (`QAA`), not thiscall.
- Cull is `ViewPoint::Add(this, matrix)`.
- Render: camera = `*(Camera**)(viewport+0x20)`; anchor = trunc(camera position at camera+0x2c, per axis) to
  whole cells of `(geometry.size - 1) * geometry.scale`; loops `-w/2 .. w/2-1` and `-h/2 .. h/2-1`; each cell
  builds the quad `(x0,y0),(x1,y0),(x1,y1),(x0,y1)` at z=0 and repeats it to eight points for
  `Camera::Clip3D(points,8)`; if the result is not BACK it builds `Translate(x0,y0,0) * matrix` with the
  original multiply (0x10005180, per-row operand order), calls `Display vt[0x54](&matrix)`, then
  `tile->Render(display, viewport, matrix)`. The diff test matches every clip point and both matrices bitwise.

### WaterSurface (0x18)

    +0x10 Vector2 scale

Vtable: 0 `WaterGrid<float>::Initialize(const Size&)` (inherited, not overridden), 1 deleting dtor,
2 `Initialize(const Size&, const Vector2&)`, 3 `GetNormal(int,int,Vector3&)`. The destructor is appended after
the base's slot because WaterGrid has no virtual destructor.

- Default ctor sets only `data = null`; width, height and scale are untouched.
- `WaterSurface(const Size&, const Vector2&)` **ignores the scale argument** (the original loads and stores
  `this+0x10/+0x14` to themselves), sets width/height and allocates `w*h*4` with the 64-bit overflow guard
  (`~0u` when the product does not fit 32 bits).
- `Initialize` stores the scale, then width/height, then allocates; it does **not** free the old block (leak
  kept).
- Copy ctor uses `WaterGrid<float>`'s copy ctor and copies scale; assignment calls `WaterGrid<float>::operator=`
  (cdecl) then copies scale. GetNormal is a no-op. GetScale is thiscall non-const (`QAE`).

## Quirks kept

- `WaterTile::SetPosition` and the scale copies use x87 `fld/fstp` (the compiler emits the same for a float
  assignment here); the AABB and CullTile arithmetic is SSE single precision with the assembly's exact operand
  and grouping order.
- `WaterTile::CullTile` writes the matrix translation row as
  `((m8*tz + m4*ty) + m0*tx) + m12`, `... m9/m5/m1 ... m13`, `... m10/m6/m2 ... m14`, and
  `((m15+m11)+m7)+m3` for w. That last value is the column-3 sum, not a point transform; it is kept.
- `WaterTile::CullTile` calls `material->Activate(-1)` after a successful clip, before ViewPoint::Add.
- The tile position (not position+origin) is what the CullTile matrix translation is built from; origin is only
  added for the Clip3D point. (This resolved an apparent decompiler discrepancy.)
- `Water::GetAxisAlignedBoundingBox` uses the tile geometry fields with no null checks, as the original.
- `Water::SetLight`'s light-type test reads the DisplayLight int at +4 (there is no exported GetLightType).
- WaterTiled's camera anchor truncates toward zero (CVTTSS2SI) and recomputes each row/column from its index,
  not by accumulation.

## Build

- Isolated module build (all 490 exports, 43 local, no U9 forwarders):
  compile the four U9 units plus WaterGrid/WaterGeometry/WaterAnimation/PNoise/FrameAnimation/DisplacementHull,
  generate a def from `u9.txt` only, link against KernelStandard, Kernel, RenderSystem and Render3DCore.
  `tools/check_exports.py` reports `DLL 490 exports, TSV 490; missing/wrong 0, extra 0`; a forwarder scan shows
  no U9 name is forwarded. Import table: KernelStandard, Kernel, RenderSystem, Render3DCore, CRT, KERNEL32;
  no WaterSystem_7c_orig.dll import (forwarders are export strings only).
- **CMakeLists.txt change (reported as instructed):** one line added under the WaterSystem links:
  `target_link_libraries(WaterSystem_7c PRIVATE "${IMPLIB}/Render3DCore_7c.lib")`. The Particle methods,
  Camera::Clip3D, ViewPoint::Add, AxisAlignedBox and ParticleVisibleBoundingBox are imported from
  Render3DCore_7c and the module did not link that import library before U9.
- The shared `cmake --build build32-ws-c --config Release --target WaterSystem_7c` now succeeds:
  483 implemented exports from 5 fragments, 7 forwarded (the remaining WaterMaterialFresWave / OneTex
  SetTexture/DuplicateTexture/IsLoaded methods, all U8). `tools/check_exports.py` on the built DLL reports
  `DLL 490 exports, TSV 490; missing/wrong 0, extra 0`; all 43 U9 names are local in that DLL too.

## Differential test

`src/jet/tests/diff_ws_u9.cpp`, run as `diff_ws_u9.exe <testdir-u9>` (testdir has our DLL, the original,
Render3DCore/RenderSystem/Kernel DLLs). It patches each module's Alloc New/Delete, both Camera::Clip3D
overloads and ViewPoint::Add to recording stubs, builds fake geometry/material/display/viewpoint objects, calls
the exported functions with the real conventions and compares logs bitwise.

Result: **35 checks, 0 mismatches**, both against the isolated 43-export DLL and against the shared
`build32-ws-c\Release\WaterSystem_7c.dll`. Scenarios:

- WaterTile: ctor, position, geometry/material reference sequences, copy ctor, assignment, light, dtors,
  Render (geometry Render slot), CullTile inside and outside the clip, vtable override slots.
- Water: ctor, IsEmpty, AddTile/RemoveTile (array shape, AxisAlignedBox refresh, allocation order), box before
  and after removal (all 24 float bits), SetLight accepted/rejected/null and GetLight, Cull with two tiles
  (clip points, Activate(-1), ViewPoint matrices), dtor (tile/light-chunk/tile-chunk free order), vtable slots
  48..52.
- WaterTiled: ctor allocations (0x24, 0xdc), size/tile/material/geometry fields, bbox adoption, Render for a
  2x3 grid (all eight clip points per cell, Display matrix, geometry Render matrix, skip path for cells left of
  the anchor), Cull, dtor frees, GetTile convention.
- WaterSurface: default ctor bytes, size ctor (scale argument ignored, allocation size), Initialize (scale,
  width/height, leaked old block), GetScale pointer, GetNormal no-op, copy, assignment (grid contents + scale),
  dtor, vtable slots 0/2/3.

Evidence gaps / deliberate deviations:

1. `ParticleVisibleBoundingBox` is constructed with Render3DCore_7c's imported constructor and keeps that
   module's real vtable. The original WaterSystem build generated its own local thunk table (0x1f0dc) because
   it had the class declaration; we do not, so the 0xdc block is freed through Render3DCore's Alloc import
   instead of WaterSystem's. Same allocation/free pair, different vtable address and one invisible free in a
   test that hooks only the WaterSystem imports. The test removes that one `free A1` line from the WaterTiled
   log.
2. Water and WaterTiled copy construction/assignment could not be diff-tested in this harness: the imported
   Render3DCore `Particle` copy ctor/assignment corrupts the heap identically in both modules (crash before any
   U9 code runs). The array part is a direct transcription of 0x10004670 (AppendChunks) and 0x10004460
   (clear + AppendChunks, no self guard); WaterTile and WaterSurface assignment are covered by the test.
3. The module-wide CMake build also passes now; the 7 remaining forwards are U8 material methods, not U9.
   `implemented.txt` was not edited, only `implemented.d/u9.txt` was added.
