p='src/jet/CMakeLists.txt'
s=open(p,encoding='utf-8').read()
s=s.replace('u11 tails tails2)','u11 tails tails2 templates)')
open(p,'w',encoding='utf-8').write(s)
print('ok')
