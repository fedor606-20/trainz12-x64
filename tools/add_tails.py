p='src/jet/CMakeLists.txt'
s=open(p,encoding='utf-8').read()
s=s.replace('u9b u9b2 u9b_rem u10 u10_rem u11','u9b u9b2 u9b_rem u10 u10_rem u11 tails')
open(p,'w',encoding='utf-8').write(s)
print('ok')
