p='MODLOG.md'
b=open(p,'rb').read()
b=b.replace(b'213,504',b'215,552').replace(b'212,480',b'215,552')
b=b.replace(b'U8a 78 checks',b'U8a 447 checks')
b=b.replace(b'212,480 bytes',b'215,552 bytes')
old=b'''Still open (behavioural polish, not export coverage): `CalculateLerpTexture` lacks the original's
`DirectX` local constant/full UV state, `SetupVertexProgramConstants` lacks the shifted matrix copy,
and `Set/ClearDisplayState` do not save every fog/stage field. Details in `notes/agents/ws_u8a.md`.'''
new=b'''U8a final exactness pass: `CalculateLerpTexture` (0x10006840/0x10015060), the full
`SetupVertexProgramConstants` chain (including the shifted 0x100064b4 copy) and
`Set/ClearDisplayState` now follow retail; Ocean `RenderChunk` gained its missing frame gate,
display 0x54 and Setup call. `diff_ws_u8a` is 447 checks, 0 mismatches. The only remaining
difference is one uninitialised stack float the retail build uploads as the first 0x2b
component (locally zero; the other eight components and all 0x14 floats are bit-exact).'''
if old not in b: raise SystemExit('modlog anchor missing')
b=b.replace(old,new)
open(p,'wb').write(b)
print('modlog updated')
