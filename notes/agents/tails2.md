# Render3DCore_7c wave 5 tails (Tails2): report

Status: 2026-11-25. **43 of the 70 remaining tail exports are now local** and verified against the pristine module;
27 stay forwarded with the reasons below. (The per-unit notes count 69 because the U2 row omits
`SplineResource::LoadTxt`; the export TSV has it and it was handled here.) The module reports **1981 implemented / 519 forwarded** in the stable
build snapshot and `check_exports` says `2500 exports, TSV 2500; missing/wrong 0, extra 0`. The tails2 differential
test reports **4,174 checks, 0 mismatches, 0 NaN-payload-only differences**. Every existing `diff_r3d_*` test is
green over 200 rounds.

## Implemented (43)

| Unit | Exports | What was done |
|---|---:|---|
| U2 | 8 | `SplineResource` ctor/copy/assign/dtor/vtable/`CreateInstance`/`LoadTxt` (7) and `SplineSegment::GetRatio` (1) |
| U7 | 5 | all five `BatchCalculateDiffuse` exports; their local bodies already existed and delegate to the original |
| U8 | 15 | the three class vtables, `ViewPort::Initialize`, the seven ViewPort traversal exports, DebugViewPort `Render`/`Update`, `Camera::Cull`/`RenderDebug` (bodies already local, delegating) |
| U9a | 6 | `SetCell`, both `TestCollision*`, both `EnumerateAttachmentPoints`, `SetCurrentReferencePoint` (bodies already local, delegating) |
| U9b2 | 9 | `Add`, both `InsertBefore`, both `InsertAfter`, `AddToBoneBlenders`, `SwitchParticles`, `UpdateBoneModifiers`, and the `Update` export through a new ABI adaptor |

The exact names are in `implemented.d/tails2.txt` (43 names).

### U2 details

- `SplineResource` was not declared by U2. `Tails2.h` adds the class deriving KernelResource_7c's local
  `Jet::Resource`; it is 0x60 bytes with the `SplineDefinition*` at +0x5c. The vtable has Resource's 18 slots plus
  `CreateInstance` at slot 18 (checked against the original's table); `LoadTxt` keeps slot 11.
- The copy constructor and assignment share the definition pointer (no clone), as the original does. The destructor
  deletes it through its virtual deleting destructor and clears the pointer before the base destructor.
- `LoadTxt` builds a `SplineDefinition` with the local class and calls the definition's text reader in the original
  module (its `ReadTextFile` is still forwarded); on failure it deletes the definition and returns result code 1.
- `GetRatio` is now exported from the existing local scan body. The wave-4 note said it was 1-2 ulp off; the current
  body (x87 expression `t - (1 - frac)*step`) matches the original over 4,000 random and awkward rounds in
  `diff_r3d_tails2`, so the forward was removed.

### U8 / U7 / U9a / U9b2 delegating bodies

These units already had local definitions (and local vtables use them) but the names were still `.def` forwarders.
Tails2 moves the exports into this image; their bodies keep calling the original module where the unit needs
Display/ViewPoint/RTTI, which is the same pattern U8 documented for its 12 delegated bodies.

The `SkeletonModifier::Update` export needed an ABI adjustor: the original export takes the secondary `Modifier*`
(object+0x18), while the local override takes `SkeletonModifier*`. `Tails2Simple.cpp` defines a cdecl member of a
small adaptor class with the same hidden-`ResultClass` layout and aliases the export to it. The vtable still points
at the local `SkeletonModifier::Update`. The export's machine code was checked: it reads `this` at `[ebp+8]`, the
hidden result at `[ebp+0xc]` and the float at `[ebp+0x10]`.

## Remaining forwarded (27) and why

| Unit | Left | Reason |
|---|---:|---|
| U1d1 | 8 | `LineSegment2D::GetSeparation/GetApproxSeparation(Line2D)` (the original's side/pointer selection differs on inf-only input; wave 4 left them for 2 mismatches), `Polygon2D::GetSeparation/GetApproxSeparation/GetSeparationSquared(Polygon2D)` (closest-edge/shuffle algorithm), `GrahamScan` (stack hull buffer), `LineSegment::TestCollision(Ellipse/Ellipsoid)` (need the U1d2 matrices) |
| U1d2 | 8 | `Ellipse::GetMatrix`, `Ellipsoid::GetMatrix` (large inlined basis/matrix conjugation; the shipped standalone body returns NaNs for a default basis), the four `Ellipse::TestCollision` overloads and `Cone::TestCollision(Line/Sphere)` |
| U2 | 4 | `SplineDefinition::ReadConfigData/WriteConfigData/ReadTextFile/WriteTextFile` (KernelStandard ConfigData/ConfigParser and KernelResource ResourceDatabase paths; not declared in the U2 header) |
| U8 | 7 | four Camera copy-with-transform constructors (bounding-volume ownership + Matrix4 plane transforms), `GetRay`, and the two `Calculate*Size` methods (Display target traversal/projection) |

## Test

`diff_r3d_tails2.cpp` loads both modules in one process. It checks:

- vtable locality and slot roles for `Camera`, `ViewPort`, `DebugViewPort` (local slots must equal the local export,
  original slots must equal the original export);
- locality of all 43 tail exports (GetProcAddress must land in this image, not in the forwarder);
- `SplineSegment::GetRatio` raw bits over 4,000 random/awkward inputs;
- `SplineResource` ctor/copy/dtor locality and `CreateInstance` through a stand-in definition vtable;
- the `SkeletonModifier::Update` export's result code over 100 constructed modifiers.

```
cmake -S src/jet -B build32-r3d-tails2-shadow -A Win32
cmake --build build32-r3d-tails2-shadow --config Release --target Render3DCore_7c
cl /nologo /EHsc /MD /O2 /Fe:testdir-tails2\diff_r3d_tails2.exe src/jet/tests/diff_r3d_tails2.cpp /link /SUBSYSTEM:CONSOLE
diff_r3d_tails2.exe <testdir-tails2>   -> 4174 checks, 0 mismatches, 0 NaN-payload-only differences
```

Test directory: `D:\universal modder\testdir-tails` copied to `testdir-tails2` (pristine `KernelStandard_7c.dll`
and `Kernel_7c.dll`), our DLL copied over `Render3DCore_7c.dll`.

All other `diff_r3d_*` binaries in `testdir-tails2`, 200 rounds each:

```
tails 2289, u1 5800, u1b 18320, u1c 10800, u1d1 34377, u1d2 34560, u2 3267002, u3 56400, u3b 31600,
u4 520840, u5a 62000, u5a2 22634, u5b 417172, u6 235080, u7 30000, u8 31000, u9a 6080, u9b 460,
u9b2 16539, u10 344400, u11 120443
```

(u1d1's 728 differences are the documented NaN-payload-only class; no value mismatch.)

## Build note

The main tree was being edited in parallel by the wave-5 templates/U5/U10/U9 agents while this pass ran. At report
time `build32-r3d-tails2` is blocked before linking by their in-flight tree: `merge_implemented.py` reports a
duplicate `?AddAllToVisible@Cell@Jet@@...` in `implemented.d/u9b_final.txt` and `u9b_rem.txt`, and
`U5bFinalChunks.cpp(105)` earlier failed with C2102/C2530 (the templates files were also changing under the build).
None of those files is a tails2 file. The verification above used a stable snapshot of `src` taken before those
files appeared, with only the other agents' `Templates.cpp`, `Templates.h` and `implemented.d/templates.txt` removed
(they are standalone units, not part of the 1938-export baseline). Re-run the normal `build32-r3d-tails2` once the
parallel wave-5 units compile; no CMake or shared file was edited here.

## Files

- `src/jet/graphics/Render3DCore_7c/Tails2.h` (new: SplineResource)
- `src/jet/graphics/Render3DCore_7c/Tails2Splines.cpp` (new)
- `src/jet/graphics/Render3DCore_7c/Tails2Simple.cpp` (new: SkeletonModifier::Update ABI adaptor)
- `src/jet/graphics/Render3DCore_7c/implemented.d/tails2.txt` (new: 43 names)
- `src/jet/tests/diff_r3d_tails2.cpp` (new)

No other unit, shared header, `implemented.txt`, `CMakeLists.txt` or note was edited.
