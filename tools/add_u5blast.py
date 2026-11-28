p='src/jet/CMakeLists.txt'
s=open(p,encoding='utf-8').read()
s=s.replace('u5b_rem u5b_final u6','u5b_rem u5b_final u5b_last u6')
open(p,'w',encoding='utf-8').write(s)
print('ok')
