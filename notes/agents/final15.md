# Render3DCore_7c Final15: the last fifteen exports

Status 2026-11-25: **2500/2500 exports are local, 0 forwarders.** Eleven multipass material texture virtuals are
reconstructed locally; the four `OrientedBox` best-fit entries are local entry points that call the renamed
original's bodies (see the caveat below). Worked in `D:\universal modder\trainz12-x64`, built in
`build32-r3d-final`.

```
check_exports.py build32-r3d-final\Release\Render3DCore_7c.dll <tsv>
    DLL 2500 exports, TSV 2500; missing/wrong 0, extra 0
diff_r3d_final15.exe testbin-r3d 1000
    13,912 checks, 0 mismatches
```

## Implemented here (15)

| Group | Export | Address |
|---|---|---:|
| `MultiPassNoTextureMaterial` | `DuplicateTexture` | 0x2d880 |
| | `SetTexture(int, db, name, amount, request)` | 0x34080 |
| | `SetTexture(int, AutoReference, amount)` | 0x34260 |
| `MultiPassOneTextureMaterial` | `DuplicateTexture` | 0x2dde0 |
| | `IsLoaded` | 0x2e030 |
| | `SetTexture(int, db, ...)` | 0x36c80 |
| | `SetTexture(int, AutoReference, ...)` | 0x36de0 |
| `MultiPassBumpMaterial` | `DuplicateTexture` | 0x2e2f0 |
| | `IsLoaded` | 0x2e4c0 |
| | `SetTexture(int, db, ...)` | 0x2e850 |
| | `SetTexture(int, AutoReference, ...)` | 0x2e9b0 |
| `OrientedBox` | `MakeBestFit(unsigned, const Vector3*)` | 0x49380 |
| | `MakeBestFit(unsigned, VertexItemPtr)` | 0x493b0 |
| | `Volume(const float*, unsigned, VertexItemPtr)` | 0x4b220 |
| | `MinimalBoxForAngles(int, VertexItemPtr, float* const)` | 0x4b600 |

## Material virtuals: behaviour and ABI facts

- The entries are ordinary `__cdecl` member virtuals returning `ResultClass` in memory: at entry `[esp+4]` is
  `this`, `[esp+8]` the hidden result pointer, then the arguments. The local bodies are plain namespace functions
  with the same parameter sequence and are exported under the original names from
  `implemented.d/final15.txt`.
- Texture slots per family: NoTexture `+0xa4` (type 0xc) and `+0xa8` (0xd); OneTexture `+0xc4` (1) and the alpha
  hint at `+0xc8`; Bump `+0xd0` (8), inherited `+0xc4`, alpha hint `+0xc8`.
- Slot 0x5c of the material vtable is `UpdateTextureArray` (not `CacheAlphaHint`); both `SetTexture` overloads call
  it after `Material::SetTextureDefinition` and before writing result 0. The database overloads also set the alpha
  hint to -1 on success (Bump's reference overload does not, and its database overload does: the asymmetry is the
  original's).
- `SetTexture(db)` takes one reference on the old texture, calls the TextureManager (`materialManager+0x98` ->
  `+0xc` -> vtable slot 3, cdecl with `this` first), releases the old slot, stores the new pointer and, on a null
  result, restores the old pointer and returns 3. The new texture's `SetResourcePriority` (vtable 0x48) gets
  `material+0x94`.
- `SetTexture(AutoReference)` returns 3 for a null reference; otherwise AddReference the new texture, release the
  old one, set priority, define (`texture+0x10` PString) and update. Unknown types fall through the family chain
  NoTexture -> OneTexture -> Bump and end as result 2.
- `DuplicateTexture` calls texture vtable slot 2 (`CreateInstance`), releases the old slot and stores the copy;
  a null slot returns 3. Families chain the same way (Bump -> OneTexture -> NoTexture).
- `IsLoaded` is result 0 when the texture exists and `IsAvailable` (vtable slot 3) is true, else 4; Bump requires
  both `+0xd0` and `+0xc4`.
- `SetTextureDefinition` is RenderSystem_7c's protected cdecl export `?SetTextureDefinition@Material@Jet@@IAAXH...`;
  the local code reaches it through a `MaterialView : Material` cast, as U5a's material code does.

## OrientedBox best-fit caveat (the four entries delegate)

The four bodies are local exports but currently call the renamed original's implementations. The family is not the
"eigen/power iteration" of `Init`; it is a bounded Hooke-Jeeves search over an axis-angle parametrisation plus a
recursive parabolic/golden 1D minimiser, all in x87:

- `MakeBestFit(unsigned, VertexItemPtr)` builds a 0x4c-byte solver state (n=3, eval = `Volume`, 32 outer
  iterations, minimiser `{8, 8, bestT, bestV, state}`), runs a 4x4x4 grid of
  `(a0 = i/3*pi, a1 = j/3*pi/2, a2 = k/3*pi)`, then 32 iterations of three axis searches inside the box
  `[0,pi] x [0,pi/2] x [0,pi]` followed by a line search along the net displacement (rows are cycled, so the
  directions are the coordinate axes plus the pattern move), then `MinimalBoxForAngles`.
- `Volume` builds the rotation matrix from the axis-angle quaternion `(w = cos(a2/2), x = sin(a2/2)cos(a1),
  y = sin(a2/2)sin(a1)cos(a0), z = sin(a2/2)sin(a1)sin(a0))` normalised by `1/sqrt` of the sum, transforms the
  points with the matrix and returns `(max0-min0)*(max1-min1)*(max2-min2)` as an x87 product.
- `MinimalBoxForAngles` uses the same matrix, records min/max per transformed axis, then writes centre = M^T*mid,
  the three matrix rows as axes and `(max-min)*0.5` as extents.
- The private helpers are at 0x1004c710 (init), 0x1004c870 (search), 0x1004c190 (three-point refine),
  0x1004c340 (bracket), 0x1004c510 (parabolic step), 0x1004cd30 (feasible t range), 0x1004d0f0 (eval); the
  decompiled forms were pulled with `ghidra_scripts/DecompileAt.java` and are quoted by address in
  `Final15OrientedBox.cpp` and this note. Reconstructing them is a separate unit; the delegating entries keep the
  exported behaviour bit-identical in the meantime (a fake "local" Volume/Minimal would not be used by the
  delegated solver anyway).

## Test

`src/jet/tests/diff_r3d_final15.cpp` (built by hand; the unit may not edit CMakeLists):

```
cmake -S src\jet -B build32-r3d-final -G "Visual Studio 18 2026" -A Win32
cmake --build build32-r3d-final --config Release --target Render3DCore_7c
call vcvars32.bat
cl /nologo /EHsc /GR /Zc:wchar_t- /Zp8 /MD /O2 src\jet\tests\diff_r3d_final15.cpp ^
   /Fe:build32-r3d-final\Release\diff_r3d_final15.exe
copy build32-r3d-final\Release\Render3DCore_7c.dll testbin-r3d\
diff_r3d_final15.exe testbin-r3d 1000
```

The material half runs on synthetic objects with recording vtables (texture manager slot 3, textures slots 2/3/0x48,
material slot 0x5c) and a hook on RenderSystem's `SetTextureDefinition`, for all three families, every interesting
type (0..9, 0xb..0xe, 0x10), both `SetTexture` overloads, null and non-null inputs, three amounts, both `IsLoaded`
answers and absent textures. It compares the result code, the recorded call order/arguments, every texture slot
(pointer identity and reference count) and the alpha-hint dword. Event coverage is asserted (duplicate 18,
available 8, priority 120, update 120, create 108, definition 120).

The OrientedBox half compares both `MakeBestFit` overloads (linear and 0x10-strided), `Volume` bit patterns and
`MinimalBoxForAngles` box images for 1..24 random points/round, including duplicate-point clouds.

Existing suites re-run in `D:\universal modder\testbin-r3d` with this DLL (100 rounds): `u1`, `u1b`, `u1c`,
`u1d1`, `u1d2`, `u2`, `u3`, `u3b`, `u4`, `u4_rem`, `u5a_rem`, `u5b`, `u5b_rem`, `u6`, `u7`, `u8`, `u9a`, `u9b`,
`u9b_rem`, `u9b2`, `u10`, `u10_rem`, `u11` and `tails`: all 0 mismatches. Two suites
that already failed with the previous merged DLL (`build32-r3d-u5bl`) fail identically here, so they are not
regressions: `diff_r3d_u5a` (`GetType@GenericShaderMaterial` static PString, 1000/31000) and `diff_r3d_u5a2`
(material ctor PString, 1119/11334).

## Files

- new `src/jet/graphics/Render3DCore_7c/Final15.h`
- new `src/jet/graphics/Render3DCore_7c/Final15Materials.cpp` - the 11 material virtuals
- new `src/jet/graphics/Render3DCore_7c/Final15OrientedBox.cpp` - the 4 OrientedBox entries
- new `src/jet/graphics/Render3DCore_7c/implemented.d/final15.txt` - 15 exports
- new `src/jet/tests/diff_r3d_final15.cpp`
- no other file changed; `implemented.txt`, `CMakeLists.txt`, `notes/Render3DCore.md` untouched.

The CMake test loop does not list `final15` (CMakeLists is not owned by this unit); add it to the `diff_r3d_*`
foreach, or build the test by hand as above.
