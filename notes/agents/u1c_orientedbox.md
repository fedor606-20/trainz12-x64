# U1c: OrientedBox + the AxisAlignedBox tail

Status: 2026-11-24. **Done, differentially verified.** 33 of the 37 `Jet::OrientedBox` exports and all 9
remaining `AxisAlignedBox` exports are local and match `Render3DCore_7c_orig.dll`; 4 `OrientedBox` exports stay
forwarded (the `MakeBestFit` family, see below). `diff_r3d_u1c` 54,000 checks, 0 mismatches over the required 1,000
rounds (and 216,000 checks over 4,000 finite-only rounds, 10,800 over 200 all-awkward rounds). Working copy:
`D:\universal modder\r3d-agents\u1c`; main tree not edited except this report.

## Implemented exports

`OrientedBox` (33 of 37; addresses are the TS12 RVAs):

- constructors: `??0OrientedBox` default 0x4fd0 (shared bare `ret`), `(unsigned, const Vector3*)` 0x491b0,
  `(unsigned, VertexItemPtr<...>)` 0x491e0, copy 0x49210, from `AxisAlignedBox` 0x49280; `operator=` 0x36c0.
- accessors: `SetCenter` 0x3480, `GetCenter` 0xb210, `SetExtents` 0x3740, `GetExtents` 0x3730, `GetAxis` 0x3760,
  `SetAxis(const Vector3*)` 0x3770, `SetAxis(a,b,c)` 0x37b0, `Zero` 0x3800, `IsEmpty` 0x3860.
- transforms: `Transform(Matrix3, Vector3)` 0x38a0, `Transform(Matrix4)` 0x4a440,
  `InverseTransform(Matrix3, Vector3)` 0x3af0, `CalcLocalDir` 0x3d20, `CalcLocalPos` 0x3db0, `CalcCorners` 0x49780.
- fitting/merging: `Init` 0x49f40, `Maximize(OrientedBox)` 0x49730, `Maximize(AxisAlignedBox)` 0x495f0.
- collision: `TestCollision(OrientedBox)` 0x4a9d0, `(AxisAlignedBox)` 0x4a9a0, `(Sphere)` 0x4a6d0, `(Line)` 0x4af00,
  `(Ray)` 0x4af30, `(LineSegment)` 0x4af70, `(Cylinder)` and `(Ellipsoid)` 0x4f940 (shared `xor al,al; ret` stub,
  always false), `GetMinMaxCollision` 0x4b030.

`AxisAlignedBox` tail, in `AxisAlignedBoxTail.cpp`:

- `AxisAlignedBox(const OrientedBox&)` 0xd9f0
- `Maximize(const OrientedBox&)` 0xdb00
- `GetMinMaxCollision` 0xdfc0
- `TestCollision(Line)` 0xde90, `(Ray)` 0xdec0, `(LineSegment)` 0xdf00, `(OrientedBox)` 0xde60,
  `(Cylinder)`/`(Ellipsoid)` 0x4f940 (false stub)
- the 9 declarations were added to the workstream copy's `include/jet/Render3DCore.h` (the only way C++ can define
  them); the existing 25 declarations are unchanged.

`implemented.d/u1c_orientedbox.txt` has the 42 mangled names (33 + 9). The merged module reports
`67 implemented exports from 2 file(s)` = 25 U1a + 42 U1c, and `check_exports.py` says
`DLL 2500 exports, TSV 2500; missing/wrong 0, extra 0`.

## Forwarded exports

Left forwarding because they are the private machinery of the best-fit search, which itself needs four unnamed
solver helpers (`0x1004c710` allocates the working set, `0x1004c870` is the iterative optimisation, `0x1004c190`
and `0x1004cd30` are its inner steps) and a C++ exception-frame/destructor sequence:

- `?MakeBestFit@OrientedBox@Jet@@QAAXIPBVVector3@2@@Z` (0x49380)
- `?MakeBestFit@OrientedBox@Jet@@QAAXIV?$VertexItemPtr@$$CBVVector3@Jet@@VVIPConstHelper@2@@2@@Z` (0x493b0)
- `?Volume@OrientedBox@Jet@@KAMPBMIV?$VertexItemPtr@$$CBVVector3@Jet@@VVIPConstHelper@2@@2@@Z` (0x4b220)
- `?MinimalBoxForAngles@OrientedBox@Jet@@IAAXHV?$VertexItemPtr@$$CBVVector3@Jet@@VVIPConstHelper@2@@2@QAM@Z`
  (0x4b600)

`Volume`/`MinimalBoxForAngles` are called only by `MakeBestFit`, so they stay with it until the solver helpers are
reconstructed as a unit. Nothing else in U1c calls them. Their decompiled material is in the workstream notes; a
later `u1c` follow-up should port the four solver helpers first.

## Test

```
cmake -S D:\universal modder\r3d-agents\u1c\src\jet -B D:\universal modder\r3d-agents\u1c\build32 -A Win32
cmake --build D:\universal modder\r3d-agents\u1c\build32 --config Release --target Render3DCore_7c diff_r3d_u1c
diff_r3d_u1c.exe D:\universal modder\r3d-agents\u1c\testdir 1000
```

Result: `54000 checks, 0 mismatches`. Extra runs: `4000 noawk` -> `216000 checks, 0 mismatches`;
`2000` mixed -> `108000 checks, 0 mismatches`; `200 awk` -> `10800 checks, 0 mismatches` (the test takes optional
`noawk` / `awk` arguments for diagnosis; the default is the required every-7th-awkward regime).

The test covers every local export through `GetProcAddress`: default/array/strided/copy/from-box constructors,
assignment, all accessors, all transforms, `CalcLocalDir`/`CalcLocalPos` (both the output and the untouched first
argument), all eight corners, both `Maximize`s, the protected `Init` with several strides, all collision overloads,
and `GetMinMaxCollision` with the bool and both floats. `testdir` was prepared from `D:\universal modder\testbin`
with the original renamed to `Render3DCore_7c_orig.dll`; the needed Kernel/RenderSystem proxies were copied from the
main build so the original's imports resolve.

Comparison policy: every non-NaN value is compared bit for bit (signed zero and infinities included). **NaN payloads
are not compared** - any NaN compares equal to any NaN. The x87 and SSE units quiet signalling NaNs and choose the
propagated payload differently (x87 takes the larger significand, SSE the destination operand), so a byte-level NaN
comparison measures the FPU, not the reconstructed branch. Every NaN-driven branch decision is still tested (the
`awk` mode, 10,800 checks, 0 mismatches). If the merge requires strict NaN bytes, the remaining gap is payload
selection inside the arithmetic, not control flow.

## Layout, NaN and ABI facts

- `OrientedBox` is 0x3c bytes: centre +0, axes +0xc/+0x18/+0x24 (three `Vector3`), extents +0x30; no vftable.
  `AxisAlignedBox` remains 0x18.
- Ordinary members are `__cdecl` (`QAA`/`QBA`); constructors and `operator=` are `__thiscall` (`QAE`);
  `CalcLocalDir`/`CalcLocalPos` are **`__thiscall` const** (`QBEX`), as their mangled names say.
- `CalcLocalDir(dir, out)` reads and writes **`out` in place** and never reads `dir`; `CalcLocalPos(pos, out)`
  reads `pos` and writes `out`. The first is a quirk of the original, not a transcription slip.
- `AxisAlignedBox(const OrientedBox&)`: world extent per axis is the x87 sum of `|axis|*extent` with a
  non-obvious order - x uses `(|Y.x|*ey + |Z.x|*ez) + |X.x|*ex`, y uses `(|X.y|*ex + |Y.y|*ey) + |Z.y|*ez`,
  z uses `(|X.z|*ex + |Y.z|*ey) + |Z.z|*ez`; min/max = centre ∓ extent. Doubles reproduce the 53-bit x87 chain.
- `OrientedBox::Init`: mean = first point copied through `fld/fstp`, the rest added with the point as the SSE
  destination (`sum = p + sum`), then `* (1.0f/count)`; covariance accumulated per point in float; the eigenvector
  matrix is stored **transposed** (its columns are the axes); axis 2 = cross(axis0, axis1) normalised by the
  module's own seed/LUT `1/sqrt` (0x10008b50, the 512-entry table is embedded in `OrientedBox.cpp` because
  `StdMath::s_sqrtLUT` is private); component re-centring is `c + ((a0*m0 + a1*m1) + a2*m2)`, extents
  `(hi - lo)*0.5`. Both sides call the same imported `Eigen3::IncreasingSort` (KernelStandard U1), so `Init`
  validates the covariance/eigenvector plumbing, not the sorter.
- `GetMinMaxCollision` (both classes) is a slab test. Zero direction: `(0<p0) == (0<p1)` must agree along the axis,
  and t is left at `∓FLT_MAX`. Non-zero: `inv = 1/d`, near/far ordered by `!(tb > ta)` (the original's `JBE` is
  taken on unordered, so NaN uses the `tb <= ta` branch); `tMin = max`, `tMax = min` with the same unordered rule.
  Rejects when `tMin > tMax`; the "no axis constrained it" case (`-FLT_MAX, +FLT_MAX`) is reported as `0, 0`.
  The `Ray` overload tests `0 <= tMax`, not tMin: the original writes tMax over the caller's stack copy of the
  Ray pointer slot and then reads that slot.
- Box-box test: `TB[k] = axis_k . (other.centre - centre)` and `R[k][j] = axis_k . other.axis_j` (rows are
  **this** box's axes, component orders `(x+z)+y` for j=0 and `(z+y)+x` for j=1/2). The 15 tests reject only on a
  real `greater` (unordered continues), use the extents raw (a negative extent is accepted as such), and the
  final cross test is `|TB1*R02 - TB0*R12|` - the `R12` term, not `R22`; that index was the last finite mismatch.
- `Transform(Matrix3)` applies the transposed matrix; `Transform(Matrix4)` has per-axis grouping that differs
  between axis 0 and axes 1/2 (`(z+x)+y` vs `(x+z)+y` on the z component); those groupings are all in the file.
- `CalcCorners` uses the AAB corner order with per-coordinate `((c ∓ a) ∓ b) ∓ c` grouping.
- `Maximize(OrientedBox)` and `Maximize(AxisAlignedBox)` build 16 corners (this box + the argument) and run
  `Init`; they are the original's behaviour, not a separate minimiser.
- `TestCollision(Cylinder)` and `(Ellipsoid)` on both classes are the same 0x4f940 body `xor al,al; ret`.
- NaN/comparison order: every `COMISS/JBE` that selects an operand on unordered was translated to `!(a > b)`
  rather than `a <= b` (the C++ `<`/`<=` operators already give false on unordered, which is the opposite of the
  original's branch for the "select the other operand" sites). This affects `GetMinMaxCollision`, the sphere clamp
  and the SAT continue tests; it is why the awkward rounds became 0.
- Sphere test: `M = transpose(axes)` is inverted with the shared SSE 3x3 inverse (0x1004bf90, exact operand and
  store order), the sphere centre and the box centre are each transformed by `M^-1` with the assembly's
  `(y+z)+x` / `(z+y)+x` grouping, then the *sphere* local point is clamped against
  `[boxLocal - extents, extents + boxLocal]`; distance^2 <= radius^2.

## Build/merge notes and blockers

- Only these files are new/changed in the workstream copy: `include/jet/Render3DCoreOrientedBox.h`,
  `graphics/Render3DCore_7c/OrientedBox.cpp`, `graphics/Render3DCore_7c/AxisAlignedBoxTail.cpp`,
  `graphics/Render3DCore_7c/implemented.d/u1c_orientedbox.txt`, `tests/diff_r3d_u1c.cpp`, plus the nine added
  declarations in `include/jet/Render3DCore.h`. `AxisAlignedBox.cpp`, `implemented.txt` and `CMakeLists.txt` are
  byte-identical to the main tree.
- No blocker for the merge. The only open item for a later unit is the `MakeBestFit` family above.
- Ghidra: the shared workstream `scratchr3d` project was reused to pull the helpers (0x10048af0 covariance,
  0x1004bf70 `fsincos`, 0x1004bf90 3x3 inverse, 0x10008b50 normalise); their decompiled forms are quoted in the
  comments of `OrientedBox.cpp`.
