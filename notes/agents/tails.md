# Render3DCore_7c wave 4 tails: report

Status: 2026-11-24. **16 exports** that the per-unit wave-1/workstream fragments still forwarded are now local and
differentially verified against the pristine `Render3DCore_7c.dll`: **57,145 checks, 0 mismatches, 0 NaN-payload-only
differences over 5,000 rounds**. The main module (`build32-r3d-tails`) and `Diff_r3d_tails` compile; the DLL reports
`2500 exports, TSV 2500; missing/wrong 0, extra 0`.

The test directory must have the **pristine** `KernelStandard_7c.dll`. The original Plane/Split code reads
`StdMath::s_sqrtLUT` through its import as a pointer-to-array; our proxy exports the table itself, so the original's
normalise returns NaNs in the usual testbin layout. `testdir-tails` is `testbin-r3d` with
`KernelStandard_7c.dll`/`Kernel_7c.dll` replaced by their `_orig` copies.

## Implemented (16)

| Unit | Export | Notes |
|---|---|---|
| U1b | `Plane::Clip` | both clippers' classification and output order; reallocates `*out` and returns FRONT/BACK/COINCIDENT |
| U1b | `Plane::Split` | same tolerance but the original's own (quirky) crossing point, see below |
| U1b | `Plane::Intersect(const Plane&)` | returns a `Line` with our vtable; point = `Plane::Intersect(tempLine)` |
| U1d1 | `??_7Line`, `??_7LineSegment`, `??_7Ray` | our local tables (objects already used them); the data exports now point into our image |
| U1d1 | `LineSegment::TestCollision(Circle&)` | plane-crossing test then `Circle::TestCollision(Line)` |
| U1d2 | `Sphere::TestCollision(Ray)` / `(LineSegment)` | distanceSquared - r^2 < 0.0001 in x87 |
| U1d2 | `Polygon::Intersect(Line, out)` | polygon as plane -> intersection -> `IsInside` |
| U1d2 | `Circle::TestCollision(Line)` | point-to-centre squared distances, z/y order kept |
| U1d2 | `Circle::TestCollision(Plane)` | the shipped body intersects the argument plane with itself; the degenerate line makes the result always false (kept bit-for-bit) |
| U1d2 | `Circle::TestCollision(Circle)` | plane-plane line, clip against both circles, `Overlap` |
| U1d2 | `Circle::Overlap` | the four chord side products and the unordered branch |
| U1d2 | `Ellipse::Overlap` | the ellipse variant of the same test |
| U7 | `??_FParallelLight` | fastcall in-place `ParallelLight(3)` helper, same `??_F` pattern as the other modules |

Fragment: `src/jet/graphics/Render3DCore_7c/implemented.d/tails.txt` (16 names; the closure uses `name=symbol`).

## Quirks reproduced

- **Clip vs Split crossing formulas differ in the shipped code.** `Plane::Clip` normalises `cur - prev` and walks
  `prev` to the plane (the geometric intersection). `Plane::Split` copies the **current vertex position** into the
  Line, normalises *that* as if it were a direction, and walks `prev` along it. The two branches (prev front / cur
  front) use the same formula. This was found by comparing the outputs, not from the decompiler; finite Clip results
  already matched before the Split fix, Split did not.
- Classification in both: `> 0.0001` front, `< -0.0001` back, else coincident; a NaN is coincident. Counts use
  "front" vs "everything else"; all-front returns 1, no-front returns 2, mixed/all-coincident builds the clipped
  polygon and returns 3. `Clip` always deletes/reallocates `*out` in the mixed path; `Split` only calls `Set` for a
  non-empty side, leaving an empty side's polygon untouched.
- Intersection dot products are accumulated in **y, z, x** order (not x, y, z). `Plane::Intersect(Plane)` uses the
  cross-product operand order from the SSE block: `x = ay*bz - az*by`, `y = bx*az - ax*bz`, `z = ax*by - bx*ay`;
  temp direction = `(n1 x n2) x n2`, temp point = `n2 * d2`, both with `MulSS/SubSS`.
- `Circle::TestCollision(Plane)` really is `plane.Intersect(plane)`, as the shipped code does. The result is a
  degenerate line and the function always returns false; the body is kept so the export matches on NaN too.
- Sphere's two tests keep the whole comparison in x87 (`fld dist; fld r; fmul; fsubp; fld tol; fcomip; seta`).
  A double expression differed for one `FLT_MAX` radius / infinite distance round.
- The vtable exports are our local tables. Their slot order is the one U1d1 verified (`Line`: Translate, Rotate,
  TestCollision(Plane), TestCollision(Sphere); `LineSegment`: Sphere, Plane; `Ray`: Plane, Sphere).
- `??_FParallelLight` is implemented as `void __fastcall ParallelLightDefaultCtorClosure(ParallelLight*)`, which
  receives the closure's `this` in ECX and constructs `ParallelLight(DLightTypes(3))` in place. The .def alias is the
  demangled fastcall name, the pattern `KernelStandard`/`RenderSystem` use.
- The Polygon returned by `Clip` is allocated with our module's global `new` and destroyed through its virtual
  scalar deleting destructor (which uses the same CRT heap); the original uses `Alloc::New` and its own deleting
  destructor. Each side stays consistent; the test destroys each result through its own vtable.

## Test

`src/jet/tests/diff_r3d_tails.cpp` loads both DLLs, calls all 16 exports through `GetProcAddress` on identical
inputs, and compares booleans, raw float bits, returned lines and polygon vertices/normals/d. `Clip` results are
destroyed through their own virtual deleting destructor; `Split` vertex arrays are freed through
`KernelStandard_7c!Alloc::Delete`. Every seventh round uses signed zero, infinities, quiet/signalling NaNs,
min-denormals and FLT_MAX. The closure is run on the same buffer address for both sides and all non-image pointer
dwords are compared.

```
cmake -S src/jet -B build32-r3d-tails -A Win32
cmake --build build32-r3d-tails --config Release --target Render3DCore_7c
:: ad-hoc (CMakeLists has no tails entry and must not be edited)
cl /nologo /EHsc /MD /O2 /Fe:diff_r3d_tails.exe src/jet/tests/diff_r3d_tails.cpp /link /SUBSYSTEM:CONSOLE
diff_r3d_tails.exe <testdir-tails> 5000   -> 57145 checks, 0 mismatches, 0 NaN-payload-only differences
```

Non-finite Sphere/Overlap operands are skipped in the test: the shipped Sphere tests read the float return from the
x87 stack while our mixed MSVC 2005/2022 process supplies it in XMM0, so the shipped result then depends on the
caller's x87 residue; the finite domain (including denormals, signed zeros and FLT_MAX) and the NaN branches that do
not depend on it are covered. This is a test-harness limit, not an implementation difference.

## Remaining forwards in this tail set (69)

| Unit | Left | Reason |
|---|---:|---|
| U1d1 | 8 | `LineSegment2D::GetSeparation/GetApproxSeparation(Line2D)` (the original's x87 pair selection differs on inf-only input; U1d1 left them forwarded), `Polygon2D::GetSeparation/GetApproxSeparation/GetSeparationSquared(Polygon2D)`, `Polygon2D::GrahamScan` (stack-buffer indexing), `LineSegment::TestCollision(Ellipse/Ellipsoid)` (need the matrix/ellipse tests below) |
| U1d2 | 8 | `Ellipse::GetMatrix` / `Ellipsoid::GetMatrix` and the four `Ellipse::TestCollision` overloads built on them; `Cone::TestCollision(Line/Sphere)` |
| U2 | 11 | `SplineSegment::GetRatio` (1-2 ulp x87 inverse), `SplineResource` 6 (KernelResource base), `SplineDefinition` config/text I/O 4 (KernelStandard/KernelResource) |
| U7 | 5 | `BatchCalculateDiffuse` for Ambient/Omni/Parallel/Spot/CubeMap: needs Display::ConvertColor, the Material colour fields and the vertex-item iteration, still original-owned |
| U8 | 22 | Camera copy constructors (bounding-volume ownership), `Cull`/`RenderDebug`/`GetRay`, ViewPort `Begin/End/Render/Update/RenderLayer/GetElementAt/GetAllWithin/Initialize` and the size calculators; these need the live Display/RenderKernel traversal or U1c/Matrix projection. `GetRay` was analysed (quaternion matrix + perspective/ortho directions) but its matrix sequence was not finished |
| U9a | 6 | `TestCollision`/`TestCollisionGeometry` (Ray + CollisionResult/ConstFaceDescriptor), `SetCell` (U9b Cell), `EnumerateAttachmentPoints` x2 (original RTTI), `SetCurrentReferencePoint` (reference-point pose helper) |
| U9b2 | 9 | `SkeletonModifier::Add/InsertBefore x2/InsertAfter x2/AddToBoneBlenders` (original `__RTDynamicCast` does not accept our BoneModifier), `SwitchParticles`, the `SkeletonModifier::Update` export (Modifier* this), `BoneBlender::UpdateBoneModifiers` (blend loop over original bone particles) |

`Ellipse::GetMatrix` deserves a note: called standalone (constructor-built object, pristine KernelStandard) the
shipped body returns NaN/garbage even with a valid ellipse, so the decompiled sequence could not be matched
reliably; its dependants stay forwarded rather than shipping a knowingly different basis.

## Files

- `src/jet/graphics/Render3DCore_7c/TailsPlane.cpp` (new)
- `src/jet/graphics/Render3DCore_7c/TailsSolids.cpp` (new)
- `src/jet/graphics/Render3DCore_7c/TailsLines.cpp` (new)
- `src/jet/graphics/Render3DCore_7c/TailsLights.cpp` (new)
- `src/jet/graphics/Render3DCore_7c/implemented.d/tails.txt` (new)
- `src/jet/tests/diff_r3d_tails.cpp` (new)
- edited for these tails only: `src/jet/include/jet/Render3DCorePlane.h` (Clip/Split/Intersect(Plane) declarations),
  `src/jet/include/jet/Render3DCoreLines.h` (shared `ClipResult` guard, the three `LineSegment::TestCollision`
  declarations).

## Regressions

All other `diff_r3d_*` tests are green in `testdir-tails` over 200 rounds. `diff_r3d_u4` is red (7,696 mismatches,
`EndOfTree`) in the current tree **with and without `tails.txt`**, i.e. from the parallel U4 work in the same
working tree, not from this tail set; U4 files were not touched.
