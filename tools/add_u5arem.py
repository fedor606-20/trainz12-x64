p='src/jet/CMakeLists.txt'
s=open(p,encoding='utf-8').read()
s=s.replace('u4 u4_rem u5a u5a2 u5b','u4 u4_rem u5a u5a2 u5a_rem u5b')
open(p,'w',encoding='utf-8').write(s)
print('ok')
