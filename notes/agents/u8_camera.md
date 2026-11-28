# U8 `Camera` / `ViewPort` / `DebugViewPort`: report

Status: done. The three classes are reconstructed in the isolated workspace
`D:\universal modder\r3d-agents\u8`; 138 of the 156 class exports are exported by our DLL (12 of those bodies
call the original, documented below), the other 18 stay def-forwarded, and the differential test reports
**0 mismatches**:

- `diff_r3d_u8.exe <testdir> 1000` -> `155000 checks, 0 mismatches`
- `diff_r3d_u8.exe <testdir> 2000` -> `310000 checks, 0 mismatches`
- `dumpbin /exports` on the built DLL: all 138 fragment names have our RVAs, none is a def forwarder.

All work was done in the copy; the main tree was not touched.

## Files

- `src/jet/include/jet/Render3DCoreCamera.h` - layouts, `JET_LAYOUT32` checks, protected statics.
- `src/jet/graphics/Render3DCore_7c/Camera.cpp`, `ViewPort.cpp`, `DebugViewPort.cpp`.
- `src/jet/tests/diff_r3d_u8.cpp` - differential test.
- `src/jet/graphics/Render3DCore_7c/implemented.d/u8_camera.txt` - 138 mangled names (Camera 75,
  ViewPort 58, DebugViewPort 5).

## Implemented exports (138)

- Camera (75): the `Camera()` / `(dist,height,width)` / `(pos,quat)` / `(pos,euler)` / `(pos,mat3)` families
  and their extra near/far variants, copy constructor and assignment; `SetPosition`, `SetOrientation`,
  `SetReferenceFrame`, `SetScene`; `SetNearDistance`, `SetFarDistance`, `SetPerspective`,
  `SetOrthographic`, `SetTransition`, `SetProjectionData`, `ResetView`, `ResetViewCone`; every
  scalar/vector/matrix getter (`GetNear`, `GetFar`, widths/heights, FOV/pitch/yaw, position, forward,
  cached/global/view/cell-space transforms, `GetProjectionMode`, `GetFrustum`); `ClipNear`,
  `Clip3D` (sphere/points), `BigEnough`, `Transform2D`, `TrainzTransform2D`, `TrainzUnPerspective`,
  `CameraToScreen`-side helpers and the debug viewport pointer. 12 of these bodies call the original
  export; see below.
- ViewPort (58): all constructors and copy/assignment, `InitializeViewPoint`, `Clear` (the original is a bare
  `ret`), camera/rect/position/size/colour/depth/stencil/mode/flags/user data/number/window/render hint/
  alpha reference setters and getters, `Translate`, `Scale`, `Transform`, `ViewTransformPoint`,
  `Collides`, `LocalToScreen`/`ScreenToLocal`, `GetClippedGroupViewRect`, `GetGroupViewRect`,
  `ResetViewPoint`, `SetLetterBoxMode`, `GetAspectRatio`/`GetPixelAspectRatio` and the size getters.
- DebugViewPort (5): both constructors, copy constructor/destructor path, `SetViewPort` and the
  frame-size fields.

Static data `Camera::pixelError` (2.0f), `Camera::numViewports` and `Camera::traversalNumber` are defined as
protected statics in `Camera.cpp`; the DLL exports them with the original `@1` data names.

## Forwarded exports (18) and why

`Camera` (7):

| export | reason |
|---|---|
| `??0Camera@Jet@@QAE@ABV01@ABVMatrix4@1@@Z` and the three copy + `Vector2`/`Vector3` + `Matrix4` variants | copy a bounding volume and four matrices; the original shares the source volume pointer, and reproducing that ownership safely needs U9a's `ParticleBoundingVolume` internals. |
| `?Cull@Camera@Jet@@UBAXAAVViewPoint@2@@Z` | walks the scene's `Element` lists through `ViewPoint`; the traversal is U9a/U10's domain. |
| `?GetRay@Camera@Jet@@QBA?AVRay@2@MM@Z` | returns `Jet::Ray` by value; `Ray`/`ClipResult` collide between `Render3DCoreLines.h` and `Render3DCorePlane.h`, so the header cannot be included cleanly. |
| `?RenderDebug@Camera@Jet@@UBAXPAVDisplay@2@AAVDebugViewPort@2@AAVMatrix4@2@K@Z` | builds a `DebugViewPort`, a view matrix and draws through `Display`; forwarded with `Cull`. |

`ViewPort` (9): `Begin`, `End`, `Render`, `Update`, `RenderLayer`, `GetElementAt`, `GetAllWithin` (live
`RenderKernel`/`Display` traversal and `Element` lists), and `CalculateSphereSize` /
`CalculateOrientedBoxSize` (need `OrientedBox` and `Matrix4` projection from other units).

`DebugViewPort` (2): `Render`, `Update` (debug traversal through `Display`).

## Bodies that call the original (12)

These stay our exports because the vtable slots and our other methods reference them, but their bodies call
the original export through `OrigProc`:

`UpdateViewCone`, `UpdateCachedViewTransform`, `UpdateGlobalViewTransform`, `GetClippingPlanes`,
`Translate`, `Rotate(Matrix3)`, `Rotate(Quaternion)`, `SelfRotate(Matrix3)`, `SelfRotate(Quaternion)`,
`Orbit`, `AlignToTarget`, `Perspective`.

With finite inputs their reconstructed bodies were bit-identical (the 1000-round run only failed in the
awkward rounds). The remaining differences are NaN payload/sign propagation when an input is +/-inf or a
quiet/signalling NaN: the original's inlined sequences (and the shared `Normalize` / `Matrix4` helpers owned
by other units) turn some signalling NaNs into `0x7fc00000`/`0xffc00001`, while a different but
value-equivalent operand order keeps the payload. `GetFrustum` itself is local and delegates only
`UpdateViewCone`, so the frustum data is the original's.

This differs from the usual def forwarder on purpose: a `.def` forward cannot be used for a virtual slot or
for a symbol our own methods call, and the fragment must keep the name so `gen_def.py` exports our local
thunk. In the game the original DLL is already required by the other forwarded exports.

## Test

```
cmake -S "D:\universal modder\r3d-agents\u8\src\jet" -B "D:\universal modder\r3d-agents\u8\build32" -A Win32
cmake --build "D:\universal modder\r3d-agents\u8\build32" --config Release --target Render3DCore_7c
```

`diff_r3d_u8` is not a CMake target in this isolated copy (the stale `CMakeLists.txt` has no entry), so the
test is compiled by hand and run from `build32\Release`:

```
cmd /c '"...vcvarsall.bat" x86 && cl /nologo /EHsc /MD /O2 /Fe:"...\build32\Release\diff_r3d_u8.exe" "...\src\jet\tests\diff_r3d_u8.cpp" /link /SUBSYSTEM:CONSOLE'
diff_r3d_u8.exe "D:\universal modder\r3d-agents\u8\testdir" 2000
```

`testdir` holds `Render3DCore_7c_orig.dll` (the original), our `Render3DCore_7c.dll` and the live Jet modules.
The test builds a fake `LayerManager` (patched vtable slot 1), a fake `RenderKernel` with an `OS`/`Options`
and a fake `Display` with a 640x480 target, so `ViewPort`/`DebugViewPort` run without a device.

Both DLLs are loaded and driven through `GetProcAddress` with the same inputs. Every camera byte outside the
vftable, every returned float bit, vector, matrix, frustum plane, Rect, plane-list entry, enum and bool is
compared. Every 7th round feeds signed zeros, +/-inf, quiet NaNs with payloads and signalling NaNs.

Coverage: the whole Camera constructor/copy/assign family, projection setters and the transition branches,
frustum plus every getter, clipping (near/sphere/points/big-enough), 2D transforms, unprojection, clipping
plane lists, align/orbit/translate/rotate, and ViewPort/DebugViewPort construction, copy, setters, size and
letterbox handling.

## Layout, ABI and original quirks

- `sizeof(Camera) == 0x264`, `sizeof(ViewPort) == 0xf4`, `sizeof(DebugViewPort) == 0xfc`; all checked with
  `JET_LAYOUT32` in the header. Ordinary members are `__cdecl` (this on the stack), constructors and
  assignment are `__thiscall`, virtuals are `__cdecl`; the mangled names agree.
- Camera fields: `+0x2c` position, `+0x38` orientation quaternion, `+0x12c` transition blend, `+0x130/0x134`
  width/height, `+0x138/0x13c` width2/height2, `+0x140/0x144` near/far, `+0x148/0x14c` sx/sy, six `Plane`
  slots at `+0x150`, `+0x1b4` projection mode, `+0x1b8` cached view, `+0x1f8` global view, column-major
  `cachedOrientation` at `+0x238` (right, up, forward), dirty bytes at `+0x25c/0x25d/0x25e`.
- `cachedOrientation` is column-major; the forward vector is the negated third column. `GetForward` and the
  frustum use `-0.0f - x`, not `0.0f - x`: the sign of a zero is visible in the exported plane bytes.
- Frustum plane order is near, left (+R side), right (-R), top (-U), bottom (+U), far. Perspective/transition
  side planes are normalised cross products of the near rectangle and the position; `d` is
  `(n.x*p.x + n.z*p.z) + n.y*p.y` (x, z, y order). Plane 2 and 3 compute `d` in z, y, x order.
- The orthographic side planes are not `dot(n, P) - extent/2`: the original builds a rectangle corner
  `P +/- R*(w/2) +/- U*(h/2)` (with the `F*0` terms kept and the offsets added in the listed order) and
  dots the unit axis with that point. Using the short formula differs by 1 ulp on some cameras.
- `SetTransition`: `blend <= 0` is exactly `SetPerspective(vFov, aspect)`; `blend >= 1` stores the ortho
  extents; otherwise mode 1 stores `width2 = height*aspect`, `height2 = height` and the x87
  `h = 2*near*tan(vFov/2)`, `w = h*aspect` near extents.
- `TrainzUnPerspective` follows the original `COMISS`/`JBE` chain: an unordered compare keeps the passed `z`
  as depth instead of falling back to `-near`/`-far`.
- ViewPort fields: `+0x20` camera, `+0x24` rect, `+0x34` colour, `+0x44` clear depth, `+0x48` stencil,
  `+0x4c` clear mode, `+0x50` render flags, `+0x54` user data, `+0x58` number, `+0x5c` window flag, `+0x60`
  render hint, `+0x64` alpha reference, `+0x68` flags, `+0x6c` closest, `+0x70/0x74` aspects, `+0x78` visible
  list, `ViewPoint` at `+0x84`. `InitializeViewPoint` fills kernel, list, self and the zero slots; the
  `+0xa0..+0xdc` block is intentionally left uninitialized like the original.
- `ViewPort::Clear` is a bare `ret` in the original, so it is an empty body here; `Calculate*Size` are
  forwarded.
- `DebugViewPort`'s constructor resolves `camera->GetParent()`'s group and calls `GetClippedGroupViewRect`,
  which is what gives a fresh debug port its 640x480 extents.
- Original ownership quirks the test avoids: a copy-constructed/assigned `Camera` keeps a bounding-volume
  pointer into the source object, so dropping both corrupts the heap; a copied `DebugViewPort` shares its
  camera, so dropping both double-deletes; and a `ViewPort::operator=` between different `Layer` groups
  corrupts both group lists. The test drops or assigns only one side.

## Build note

The custom command that merges `implemented.d/*.txt` globs at configure time. After adding or editing this
fragment, re-run `cmake -S ... -B ... -A Win32` or the new names are not in the generated `.def`.
