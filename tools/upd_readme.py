p='README.md'
s=open(p,encoding='utf-8').read()
import re
s=re.sub(r'\| WaterSystem_7c \| 92 KB \| 714 \| 483/490 \| [^|]+\|','| WaterSystem_7c | 92 KB | 714 | 490/490 | ✅ done |',s)
open(p,'w',encoding='utf-8').write(s)
print('README updated')
