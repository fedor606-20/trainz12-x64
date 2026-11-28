p='src/jet/CMakeLists.txt'
s=open(p,encoding='utf-8').read()
s=s.replace('u5b u5b_rem u6','u5b u5b_rem u5b_final u6')
open(p,'w',encoding='utf-8').write(s)
print('ok')
