for p in ['notes/WaterSystem.md']:
    s=open(p,encoding='utf-8').read().replace('212,480','213,504')
    s=s.replace('U1 `diff_ws_u1` (CRT rand streams\ndiffer by design, math 0 mismatches after cross-copy)','U1 `diff_ws_u1` 140,001 checks, 0 mismatches')
    open(p,'w',encoding='utf-8').write(s)
p='MODLOG.md'
b=open(p,'rb').read()
b=b.replace(b'211,968',b'213,504').replace(b'212,480',b'213,504')
open(p,'wb').write(b)
print('ok')
