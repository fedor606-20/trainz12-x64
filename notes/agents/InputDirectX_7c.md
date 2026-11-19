# InputDirectX_7c — analysis/planning report (read-only)

- **Scope:** unfinished module `InputDirectX_7c` (group: input), Trainz 2012 64-bit port project.
- **Repo:** `D:\universal modder\trainz12-x64`
- **Evidence read:** `README.md`, `PORT_PLAN.md`, `notes/InputSystem.md`, `notes/agents/COORDINATION.md`, `notes/agents/STATUS.md`, `src/jet/input/InputSystem_7c/{Input.cpp,Devices.cpp,Force.cpp,implemented.txt}`, `src/jet/input/InputWin32_7c/InputWin32.cpp`, `ghidra_maps/jet/input/InputDirectX_7c.dll.functions.tsv`, `ghidra_maps/jet/input/vtnames_InputDirectX_7c.dll.tsv`, `D:\trainz12-decomp\exports\InputDirectX_7c.tsv`.
- **Constraint:** no source/build/doc edits outside this report; no build/test/game/git actions.

## 1. Overview / role

- **Current status (per README/PORT_PLAN context):** 19 KB, 122 functions, **0/1 exports implemented**, not started. PORT_PLAN step `8g`: *"InputDirectX_7c, the DirectInput driver (joysticks, wheels, force feedback through InputSystem's ForceEffect). Loaded at the menu but keyboard and mouse come from InputWin32; left out of step 5."*
- **Exported surface:** exactly 1 export (`D:\trainz12-decomp\exports\InputDirectX_7c.tsv`): ordinal 1, `JetGetPluginsInputDirectX` (rva 0x1ec0, thunked in function map at `10001ec0`, size 5, `IMPORTED|true`).
- **Purpose (evidenced/inferred from names + InputSystem contracts):** a Jet input-driver plugin DLL that registers a DirectInput-backed `InputDriver` implementation ("DirectX") so `Jet::Input` can create/use DirectInput devices, especially **joysticks/wheels and force feedback**. Keyboard/mouse are expected to remain primarily served by `InputWin32_7c` in normal menu/gameplay flow (PORT_PLAN note), but function/vtable names show the binary also contains DirectX keyboard/mouse device classes — implementation must preserve original behavior/registration semantics, not assumptions.
- **Relation to done input work:** `InputWin32_7c` is complete and handles system keyboard/mouse via window hooks + WinMM joystick API (`src/jet/input/InputWin32_7c/InputWin32.cpp`). `InputSystem_7c` is complete (327/327) including driver/device/observer abstractions, emulation, action mapping, and force-feedback base/resource logic (`Input.cpp`, `Devices.cpp`, `Force.cpp`, `notes/InputSystem.md`). `InputDirectX_7c` therefore plugs into a stable, mostly-known API contract rather than defining it.

## 2. Inventory (from exports, function map, vtable names)

### 2.1 Exports and top-level entry
- `JetGetPluginsInputDirectX` (`10001ec0`, thunk): sole plugin-table entry point, analogous role to other Jet plugin DLLs / `JetGetPluginsInputSystem` in `InputSystem_7c/Force.cpp` area of responsibility (plugin registration side). Exact table contents/format to be recovered from original behavior during implementation.
- `PluginFactoryOf<Jet::InputDriverDirectX,Jet::InputDriver>` present: `vf0/vf1` thunks `1000609c/100060a2`, `vf2` `10001f50` (36), `vf3` `10001ed0` (126). This strongly indicates factory-based registration/creation of the driver instance.

### 2.2 Function-map summary
- `ghidra_maps/jet/input/InputDirectX_7c.dll.functions.tsv`: **156 data rows + header = 157 lines total** (122 functions stated in README; treat Ghidra row count as current map granularity including thunks/unwind/CRT/import helpers — use per-row classification during implementation, not raw count alone).
- External/import thunks visible: `DINPUT8.DLL DirectInput8Create` (`100060a8`), MSVCR80 symbols (`__RTDynamicCast`, `initterm*`, `_amsg_exit`, `memset`, `memcpy`, etc.), CRT/security/init/unwind helpers.
- Large/behavior-heavy functions (likely core logic, by size/name): `FUN_100016d0` (1333), `Jet::DirectXJoystick::vf14` (1567), `Jet::DirectXJoystick::vf18` (1302), `Jet::DirectXKeyboard::vf14` (943), `Jet::DirectXMouse::vf14` (897), `Jet::DirectXJoystick::vf6` (783), `FUN_10002660` (687), `Jet::DirectXJoystick::vf5` (612), `Jet::DirectXMouse::vf4` (609), `Jet::InputDriverDirectX::vf3` (502), `Jet::ForceEffectDirectX::vf8` (396), `Jet::DirectXKeyboard::vf4` (387), `FUN_10004380` (362), `Jet::ForceEffectDirectX::vf12` (330), etc.

### 2.3 Identified classes / vtable slots (from `vtnames_InputDirectX_7c.dll.tsv` + function map)
- **`Jet::InputDriverDirectX`** (driver): `vf0` appears at `10002230` (83) and `10002aa0` (20), `vf1` thunk `10006096` / `10002a90` (8), `vf2` `10002290` (131), `vf3` `10002320` (502), `vf4` `10002520` (29), `vf5` `10002540` (1). Maps naturally onto `Jet::InputDriver` contract in `InputSystem_7c/Devices.cpp` (`Init`, `Update`, `Enumerate`, `CreateDevice`, `CriticalShutdown`, `CriticalStartup`).
- **`Jet::InputDeviceDirectX`** (base/device helpers): shared/thunked slots across keyboard/mouse/joystick (`vf0..vf3`, many common no-op/thunk slots `vf5..vf13`, `vf17`, plus concrete `vf1` `10001db0`, `vf15` `10001d60`, `vf16` `10001da0`, `vf4/vf14/vf18` collapsed to thunk `1000747e` in vtnames view — needs careful decompilation to separate actual implementations per derived class).
- **`Jet::DirectXJoystick`**: rich override set (`vf0,vf1,vf4..vf16,vf18` all present with substantial bodies/thunks), indicating joystick-specific polling/state, caps/properties, and likely force-feedback creation/management hooks.
- **`Jet::DirectXKeyboard`**: `vf4,vf5,vf6,vf14,vf18` concrete.
- **`Jet::DirectXMouse`**: `vf4,vf5,vf6,vf14,vf18` concrete.
- **`Jet::ForceEffectDirectX`**: `vf0..vf13` present (mix of thunks and concrete methods: `vf1` 36, `vf4` 102, `vf5` 27, `vf6` 124, `vf7` 56, `vf8` 396, `vf9` 170, `vf10` 88, `vf12` 330, `vf13` 45). This is the DirectInput-backed counterpart to base `Jet::ForceEffect` behavior in `InputSystem_7c/Force.cpp`.
- **`Jet::ReferenceCount`**: `vf1` `100020b0` (36) present (shared refcount infrastructure, consistent with InputSystem/Kernel patterns).
- **Helpers/unnamed (`FUN_*`)**: numerous globals (e.g. `100016d0`, `10002660`, `10004380`, ctor/dtor/helper-like small functions around device/effect classes). Some likely implement DirectInput callback/setup/format translation/shared logic not exported by name.

### 2.4 Notable absences / boundaries
- No evidence in repo of a separate source skeleton for `InputDirectX_7c` under `src/jet/input/` yet (only `InputSystem_7c`, `InputWin32_7c` exist).
- No `notes/InputDirectX_7c.md` module note found yet (this report is the first agent note for it).
- Keyboard/mouse DirectX classes exist in binary, but runtime role is constrained by PORT_PLAN ("keyboard and mouse come from InputWin32"); coexistence/selection behavior is an implementation requirement to confirm against original.

## 3. Dependencies and interfaces

### 3.1 External/system dependencies (evidenced)
- **DirectInput 8:** `DINPUT8.DLL!DirectInput8Create` imported (`100060a8`). Implies COM-style `IDirectInput8`, device interfaces (`IDirectInputDevice8`), and likely `IDirectInputEffect` for force feedback; exact GUIDs/structs/flags must be taken from original usage.
- **Win32 platform APIs (expected for DI integration):** HWND/cooperative level/window focus handling, timing, possibly thread/message considerations; confirm from decomp.
- **CRT/runtime:** MSVCR80-era imports/helpers visible in map (matches existing 32-bit/original ABI/toolchain context).
- **Kernel/Jet runtime services:** allocation/refcounting/strings/containers/profile/log/error conventions as used by InputSystem/InputWin32 patterns; `Jet::Alloc`, `Jet::ReferenceCount`, `PString`/`UId`/`Array`/lists implied by neighboring modules.

### 3.2 Jet/InputSystem interfaces this module must satisfy (from `src/jet/input/InputSystem_7c/*`)
- **Driver lifecycle/contract (`InputDriver` in `Devices.cpp`, used by `Input.cpp`):** `Init(OS*)`, `Update(float)`, `Enumerate(InputDeviceCapsList&, int)`, `CreateDevice(const UId&) -> AutoReference<InputDevice>`, `CriticalShutdown()`, `CriticalStartup()`. `Input::LoadDriver` behavior (important): for each type it looks up plugin type in group/key by name; `"Default"` maps to `"Win32"` for keyboard/mouse and `"DirectX"` for joystick, then instantiates and `Init`s; on failure it may continue/stop in specific ways and stores driver/type-driver refs. **Driver registration name and plugin group/type identity must match original** or joystick loading/selection breaks.
- **Device contract (`InputDevice` and derived `KeyboardDevice`/`MouseDevice`/`JoystickDevice`):** state blocks (`InputData` keyboard/mouse/joystick), caps/id/type, `GetProperty/SetProperty`, `Update`, `Done`, `CriticalShutdown/Startup`, event emission via `NotifyObservers`/`KeyEvent`/`ButtonEvent`/`AxisEvent`/`PovEvent`/`PropertyChangedEvent` etc., handle/validity semantics.
- **Force feedback contract (`Force.cpp`, base `ForceEffect`):** base class already implements composite behavior (attach/detach child effects, aggregate `IsValid/IsPlaying/Start/Stop/Update/Load/Unload`, `Create(name, db, flags)` loading `ForceResource` and creating first effect on self then additional effects via `device->CreateEffect` and attaching). A DirectX effect implementation must correctly override the device-backed operations the base expects (device-bound create/start/stop/update/load/unload/playing/validity as per original), and interact with `InputDevice::{CreateEffect,EnumerateEffects,StopAllEffects,ResetEffects,PauseEffects,MuteEffects}`.
- **Force data/resource model (`ForceInfo`, `ForceResource`, `.ffe`):** `Force.cpp` shows GUID/type mapping for Constant/Ramp/Periodic/Condition effects, `ParamSize` by type, and `.ffe` resource parsing/appending flow in progress (file excerpt truncated in read, but structure is clear). DirectX module must translate `ForceInfo`/resource semantics into `DIEFFECT`/type-specific parameter structs and `IDirectInputEffect` calls compatibly.
- **Reference counting/lifetime:** pervasive `AddReference`/`ReleaseReference`/`AutoReference` usage in InputSystem; DirectX COM object lifetimes must be coordinated with Jet refcounts without leaks/double-release/UAF, especially across shutdown/device-lost paths.

### 3.3 Interaction with `InputWin32_7c` (behavioral dependency)
- Both are driver plugins; selection is mediated by `Input` options/plugin lookup (`optionKeys` `InputKeyboard/InputMouse/InputJoystick`, default name mapping in `LoadDriver`). PORT_PLAN states keyboard/mouse come from Win32 and DirectX is for joysticks/wheels/FF — but implementation must preserve **actual** original enumeration/override behavior, including any fallback or multi-driver enumeration quirks, because games/configs/add-ons may observe it.

## 4. Proposed implementation units and order (for future implementation)

Goal: minimize unknowns first, keep each unit differentially testable against original where feasible, and land force feedback only after device/driver plumbing is stable.

1. **U0 — Skeleton & plugin plumbing**
   - Add module directory/build wiring (follow existing `src/jet/input/*` and module conventions) *when implementing*.
   - Implement/export `JetGetPluginsInputDirectX` and `PluginFactoryOf<InputDriverDirectX,InputDriver>` with correct plugin type/group/name metadata.
   - Verify registration identity matches what `Input::LoadDriver` / plugin lookup expects (esp. joystick "DirectX").

2. **U1 — `InputDriverDirectX` lifecycle**
   - Reconstruct `Init(OS*)` (`DirectInput8Create`, global/state setup, window/HINSTANCE acquisition as original), `Update`, `Enumerate` (device caps population for relevant types/masks), `CreateDevice`, `CriticalShutdown/Startup`, teardown/refcounting.
   - Map vfuncs `vf0..vf5` to the `InputDriver` ABI/order used by InputSystem.
   - Acceptance focus: driver can be loaded/initialized and enumerate devices without disturbing Win32 keyboard/mouse path.

3. **U2 — Shared DirectX device base/helpers (`InputDeviceDirectX` + common `FUN_*`)**
   - Recover common COM device acquisition, data format/setup, buffering/poll/acquire, property handling, `Done`/shutdown/lost-device behavior, and observer/event dispatch integration with base `InputDevice`.
   - Untangle shared vtable thunks (`vtnames` shows heavy slot aliasing) so derived class behavior is not conflated.

4. **U3 — `DirectXJoystick` (core input, no FF yet or minimal)**
   - Highest product value per PORT_PLAN. Implement caps, axis/button/pov state mapping into `InputData::joystick` and `JoystickDevice` semantics, polling/update, device instance IDs/names, ranges/properties.
   - Prioritize largest/central methods first (`vf14`, `vf18`, `vf5`, `vf6`, property/effect-related slots later as needed).

5. **U4 — `DirectXKeyboard` / `DirectXMouse` compatibility layer**
   - Implement only to the fidelity required by original behavior/coexistence (may be used, enumerated, or selected in some configurations even if Win32 is default). Keep behavior identical; avoid introducing a competing default path.

6. **U5 — Force feedback: `ForceEffectDirectX` + joystick FF hooks**
   - Implement effect creation from `ForceInfo` and from resource-backed flows in cooperation with base `ForceEffect::Create(name,db,flags)` / `ForceResource`.
   - Reconstruct DI effect parameter translation for all supported types (Constant/Ramp/Periodic/Condition as evidenced in `Force.cpp` tables), download/start/stop/update/unload, gain/pause/mute/reset/stop-all, validity on device loss.
   - Map `ForceEffectDirectX` vfunc slots to base `ForceEffect` vtable layout; ensure child/aggregate semantics remain correct.

7. **U6 — Hardening, differential probes, and game validation**
   - Differential comparison where harnessable (caps enumeration, state translation, effect parameter packing) with care around timing/hardware-dependent state.
   - In-game checks per PORT_PLAN intent: menu load, joystick/wheel detection, axes/buttons/POV, and force feedback on supported hardware/configs.

**Ordering rationale:** U0-U2 establish ABI and shared infrastructure; U3 delivers the stated primary feature; U4 avoids hidden regressions; U5 depends on correct device/COM lifetime and `ForceInfo` mapping.

## 5. Risks and x64 / correctness concerns

### 5.1 DirectInput / COM / ABI
- **COM calling convention/object layout:** DI8 methods are stdcall vtable calls; x64 changes calling convention and pointer sizes. Any hand-transcribed vtable layouts, thunk/callback signatures, or struct offsets from 32-bit RE must be revalidated for x64.
- **`DirectInput8Create` and interface versions:** must use the correct x64 import/library and preserve GUIDs/behavior expected by the game logic (including any quirks relied upon).
- **Callback signatures:** enumeration/callback APIs (if used; `FUN_*` likely) have strict typedefs — mismatch is UB/hard crash on x64.

### 5.2 Struct packing, handles, and data model
- **DINPUT structures:** `DIDEVICEINSTANCE`, `DIDEVCAPS`, `DIDEVICEOBJECTDATA`, `DIEFFECT`, `DICONSTANTFORCE`, `DIRAMPFORCE`, `DIPERIODIC`, `DICONDITION`, `DIENVELOPE`, etc. are pointer/size-sensitive and historically packing-sensitive; `sizeof`/alignment assumptions from 32-bit will not carry over.
- **GUID/UId mapping:** `Force.cpp` compares raw 16-byte IDs (`std::memcmp` to DirectInput effect GUIDs) — endianness/layout is fine if treated as bytes, but all conversions must be exact.
- **Force parameter sizes/types:** `ParamSize` in `Force.cpp` (`4,8,0x10,0x30` by effect type) and `ForceInfo` layout (fixed byte blob offsets used by memcpy in create paths) must match the original resource/runtime format; wrong mapping corrupts effects or crashes drivers.
- **HWND/window lifetime:** cooperative level and acquisition depend on focus/window handle from `OS/Window`; 32-bit `HWND` truncation in stored fields is a common x64 hazard.

### 5.3 Device enumeration, acquisition, and runtime behavior
- **Hotplug/loss/focus:** `DIERR_INPUTLOST`, `DIERR_NOTACQUIRED`, foreground/background cooperative behavior, and `CriticalShutdown/Startup` ordering must mirror original to avoid stuck devices or missed input.
- **Polling vs buffered data:** large joystick/keyboard/mouse `vf14/vf18` methods suggest non-trivial acquisition/read paths; correctness depends on matching buffer format, timestamping, and state-delta semantics.
- **Axis ranges/dead zones/POV encoding:** POV is often represented in hundredths of degrees with centered sentinel; axis normalization must match `InputData`/existing Win32/InputSystem expectations.
- **Coexistence with Win32 driver:** duplicate enumeration or different device IDs/caps can change action mappings or UI listings; preserve original precedence/selection.

### 5.4 Force feedback specifics
- **Effect lifecycle:** base `ForceEffect` aggregates children and assumes specific success/failure propagation (`Start/Stop/Update/Load/Unload` all-children semantics). Device-bound effects must not desync Jet state from DI download/play state.
- **Supported effect subset:** only types represented/mapped in `Force.cpp` are clearly evidenced; unknown/custom types may need graceful rejection identical to original.
- **Hardware variability:** FF behavior is device/driver-dependent, making full differential automation difficult; plan for targeted structural tests plus manual hardware validation.

### 5.5 Porting/testing risks
- **No existing harness in repo for DirectX input:** unlike Win32/InputSystem units with `diff_*` history noted in MODLOG/InputSystem notes, creating reliable DI/FF tests may require mocks, captured parameter logs, or hardware-in-the-loop checks.
- **Map ambiguity:** several vtable slots are thunks/shared (`vf4/vf14/vf18` aliasing in vtnames, multiple `vf0/vf1` entries) — easy to misattribute behavior without tracing callsites/vtables in the original binary.
- **Menu-load vs in-session behavior:** module is "loaded at the menu" (PORT_PLAN); regressions may appear later (device reacquire on focus/session transitions, FF during driving).

## 6. Cross-agent notes (InputSystem / InputWin32 / others)

### For `InputSystem_7c` owners/consumers
- **Do not change InputSystem ABI/semantics casually:** `InputDirectX_7c` depends on stable `InputDriver`, `InputDevice` (+derived), `ForceEffect/ForceInfo/ForceResource`, observer/event, and plugin-loading behavior already implemented and tested in InputSystem. Any vtable/order/layout/behavior change there will break or invalidate this driver.
- **Key integration points to keep explicit:** `Input::LoadDriver` default-name mapping (`"DirectX"` for joystick), `GetDevice` emu-vs-real device flow, `CreateEffect(const ForceInfo&, flags)` / `CreateEffect(name,db,flags)` device hooks, and `ForceEffect` composite semantics.
- **`Force.cpp` truncation in this read:** `src/jet/input/InputSystem_7c/Force.cpp` was partially viewed (resource append/parse logic continues past shown range). Future implementers of DirectX FF should read the full file to align on exact `.ffe`/`ForceInfo` population rules.

### For `InputWin32_7c` owners
- **Coexistence is intentional:** Win32 remains the keyboard/mouse path per PORT_PLAN while DirectX targets joysticks/wheels/FF. Avoid changes that assume exclusive ownership of all device types without checking original `Input` driver selection/enumeration behavior.
- **Device identity/caps consistency:** instance IDs, names, caps masks, and property behavior should be coherent across drivers because action mapping/UI/config can key off them.
- **Reference patterns:** `InputWin32_7c/InputWin32.cpp` is the closest existing concrete driver implementation in-repo (lifecycle, hooks/polling, caps, properties, shutdown/startup) — useful style/contract reference, but not a behavioral substitute for DirectInput semantics.

### For graphics/render or broader integration agents
- No direct render dependency evidenced, but input device loss/reacquire and critical shutdown/startup can correlate with window/focus/device transitions managed elsewhere; coordinate if display/window ownership changes in x64 work.

### Coordination/status
- Per `COORDINATION.md` (2026-11-19): RenderSystem work is in-flight/owned in tree; WaterSystem has known pre-existing build issue — unrelated, but avoid touching shared files/builds. This analysis produced only this report file as requested.

## 7. Recommended next concrete actions (when implementation is scheduled)

1. Read the **full** `src/jet/input/InputSystem_7c/Force.cpp` and any force-related headers/struct definitions in `src/jet/include/jet/...` (not fully enumerated in this read-only pass) to lock `ForceInfo`/`ForceResource`/`ForceEffect` ABI.
2. Recover **plugin group/name strings and vtable layouts** for `InputDriverDirectX`, device classes, and `ForceEffectDirectX` from original binary/RE artifacts (Ghidra project, RTTI/vftables, callsites from `InputSystem`).
3. Confirm **`Input::LoadDriver` + options behavior** against original for keyboard/mouse/joystick selection to define the exact coexistence contract with `InputWin32_7c`.
4. Implement in unit order U0→U5 with small differential checks per unit where possible; defer hardware-dependent FF signoff to dedicated game/hardware validation.
5. Update `notes/agents/STATUS.md` only as part of coordinated workflow when making shared-status edits (this analysis task did not modify it). 

## Summary
- `InputDirectX_7c` is a small but ABI-sensitive DirectInput8 driver plugin: 1 export, ~122 functions, driver/device/FF classes evidenced by function/vtable maps.
- Main implementation challenge is **correct DirectInput/COM + force-feedback translation** on top of already-specified InputSystem contracts, with careful coexistence with `InputWin32_7c`.
- Highest-value path: plugin/factory + driver init/enumeration + joystick device, then FF; keep vtable/layout/ID semantics identical to original, especially for x64 pointer/packing/calling-convention hazards.

## Scaffold (Wave 1)

- **Date:** 2026-11-19
- **Scope/constraints:** module-owned scaffold only under `src/jet/input/InputDirectX_7c/` plus this report update and STATUS append; no shared/contended files edited, no build/test/game actions.

### Created files
- `src/jet/input/InputDirectX_7c/implemented.txt`
  - Marks `JetGetPluginsInputDirectX` as **stub/scaffold only** (no DirectInput behavior, no registration wired pending ABI/ID verification).
- `src/jet/input/InputDirectX_7c/InputDirectX.h`
  - Declares `extern "C" void __cdecl JetGetPluginsInputDirectX();`
  - Declares scaffold `class InputDriverDirectX : public InputDriver` with overrides matching `InputDriver` contract from `jet/InputSystem.h`: `Init(OS*)`, `Enumerate(InputDeviceCapsList&, int)`, `CreateDevice(const UId&)`, protected `Update(float)`, `CriticalShutdown()`, `CriticalStartup()`.
  - Stores only `OS* os_` placeholder member (no DI/COM state yet).
  - Documents placeholder/deferred class targets evidenced by function/vtable maps (no ABI/layout claims, mostly commented): `InputDeviceDirectX` base/helpers, `DirectXKeyboard`, `DirectXMouse`, `DirectXJoystick`, `ForceEffectDirectX`.
- `src/jet/input/InputDirectX_7c/InputDirectX.cpp`
  - Implements minimal safe stubs: ctor/dtor, `Init` stores `OS*` and returns `false` (driver unavailable), `Update`/`Enumerate`/`CriticalShutdown`/`CriticalStartup` no-op, `CreateDevice` returns empty `AutoReference<InputDevice>`.
  - Exports `extern "C" void __cdecl JetGetPluginsInputDirectX()` as a **no-op stub** with TODOs (does not register a factory with guessed IDs).

### Stubbed now vs deferred
- **Stubbed (scaffold/ABI-facing shape only):**
  - Sole export symbol present: `JetGetPluginsInputDirectX` (correct C name/calling convention `__cdecl` consistent with other modules, e.g. `JetGetPluginsInputWin32`/`JetGetPluginsInputSystem`), but intentionally performs no registration until factory/type IDs are verified.
  - `InputDriverDirectX` skeleton satisfying current `Jet::InputDriver` virtual interface signatures from headers; returns safe failure/empty results.
- **Deferred (requires RE/contract verification before implementation):**
  - DirectInput8/COM initialization and teardown (`DirectInput8Create` import evidenced at `100060a8`), HWND/HINSTANCE/cooperative-level acquisition, focus/acquire/lost handling.
  - Device enumeration, caps/IDs/names/properties, instance creation, and per-device input polling/state translation for `DirectXJoystick` (primary), `DirectXKeyboard`, `DirectXMouse` (compatibility/coexistence).
  - Shared `InputDeviceDirectX` helpers and vtable-slot mapping (vtnames show aliased/thunked slots; must untangle per derived class).
  - Force feedback: `ForceEffectDirectX` and device FF hooks, `ForceInfo`/resource/`DIEFFECT` translation, start/stop/update/load/unload/pause/mute/reset/validity semantics aligned with `InputSystem_7c/Force.cpp`.
  - Exact plugin factory/type metadata (factory UId, name string identity confirmed, registration side effects) and any internal globals/helpers (`FUN_*`).

### ABI / contract assumptions and TODOs
- **Plugin/registration:** `Input::LoadDriver` maps default joystick driver name to `"DirectX"` and looks up factories in plugin group `UId("F3117B18-F364-41a1-8993-966236A35B5E")` (`src/jet/input/InputSystem_7c/Input.cpp`). Expected registration pattern is `new PluginFactoryOf<InputDriverDirectX, InputDriver>(PString("DirectX"), UId(<factory-id-TODO>), UId("F3117B18-F364-41a1-8993-966236A35B5E"))`, but **factory UId and any additional table/side effects are unknown and must be recovered from the original** (no guessed IDs registered).
- **DirectInput version/interfaces:** function map evidences `DINPUT8.DLL!DirectInput8Create` import; assume DirectInput 8 / `IDirectInput8` and `IDirectInputDevice8` family plus `IDirectInputEffect` for FF, but exact GUIDs, structs, flags, data formats, buffering/polling, cooperative level, and callback signatures must be taken from decomp/original usage.
- **Coexistence with InputWin32:** PORT_PLAN states keyboard/mouse come from `InputWin32` while DirectX targets joysticks/wheels/FF; however DX keyboard/mouse classes exist in the binary. Preserve original enumeration/selection/override/fallback behavior (device IDs/caps/type masks) to avoid breaking action maps/UI/configs — do not assume defaults beyond original evidence.
- **Force feedback contracts:** must satisfy `InputDevice` FF hooks (`CreateEffect` overloads, `EnumerateEffects`, `StopAllEffects`, `ResetEffects`, `PauseEffects`, `MuteEffects`) and `ForceEffect` composite semantics in InputSystem; `ForceInfo` GUID/type/parameter mapping (Constant/Ramp/Periodic/Condition etc.) must be byte/behavior-exact per `.ffe`/resource flow.
- **x64 considerations:** COM/stdcall vtable calling, pointer/handle sizes (`HWND`, COM interface pointers), `DINPUT`/DI effect struct sizes/alignment/packing, enumeration/callback typedefs, and any 32-bit-assumed storage/thunk behavior are ABI-sensitive; cannot be inferred reliably from 32-bit RE alone. Refcount/lifetime coordination between Jet `ReferenceCount`/`AutoReference` and COM `AddRef/Release` must be verified (esp. device-lost, critical shutdown/startup, teardown order).
- **Interface vs original ABI:** current `InputDriver` virtuals in headers match InputSystem usage, but original vtable slot order/layout/names for `InputDriverDirectX`, device classes, and `ForceEffectDirectX` are not yet confirmed from binary; stubs prioritize compile-shape placeholders, not binary-compatible overrides beyond known driver contract.

### How to wire into build later (deferred; shared files not edited now)
- Add a module-owned `CMakeLists.txt` under `src/jet/input/InputDirectX_7c/` or equivalent target definition consistent with other Jet input plugin modules, producing the `InputDirectX_7c` binary/artifact expected by the game/plugin loader.
- Register the target in the shared build graph (e.g. parent `src/jet/input/CMakeLists.txt` and/or top-level Jet module wiring) **under an explicit claim/coordination** once WaterSystem/build state and in-flight work allow shared-file edits.
- Link/compile dependencies as required by evidence: Jet/InputSystem headers+runtime (`InputDriver`, devices, `ForceEffect/ForceInfo`, `Plugin`/`ReferenceCount`/`Alloc`), Windows, and DirectInput 8 (`dinput8`, and `dxguid` if needed) — confirm exact libs/defines for the x64 port.
- Once factory/type IDs and behavior are verified, replace `JetGetPluginsInputDirectX` no-op with real registration and implement units U1→U5 incrementally; update `implemented.txt`, module docs, and progress exports only when those shared bookkeeping files are in-scope/coordinated.
