p='src/jet/CMakeLists.txt'
s=open(p,encoding='utf-8').read()
s=s.replace('foreach(t u1b u1c u1d1 u1d2 u2 u3 u3b u4 u5a u5a2 u5b u6 u7 u8 u9a u9b u9b2 u10 u11)','foreach(t u1b u1c u1d1 u1d2 u2 u3 u3b u4 u5a u5a2 u5b u6 u7 u8 u9a u9b u9b2 u10 u10_rem u11)')
open(p,'w',encoding='utf-8').write(s)
print('ok')
