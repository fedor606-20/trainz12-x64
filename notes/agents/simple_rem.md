# Wave 6: simple remaining classes (simple_rem)

Scope: Camera copy/transform ctors + GetRay (5), DebugRender (15), Tube (15), Transformation (9),
Primitive3D (7), ParticleFXRenderItem (4), ViewPoint (16), LightLoader (3). The two `ViewPort::Calculate*`
projection helpers are owned by `tails3` (`implemented.d/tails3.txt`) and were not duplicated here.

Files:
- `src/jet/graphics/Render3DCore_7c/SimpleRemain.h`
- `src/jet/graphics/Render3DCore_7c/SimpleRemainTransformation.cpp`
- `src/jet/graphics/Render3DCore_7c/SimpleRemainTube.cpp`
- `src/jet/graphics/Render3DCore_7c/SimpleRemainScene.cpp`
- `src/jet/graphics/Render3DCore_7c/SimpleRemainViewPoint.cpp`
- `src/jet/graphics/Render3DCore_7c/SimpleRemainCamera.cpp`
- `src/jet/tests/diff_r3d_simple_rem.cpp`
- `src/jet/graphics/Render3DCore_7c/implemented.d/simple_rem.txt` (74 mangled names, all local)

## Implemented

**Transformation (0x30)** – default/Matrix3+Vector3 ctors, assignment, `Transform`, `InverseTransform`,
`MultiplyTransform`, `operator*`, `operator*=`, `TransformArray`. Row-major Matrix3, row vector: `v*M + t`;
the inverse dots with the rows. This unit is SSE only. `operator*=` and the array loop have their own
per-element term orders/groupings (the compiler unrolled 4 lanes differently and `operator*=` uses an inline
sequence instead of helper 0x10008c10); they are reproduced lane by lane so NaN payloads and last bits match.
`operator*`/`MultiplyTransform` use the 4x4/3x3 helper orders (`0x10008c10`, `0x1001ab30`).

**Tube (0x1c)** – two centres then radius. Default/3-arg ctors, assignment, all three `GetCenter` forms,
`GetCenters`, `GetRadius`/`SetRadius`, `SetCenter`/`SetCenters`, four `TestCollision`. Tests build the centre
line from `centers[1]-centers[0]`, normalise, then compare `Line::Distance - radii < 1e-4` (constant
0x1007d27c); unordered compares answer false like the original's `fcomip`/`jbe`.

**Primitive3D (4 bytes)** – vftable only, 12 slots, no virtual destructor. `Transform`/`InverseTransform`
call slots 2/3 with `(this, &transformation, &transformation.translation)`; slot 4 is `Intersects` (false);
the rest return 0. Ctor/copy/assignment set the vtable only.

**ParticleFXRenderItem (4 bytes)** – vftable only, the same 12-slot shape; ctor/copy/assignment only.

**DebugRender (empty)** – ctor ignores its Color/mask, assignment is trivial, `Cull`/setters are no-ops,
`GetShowBoundingVolume` false, `GetBoundingVolumeMask` 0, `GetBoundingVolumeColor` returns the function-local
static `cx` (zeroed once). The static flags are the DebugInterface data exports, declared as
`Jet::DebugAccess::globalBoundingVolumes` / `boundingVolumeMask`; they mangle `1_NA` / `1KA` (protected static
data) and are `__declspec(dllimport)`. The function-local guard and `cx` are emitted with the original names
(ordinals 772 / 2478).

**LightLoader (empty)** – assignment trivial, `Load` always null, `Save` opens the first PString as read-mode
File (mode 0x10, encoding 0) and writes the three literals `*LIGHT\n{\n`, `*TYPE\t`, `\n}\n`.

**ViewPoint (0x70)** – public/private ctors, destructor, assignment, Add (both), AddPortal, Get* and
RemoveAll, Render, TraversePortal, Update, CalcWantedMipLevelsForCull.
- Layout: +0 kernel, +4 camera, +8 `Array<VisibleElement,128>*`, +0xc next sibling, +0x10 first child,
  +0x14 Portal*, +0x18 ViewPort*, +0x1c Matrix4, +0x5c depth argument, +0x60 visible first, +0x64 count,
  +0x68 portal depth, +0x6c pixel size.
- Destructor runs child destructors then `Alloc::Delete`; RemoveAll uses the original's 0x100072b20 tree
  deletion (no destructors), then clears the visible list/count.
- `Update` reproduces the x87 atan/tan pixel-size round trip; `Render` enters/leaves the portal and calls
  `Element::Render(display, viewport, visibleElement.matrix)`; `CalcWantedMipLevelsForCull` uses an
  x87 extended `|delta|` (0x10008ab0) so CeilPow2 boundaries match. A NaN `f` continues (the original's
  `ucomiss`+`lahn` idiom only rejects zero); `cvttss2si(NaN)` then yields -2147483648 and level 32.
- `VisibleElement` in `Render3DCoreCamera.h` now has the real layout (Matrix4 at 0, Element* at 0x40,
  key at 0x44) and the two RenderSystem ctor declarations; size stays 0x48.

**Camera (5)** –
- 0x10013ee0 `GetRay`: rebuilds cached orientation from the quaternion (same order as ResetView), then
  orthographic direction is `-forward`, origin is `position + sx*right + sy*up`; perspective/transition
  direction is `sx*right + sy*up - near*forward` (the constant is -0.0, not 1.0), origin is the position.
  The direction is normalised with 0x10008b50.
- 0x10017320 `Camera(const Camera&, const Matrix4&)`: copies Camera-only fields, transforms position with
  perspective divide, transforms all six planes, refreshes the source's cached view transform when dirty, and
  premultiplies `inverse(matrix) * source.cachedViewTransform` with the exact 0x1001ab30 term order.
- 0x10017650, 0x100182a0, 0x10019070 (the portal/quad clipping copies) still **call the original export**
  through `SolidsDetail::OrigProc`, the same pattern U8 used for its twelve hard Camera methods. They are
  local definitions and are covered by the test through byte comparison, but the maths behind them is still
  the original's. This is the deliberate residual of this unit.

## Verification

`diff_r3d_simple_rem` compares both modules in one process: raw bytes for Transformation/Tube/ViewPoint/
Camera, returned bools and raw float bits, and every exported behaviour above. It builds Camera/Line/Sphere
objects with each module's own constructors and includes awkward rounds (signed zero, infinities, quiet and
signalling NaNs) every seventh round. `VisibleElement` pointers and the embedded bounding volume's
self-references are the only bytes excluded from comparisons.

Results (Win32 Release, `build32-r3d-simple`):
- `diff_r3d_simple_rem . 5000` – **730,000 checks, 0 mismatches**.
- Existing suites re-run against the same DLL: u1, u1b, u1c, u1d1, u1d2, u2, u3, u3b, u4, u4_rem, u5a_final,
  u5b_last, u6, u7, u8, u9a, u9b, u9b2, u9b_rem, u10, u10_rem, tails, tails2, tails3, templates, livepaths –
  all 0 mismatches. `diff_r3d_u5b_final` faults inside `Jet::MultiPass::Last::StateSetScissor`
  (0x100641c9 in our DLL, with a null object vtable), which is another unit's current work, not this slice:
  the wave-5 DLL passes the same test.
- `tools/check_exports.py` – 2500/2500 exports, correct ordinals, no extras/forwarders among the 74 names.

## Build

```
cmake -S src/jet -B build32-r3d-simple -A Win32
cmake --build build32-r3d-simple --config Release --target Render3DCore_7c
```

`diff_r3d_simple_rem` is not in the `foreach(t ...)` target list in `src/jet/CMakeLists.txt` (that file is
not owned by this unit), so the test was compiled directly:

```
cl /nologo /std:c++20 /EHsc /GR /Zc:wchar_t- /Zp8 /W3 /D_CRT_SECURE_NO_WARNINGS /I src\jet\include ^
   /Fe:build32-r3d-simple\diff_r3d_simple_rem.exe /Fo:build32-r3d-simple\diff_r3d_simple_rem.obj ^
   src\jet\tests\diff_r3d_simple_rem.cpp
```

Run it in a directory with our DLL as `Render3DCore_7c.dll` and the pristine one as
`Render3DCore_7c_orig.dll`, plus RenderSystem/KernelStandard/Kernel/KernelResource/DebugInterface.

## Remaining in the module

The export TSV now has 15 names not covered by any fragment: `Material` 11 and `OrientedBox` 4. They are not
part of this slice. The three Camera portal copies above are the only forwarded behaviour inside the slice.
