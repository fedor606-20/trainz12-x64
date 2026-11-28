p='README.md'
s=open(p,encoding='utf-8').read()
import re
s=re.sub(r'\| Render3DCore_7c \| 458 KB \| 3,168 \| [^|]+ \| [^|]+ \|','| Render3DCore_7c | 458 KB | 3,168 | 1938/2500 | 🟡 partial |',s)
open(p,'w',encoding='utf-8').write(s)
print('readme')
