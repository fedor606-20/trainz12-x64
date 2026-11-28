p='MODLOG.md'
b=open(p,'rb').read()
old=b'''- `diff_ws_u1`: FrameAnimation 0 mismatches; WaterGrid/noise math 0 mismatches after cross-copying
  the PNoise tables. The only allowed difference is the table bytes themselves: the original builds
  them from msvcr80's `rand()`, our module from the UCRT's independent state.'''
new=b'''- `diff_ws_u1`: 140,001 checks, 0 mismatches. The PNoise ctor tables are byte-identical (same LCG,
  seed 1); Noise2/3 and the turbulence variants now follow the original's SSE/x87 split and the
  unrounded-x87 returns. WaterGrid element stores use x87 FLD/FSTP so signaling NaNs quiet the same way.'''
if old not in b: raise SystemExit('modlog anchor missing')
b=b.replace(old,new)
open(p,'wb').write(b)
print('ok')
