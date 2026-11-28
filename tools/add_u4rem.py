p='src/jet/CMakeLists.txt'
s=open(p,encoding='utf-8').read()
s=s.replace('u3 u3b u4 u5a','u3 u3b u4 u4_rem u5a')
open(p,'w',encoding='utf-8').write(s)
print('ok')
