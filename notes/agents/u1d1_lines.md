# U1d1 `u1d1_lines`: Line, LineSegment, Ray, Line2D, LineSegment2D, Circle2D, Polygon2D

Status: 2026-11-24, workstream `u1d1_lines` complete in its isolated tree
`D:\universal modder\r3d-agents\u1d1`. 164 of the 175 slice exports are implemented; 11 stay forwarded.
Differential test: **0 mismatches in 343,921 checks over 2,000 rounds** (1,000-round run: 171,977 checks, 0
mismatches). Raw bits are compared for every non-NaN value; two NaNs compare equal regardless of payload (see
"NaN payloads" below, which is the one deviation from a strict raw compare). No game run was made.

## Export slice disposition

| Class | Slice | Implemented | Forwarded |
|---|---:|---:|---:|
| `Line` | 30 | 29 | 1 (vftable) |
| `LineSegment` | 25 | 21 | 4 (vftable + Circle/Ellipse/Ellipsoid tests) |
| `Ray` | 20 | 19 | 1 (vftable) |
| `Line2D` | 17 | 17 | 0 |
| `LineSegment2D` | 22 | 20 | 2 |
| `Circle2D` | 24 | 24 | 0 |
| `Polygon2D` | 37 | 34 | 3 |
| **Total** | **175** | **164** | **11** |

Implemented exports are listed one mangled name per line in
`src/jet/graphics/Render3DCore_7c/implemented.d/u1d1_lines.txt`.

### What was implemented

- `Line`: all five constructors, assignment, direction/point accessors, `Translate`, `Rotate`, `SelfRotate`,
  `Scale`, `Normalize`, `IsColinear`, `operator+=/-=/*=//=`, `Distance(Vector3)`, `Distance(Line)`,
  `DistanceSquared`, virtual `TestCollision(Plane)` and `TestCollision(Sphere)`, and the three
  `TestCollision(Vector3, float, ...)` overloads (quadratic roots, nearer-root point, both points).
- `LineSegment`: all four constructors, assignment, accessors, `GetClosestPoint`, `Distance`,
  `DistanceSquared`, `InSegment`, `IsColinear`, `Normalize`, `Translate`, `Rotate`, virtual
  `TestCollision(Plane)` and `TestCollision(Sphere)`.
- `Ray`: all five constructors, assignment, accessors, `Normalize`, `IsColinear`, `Distance`,
  `DistanceSquared` (one-sided projection), virtual `TestCollision(Plane)` and `TestCollision(Sphere)`, and
  the three `TestCollision(Vector3, float, ...)` overloads.
- `Line2D`: constructors, assignment, accessors, both `WhichSide`s, `Intersects`, `Intercept`, and all six
  separation / approximate-separation / squared-separation methods.
- `LineSegment2D`: constructors, assignment, `SetExtents`, `GetV0/GetV1`, both `WhichSide`s, `IsBeside`,
  both `Intersects`, both `Intercept`, and the Vector2/Vector2-Segment separation methods.
- `Circle2D`: both centre/radius constructors, assignment, accessors, all four `Intersects`, all four
  `GetSeparation`, all four `GetApproxSeparation`, all four `GetSeparationSquared`.
- `Polygon2D`: default/count/copy constructors, destructor, assignment, `SetNumVerts`, `GetNumVerts`,
  `GetVertices`, `GetConstVertices`, `Set(int, Vector2*, bool)`, `Set(int)`,
  `Set(FastDynamicArray<Vector2,13,Less<Vector2,Vector2>>&)`, `GetCenter`, all five `Intersects`, the twelve
  constant-zero separation exports, `CounterClockwise`, `Theta`, `ShellSortTheta`.

### Forwarded, and why

| Export | Reason |
|---|---|
| `??_7Line@Jet@@6B@`, `??_7LineSegment@Jet@@6B@`, `??_7Ray@Jet@@6B@` | Vtables stay the original's data symbols. Our objects carry our own local vtable with the same slot order, but external code that imports the vtable symbol keeps seeing the original address/table. |
| `LineSegment::TestCollision(Circle&)`, `(Ellipse&)`, `(Ellipsoid&)` | Need the U1d2 classes' layout and methods (`Circle/Ellipse::TestCollision(Line&)`, `Ellipsoid::GetMatrix`); not copied here. |
| `Polygon2D::GrahamScan()` | Its decompiled form is stack-heavy (`__alloca_probe`, stack-buffer index arithmetic, `ShellSortTheta` call with an out count). `ShellSortTheta` and `Theta` themselves are implemented and tested, but the scan's buffer shuffling was not reproduced with confidence. |
| `Polygon2D::GetSeparation(Polygon2D)` / `GetSeparationSquared(Polygon2D)` | The original picks a closest vertex pair, builds two adjacent edges, tests their crossing, then minimises four point-to-edge distances; the decompiled locals/indices are inconsistent (`extraout_ECX`, repeated locals). Not reconstructed. |
| `LineSegment2D::GetSeparation(Line2D)` / `GetApproxSeparation(Line2D)` | Implemented during the work, but the original's x87 distance-pair selection differs from any C++ min on *inf-only* inputs (2 mismatches in 1,000 rounds: `0`/`inf` versus `NaN`). They are left forwarded so the module is bit-exact; the Vector2 and LineSegment2D variants are implemented. |

## Layout / ABI facts

- `Line` 0x1c: vftable +0, `direction` +4, `point` +0x10.
- `Ray` 0x1c: same fields, own vftable.
- `LineSegment` 0x28: vftable +0, `direction` +4, `point` +0x10, `endPoint` +0x1c.
- `Line2D` 0x10: `direction` +0, `point` +8, no vftable.
- `LineSegment2D` 0x10: `v0` +0, `v1` +8, no vftable.
- `Circle2D` 0xc: `center` +0, `radius` +8, no vftable.
- `Polygon2D` 0xc: `numVerts` +0, `vertices` +4, `ownsVertices` (bool) +8; `PointNode` is
  `{Vector2 point; float theta;}` (0xc).
- Vtable order (from the original's vtables, checked with `DumpVtables`):
  `Line` = `[Translate, Rotate, TestCollision(Plane), TestCollision(Sphere)]`,
  `LineSegment` = `[TestCollision(Sphere), TestCollision(Plane)]`,
  `Ray` = `[TestCollision(Plane), TestCollision(Sphere)]`.
- Calling convention: ordinary members are `__cdecl` (the `QAA/QBA/UAA/UBA` mangling; `this` at `[esp+4]`),
  constructors and assignment are `__thiscall` (`QAE`). `Vector3`/`Vector2` are returned by hidden pointer and
  taken by value on the stack; the test calls the value-returning members through member-function pointers.
- `Line(Vector3 const&, Vector3 const&)` and `Ray(...)` are **(direction, point)** and normalise the direction;
  the pointer constructors take two points and build `p1-p0`.
- `LineSegment2D` and `Line2D` both store direction/point-like pairs: `LineSegment2D::SetExtents(a,b)` writes
  `a` at +0 and `b` at +8, exactly like the `Line2D(direction, point)` constructor.

## Arithmetic / NaN / quirks kept

- TS12's direction normalisation is an inline approximation (not the SDK's `Magnitude`): `s = (x*x + y*y) +
  z*z`, a seed from `KernelStandard_7c!s_sqrtLUT`, two Newton steps in x87 double, rounded to float, then the
  three components scaled. `StdMath::InvSqrt` is the same worker produced by KernelStandard, so it is called
  directly. The approximate magnitude used by `Line::Distance(Line)` for parallel lines is a local copy of
  `0x10008ab0` (one seed, two double Newton steps, times the squared length); it binds the LUT through the
  module's import (`/alternatename:_JetSqrtLutPtr=__imp_?s_sqrtLUT@StdMath@Jet@@0PAIA`). A runtime
  `GetProcAddress` lookup of that data export returns a non-array address in the proxy setup and crashed; the
  import binding is the correct route and same as the original's code.
- `Line::Rotate` and `LineSegment::Rotate` transform as a **row vector times the row-major matrix**. The
  direction and the points use different add groupings in the original; both are reproduced (`direction`:
  `(m0*dx + m6*dz) + m3*dy`, point: `(px*m0 + m3*py) + pz*m6`, etc.). `SelfRotate` rotates only the
  direction with its own grouping.
- Deliberate original quirks kept: `Line2D::GetSeparation(Line2D)` / `GetApproxSeparation(Line2D)` /
  `GetSeparationSquared(Line2D)` return **0 when the directions are not nearly parallel** (`|cross| >=
  1e-4`) and compute a value for nearly parallel lines; `Line2D::Intersects` is true when they are *not*
  parallel. `LineSegment2D::GetSeparationSquared(LineSegment2D)` calls `other.GetSeparationSquared(v0)`
  twice and never uses `v1`.
- `Polygon2D::Polygon2D(const Polygon2D&)` deep-copies but leaves `ownsVertices` unwritten; `operator=`
  copies the three dwords (shallow). `Set(int)` keeps the buffer when shrinking, reallocates and copies
  `numVerts` items when growing, and does not touch `ownsVertices`. `Set(count, ptr, owns)` and the
  destructor only free when `!ownsVertices`.
- `Polygon2D::GetCenter` uses the original's 4-way unrolled accumulation (`v3+v2+v1+v0+sum` per group) then
  `1.0f/n` in SSE; order matters for identical rounding.
- `Polygon2D::Intersects(LineSegment2D)` treats `cross == 0` and NaN crosses as "keep" (the original's
  `JBE`/`JNC` pair); its edge test is the one-direction same-side test of `0x10057450`. Polygon-vs-polygon
  uses the two-direction `LineSegment2D::Intersects`.
- `Polygon2D::Intersects(Circle2D)` takes the "radius below perpendicular distance" path for `r2 >= q`
  *and* for an unordered compare; the earlier form that only tested `r2 < q` returned true where the
  original returns false on NaN radii.
- `Circle2D::GetSeparation(Vector2)` and `(Circle2D)` are written with inline x87 (`__asm`) because the
  original's `FSQRT`/radius subtraction happen in extended precision and rounded once; a double `sqrt`
  followed by a double subtraction differed by one ULP in about 1 in 1,000 rounds. The other circle
  separations use double where the assembly uses x87 double.
- NaN/infinity handling is transcribed from the branch opcodes (`COMISS/JBE/JA/JC/JNC`, `FCOMIP/JBE/JA`).
  Comparisons intentionally use the same volatility: e.g. `LineSegment::TestCollision(Plane)` returns false
  for a NaN product, `Ray::TestCollision(Plane)` accepts NaN height in the second branch, and
  `LineSegment::GetSeparation(Line2D)` propagates a NaN distance.

## NaN payloads (the one comparison deviation)

The original's x87 double intermediates select NaN payloads that SSE2 double arithmetic cannot reproduce
(and its x87 compares also quiet signalling NaNs differently). The differential test therefore compares
NaN against NaN as equal and reports those separately; **all non-NaN float bits and all object bytes are
compared raw**. Over 2,000 rounds there were 7,023 such payload-only field differences and **0 real
mismatches**. The affected methods are the ones whose intermediate work is x87 double in the original:
`Line/Ray/LineSegment` distances and rotations on NaN operands, `Polygon2D::GetCenter`, `Theta`,
`Line2D`/`Circle2D` separations, and the approximate-magnitude paths. Two methods whose *selection* could not
be matched even after reproducing the formulas were left forwarded instead (see above).

## Test harness

`src/jet/tests/diff_r3d_u1d1.cpp` loads both DLLs in one process, calls every implemented export through
`GetProcAddress` on identical inputs, and compares object memory (ignoring vftable words), float bit
patterns, booleans and out-parameters. Inputs are seeded (`0x1d1d`) and every fifth round is an
awkward-value round (signed zero, +-inf, quiet NaNs with two payloads, sNaN, min denormal, +-FLT_MAX,
1e-30). The harness also exercises the copy/shallow-assignment ownership rules without freeing unowned
buffers.

```
cmake -S "D:\universal modder\r3d-agents\u1d1\src\jet" -B "D:\universal modder\r3d-agents\u1d1\build32" -A Win32
cmake --build "D:\universal modder\r3d-agents\u1d1\build32" --config Release --target Render3DCore_7c diff_r3d_u1d1
:: testdir: D:\universal modder\testbin copied, Render3DCore_7c.dll renamed to Render3DCore_7c_orig.dll,
:: our Render3DCore_7c.dll copied in; the testbin copies of KernelStandard_7c/Kernel_7c/
:: KernelResource_7c/KernelResources_7c are renamed *_orig there, so our proxy DLLs for those four
:: modules were copied into testdir as well (a test-directory setup detail, not a change to any module).
"D:\universal modder\r3d-agents\u1d1\build32\Release\diff_r3d_u1d1.exe" "D:\universal modder\r3d-agents\u1d1\testdir" 1000
```

Result:

```
171977 checks, 0 mismatches, 3402 NaN-payload-only differences      (1,000 rounds)
343921 checks, 0 mismatches, 7023 NaN-payload-only differences      (2,000 rounds)
```

`python tools/check_exports.py build32/Release/Render3DCore_7c.dll
D:/trainz12-decomp/exports/Render3DCore_7c.tsv` reports `2500 exports ... missing/wrong 0, extra 0`.

## Files

- `src/jet/include/jet/Render3DCoreLines.h` (one header for the slice)
- `src/jet/graphics/Render3DCore_7c/Line.cpp`
- `src/jet/graphics/Render3DCore_7c/LineSegment.cpp`
- `src/jet/graphics/Render3DCore_7c/Ray.cpp`
- `src/jet/graphics/Render3DCore_7c/Shapes2D.cpp` (Line2D, LineSegment2D, Circle2D)
- `src/jet/graphics/Render3DCore_7c/Polygon2D.cpp`
- `src/jet/graphics/Render3DCore_7c/implemented.d/u1d1_lines.txt` (164 names)
- `src/jet/tests/diff_r3d_u1d1.cpp`

## Blockers / open items

- The 11 forwarded exports above. In particular `GrahamScan` and polygon-to-polygon separation need a
  second, more careful decompilation pass (the original's stack-buffer indexing and closest-edge chooser are
  not in the current material in a usable form).
- NaN payload exactness: x87 double versus SSE2 double is a toolchain limitation; the test reports it
  separately. If the merge gate requires strict raw NaN comparison, the two forwarded `LineSegment2D`
  methods and any NaN-producing method would need inline x87 rewrites; the module currently matches every
  ordered comparison and every non-NaN bit.
- No in-game check yet. These classes are used by camera/culling and scene code; a menu/loading check after
  merge is advisable.
