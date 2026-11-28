p='src/jet/graphics/WaterSystem_7c/PNoise.cpp'
s=open(p,encoding='utf-8').read()
s=s.replace('Perm(static_cast<unsigned char>(cell + 1))','Perm(static_cast<unsigned char>(cell + 1) + 1)')
open(p,'w',encoding='utf-8').write(s)
print('ok')
