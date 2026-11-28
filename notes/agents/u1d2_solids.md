# U1d2 solids: Polygon, Sphere, Ellipsoid, Ellipse, Cylinder, Cone, Circle

Status: 2026-11-24. 113 of the 129 exports in `notes/agents/r3d_workstreams/u1d2_solids.tsv` are ours and
differentially verified: 172,848 checks, 0 mismatches over 1,000 rounds (every 7th round uses signed zero,
infinities, quiet and signalling NaNs). The other 16 exports still forward to `Render3DCore_7c_orig.dll`; 10 of
the local exports are virtual-slot wrappers that call the same original export at run time (see below).

## What is local

| Class | Local / slice | Notes |
|---|---:|---|
| `Polygon` | 33 / 34 | everything except `Intersect` |
| `Sphere` | 20 / 22 | everything except `TestCollision(LineSegment)` and `TestCollision(Ray)` |
| `Ellipsoid` | 20 / 21 | everything except `GetMatrix`; the 7 vtable slots are local |
| `Cylinder` | 15 / 15 | all, including the 6 vtable slots |
| `Cone` | 12 / 14 | `TestCollision(Line)` and `TestCollision(Sphere)` forward |
| `Circle` | 9 / 13 | `TestCollision(Line/Plane/Circle)` and `Overlap` forward |
| `Ellipse` | 4 / 10 | constructors/assignment local; `GetMatrix`, `Overlap`, 4 collision tests forward |

The exact list of 113 mangled names is `graphics/Render3DCore_7c/implemented.d/u1d2_solids.txt`; it includes the
four vftables (`??_7Cylinder@Jet@@6B@`, `??_7Ellipsoid@Jet@@6B@`, `??_7Polygon@Jet@@6B@`,
`??_7Sphere@Jet@@6B@`), because the DLL really defines them.

Local but delegating to the original (10 exports, one call each, same export name through `GetProcAddress`;
needed because a vtable slot must be an address in our image, and a `.def` forwarder is not a linkable symbol):

- `Cylinder::TestCollision(Ellipsoid)`, `(Cylinder)`, `(Sphere)`, `(Line)`, `(Vector3)`
- `Ellipsoid::TestCollision(Ellipsoid)`, `(Sphere)`, `(Plane)`, `(Line)`, `(Vector3)`

`Cylinder::TestCollision(AxisAlignedBox)` and `Ellipsoid::TestCollision(AxisAlignedBox/Cylinder)` are local
`return false` stubs: those pairs point at the original's shared `ret false` body (`0x1004f940`).

## Still forwarded (16 exports, `.def`)

```
?Overlap@Circle@Jet@@QAA_NAAVVector3@2@000@Z
?TestCollision@Circle@Jet@@QAA_NABV12@@Z
?TestCollision@Circle@Jet@@QAA_NABVLine@2@@Z
?TestCollision@Circle@Jet@@QAA_NABVPlane@2@@Z
?TestCollision@Cone@Jet@@QAA_NABVLine@2@@Z
?TestCollision@Cone@Jet@@QAA_NABVSphere@2@@Z
?GetMatrix@Ellipse@Jet@@QAA?AVMatrix3@2@XZ
?Overlap@Ellipse@Jet@@QAA_NAAVVector3@2@000@Z
?TestCollision@Ellipse@Jet@@QAA_NABVCircle@2@@Z
?TestCollision@Ellipse@Jet@@QAA_NABVLine@2@@Z
?TestCollision@Ellipse@Jet@@QAA_NABVPlane@2@@Z
?TestCollision@Ellipse@Jet@@QAA_NABVSphere@2@@Z
?GetMatrix@Ellipsoid@Jet@@QBA?AVMatrix3@2@XZ
?Intersect@Polygon@Jet@@QBA_NABVLine@2@AAVVector3@2@@Z
?TestCollision@Sphere@Jet@@QBA_NABVLineSegment@2@@Z
?TestCollision@Sphere@Jet@@QBA_NABVRay@2@@Z
```

Reasons: the two `GetMatrix` methods and everything downstream of them need `Plane::Intersect`, the `Line`/
`LineSegment`/`Ray` distance helpers (U1b/U1d1), or the `Matrix3` basis construction. `Polygon::Intersect` is
just `Plane::Intersect` + the local `IsInside`. `Circle::Overlap`/`Ellipse::Overlap` are only used by the
forwarded `TestCollision(Circle)` paths; their decompiled scalar form was ambiguous enough that leaving them
forwarded was safer than guessing. `Cone::TestCollision(Sphere)` builds a temporary cone and calls the virtual
point test; it was left to the original with the other non-local cone paths.

## Test

```
cmake -S "D:\universal modder\r3d-agents\u1d2\src\jet" -B "D:\universal modder\r3d-agents\u1d2\build32" -A Win32
cmake --build "D:\universal modder\r3d-agents\u1d2\build32" --config Release --target Render3DCore_7c diff_r3d_u1d2
D:\universal modder\r3d-agents\u1d2\build32\Release\diff_r3d_u1d2.exe D:\universal modder\r3d-agents\u1d2\testdir 1000
```

Result: `172848 checks, 0 mismatches`, repeated three times (the count is lower than 173,000 because
`Polygon::Reset` is skipped when `ReduceWithTolerance` leaves fewer than three vertices; the original then
reads past `numVerts`, which no differential test can compare meaningfully).

Test directory: copied `D:\universal modder\testbin\*`, renamed `Render3DCore_7c.dll` to
`Render3DCore_7c_orig.dll`, copied the built DLL in. `testbin` had only `KernelStandard_7c_orig.dll` and
`Kernel_7c_orig.dll`, so the main build's `KernelStandard_7c.dll` (and `Kernel_7c.dll`) were copied beside them;
our DLL imports `Alloc::New/Delete` from KernelStandard and reads its exported `s_sqrtLUT`.

Build checks: `tools/check_exports.py` reports `DLL 2500 exports, TSV 2500; missing/wrong 0, extra 0`; the
merged implemented list has 138 names (25 from U1a + 113 here).

## Layout / ABI facts

- Ordinary members are **`__cdecl`** (`this` at `[esp+4]`, mangled `QAA`/`UAA`); constructors and the
  compiler-generated assignments are `__thiscall` (`QAE`). `Polygon::operator=` is explicitly `__cdecl`
  (`QAAAA`). By-value `Vector3` returns (`Cone::GetCenter(int)`, `Polygon::GetVertex(int)`) must be called
  through member-function pointers, as the test does.
- `Sphere` 0x14: vtable +0x00, centre +0x04, radius +0x10. TS12 has **no `radiusXY`** (the RC1 SDK's field is
  gone; the copy ctor stores only four floats). The U1a `SphereFields` helper (0x18) still reads the same
  centre/radius.
- `Circle` 0x20: Plane base normal +0x00 / d +0x0c, centre +0x10, radius +0x1c. Not polymorphic.
- `Cone` 0x20: `centers[2]` +0x00, radius +0x18, `cosTheta` +0x1c. Not polymorphic.
- `Cylinder` 0x20: vtable +0x00, `centers[2]` +0x04, radius +0x1c. Vtable order:
  `AxisAlignedBox, Ellipsoid, Cylinder, Sphere, Line, Vector3`; there is no virtual destructor.
- `Ellipsoid` 0x38: vtable +0x00, Plane base normal +0x04 / d +0x10, centre +0x14, major +0x20, minor +0x24,
  up +0x28, point +0x2c. Vtable order: `AxisAlignedBox, Ellipsoid, Cylinder, Sphere, Plane, Line, Vector3`.
- `Ellipse` 0x30: Plane base normal +0x00 / d +0x0c, centre +0x10, major +0x1c, minor +0x20, point +0x24.
  Not polymorphic.
- `Polygon` 0x1c: vtable +0x00, normal +0x04, d +0x10, `numVerts` +0x14, `verts` +0x18. Vertex storage goes
  through `KernelStandard` `Alloc::New`/`Delete`; the copy ctor allocates even for zero vertices, `operator=`
  and `Set(int)` do not.
- **MSVC emits virtual overloads of the same name in reverse declaration order.** The header declares the
  `TestCollision` overloads backwards so the emitted vtables (checked slot by slot with `dumpbin`) are in the
  original's order. Getting this wrong made `Cylinder::TestCollision(Ellipsoid)` dispatch to the plane slot.
- Mapping between the class pointer and the RDX `Plane&` argument is by layout only: our classes do not name
  `Plane` as a base (U1b owns it), matching the binary (Plane has no vtable).

## NaN / float-order facts worth keeping

- Tolerances are the original bits: `0.0001f` = `0x38d1b717` in all collision tests, and `1e-8f` =
  `0x322bcc76` for `Sphere::TestCollision(Vector3)` / `(Vector3,float)`.
- Branches compiled from `COMISS`/`JBE` treat unordered as "less or equal". `Polygon::IsInside` therefore uses
  `!(a > b)` and `!(normal.x > 0)`, not `<=` (the naive form picked the wrong projection for NaN axes).
- `Normalize` (0x10008b50) and `Length` (0x10008ab0) are transcribed as x87 instruction sequences, not as a C++
  formula, and read `StdMath::s_sqrtLUT` through `GetProcAddress("?s_sqrtLUT@StdMath@Jet@@0PAIA")`. The two
  helpers have different x87 stacks: `Length` keeps the squared sum on the stack for the final multiply, while
  `Normalize` does not (no final `* sum`). A C++ formula differed in the last bit / NaN payload.
- Where SSE operand order decides a NaN payload (cross product, dot, `Offset`, `CalculateCenter`,
  `CalculateBoundingSphere`, `Sphere::Maximize`, `Normalize` tail), the code calls small scalar helpers
  (`MulSS/AddSS/SubSS/DivSS`) built on `_mm_*_ss` so the destination operand is fixed. `SqrtF` uses x87
  `fsqrt`+`fstp` for the same reason.
- `Polygon::Reset` cross is `(v1-v0) x (v2-v1)`, the reverse of the RC1 SDK's `(v2-v0) x (v1-v0)`; it is
  transcribed from the SSE block at 0x10054eb0.
- `RemoveDuplicates`/`ReduceWithTolerance` compare the pair `(verts[off-1], verts[off])` with `off` starting at
  1, and remove `verts[off]` by shifting `off+1..n-1` down; the decompiler's `i`/`offset` pair is misleading
  here. `ReduceWithTolerance` compares `(double)tolerance > Length(delta)` (strict), and `Length` returns the
  x87 value rounded to double.

## Files

- `src/jet/include/jet/Render3DCoreSolids.h` (class layouts, method declarations, internal `SolidsDetail` helpers)
- `src/jet/graphics/Render3DCore_7c/SolidsMath.cpp` (Length/Normalize, x87 copy, SSE helpers, original-export lookup)
- `.../Sphere.cpp`, `.../CircleBox.cpp`, `.../CylinderCone.cpp`, `.../Ellipsoid.cpp`, `.../Ellipse.cpp`,
  `.../Polygon.cpp`
- `src/jet/tests/diff_r3d_u1d2.cpp`
- `src/jet/graphics/Render3DCore_7c/implemented.d/u1d2_solids.txt`

No shared file (`CMakeLists.txt`, `implemented.txt`, `Render3DCore.h`, README/MODLOG/PORT_PLAN) was touched.

## Blockers / caveats

- The 10 delegating virtual slots and all 16 `.def` forwards need `Render3DCore_7c_orig.dll` present. Without it
  the delegates return `false`; the forwarded exports fail the same way the rest of the proxy does.
- `Ellipsoid::GetMatrix`/`Ellipse::GetMatrix` are the real remaining work for these classes. Once U1b (Plane)
  and U1d1 (Line/Ray/LineSegment) are merged, the collision tails can be made local too.
- `Polygon::ReduceWithTolerance`/`RemoveDuplicates` and `Polygon::Reset` assume at least three vertices (as the
  original does); with fewer, `Reset` reads past `numVerts`. The test skips that comparison rather than compare
  uninitialised heap bytes.
