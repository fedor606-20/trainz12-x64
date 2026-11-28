p='src/jet/CMakeLists.txt'
s=open(p,encoding='utf-8').read()
s=s.replace('livepaths)','livepaths simple_rem)')
open(p,'w',encoding='utf-8').write(s)
print('ok')
