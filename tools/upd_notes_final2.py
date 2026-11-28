p = 'notes/WaterSystem.md'
s = open(p, encoding='utf-8').read()
s = s.replace('213,504', '215,552')
s = s.replace('U8a 78 checks', 'U8a 447 checks')
old = '''Known remaining simplifications in U8a (behavioural, not export coverage): `CalculateLerpTexture`
does not bind the original's `DirectX` local constant or the full frame/UV member set;
`SetupVertexProgramConstants` does not reproduce the original's shifted local matrix copy;
`Set/ClearDisplayState` do not save/restore every fog/texture-stage field; the chunk
`InitializeBuffers/LoadBuffers` now allocate through the display/buffer manager but the
per-record fill path is the generic base one. These are listed in `notes/agents/ws_u8a.md`.'''
new = '''U8a final exactness pass: `CalculateLerpTexture` now binds the `DirectX` PString through
ExtendedGfxLanguage slot 0x1c, writes the `texSize` local constant (0x5c), binds the frame pair
on stages 0/1, sets/clears depth and render target, and drives RenderSquare exactly as
0x10006840/0x10015060. `SetupVertexProgramConstants` follows the full camera-basis/flip/view
chain, the shifted local copy at 0x100064b4 and the 0x14/0x2b uploads; the only remaining
difference is one uninitialised stack float the retail build reads one word past `basis*flip`
and uploads as the first 0x2b component (locally zero; the other eight components and all 0x14
floats are bit-exact). `SetDisplayState`/`ClearDisplayState` match 0x10006070/0x10006160 and
touch no fog/blend fields (the original touches only display +0x3c and the material's own
program/texture pointers). Ocean `RenderChunk` gained the missing 0x2f854 frame gate, display
0x54 and Setup call. The chunk buffers allocate/upload through display 0x104/0x108 and buffer
manager 0x20/0x2c; `CreateArray` leaves +8/+c as allocated like the template ctor.
`diff_ws_u8a` is now 447 checks, 0 mismatches.'''
if old not in s:
    raise SystemExit('notes anchor missing')
s = s.replace(old, new)
open(p, 'w', encoding='utf-8').write(s)
print('notes updated')
