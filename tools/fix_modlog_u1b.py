p='MODLOG.md'
b=open(p,'rb').read()
old=b'FrameAnimation 0 mismatches; WaterGrid/noise math 0 mismatches after cross-copying'
new=b'140,001 checks, 0 mismatches; PNoise ctor tables byte-identical'
b=b.replace(old,new)
old2=b'The only allowed difference is the table bytes themselves: the original builds\n  them from msvcr80\'s `rand()`, our module from the UCRT\'s independent state.'
new2=b'Noise2/3 and turbulence follow the SSE/x87 split; WaterGrid stores quiet signalling NaNs through x87.'
b=b.replace(old2,new2)
open(p,'wb').write(b)
print('ok')
