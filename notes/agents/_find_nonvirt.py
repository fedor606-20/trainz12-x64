import csv, re
impl=set()
for l in open(r'src/jet/graphics/RenderSystem_7c/implemented.txt',encoding='ascii',errors='ignore'):
    l=l.strip()
    if not l or l.startswith('#'): continue
    impl.add(l.split('=',1)[0])
rows=[]
with open(r'D:\trainz12-decomp\exports\RenderSystem_7c.tsv',encoding='utf-8') as f:
    for row in csv.DictReader(f,delimiter='\t'):
        if row['mangled'] not in impl:
            rows.append(row)
# prefer public non-virtual member funcs? demangled starts with 'public:' and not 'public: virtual'
c=[]
for r in rows:
    d=r['demangled']
    if d.startswith('public: virtual'): continue
    if d.startswith('protected: virtual'): continue
    if d.startswith('private: virtual'): continue
    # classes of interest
    if any(x in d for x in ['Jet::Layer::','Jet::LayerGroup::','Jet::LayerManager::','Jet::SceneGraph::','Jet::NonRenderContext::','Jet::Overlay::','Jet::DisplaySaveBuffer::','Jet::SystemResourceViewer::','Jet::RenderKernel::']):
        c.append(r)
    elif 'Jet::Display::' in d and ('GetAllVideo' in d or 'GetExtensionString' in d or 'ConvertTexCompressionString' in d or 'CheckResetDynamicBufferOffsets' in d or 'ResetDynamicBufferOffsets' in d or 'GetMax' in d or 'GetCaps' in d or 'Is' in d or 'Get' in d or 'Set' in d):
        c.append(r)
print(len(c))
for r in c:
    print(r['rva'], r['mangled'], '||', r['demangled'])
