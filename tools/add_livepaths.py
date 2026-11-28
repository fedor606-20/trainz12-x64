p='src/jet/CMakeLists.txt'
s=open(p,encoding='utf-8').read()
s=s.replace('tails tails2 templates)','tails tails2 templates livepaths)')
open(p,'w',encoding='utf-8').write(s)
print('ok')
