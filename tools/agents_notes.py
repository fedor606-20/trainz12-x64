p='notes/agents/ws_u8a.md'
s=open(p,encoding='utf-8').read()
s += '''

## Follow-up 2026-11-24 (final integration)

- The chunk `InitializeBuffers`/`LoadBuffers` stubs were replaced with the original's display-slot
  0x104/0x108 allocation and buffer-manager 0x20/0x2c upload path; `AssignVertexType` now fetches
  the VertexType through Display slot 0x10c. This closes the largest rendering gap listed above.
- `WaterMaterialStatics.cpp` no longer loads or mirrors `WaterSystem_7c_orig.dll`: the four factory
  tables are built from the imported `PluginType::AddReference/ReleaseReference` and
  `MaterialPlugin::CreateInstance` addresses. RTTI locators are absent by design.
- The `WaterAnimation`/`WaterAnimationPN` virtual order was corrected to `Initialize, GetFrame,
  GetXYFrame`; U6's slot-1 `GetFrame` call and U10's factory path now agree.
- Final module: 490/490 exports local, 0 forwards. Clean build `build32-final`.
'''
open(p,'w',encoding='utf-8').write(s)
p='notes/agents/ws_u10.md'
s=open(p,encoding='utf-8').read()
s += '''

## Final integration note (2026-11-24)

The inherited runtime mirror dependency is gone: `WaterMaterialStatics.cpp` now builds the four
factory vtables locally from the imported base methods, so `WsEnsureMirrors()` never loads
`WaterSystem_7c_orig.dll`. The whole module is 490/490 local.
'''
open(p,'w',encoding='utf-8').write(s)
print('ok')
