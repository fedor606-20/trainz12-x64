for p in ['notes/WaterSystem.md','MODLOG.md']:
    s=open(p,encoding='utf-8').read().replace('211,968','212,480')
    open(p,'w',encoding='utf-8').write(s)
print('ok')
