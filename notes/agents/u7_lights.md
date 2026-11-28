# U7 lights: report

Status: **done for the export slice** (2026-11-24). 90 of 95 exports are local and differentially verified;
the 5 `BatchCalculateDiffuse` methods stay forwarded. Win32 Release, `diff_r3d_u7` 150,000 checks,
0 mismatches over 1,000 rounds (every 7th round is an awkward-value round: signed zero, infinities,
quiet/signalling NaNs).

## Files

- `src/jet/include/jet/Render3DCoreLights.h`
- `src/jet/graphics/Render3DCore_7c/Light.cpp` (`Light`, `AmbientLight`)
- `src/jet/graphics/Render3DCore_7c/PointLights.cpp` (`OmniLight`, `ParallelLight`, `SpotLight`, the
  `??_FParallelLight` closure)
- `src/jet/graphics/Render3DCore_7c/CubeMapLight.cpp`
- `src/jet/tests/diff_r3d_u7.cpp`
- `src/jet/graphics/Render3DCore_7c/implemented.d/u7_lights.txt` (90 names; four virtual-name aliases)

The isolated workspace's `src/jet/CMakeLists.txt` predated the wave-2 tests (no `u7` in the `diff_r3d_*`
loop), so it was synced from the main tree, which already lists `u7`; nothing was copied back into main.

## Implemented

90 exports: all constructors, destructors, copy constructors and assignments of the six classes; all 12
vtable symbols; `GetID`, `IsOfType`, `CreateInstance`, `InitInstance`, `SetLightingBegin`, the light
position/direction accessors, `Light::operator==`, `OmniLight::operator==`, the whole `CubeMapLight` state
(texture reference counting, light matrix, front-face flag, orientation/reference-frame/rotate/self-rotate,
`UpdateLightMatrix`), `SetLightingBegin`, and `??_FParallelLight` (the default-constructor closure, via a
linker alias to a matching thiscall ABI).

Virtual-name aliases in `implemented.d` (the original exports are virtual `UAA`/`UBA`; the bodies are the
plain `QAA`/`QBA` methods because RenderSystem names DisplayLight's slots `Slot2..Slot5`):

```
?SetLightPosition@Light@Jet@@UAAXABVVector3@2@@Z=?SetLightPosition@Light@Jet@@QAAXABVVector3@2@@Z
?GetLightPosition@Light@Jet@@UBAABVVector3@2@XZ=?GetLightPosition@Light@Jet@@QBAABVVector3@2@XZ
?SetLightDirection@{Ambient,CubeMap,Omni,Parallel}Light@...UAA...=?SetLightDirection@...QAA...
?GetLightDirection@{Ambient,CubeMap,Omni,Parallel}Light@...UBA...=?GetLightDirection@...QBA...
```

`Slot2`/`Slot4` (setters) are naked bridges: DisplayLight's reconstructed declarations have no argument,
but the original vtable slot is the cdecl `void(const Vector3&)` setter; the bridge reads the caller's
argument from the stack and calls the plain method.

## Forwarded (5)

`BatchCalculateDiffuse` for `AmbientLight`, `OmniLight`, `ParallelLight`, `SpotLight`, `CubeMapLight`.
The four real ones need `Display::ConvertColor`, `Material`'s colour fields, the sqrt LUT and the
vertex-item contract; the export forwards to `Render3DCore_7c_orig.dll` and the vtable slot points at a
local thunk that calls the same original method. `CubeMapLight`'s method is the shared empty function
(implemented locally as an empty body, export still forwarded for consistency).

## Layout and ABI

- `DisplayLight` 0x50, `Particle` at +0x50 (0x12c), `lightingBegin` at +0x17c.
  Sizes: `Light`/`AmbientLight` 0x180, `OmniLight` 0x18c, `ParallelLight` 0x1a8, `SpotLight` 0x1b4,
  `CubeMapLight` 0x1ac (texture pointer +0x180, matrix +0x184, front-face byte +0x1a8).
- Ordinary members and **all virtuals are `__cdecl`**. Constructors and the derived assignments are
  `__thiscall`; `Light::operator=` is `__cdecl` (`QAAAA`). Destructors `UAE` (thiscall).
- Primary vtable 10 slots: DisplayLight 0..7, `SetLightingBegin` 8, `operator==(Light)` 9.
  `OmniLight` adds `operator==(OmniLight)` at 10. `CubeMapLight` adds, at 10..13,
  `Rotate(Quaternion)`, `Rotate(Matrix3)`, `SelfRotate(Quaternion)`, `SelfRotate(Matrix3)` — MSVC reverses
  each overload pair, so the header declares the Matrix3 form first. Vtable bytes in the built DLL were
  checked against these slot roles; the diff test calls slots 2/3/4/5/8/9/10..13 and compares effects.
- **Particle-override exports take the Particle subobject pointer** (`object+0x50`), not the Light pointer:
  `CreateInstance`, `SetOrientation`, `SetReferenceFrame` and (for the classes that override them)
  `GetID`/`IsOfType`. Their bodies subtract 0x50 where they need the Light. This is the original's ABI as
  the assembly shows, and the test calls them with `object+0x50`. `BatchCalculateDiffuse` and the
  DisplayLight slot 5 direction getters take the Light pointer (primary vtable).
- Position/direction slots are DisplayLight slots 2..5: `SetLightPosition`, `GetLightPosition`,
  `SetLightDirection`, `GetLightDirection`. Light/Ambient/Omni/CubeMap slot 4 is the shared empty function,
  slot 5 of Ambient/Omni/CubeMap is a static zero vector, ParallelLight implements both for real.
- **DisplayLight field copies use x87.** The original `DisplayLight` copy constructor, `InitInstance` and
  `operator=` copy every float through `fld/fstp`, which quiets a signalling NaN; the local RenderSystem
  copy is a plain assignment. `Light.cpp` therefore repeats the 0x50-byte copy itself
  (`DisplayLightMirror`, 18 floats + the type word) so constructors/InitInstance/assignment stay
  bit-identical for sNaN. `CubeMapLight`'s copy constructor copies the matrix with plain MOVs (as the
  original does) while `CubeMapLight::operator=` and `SetLightMatrix` use x87.
- `Light::operator==` compares DisplayLight fields, the lighting-begin byte, then the virtual light
  direction and position vectors; NaN never compares equal. `OmniLight::operator==` deliberately does not
  compare the type word, spot exponent or spot cutoff (the original's field list), and skips the
  DisplayLight base comparison.
- ParallelLight caches the bounding volume's global orientation at +0x198 and derives the direction at
  +0x18c; `GetLightDirection` recomputes when the four cached floats differ from the bounding volume
  (NaN counts as different), with the original's SSE operand order.
- `CubeMapLight::UpdateLightMatrix` is the original's SSE-ordered quaternion-to-3x3 matrix; `SpotLight`
  clamps the cutoff only when it is `> 90.0f`, so NaN stays NaN.

## Test

```
cmake -S "D:\universal modder\r3d-agents\u7\src\jet" -B "D:\universal modder\r3d-agents\u7\build32" -A Win32
cmake --build "D:\universal modder\r3d-agents\u7\build32" --config Release --target Render3DCore_7c diff_r3d_u7
copy build32\Release\Render3DCore_7c.dll <testdir>
copy build32\Release\diff_r3d_u7.exe <testdir>
diff_r3d_u7.exe <testdir> 1000
```

Test directory: `testbin` copied, its pristine `Render3DCore_7c.dll` renamed to `_orig`, our DLL and the
main build's live `Kernel*/RenderSystem/Render2DCore` DLLs beside it. The test constructs each class in
turn at the same buffer address for both modules and compares raw bytes (only the +0/+0x50 vtable pointers
skipped), so embedded bounding-volume self pointers match; also compares DisplayLight fields, positions,
orientations, light matrices, texture refcounts, the default-constructor closure, InitInstance,
CreateInstance, and every reachable virtual slot, including NaN/inf/signed-zero inputs.

Result: `150000 checks, 0 mismatches` (1,000 rounds). `tools/check_exports.py`: 2500 exports, 0
missing/wrong, 0 extra.

## Blockers / next

- None for the 90 implemented exports. The five `BatchCalculateDiffuse` methods can be taken once
  `Display::ConvertColor`, the `Material` colour fields and the vertex-item iteration are local in the same
  unit; they are currently the original's and the vtable thunk calls it.
- `LightLoader` is not in this export slice (the TSV has only the six light classes), so it does not move
  with U7.
