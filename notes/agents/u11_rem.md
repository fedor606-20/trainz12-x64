# U11 remainder: Facility, static tables, plugin entry

Date: 2026-11-24. Wave 4 continuation of `u11_statics`. The U11 slice is **59 of 167 exports local**: the 49 from
`u11_statics` plus the 10 added here. The other 108 are the orphaned template/free-function exports that belong to
other units (listed below); U11 does not re-implement them.

Built and tested in `build32-r3d-u11` (Win32 Release, no `/FORCE`, default linker options).

## Implemented (10 new)

`Facility` (4):

```
??0Facility@Jet@@QAE@ABV01@@Z
??0Facility@Jet@@QAE@XZ
??4Facility@Jet@@QAEAAV01@ABV01@@Z
??_7Facility@Jet@@6B@
```

`BackgroundResource` / `BackgroundLoadingThread` exported data (5):

```
?actionNames@BackgroundResource@Jet@@1QBUNameCheck@12@B
?fsm@BackgroundResource@Jet@@1QAY08$$CBUStateTable@12@A
?inputNames@BackgroundResource@Jet@@1QBUNameCheck@12@B
?stateNames@BackgroundResource@Jet@@1QBUNameCheck@12@B
?results@BackgroundLoadingThread@Jet@@2QBUResultTable@2@B
```

`JetGetPluginsRender3DCore` (1). Local: `JetGetPluginsRender3DCore`.

Total U11 local: 59 (the previous fragment 49 + `implemented.d/u11_rem.txt` 10).

## What the 10 are, and the ABI facts

- **`Facility` is exactly one virtual slot.** 0x1007d420 holds slot 0 = 0x100079e0, the shared `xor eax,eax; ret`
  stub; the Complete Object Locator is at 0x1007d418/-4 and the next vtable's COL starts at 0x1007d424. The notes'
  earlier "at least 15 slots" read the following unrelated vtables as one table. `Facility` is 4 bytes, has no
  virtual destructor, constructors only write the vftable (0x10003150/0x10003160), `operator=` is `mov eax,ecx;
  ret 4` (0x100021cb0, shared with `DefaultInterpolator<Quaternion>` and `Function`), and `GetName` returns a null
  `PString&` (the same stub). The copy constructor must write the class vftable, not copy the source's; the test
  proves it by poisoning the source vptr.
- **The debug tables are arrays, not pointers.** MSVC mangles an array of `const T` like a pointer to `const T`,
  which is why the export table says `NameCheck const * const` / `ResultTable const * const`. The original data:
  - `stateNames` at 0x1007d158: 10 entries, 0x50 bytes, ends exactly at `inputNames`.
  - `inputNames` at 0x1007d1a8: 9 entries, 0x48 bytes, ends exactly at `actionNames`.
  - `actionNames` at 0x1007d1f0: 12 entries, 0x60 bytes, ends exactly at `results`.
  - `results` at 0x1007d250: 3 `ResultTable` entries, 24 bytes. The original's `results` symbol *is the array*.
    State 4's name is the original's "Requesed" typo.
  - `fsm` at 0x1007cbb8 is a 4-byte null pointer; the real 10x9 table starts at 0x1007cbc0 and is read directly
    by `DoAction`.
- `BackgroundLoadingThread::results` must stay a pointer for `ResultClass<&results, Results>`, so the **exported
  name is an alias** to an internal `resultsTable[3]` array (`...2QBU...=?resultsTable@...1QBU...`). That keeps the
  exported data byte-shaped like the original (24-byte table) while the local template symbol remains a pointer.
- **`JetGetPluginsRender3DCore` order** (0x100608e0): AutoTex, NoTexture, OneTexture, OneTextureNoAlpha, Billboard,
  Gloss, GenericShader, Reflective, MultiPassBump, MultiPassBump2, MultiPassBumpSpec, MultiPassBumpSpec2,
  MultiPassNoTexture, MultiPassOneTexture, `ResourceType<SplineResource>`, ColorOneTexture. The first four helpers
  also call the shared animatedLit registration at 0x10060050; the others do not. Billboard (0x10060310) and the
  SplineResource resource type (0x10060770) have no exported default constructor.
- The entry does not re-implement any plugin class. The eight U5a2 classes with a tested local default constructor
  (AutoTex, NoTexture, OneTexture, OneTextureNoAlpha, Gloss, GenericShader, Reflective, ColorOneTexture) are
  constructed through their exports and then the first four call 0x10060050 at its RVA. The six MultiPass plugins
  (U5b), Billboard and the resource type run the original's static helpers at their RVAs; an export that is missing
  at run time also falls back to that helper. This leaves every object byte-identical to retail.

## Test

Command:

```
cmake -S src/jet -B build32-r3d-u11 -A Win32
cmake --build build32-r3d-u11 --config Release --target Render3DCore_7c diff_r3d_u11
diff_r3d_u11.exe <testdir> 1000
```

`testdir-u11` is `testbin-r3d` plus the built DLL; `Render3DCore_7c.dll` is renamed `Render3DCore_7c_orig.dll`.

Result: **600596 checks, 0 mismatches over 1000 rounds** (previous test: 600443; +153 checks). The additions:

- every entry of `actionNames` (12), `inputNames` (9), `stateNames` (10): code, array index and text against the
  original and the expected literal;
- `fsm` is null on both sides; the three `results` names/descriptions against the original and the literals;
- `Facility`: ctor, copy ctor (with the source vptr poisoned), assignment returns `this` and keeps the vtable, and
  the one virtual slot returns a null `PString&`;
- the plugin entry is called and returns without raising. It is called **last**, after the background tests:
  registering the plugin types rearranges KernelStandard's `Alloc` pools, and the hand-built fake
  `BackgroundResource`/`BackgroundLoadingThread` fixtures in this test are sensitive to that rearrangement. Calling
  the original entry at RVA 0x100608e0 in the same position reproduces the same effect, so it is a test-fixture
  interaction, not this implementation.

`python -I tools/check_exports.py build32-r3d-u11/Release/Render3DCore_7c.dll
D:/trainz12-decomp/exports/Render3DCore_7c.tsv` -> `2500 exports, TSV 2500; missing/wrong 0, extra 0`.
`tools/check_thunks.py` with the merged implemented list: 1938 checked, 6 import thunks, none from U11 (the 6 are
pre-existing `AlphaBlendCache`/`Render2DPrimitive`/`Render3DPrimitive`/`RenderPrimitive`/`Cell` entries owned by
other units).

## Remaining forwards (108) and why

These are the same orphaned names `u11_statics` listed; no other stream's TSV owns them, but they are not U11
classes:

- 56 `GeometryChunkTemplate<PackedVertexPNTA|PNTTA, GeometryChunkSingleStream>` members (U6/U5b). The geometry
  chunk contract is not reconstructed here.
- 37 `ITree<Particle>` members (U9a). `Render3DCoreParticle.h` declares the template but no local translation unit
  emits its out-of-line members.
- 7 `StdMaterialPluginTemplate<AutoTexMaterial>` members (U5a).
- 6 free `Line` operators `??D/??G/??H/??KJet` (U1d1; `Line.cpp` implements the member operators only).
- 2 `DefaultInterpolator<Quaternion>` members (animation/Foundation; no local Quaternion interpolator).

No U11-owned export remains forwarded. The four `BackgroundResource` debug statics and
`BackgroundLoadingThread::results` are local now; the previous notes' four "debug-only" forwards are gone.

## Files

Added:

- `src/jet/graphics/Render3DCore_7c/U11Facility.cpp`
- `src/jet/graphics/Render3DCore_7c/U11Statics.cpp`
- `src/jet/graphics/Render3DCore_7c/U11Plugins.cpp`
- `src/jet/graphics/Render3DCore_7c/implemented.d/u11_rem.txt` (10 names, with the `results` alias)
- `notes/agents/u11_rem.md`

Edited (existing U11 files):

- `src/jet/include/jet/Render3DCoreBackground.h`: `NameCheck`/`StateTable` declarations, the three name arrays,
  `fsm`, the local `results` pointer plus `resultsTable[3]`, and `Facility`.
- `src/jet/graphics/Render3DCore_7c/Background.cpp`: the file-local FSM struct was renamed `LocalStateTable` so
  the new nested `BackgroundResource::StateTable` cannot shadow it.
- `src/jet/tests/diff_r3d_u11.cpp`: the static-table, Facility and plugin-entry checks.

## Blockers

None inside U11. The 108 forwards need the owning units' headers/instantiation contexts (U6, U9a, U5a, U1d1,
animation/Foundation) before they can move.
