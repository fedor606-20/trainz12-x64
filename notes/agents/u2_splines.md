# u2_splines: Render3DCore_7c spline classes

Status: 2026-11-24. 120 of 131 workstream exports implemented, 11 forwarded. Builds and runs; the differential test is
16,345,338 checks with 0 mismatches. `SplineSegment::GetRatio` is deliberately forwarded (see below), so the test no
longer compares it directly.

Workspace: `D:\universal modder\r3d-agents\u2\src\jet` (isolated copy; the main tree was not edited).
Export slice: `notes/agents/r3d_workstreams/u2_splines.tsv` (131 after TCBSpline was added); material
`u2_splines_decomp.c`, `u2_splines_asm.txt`, `u2_tcb_decomp.c`, `u2_tcb_asm.txt`.

## Implemented (120)

| Class | Exports | Notes |
|---|---:|---|
| `Spline` | 15 | ctor/copy/assign/dtor, Add, Remove, RemoveDelete, RemoveDeleteAll, GetLength, virtual and value `GetPositionByRatio`/`GetPositionByLength`, `GetSplineSegmentArray`, vftable |
| `SplineSegment` | 22 | both ctors, copy/assign/dtor, Set, virtual+value position/length, derivatives, tangent/normal/binormal, DetermineNumSplineSteps, IsNullSplineSegment, GetSegmentConstants, SetHermiteSplineData, vftable (`GetRatio` forwarded) |
| `BSpline`, `BSplineSegment` | 8 + 6 | scalar and per-segment `tension` overloads |
| `BezierSpline`, `BezierSplineSegment` | 6 + 6 | |
| `CardinalSpline`, `CardinalSplineSegment` | 8 + 6 | |
| `HermiteSpline`, `HermiteSplineSegment` | 8 + 6 | separate tangent array and interleaved point/tangent overloads |
| `TCBSpline`, `TCBSplineSegment` | 8 + 6 | scalar and per-point tension/bias/continuity overloads |
| `SplineDefinition` | 15 | ctor/copy/assign/dtor, CreateSpline, SetType/GetType, SetParameters/GetParameters, AddPoint, GetNumPoints, RemoveAllPoints, both GetPointArray overloads, vftable |

Fragment: `src/jet/graphics/Render3DCore_7c/implemented.d/u2_splines.txt` (120 mangled names).
All 120 are exported by the built DLL (`pefile` export-table check), and the 11 forwarded names are not implemented.

## Forwarded (11) and why

- `SplineSegment::GetRatio` (`?GetRatio@SplineSegment@Jet@@UBAMMMMI@Z`): the volume/ratio inverse is numerically
  correct but not bit-exact (1-2 ulp). The original interpolates in x87 from a slightly different extended-precision
  `len`/`step` than the float-rounded values used for its length comparisons; every plain float/double/x87 variant
  tried lands 1-2 ulp away. It is therefore not merged as local: the `.def` forwards the export to
  `Render3DCore_7c_orig.dll`, the name is absent from the fragment, and the test's GetRatio case is removed. The
  local virtual slot calls the original body through `GetProcAddress` when `_orig` is loaded (with the scan body as
  a standalone fallback), so virtual calls through our objects are also bit-exact.
- `SplineResource` (5 functions + `??_7SplineResource` vftable): derives from `Jet::Resource` (KernelResource_7c,
  0x5c-byte base) and its virtuals are Resource's thunks plus `LoadTxt`/`CreateInstance`. This is resource I/O owned
  by KernelResource; the class is not declared locally, so the original's ctor/dtor/assign/CreateInstance and vftable
  stay forwarded.
- `SplineDefinition::ReadConfigData`, `WriteConfigData`, `ReadTextFile`, `WriteTextFile`: need KernelStandard's
  `ConfigData`/`ConfigParser`/`PStringCache` and KernelResource's `ResourceDatabase`; the definition's data/math
  (including `CreateSpline`) is local, these four file/config methods are not declared in the header and forward.

## Layout / ABI facts

- `Spline` = 0x08: vftable + `FastDynamicArray<SplineSegment*,13,Less<...> >` head. Chunk = `{next, prev, count,
  data[13]}` = 0x40 bytes, allocated with `Jet::Alloc`; copy/assign copy chunk layout via `AppendChunks`, they do
  not clone segments (shallow, as the original).
- `SplineSegment` = 0x34: vftable + four `Vector3` coefficient vectors `c0..c3` at +0x04/+0x10/+0x1c/+0x28;
  `position(t) = ((c0*t + c1)*t + c2)*t + c3`.
- `SplineDefinition` = 0x24: vftable; Type +0x04; closed byte +0x08; tension +0x0c; bias +0x10; continuity +0x14;
  count +0x18; capacity +0x1c; `Vector3*` +0x20. Default type 5 (unknown), closed true, params 0, empty array.
  The point array grows to multiples of 0x80 entries (`(required+0x7f)>>7 * 0x80`) and keeps its capacity on copy
  when it is already large enough (0x10060b70/0x100609e0).
- Vtables: `Spline` slot 0 dtor, 1 `GetPositionByRatio(float, Vector3&)`, 2 `GetPositionByLength(float,
  Vector3&, unsigned)`. `SplineSegment` slot 0 dtor, 1 `GetPositionByLength(float, Vector3&, float,float,unsigned)`,
  2 `GetRatio`, 3 `GetLength`. All derived classes override only the destructor. `SplineDefinition` has exactly two
  slots: dtor, `CreateSpline`.
- Calling convention: ordinary members are `__cdecl` with `this` as the first stack argument (mangled `QAA`);
  constructors, assignment and virtual functions are `__thiscall`. By-value `Vector3` returns use the hidden
  return slot directly after `this`; the test calls them through member-function pointers.
- All classes use `JET_HEAP_OBJECT` (class `operator new`/`delete` -> `Jet::Alloc`). This is required because the
  original's deleting destructors free with `Jet::Alloc::Delete`; without it a local `delete` would use the CRT heap.
  The tests also verify the chunk layout and per-chunk fill counts (13 pointers per chunk) match.
- NaN semantics: the original's spline arithmetic is SSE and an add/sub/mul/div with two NaN operands returns the
  **second** operand. The compiler may swap commutative operands, so the coefficient formulas use
  `Jet::SplinesDetail::{Add,Sub,Mul,Div}` (inline `addss/subss/mulss/divss` with explicit dest/source) and keep the
  original per-axis operand order. This matters for BSpline (x/y/z differ!), TCB and the segment evaluations and was
  verified exhaustively over NaN-payload combinations for every `Set` (3,125 combinations each).
- `Spline::GetPositionByRatio(float, Vector3&)` evaluates `((c0*t+c1)*t+c2)*t+c3` with inline x87. The index is
  x87 `FISTP` (truncate) and the low 32 bits are used, so NaN/out-of-range ratios become index 0; a negative or
  out-of-range index takes the "last segment at t=1" path (the original dereferences a null segment for an empty
  spline - the test avoids that).
- `SplineSegment::GetPositionByLength` keeps the last sampled point (zero when `steps == 0`) on the not-found path;
  `GetRatio`'s fallback returns the `end` argument (not the step count).
- `GetLength`/`GetRatio`/`GetPositionByLength` use the original's fast sqrt (`StdMath::Sqrt`, one float rounding);
  the derivative/evaluation chains are SSE, and `GetNormal`'s inline second derivative uses `6*c0*t + c1` (not
  `2*c1`) even though `GetSecondDerivative` also uses `c1` once - both were checked against the assembly.
- `Jet::StdMath::s_sqrtLUT` is not referenced directly (Math.h declares it private without dllimport); the final
  code uses the exported `StdMath` functions instead.

## Test command and result

```
cmake -S D:\universal modder\r3d-agents\u2\src\jet -B D:\universal modder\r3d-agents\u2\build32 -A Win32
cmake --build D:\universal modder\r3d-agents\u2\build32 --config Release --target Render3DCore_7c diff_r3d_u2
```

Test directory (testbin has the originals renamed but no Kernel proxies):

```
copy D:\universal modder\testbin\* D:\universal modder\r3d-agents\u2\testdir\
ren Render3DCore_7c.dll Render3DCore_7c_orig.dll
copy build32\Release\Render3DCore_7c.dll testdir\
copy D:\universal modder\trainz12-x64\build32\Release\KernelStandard_7c.dll ^
     Kernel_7c.dll KernelResource_7c.dll KernelResources_7c.dll testdir\
diff_r3d_u2.exe D:\universal modder\r3d-agents\u2\testdir 1000
```

Result (1,000 rounds, fixed seed, every 7th an awkward-value round with signed zero, infinities, quiet and
signalling NaNs, duplicate control points and 0/1/2/3/8/14/16 point cases):

```
16345338 checks, 0 mismatches
```

The test prints a per-label mismatch tally; the complete run prints none. The test covers both `Set` overloads
(scalar and per-point tension/bias/continuity), chunk layout at 14/16 segments, shallow copy/assign,
Add/Remove/RemoveDelete, segment copy/assign/Set and odd/duplicate/degenerate points.

## Forwarded GetRatio

`SplineSegment::GetRatio` (0x1006f0f0) is numerically correct but not bit-exact: the original computes the
interpolation in x87 from the extended-precision sqrt and (apparently) the unrounded step, while the local body uses
the float-rounded `len`/`step` (the same values the original uses for its length comparisons). A focused probe shows
the original's return is `t - (1-frac)*step` with `frac` built from a slightly different extended `len`; all plain
float/double/x87 variants tried return 1-2 ulp away. It is therefore forwarded, not merged:

- the mangled name is absent from `implemented.d/u2_splines.txt` and the generated `.def` exports it as
  `Render3DCore_7c_orig.?GetRatio@SplineSegment@Jet@@UBAMMMMI@Z`;
- the `diff_r3d_u2` GetRatio case was removed;
- the local virtual slot delegates to the `_orig` body via `GetProcAddress` when it is loaded, so a virtual call
  through one of our objects returns the original's bits too (the scan body remains as a standalone fallback).

Helper decompilations used for this (FUN_100609e0 point-array copy, FUN_10060b70 point-array growth, FUN_1003ca30
array append, FUN_1006d640/540/460/4c0 FastDynamicArray find/remove/advance, FUN_1000fcf0/1000fca0 array
copy/assign, FUN_10008b50 normalise) were produced in the scratch Ghidra project and are summarised above; the
temporary dumps were removed from the shared notes directory.

## Files

- `src/jet/include/jet/Render3DCoreSplines.h` (layout, class declarations, `SplinesDetail` SSE helpers)
- `src/jet/graphics/Render3DCore_7c/Spline.cpp`
- `src/jet/graphics/Render3DCore_7c/SplineSegment.cpp`
- `src/jet/graphics/Render3DCore_7c/BSpline.cpp`
- `src/jet/graphics/Render3DCore_7c/BezierSpline.cpp`
- `src/jet/graphics/Render3DCore_7c/CardinalSpline.cpp`
- `src/jet/graphics/Render3DCore_7c/HermiteSpline.cpp`
- `src/jet/graphics/Render3DCore_7c/TCBSpline.cpp`
- `src/jet/graphics/Render3DCore_7c/SplineDefinition.cpp`
- `src/jet/graphics/Render3DCore_7c/implemented.d/u2_splines.txt`
- `src/jet/tests/diff_r3d_u2.cpp`
