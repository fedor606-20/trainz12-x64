import sys
d=open(r'build32/Release/WaterSystem_7c.dll','rb').read()
print('contains orig name:', b'WaterSystem_7c_orig.dll' in d)
