# Render3DCore_7c wave 6 tails (Tails3): report

Status: 2026-11-25. The tails3 unit moved **26 exports** into this image. The module now builds as
**2485 implemented / 15 forwarded**, and `check_exports` reports `2500 exports, TSV 2500; missing/wrong 0,
extra 0`. The 15 remaining forwards are seven MultiPass `SetTexture`/`DuplicateTexture`/`IsLoaded` methods and
the three OrientedBox best-fit/volume helpers, i.e. not this unit.

`diff_r3d_tails3` (new) reports **76,066 checks, 0 mismatches**. Every other `diff_r3d_*` test is green with
the wave-6 module except `diff_r3d_u5a` and `diff_r3d_u5a2`. Those two fail identically with a module built
**without** this unit too (50 and 71 mismatches at five rounds); the current wave-6 material tree, not Tails3,
is their cause. With Tails3 the u5a2 count is 74: three extra checks compare the now-local
`??_7AutoTexMaterialPlugin` address against the original's address, which an old test written for a forwarded
vtable cannot pass.

## Implemented (26)

| Unit | Exports | What was done |
|---|---:|---|
| U1d1 | 8 | `LineSegment2D::GetSeparation/GetApproxSeparation(Line2D)`, `Polygon2D::GetSeparation/GetSeparationSquared/GetApproxSeparation(Polygon2D)`, `GrahamScan`, `LineSegment::TestCollision(Ellipse/Ellipsoid)` |
| U1d2 | 8 | `Ellipse::GetMatrix`, `Ellipsoid::GetMatrix`, four `Ellipse::TestCollision` overloads, `Cone::TestCollision(Line/Sphere)` |
| U2 | 4 | `SplineDefinition::ReadConfigData/WriteConfigData/ReadTextFile/WriteTextFile` |
| U8 | 2 | `ViewPort::CalculateSphereSize/CalculateOrientedBoxSize` (the four copy-with-transform Camera constructors and `GetRay` are local in the parallel simple-classes unit; all seven U8 tails are local in the final module) |
| U5a | 4 | `??_7AutoTexMaterialPlugin`, `??1AutoTexMaterialPlugin`, `ReflectiveMaterial::CreateGeometryChunk`, `GenericShaderMaterial::CreateGeometryChunk` |

The exact names are in `implemented.d/tails3.txt` (26 lines).

### U1d1

- The `LineSegment2D` line-separation bodies use the original's exact operand order. The distance denominator
  is `sqrt(double(dir.y)*dir.y + double(float(dir.x*dir.x)))`: the original rounds `dir.x²` to float before
  the x87 sum. The first implementation used an SSE/double denominator and was consistently 1 ulp off; the
  float-rounded square fixes it (tested over 25,000 random/awkward lines). The approximate variant divides
  `fabs(cross)` by the x87 Quake-root in double.
- `Polygon2D::GetSeparation` is `sqrt(GetSeparationSquared)`; `GetApproxSeparation(Polygon2D)` is the module's
  empty `0x10010e60` body.
- `GetSeparationSquared(Polygon2D)` finds the closest vertex pair, builds the two adjacent edges, rejects
  crossing edges, then minimises four expanded point-to-edge distances. The expanded
  `(q.x*ey - q.y*ex) - (p.x*ey - p.y*ex)` form matters for the last bit. The original's next-edge index
  expression was reproduced as "same index, wrapped to 0 only at the last vertex"; the fourth distance is the
  closest point to the previous edge again (the decompiled index aliasing is not usable). General-position
  polygons match; degenerate integer polygons with repeated/collinear vertices can still differ and are not
  value-compared by the test. The function stays local.
- `GrahamScan` is the original's in-place scan: 0.0005 input thinning, lowest-then-leftmost start, the
  `Theta`/`ShellSortTheta` clockwise pseudo-angle order, the same-theta group pass, an in-place hull pass
  driven by `CounterClockwise`, and the reversed copy into the existing vertex block. A final adjacent
  duplicate removal covers the original's duplicate-start case. Continuous-coordinate polygons match
  bit-for-bit over 100,000 random probes and 2,500 test rounds; the original's own grouping quirks on
  duplicate/collinear integer input are not reproduced exactly.
- `LineSegment::TestCollision(Ellipse/Ellipsoid)` transform the segment by `GetMatrix` and test the result
  against the unit sphere (`distance² - 1 < 1e-4`).

### U1d2

- **The shipped `Ellipse::GetMatrix` is defective.** Its last `Matrix3` multiply operand at `[esp+0x88]` is
  never written; the function reads uninitialised stack. A probe calling the original twice, or after different
  stack seeds, returns different matrices (and all-NaN for some seeds). The body here builds the intended
  `basis * diag(1/major, 1/minor, 1) * basisᵀ` from `normalize(normal)`, `normalize(point-centre)` and the
  cross product instead. `Ellipsoid::GetMatrix` has the same shape with `1/up` for the third axis and a
  deterministic stack block in the original; the same intended formula is used.
- The four `Ellipse::TestCollision` overloads and the two `Cone::TestCollision` methods are local analytic
  implementations. They are not bit-compared against the original: the ellipse ones call the broken matrix,
  and the cone ones use the original's finite-cone/line and cone/sphere tests semantically. Locality is tested.

### U2

- Configuration keys are `version` (optional), `Spline` (the type as a string value), `Loop`, `Tension`,
  `Continuity`, `Bias`, `Points`. The field order is the original's: `+0x10` is "Continuity" and `+0x14` is
  "Bias", so the header's `bias`/`continuity` names are opposite to the key names.
- `WriteConfigData` writes the type string, then `Loop`, `Tension` (types Cardinal/TCB/BSpline/Unknown),
  `Continuity` and `Bias` (TCB/Unknown), then one anonymous point node with three children whose values are
  the `"%f"` coordinates. `ReadConfigData` reads a three-element float array from each point child; as in the
  original, the shipped writer and reader therefore do not round-trip points.
- A private probe compared both exports over all six types: the written `ConfigData` text is identical and the
  read-back type/closed/tension/bias/continuity/count/points are bit-identical.
- `ReadTextFile` uses `ResourceDatabase::GetFileName` and `ConfigParser::Parse`; `WriteTextFile` opens the
  file with `File::CREATE_WRITE` and `ConfigData::Write(file, 0)` (decoded from the original's calls).

### U8 / U5a

- `CalculateSphereSize`/`CalculateOrientedBoxSize` project the eight volume corners with the supplied Matrix4,
  divide by w, map through the viewport extents and round to the original `Rect` pixel layout. They are local
  and deterministic; the original's Display target-size/letterbox path is simplified to the viewport rect.
- `AutoTexMaterialPlugin` now has its three cache pointers at `+0x20/+0x24/+0x28` (size 0x2c). Defining its
  destructor out of line makes VS18 emit a local `??_7AutoTexMaterialPlugin` with the seven plugin slots; the
  factory calls the local `StdMaterialPluginTemplate<AutoTexMaterial>::CreateInstance`, so the caches and
  material construction follow the template. (The old u5a2 test compares the vtable address itself against the
  original's, which cannot pass once the table is local.)
- `ReflectiveMaterial::CreateGeometryChunk` and `GenericShaderMaterial::CreateGeometryChunk` build the local
  one-texture (PCT/nml) and no-texture (PN) chunk classes with `MakeGeometryChunkFlags`. Their original
  shader/cache branches are not reproduced; the exports are local and usable.

## Test

`src/jet/tests/diff_r3d_tails3.cpp` loads our module and the pristine `Render3DCore_7c_orig.dll` in one
process. It checks:

- locality of all 26 exports (GetProcAddress must land in this image) and the AutoTex vtable/dtor;
- `LineSegment2D` line separations, raw bits, over random and awkward values (NaN/inf/-0) — 25,000 rounds of
  each at the default size;
- three fixed polygon pairs for `Polygon2D` separations;
- `GrahamScan` vertex-for-vertex on continuous random polygons;
- an empty `ConfigData` is rejected by `ReadConfigData`.

Because the test target is not in `CMakeLists.txt` (shared file, not edited by this unit), it was compiled
manually:

```
cl /nologo /c /O2 /MD /EHsc /GR /Zc:wchar_t- /Zp8 /D_CRT_SECURE_NO_WARNINGS /I src\jet\include ^
   src\jet\tests\diff_r3d_tails3.cpp
cl /nologo /Fe:diff_r3d_tails3.exe diff_r3d_tails3.obj
diff_r3d_tails3.exe <testdir> 5000   -> 76066 checks, 0 mismatches
```

Test directory: `testdir-tails3` (pristine `KernelStandard_7c.dll`, `Kernel_7c.dll`,
`Render3DCore_7c_orig.dll`, our module over `Render3DCore_7c.dll`). All other `diff_r3d_*` binaries from the
wave-5 fixture are green except the inherited u5a/u5a2 issue described above.

## Build

`cmake --build build32-r3d-tails3 --config Release --target Render3DCore_7c` succeeded after the parallel
wave-6 units settled: 2485 implemented, 15 forwarded. `python tools/check_exports.py ...` reports zero
missing/wrong/extra. No CMake, `implemented.txt`, `notes/Render3DCore.md` or other unit file was edited.

## Files

- `src/jet/graphics/Render3DCore_7c/Tails3Shapes.cpp` (new: U1d1 separations and GrahamScan)
- `src/jet/graphics/Render3DCore_7c/Tails3Solids.cpp` (new: matrices, ellipse/cone tests)
- `src/jet/graphics/Render3DCore_7c/Tails3Splines.cpp` (new: config and text I/O)
- `src/jet/graphics/Render3DCore_7c/Tails3ViewPort.cpp` (new: the two projection helpers)
- `src/jet/graphics/Render3DCore_7c/Tails3Materials.cpp` (new: AutoTex vtable/dtor and the two chunk paths)
- `src/jet/graphics/Render3DCore_7c/implemented.d/tails3.txt` (new: 26 names)
- `src/jet/tests/diff_r3d_tails3.cpp` (new)
- `src/jet/include/jet/Render3DCoreSplines.h` (SplineDefinition config declarations, forward declarations)
- `src/jet/include/jet/Render3DCoreMaterials.h` (AutoTexMaterialPlugin cache fields and layout assert)
- `src/jet/include/jet/Render3DCoreCamera.h` (the two ViewPort Calculate declarations; the four Camera
  copy constructors were added by the parallel simple-classes unit)
