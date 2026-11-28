# U9b2 `modifiers`: report

Status: done for the bone/skeleton modifier state and arrays. **55 of the 64 forwarded bone/skeleton/modifier
exports are local**, 9 still forward (7 `SkeletonModifier` chain/RTTI methods, `BoneBlender::UpdateBoneModifiers` and
the `SkeletonModifier::Update` export). The differential test reports 0 mismatches over 1000 rounds (82,726 checks),
the module still has 2500 exports and the fragment audit shows all 55 names local (no forwarders). The 26 remaining
scene forwards from U9b are unchanged and were reviewed again; they are not bone/modifier exports.

- `diff_r3d_u9b2.exe <testdir> 1000` -> `82726 checks, 0 mismatches`
- `python tools/check_exports.py build32/Release/Render3DCore_7c.dll D:/trainz12-decomp/exports/Render3DCore_7c.tsv`
  -> `DLL 2500 exports, TSV 2500; missing/wrong 0, extra 0`
- fragment audit: `55 names, missing 0, forwarded 0`

## Files

- `src/jet/include/jet/Render3DCoreBones.h` - `BoneModifier`, `BoneBlender`, `SkeletonModifier`. Layouts and the
  overload/vtable order are static-asserted against the original vftables at RVAs 0x7e36c, 0x7e3d0/0x7e400 and
  0x7e428.
- `src/jet/graphics/Render3DCore_7c/Bones.cpp` - object lifetime, the priority-sorted `BoneModifier` array of a
  `BoneBlender`, the `SkeletonModifier` blender array (built from the `b.` particle children) and the wrappers over
  those arrays; originals are called through `Render3DCore_7c_orig.dll` for the forwarded bodies.
- `src/jet/tests/diff_r3d_u9b2.cpp` - own vs original, raw byte compare for the value fields, list-order compare by
  priority, vtable-slot identity checks; 1000 deterministic rounds with an awkward (signed zero/inf/NaN) round every
  seventh.
- `src/jet/graphics/Render3DCore_7c/implemented.d/u9b2_modifiers.txt` - 55 mangled names, newly implemented only.

No shared file was edited. This copy's `CMakeLists.txt` test loop predates `u9b2`; for local verification only the
target was injected with `-DCMAKE_PROJECT_trainz12_jet_INCLUDE=<tmp>/u9b2_target.cmake`. The main tree already has
`u9b2` in its loop, so the merge needs no CMake change.

## Implemented (55)

| class | local/total | names |
|---|---:|---|
| `BoneModifier` | 18/18 | 5 ctors (default/copy/name/host/kernel), dtor, assign, vtable, `GetInfluence`/`GetInfluenceLimit`/`GetInfluenceInTime`/`GetInfluenceOutTime` (fldz 0.0f), `GetRootBone`, `SetRootBone`, `GetSkeleton`, `GetUpdateFlag`, `SetUpdateFlag`, `nullString` |
| `BoneBlender` | 18/19 | 2 ctors, dtor, assign, vtable, `Add`, both `InsertBefore`, both `InsertAfter`, both `Remove`, `RemoveAll` (`__thiscall`), `GetBoneName`, `GetBone` (2), `SetBone`, `GetFirstReplaceable` |
| `SkeletonModifier` | 19/27 | 4 ctors, dtor, assign, both vtables, `GetRootBone`, `GetNumBones`, `GetBoneBlender`, `UnlinkBlenders`, `RemoveBlender`, `RemoveBlenders`, both `Remove`, `RemoveAll`, `UpdateModifiers`, `Update` (local vtable override), `CreateBlenders` |

The priority list is the original's: `Add`/`InsertBefore(m)` stop before the first entry of equal or higher priority,
`InsertAfter(m)` after its equals, and a NaN comparison is unordered so the scan continues to the end (the loops are
written as the original's negated `FCOMIP` conditions, not as plain `<`/`<=`). `InsertBefore/After(m, other)` require
equal priorities, insert next to `other` and append when `other` is not in the list. `BoneBlender` copies/assignments
use `FastDynamicArray::AppendChunks`, matching the original's chunk-wise `FUN_1000fcf0`/`FUN_1000fca0`.

`SkeletonModifier::CreateBlenders` walks the particle subtree depth first (children at `Particle+0x14`, next at
`+0x1c`) and creates one `BoneBlender` for every child whose interned name starts `b.`; the constructor calls it,
and the destructor removes/deletes the blenders under `ModifierList::modifierListLock` after detaching the embedded
`Modifier` from its host.

## Forwarded (9) and why

- **`BoneBlender::UpdateBoneModifiers`** - the influence blend loop: it calls `BoneModifier::GetUpdateFlag(FIXED/
  FINAL/ALWAYS)`, `GetInfluence`, `GetRootBone`, `UpdateBone`, reads the bone particle's visible bit (+0x48) and
  keeps the remaining influence in x87. The vtable slot is a local thunk to the original body (`GetProcAddress` on
  `Render3DCore_7c_orig.dll`), so a local `BoneBlender` still updates exactly as the original; the export forwards.
- **`SkeletonModifier::Add`, `InsertBefore` (2), `InsertAfter` (2)** - all go through `AddToBoneBlenders`, which does
  `__RTDynamicCast` from `Modifier` to `BoneModifier`, calls the pure `EnumerateBones` slot and inserts into the
  blenders. The local thunks call the original bodies; a differential probe showed the original's RTTI does **not**
  accept a `BoneModifier` built by our module (crash/refusal), so these are real blockers, not only forwarding.
- **`SkeletonModifier::AddToBoneBlenders`** - same RTTI + `EnumerateBones` + `PStringList` path; needs the derived
  `BoneModifier` subclasses' type_info (they live in Render3DFoundation/trainz) before it can be local.
- **`SkeletonModifier::SwitchParticles`** - rebuilds the bone blenders across an old/new particle tree and swaps the
  arrays; the original body is kept behind a thunk.
- **`SkeletonModifier::Update` export** - the original's exported body at 0x1006c450 takes a **`Modifier*`**
  (object+0x18): it reads complete at +0x10 and the blender array at +0x18. Our local override is a normal
  `SkeletonModifier*` member, so its vtable entry in the secondary `Modifier` vtable is an adjustor thunk and the
  named export would have a different `this` convention. The export therefore stays forwarded; the local override is
  what the vtable uses (the test calls it through the vtable).

The U9b scene forwards (26: `Cell` traversal/stats, `Portal`/`PhysicalPortal` `Enter`/`Leave`/`Cull`/`Traverse`/
`CreateCamera`/`IsBackfacing`, `Mirror` `Cull`/`CreateCamera`, `ReferencePoint` ctors/`GetSubNames`, `Corona::cache`)
keep their U9b reasons: they need U3 dirty-flag helpers, U8 `ViewPoint`/`Camera` and the Display/stencil API, or the
original-owned stats/cache data. None of them is in the U9b2 bone/modifier slice.

## Vtable / ABI facts

- `BoneBlender` 0x14: `{vftable +0, SkeletonModifier* +4, PString boneName +8, Particle* bone +0xc,
  FastDynamicArray<BoneModifier*> +0x10}`; no virtual destructor (`??1` is `QAE`). Vtable 9 slots: 0 `Add`,
  1 `InsertBefore(m, before)`, 2 `InsertBefore(m)`, 3 `InsertAfter(m, after)`, 4 `InsertAfter(m)`, 5
  `Remove(name)`, 6 `Remove(modifier)`, 7 `RemoveAll` (the one `__thiscall` virtual, `UAE`), 8
  `UpdateBoneModifiers`. MSVC reverses each overload set; the test checks each slot against that side's export.
- `BoneModifier` 0x20: `Modifier` 0x18 + root-bone `PString` +0x18 + `unsigned long` flags +0x1c. Vtable 24 slots
  (0x7e36c): 0 deleting dtor, 1..7 import thunks to RenderSystem_7c's `Modifier::GetName`/`SetName`/`GetPriority`/
  `SetPriority`/`SetHost(kernel)`/`SetHost(host)`/`SetComplete`, 8 the shared `0x100079e0` stub (`Update`; ours
  returns result 0), 9 `EnumerateBones` stub, 10 `GetSkeleton`, 11 `GetRootBone`, 12 `SetRootBone`, 13..16 the
  `fldz` influence getters (0x10010e60, all 0.0f), 17..20 `SetInfluence*` stubs, 21 `GetUpdateFlag`,
  22 `SetUpdateFlag`, 23 `UpdateBone` stub. The stubs are not exported; a local thunk to the original body is only
  needed for the blend loop.
- `SkeletonModifier` 0x34: `ModifierList` 0x18, `Modifier` at +0x18, `FastDynamicArray<BoneBlender*>` head at
  +0x30. Two exported vtables: `ModifierList` 0x7e3d0 (11 slots) and `Modifier` 0x7e400 (9 slots; slot 8 is the
  local `Update` adjustor thunk).
- `BoneModifier::nullString` is a `PString&` to the original's zero-initialised empty string (node 0 at
  0x100dd0a0); ours binds a zeroed static `PString` (node null).
- `SkeletonModifier`'s modifier name is the interned `"JETSkeletonModifier"`; both modules share KernelStandard's
  `PStringCache`, so name nodes are identical across sides and raw byte comparisons are valid for every value field.
- `ModifierList::modifierListLock` is a data export of RenderSystem_7c; the shared `Modifier.h` cannot mark it
  `dllimport` (RenderSystem defines it), so `Bones.cpp` resolves its address with `GetProcAddress` once, as the
  module does for other original data.

## Test

```
cmake -S "D:\universal modder\r3d-agents\u9b2\src\jet" -B "D:\universal modder\r3d-agents\u9b2\build32" -A Win32 \
  -DCMAKE_PROJECT_trainz12_jet_INCLUDE=C:/Users/Fedor/AppData/Local/Temp/opencode/u9b2_target.cmake
cmake --build "D:\universal modder\r3d-agents\u9b2\build32" --config Release --target Render3DCore_7c diff_r3d_u9b2
copy D:\universal modder\testbin\* testdir
ren testdir\Render3DCore_7c.dll Render3DCore_7c_orig.dll
copy build32\Release\Render3DCore_7c.dll testdir
copy (main build) Kernel_7c/KernelStandard_7c/KernelResource_7c/KernelResources_7c/Render2DCore_7c/RenderSystem_7c.dll testdir
set PATH=testdir;%PATH%
diff_r3d_u9b2.exe testdir 1000   -> 82726 checks, 0 mismatches
```

Coverage: `BoneModifier` ctors/copy/assign/default state/getters/setters/flags and vtable slots; `BoneBlender`
construction, sorted Add/InsertBefore/InsertAfter (including the `other` forms and repeated priorities), recompute
order after every operation, Remove by pointer/name/absent, RemoveAll, copy/assign, GetFirstReplaceable with FIXED,
GetBone/SetBone/GetBoneName and vtable slots; `SkeletonModifier` ctors (null particle), empty getters, Remove/
RemoveAll/UpdateModifiers/Update (including the complete-bit early out) and copy/assign; `CreateBlenders` on a real
particle tree (`b.one`, `b.two`, `b.three` direct children, sorted order/root bone/GetBoneBlender), `UnlinkBlenders`,
`RemoveBlender` and `Update` over blenders. Every 7th round uses signed zero, infinities, quiet/signalling NaNs and
the resulting insertion order is compared bit for bit.

## Blockers / follow-ups

- `AddToBoneBlenders`/`Add`/`Insert*` need local RTTI and the `EnumerateBones` list. Until then a `BoneModifier`
  created by this module is not accepted by the original body's `__RTDynamicCast`, so `Add`/`Insert` through the
  vtable is not equivalent for our own objects. This is the main remaining functional gap of the unit.
- `UpdateBoneModifiers` and `SwitchParticles` stay original bodies behind vtable thunks; they work on our object
  layouts but keep the unit dependent on `Render3DCore_7c_orig.dll`.
- The `BoneModifier` vtable slot 8 (`Update`) is the original's shared `xor eax,eax; ret` stub (it does not write
  the memory result); our local override returns result 0. Observable only when a bare `BoneModifier` is updated
  inside a `ModifierList` (derived classes override it); kept as the safe behaviour.
- `Cell`, the Portal families, `Mirror`, `ReferencePoint` and `Corona` remain U9b's forwards (U3/U8/Display and the
  original-owned stats/cache data); `ModifierHost`, `Modifier` and `ModifierList` are RenderSystem_7c's local
  implementation and are used here only as bases, with no new exports.
