# U1b `Plane`: report

Status: done. `Jet::Plane` is implemented in the isolated workspace, 41 of the 44 exports are local, 3 forward
(`Clip`, `Split`, `Intersect(const Plane&)`), and the differential test reports **0 mismatches**:

- `diff_r3d_u1b.exe <testdir> 1000` -> `90320 checks, 0 mismatches`
- `diff_r3d_u1b.exe <testdir> 5000` -> `450320 checks, 0 mismatches`
- `tools/check_exports.py` on the built DLL: `2500 exports, TSV 2500; missing/wrong 0, extra 0`.

## Implemented exports (41)

Fragment: `src/jet/graphics/Render3DCore_7c/implemented.d/u1b_plane.txt` (one mangled name per line, 41 lines).
`implemented.txt` was not touched; the merge with U1a produces 66 implemented exports.

- constructors: default (`0x4fd0`, leaves the storage untouched), copy (`0x1ec50`, four x87 copies), `(normal,d)`,
  `(normal,point)`, `(p1,p2,p3)`; assignment.
- `Set` (Plane / 3 points / normal+point / normal+d / point array), `SetDistance` (point / float), `Normalize`.
- `GetNormal`, `SetNormal` (vector / three floats), `GetDistance`, `GetPoint`, `Flip`.
- `WhichSide` (point / point+threshold / vertex list / Sphere), `WhichSideSphere`, `WhichSideAxisBox`.
- `IsBehind`, `IsInFront`, `Distance`, `Project`.
- `Intersect` (Line returning Vector3, with and without `t`; bool Line / Ray / LineSegment, with and without `t`).
- `Transform`, `SetFromTransformPlane`, static `TransformPlane`.

## Forwarded exports (3) and why

| export | reason |
|---|---|
| `?Clip@Plane@Jet@@QBA?AW4ClipResult@2@ABVPolygon@2@AAPAV42@@Z` | needs `Jet::Polygon`'s vertex storage, `Polygon::Set` and the clipped-polygon allocation; Polygon is U1d2's class and not local. Left forwarded per the workstream rule. |
| `?Split@Plane@Jet@@QBAXABVPolygon@2@AAV32@1@Z` | same: reads Polygon vertices and writes two Polygons. Forwarded. |
| `?Intersect@Plane@Jet@@QBA?AVLine@2@ABV12@@Z` | returns `Jet::Line` by value and must put `Jet::Line::vftable` (`??_7Line@Jet@@6B@`, data export at RVA `0x7d428`) into it. `Line` belongs to U1d1 and is not local, so building the object would mean defining another unit's class. Forwarded. The maths is understood: direction = `n1 x n2`; temp line through `n2*d2` with direction `(n1 x n2) x n2`; result point = `Intersect(temp)`. |

## Test

Build and run (isolated workspace):

```
cmake -S "D:\universal modder\r3d-agents\u1b\src\jet" -B "D:\universal modder\r3d-agents\u1b\build32" -A Win32
cmake --build "D:\universal modder\r3d-agents\u1b\build32" --config Release --target Render3DCore_7c diff_r3d_u1b
diff_r3d_u1b.exe "D:\universal modder\r3d-agents\u1b\testdir" 1000
```

Test directory: `D:\universal modder\r3d-agents\u1b\testdir` is `testbin\*` with `Render3DCore_7c.dll` renamed to
`Render3DCore_7c_orig.dll` and our `build32\Release\Render3DCore_7c.dll` beside it. The isolated build only builds
this unit, so the live modules the two Render3DCore DLLs import (`KernelStandard_7c.dll`, `Kernel_7c.dll`,
`KernelResource_7c.dll`, `KernelResources_7c.dll`, `RenderSystem_7c.dll`, `Render2DCore_7c.dll`) were copied into the
test directory from the main `trainz12-x64\build32\Release`; `msvcr80.dll` comes with `testbin`.

The test drives every implemented export through `GetProcAddress`: struct returns through member-function pointers
(this first, hidden return slot after it), the static through a free pointer. It compares raw bytes of the whole
16-byte plane after each mutation, returned Vector3s, `t` and float outputs bit for bit, and enum/bool results, over
random and awkward rounds (every 7th: signed zero, +-inf, quiet NaNs with payloads, signalling NaNs). It covers
`Normalize`/`Project` on zero, +-0, inf and NaN normals, `Flip` on sNaN, all `WhichSide` NaN paths, `WhichSideAxisBox`
degenerate extents, `Intersect` parallel/perpendicular/NaN lines and segments (including the failure cases where only
`t` is written), and `Transform`/`SetFromTransformPlane`/`TransformPlane` with random, NaN-filled, identity and
translation matrices, including the aliasing cases (`Set(n,n)`, `Set(p1)` with p1 = the plane, `SetFromTransformPlane`
with src = this, `TransformPlane` with out = in).

## Layout, ABI and NaN facts

- `Plane` is `{Vector3 normal; float d;}` = `0x10` bytes, no vftable. `JET_LAYOUT32` check in the header.
- Calling conventions from the mangled names: ordinary members `QAA`/`QBA` = `__cdecl` with `this` pushed;
  constructors/assignment `QAE` = `__thiscall`; `WhichSideAxisBox` `QBE` = `__thiscall`; static `TransformPlane`
  `SAX` = `__cdecl`. The header spells the cdecl members `__cdecl` and the two thiscall members accordingly.
- By-value `Vector3` returns put the hidden return slot at `[esp+8]` (after `this`); the test calls those through
  member-function pointers.
- x87 copies (`fld`/`fstp`) quiet a signalling NaN: copy ctor, assignment, `Set(Plane)`, `Set(n,d)`, `SetNormal(Vector3)`
  and the x/y parts of `ctor(n,point)`/`Set(n,point)`. `Set(n,point)` copies z with `movss` (sNaN preserved); `SetNormal(f,f,f)`
  is three `movss`. `Flip` is `-0.0f - x` in SSE, so an sNaN comes out quiet with the same payload.
- Comparison order is the original's `COMISS`/`JBE`: `WhichSide(point[,threshold])` gives `COINCIDENT` for NaN,
  `WhichSideSphere`/`WhichSide(Sphere)` give `BACK` for a NaN test, `WhichSideAxisBox` gives `COINCIDENT` for NaN.
  `WhichSide(int,...)` returns `COPLANAR` (4) when nothing is front or back, including an empty list.
- `Normalize` divides 1 by the module's *approximate* length (`0x10008ab0`, table seed + two Newton steps in x87),
  keeps the reciprocal unrounded in x87, and scales normal and d with it. The seed table is
  `?s_sqrtLUT@StdMath@Jet@@0PAIA` from `KernelStandard_7c.dll`, read with `GetProcAddress` (the original reads the
  same table through its IAT). `Set(p1,p2,p3)`, `Project` and `TransformPlane(transform=true)` use the in-place
  approximate normalize (`0x10008b50`), so their normals are the original's near-unit vectors, not exact unit vectors.
- `Intersect` bool overloads: the line direction is rejected (returns false, `t` unwritten) unless
  `dir.n <= -FLT_EPSILON` or (`twoSided` and `dir.n >= FLT_EPSILON`); NaN fails. Ray and LineSegment additionally
  require `t >= 0`; LineSegment requires `t*t <= |point - end|^2` (the original's test, not `t <= 1`). `t` is written
  before the `t >= 0` / segment tests, so failed calls still return it.
- `Intersect(Line)` / `Intersect(Line,t)` compute `t = (d - point.normal) / (direction.normal)` and
  `point + t*direction`; the two overloads add the z component in opposite operand order in the original (`p.z + t*d.z`
  vs `t*d.z + p.z`), kept for NaN payloads. The bool overloads use yet another order (`p.y + t*d.y`). All three are
  transcribed per instruction.
- `TransformPlane`: `Matrix4` is row-major, translation in row 3 (`m[12..14]`, byte offsets `0x30/0x34/0x38`).
  `transform=false` rotates the normal and sets `d = ((nx*tx + nz*tz) + ny*ty) + inD + extra`. `transform=true`
  multiplies by the cofactor matrix of the 3x3 part, normalizes it, transforms the point `v*inD`, and sets
  `d = ((nx*px + ny*py) + nz*pz) + extra`. The out vector may alias the input vector; the stores/reads are in the
  original's order. `Transform` passes a local copy of the normal and this->d; `SetFromTransformPlane` passes the
  source plane directly, so its aliasing (src = this) behaves like the original.
- `WhichSideAxisBox(center, extent, planeDistance, radius, threshold)`: `planeDistance = center.normal - d`;
  `radius = |n.x|*e.x + |n.y|*e.y + |n.z|*e.z` in x87 (one rounding), then `FRONT` if `radius+threshold < planeDistance`,
  `BACK` if `-(radius+threshold) > planeDistance`, else `COINCIDENT` (NaN too).
- `ClipResult` values are the SDK's `FRONT=1, BACK=2, COINCIDENT=3, COPLANAR=4`; the header defines the enum under the
  `JET_R3D_CLIPRESULT_DEFINED` guard until it has a home in `Render3DCore.h`.

## Internal helpers decompiled for this unit

Not in `u1b_plane_decomp.c`; read from the already analysed main Ghidra project with `tools/ghidra_run.cmd`
(read-only, no analysis) and reproduced here as required:

`FUN_10008ab0` (vector length, from `EAX` = pointer, returned unrounded in `st(0)`):

```
fVar3 = x*x + y*y + z*z;
fVar1 = 0.5f * fVar3;
fVar2 = bitcast(((0x17c - ((bits(fVar3)>>23)&0xff)) << 22 & 0xff800000) | s_sqrtLUT[(bits(fVar3)>>15)&0x1ff]);
fVar2 = (1.5f - fVar2*fVar2*fVar1) * fVar2;
return fVar2 * (1.5f - fVar2*fVar2*fVar1) * fVar3;
```

`FUN_10008b50` (in-place normalize, `EAX` = pointer):

```
len2 = x*x + y*y + z*z;
r = bitcast(((0x17c - ((bits(len2)>>23)&0xff)) << 22 & 0xff800000) | s_sqrtLUT[(bits(len2)>>15)&0x1ff]);
r = (1.5f - r*r*len2*0.5f) * r;
r = r * (1.5f - r*r*len2*0.5f);
x *= r; y *= r; z *= r;     // r rounded to float, SSE multiplies
```

Both are reproduced locally in x87 (the same table-seeded two-Newton code RenderSystem has in its own module); the
only external input is the exported `s_sqrtLUT`. `Plane::Normalize` itself keeps the `1/length` unrounded in x87.

`0x4fd0` (default ctor) is `mov eax,ecx; ret`; `0xb210` (`GetNormal`) is `mov eax,[esp+4]; ret`; `0x1ec50` (copy
ctor / assignment) copies the four floats through x87 and is shared with `Line2D`/`LineSegment2D` assignments.

## Files

- `src/jet/include/jet/Render3DCorePlane.h` — new header (class, `ClipResult`).
- `src/jet/graphics/Render3DCore_7c/Plane.cpp` — new unit.
- `src/jet/tests/diff_r3d_u1b.cpp` — new differential test.
- `src/jet/graphics/Render3DCore_7c/implemented.d/u1b_plane.txt` — new fragment.

No shared file was edited: `implemented.txt`, `Render3DCore.h`, `CMakeLists.txt`, README/MODLOG/PORT_PLAN are untouched.

## Blockers

None. Merge notes: (1) the `ClipResult` enum guard should be unified if another unit adds the same type; (2)
`Intersect(const Plane&)` can move to this unit when U1d1's `Line` is local (it then needs `Line`'s vftable and
layout, nothing else); (3) `Clip`/`Split` need U1d2's `Polygon`.
