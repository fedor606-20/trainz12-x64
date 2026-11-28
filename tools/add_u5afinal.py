p='src/jet/CMakeLists.txt'
s=open(p,encoding='utf-8').read()
s=s.replace('u5a_rem u5b','u5a_rem u5a_final u5b')
open(p,'w',encoding='utf-8').write(s)
print('ok')
