p = 'notes/WaterSystem.md'
s = open(p, encoding='utf-8').read()
s = s.replace(
    "| U7b | WaterMaterial base and the factories' CreateInstance | 10003530..10003700, 1000D080, 1000D220 | ~13 | \u2b1c |",
    "| U7b | WaterMaterial base and the factories' CreateInstance | 10003530..10003700, 1000D080, 1000D220 | ~13 | \u2705 ours (2026-11-24) |")
s = s.replace(
    "| U8 | WaterBumpMaterial, WaterOceanMaterial, WaterMaterialFresWave, WaterMaterialOneTex | 10005710..10006B20, 1000D290..10011930, 10013040..10013800, 10013FD0..10015410 | ~150 | \u2b1c |",
    "| U8 | WaterBumpMaterial, WaterOceanMaterial, WaterMaterialFresWave, WaterMaterialOneTex | 10005710..10006B20, 1000D290..10011930, 10013040..10013800, 10013FD0..10015410 | ~150 | \u2705 ours (2026-11-24) |")
s = s.replace(
    "| U9 | WaterTile, Water, WaterTiled, WaterSurface | 10003700..10003F20, 10015C10..10015D00, 100171A0..10017810 | ~40 | \u2b1c |",
    "| U9 | WaterTile, Water, WaterTiled, WaterSurface | 10003700..10003F20, 10015C10..10015D00, 100171A0..10017810 | ~40 | \u2705 ours (2026-11-24) |")
s = s.replace(
    "| U10 | WaterManager, WaterManagerGeneric, WaterManagerOceanSim, JetGetPluginsWaterSystem | 100092A0..1000CB90, 100162F0 | ~35 | \u2b1c |",
    "| U10 | WaterManager, WaterManagerGeneric, WaterManagerOceanSim, JetGetPluginsWaterSystem | 100092A0..1000CB90, 100162F0 | ~35 | \u2705 ours (2026-11-24) |")
s += """

## Complete (2026-11-24)

All 490 exports are local. `build32-final\\Release\\WaterSystem_7c.dll` is 211,968 bytes;
`check_exports.py --forbid-dll WaterSystem_7c_orig.dll` reports 490/490 names and ordinals,
no import, delay import or export forwarder to the original, and the string
`WaterSystem_7c_orig.dll` no longer occurs in the DLL.

- **U7b**: `WaterMaterial` lifetime/state and all appended virtuals; local vtables plus a
  slot-rotation fix (`WaterMaterialTables.cpp`) because VS 2026 puts the added
  `CreateGeometryChunk` overload at slot 23 while TS12 has it at 46. The four factory
  vtables are hand-built from the imported `PluginType::AddReference/ReleaseReference` and
  `MaterialPlugin::CreateInstance` addresses; RTTI locators are intentionally absent.
- **U8**: Bump, Ocean, FresWave and OneTex, including their local chunk templates, off-screen
  textures, vertex programs/combiners, cube-map textures and the two remaining factory
  `CreateInstance` bodies. `WaterAnimation` was corrected to the retail virtual order
  `Initialize, GetFrame, GetXYFrame` (U10 reported the rotated table).
- **U9**: `WaterTile`, `Water`, `WaterTiled`, `WaterSurface`; `WaterTiled` derives `Particle`
  (not `Water`) per RTTI and the assembly.
- **U10**: the three managers, their statics, vtable/vbtables and
  `JetGetPluginsWaterSystem` with the retail UIDs and registration order.

Verification: clean Win32 Release build (`build32-final`); U1 `diff_ws_u1` (CRT rand streams
differ by design, math 0 mismatches after cross-copy), U5 4 scenarios, U7 71 scenarios,
U8a 78 checks, U9 35 checks, U10 66 checks - all 0 mismatches. A no-original lab copy loads
the module, calls `JetGetPluginsWaterSystem`, reaches the main menu and renders the Railyard
3D scene; the lab DLL was restored afterwards.

Known remaining simplifications in U8a (behavioural, not export coverage): `CalculateLerpTexture`
does not bind the original's `DirectX` local constant or the full frame/UV member set;
`SetupVertexProgramConstants` does not reproduce the original's shifted local matrix copy;
`Set/ClearDisplayState` do not save/restore every fog/texture-stage field; the chunk
`InitializeBuffers/LoadBuffers` now allocate through the display/buffer manager but the
per-record fill path is the generic base one. These are listed in `notes/agents/ws_u8a.md`.
"""
open(p, 'w', encoding='utf-8').write(s)
print('notes updated')
