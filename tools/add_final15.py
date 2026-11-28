p='src/jet/CMakeLists.txt'
s=open(p,encoding='utf-8').read()
s=s.replace('livepaths simple_rem)','livepaths simple_rem final15)')
open(p,'w',encoding='utf-8').write(s)
print('ok')
