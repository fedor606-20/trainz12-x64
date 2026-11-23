# InputDirectX_7c reconstruction notes (private)

DirectInput 8 driver plugin. One export: `JetGetPluginsInputDirectX` (0x10001ec0 -> 0x10001de0). All code is in
`src/jet/input/InputDirectX_7c/InputDirectX.cpp`; the private dependency is InputSystem_7c (InputDevice, InputDriver,
ForceEffect, ASCIICode, UId) plus KernelStandard/Kernel.

## Classes and layouts
| Class | Size | What |
|---|---:|---|
| `ForceEffectDirectX : ForceEffect` | 0x9c | effect ptr +0x88, flags +0x8c, time +0x90, duration +0x94, axis mask +0x98 |
| `InputDeviceDirectX : InputDevice` | 0x23c | `IDirectInputDevice8A*` +0x238; new virtual slot 18 `Init(Window*, IDirectInput8A*, const UId&)` |
| `DirectXKeyboard : InputDeviceDirectX` | 0xa40 | lost +0x23c, 256 key records +0x240 (8 bytes: time, state, pad) |
| `DirectXMouse : InputDeviceDirectX` | 0x27c | buffered-axis deltas +0x23c, ranges +0x24c (12-byte stride, min/max/pad), thresholds +0x26c, lost +0x278 |
| `DirectXJoystick : InputDeviceDirectX` | 0x2cc | window +0x23c, thresholds +0x240, reserved +0x260, cached caps +0x280, capability array +0x2b8/+0x2c0, effect list +0x2c4, flags +0x2c8 |
| `InputDriverDirectX : InputDriver` | 0x20 | OS* +0x0c, `IDirectInput8A*` +0x10 |
| `PluginFactoryOf<InputDriverDirectX, InputDriver>` | 0x20 | name `DirectX`, id `D2488477-E537-4542-9ED2-554C93E28F3C`, parent `F3117B18-F364-41a1-8993-966236A35B5E` |

`InputDeviceDirectX` is abstract (GetCaps/Update pure) so its vtable keeps the original `purecall` entries; the three
device classes fill slots 4/5/6/14/18 and joystick also 15/16.

## Driver
- `Init(OS*)`: window must exist and have a handle; `DirectInput8Create(GetModuleHandleA(0), 0x800, IID_IDirectInput8A,
  &di, 0)`.
- `Enumerate(list, type)`: `EnumDevices` with the legacy filter values 3 keyboard, 2 mouse, 4 joystick (the original
  passes those, not `DI8DEVTYPE_*`). The callback creates each device only to read its capabilities, maps the
  instance GUID to `UIdKeyboard`/`UIdMouse` or copies it, names the caps from `DIDEVICEINSTANCEA`, and adds the caps.
  The original fills the EnumObjects axis mask into the caps' `numPovs` word, then overwrites it; that is why
  `axisMask` in enumeration is 0 (kept).
- `CreateDevice(id)`: keyboard / mouse / everything-else joystick; `new`, `Init`, on failure release. Success is an
  `AutoReference` with the one reference the caller owns.

## Keyboard
- Init: `GUID_SysKeyboard`, `c_dfDIKeyboard`, cooperative level 6 (`DISCL_FOREGROUND|DISCL_NONEXCLUSIVE`), buffer 0x100,
  Acquire (ignored).
- Update: Acquire, GetDeviceState(0x100), GetDeviceData(0x100); scan code -> Jet key through the 256-byte table built by
  the module's static initializer at 0x100053c0 (0 = none); lock keys from `GetKeyState`; key-down / key-up / repeat
  (400 ms first, 80 ms after) events with modifier bits 24..29; `CharEvent` through `ASCIICode::GetChar` for down/repeat.
- Caps: `0xDC000101` (plus `0x3000000` when a device answered GetDeviceInfo), 0x100 buttons. Lock properties use
  Jet keys 0x3b/0x46/0x47 -> VK_CAPITAL/VK_NUMLOCK/VK_SCROLL, with synthesized `SendInput` on SetProperty.

## Mouse
- Init: `GUID_SysMouse`, `c_dfDIMouse2`, cooperative level 5 (`DISCL_FOREGROUND|DISCL_EXCLUSIVE`), buffer 0x200,
  axis mode relative (DIPROP_AXISMODE=1), Acquire.
- Update: GetDeviceState(0x14), per-axis delta threshold/range clamp, GetDeviceData; axis events, button events from
  DIMOFS_BUTTON0..7; lost/get events like the keyboard.
- Caps: base 0x3800, device names add 0x3000000, `DIDEVCAPS` flags/buttons/axes, default 3 buttons and 2 axes.
- The buffered-axis delta words at +0x23c are written during events and never reset by Update (kept).

## Joystick
- Init: CreateDevice with the instance GUID (the UId from enumeration), caches caps (type 4, name/description
  `Unknown` then `DIDEVICEINSTANCEA`, caps 0x1000F800, flags from `DIDEVCAPS`, button/POV masks), `c_dfDIJoystick`,
  cooperative 5, buffer 0x200, axis mode absolute, ranges -32767..32767 on all eight axes, optional force feedback
  (`EnumEffects` capability list, AUTOCENTER 9, Acquire, window hook 0x10004230).
- Update: Acquire/Poll, prev state, `DIJOYSTATE` 0x50, buttons/pressure, axis threshold from the cached caps,
  POVs, buffered events (axis offsets <0x20, POV 0x20..0x2c, buttons 0x30..0x4f), then the effect list.
- Effects: `CreateEffect` Acquires and allocates a 0x9c `ForceEffectDirectX`, stores the capability mask from the
  EnumEffects list in `ForceInfo.unknown00`, adds one list reference; an effect whose reference count is 1 is dropped
  by Update. The trigger-duration code tests bit 6 of `info.unknown00` but sets bit 6 of the effect flags (kept).
- Properties: 3 range, 4 threshold (cached), 5 position, 6 dead zone and 7 saturation through DIPROP_DEADZONE /
  DIPROP_SATURATION, 8 autocenter through DIPROP_AUTOCENTER, 10/11/12 flag bits; SetProperty(10) falls through 11 and
  12 in the original (kept).

## ForceEffectDirectX
- Builds the old 0x38-byte `DIEFFECT` plus envelope, 0x30-byte type parameters, two axes/directions and the effect
  GUID at +0x90. Custom force (type 3) allocates a copy of the samples.
- `Create(info)`: copies ForceInfo, ORs `flags & 0x200` into `unknown00`, sets the trigger flag/duration, calls
  `CreateEffect(&guid, &dieffect, &effect, 0)`, frees the custom sample copy.
- `Update`: SetParameters `0x400003df`, retries with `0x3df` on `0x80040208`; duration in seconds; validity/trigger bits.
- `Load` is DirectInput `Download`; `Unload` is DirectInput `Unload`. `IsPlaying` combines the `GetEffectStatus` bit
  with the base children. `Start`/`Stop`/`Update`/`Load`/`Unload` forward to the base after the DirectInput call.

## Verification
- Win32 Release builds; 1/1 export is local, no `InputDirectX_7c_orig` import.
- Game copy: retail `InputDirectX_7c.dll` renamed to `_orig.dll`, ours in place. Main menu reached, clean exit,
  `JetLog` diff against the baseline 0 lines. No joystick/wheel was attached, so `CreateDevice` for a real joystick and
  force feedback are not exercised yet; the Logitech G27 in the device list is not currently connected.
- Static initializer table and all GUIDs/offsets were read from the shipped image (`0x100053c0`, GUIDs at
  0x100092bc..0x1000936c, effect GUIDs at 0x1000929c..).
