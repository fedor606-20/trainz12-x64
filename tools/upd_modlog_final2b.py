p='MODLOG.md'
b=open(p,'rb').read()
old=b'''Still open (behavioural polish, not export coverage): `CalculateLerpTexture` lacks the original's\r
`DirectX` local constant/full UV state, `SetupVertexProgramConstants` lacks the shifted matrix copy,\r
and `Set/ClearDisplayState` do not save every fog/stage field. Details in `notes/agents/ws_u8a.md`.'''
new=b'''U8a final exactness pass: `CalculateLerpTexture` (0x10006840/0x10015060), the full\r
`SetupVertexProgramConstants` chain (including the shifted 0x100064b4 copy) and\r
`Set/ClearDisplayState` now follow retail; Ocean `RenderChunk` gained its missing frame gate,\r
display 0x54 and Setup call. `diff_ws_u8a` is 447 checks, 0 mismatches. The only remaining\r
difference is one uninitialised stack float the retail build uploads as the first 0x2b\r
component (locally zero; the other eight components and all 0x14 floats are bit-exact).'''
if old not in b:
    old=old.replace(b'\r\n',b'\n')
    new=new.replace(b'\r\n',b'\n')
    if old not in b: raise SystemExit('anchor still missing')
    b=b.replace(old,new)
else:
    b=b.replace(old,new)
open(p,'wb').write(b)
print('ok')
