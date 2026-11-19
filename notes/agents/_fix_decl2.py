p=r'src/jet/include/jet/RenderSystem.h'
s=open(p,encoding='utf-8').read()
reps=[
('    virtual bool GetTextureMode() const;','    bool GetTextureMode() const;'),
('    virtual bool GetGlobalTextureMode() const;','    bool GetGlobalTextureMode() const;'),
('    virtual int GetNumStencilBits() const;','    int GetNumStencilBits() const;'),
('    virtual TextureCombinerManager* GetTextureCombinerManager() const;','    TextureCombinerManager* GetTextureCombinerManager() const;'),
('    virtual VertexProgramManager* GetVertexProgramManager() const;','    VertexProgramManager* GetVertexProgramManager() const;'),
('    virtual bool CheckPlatform(OS* os);','    bool CheckPlatform(OS* os);'),
('    virtual bool GetZBiasEnabled();','    bool GetZBiasEnabled();'),
('    virtual bool IsBetweenBeginAndEndFrame();','    bool IsBetweenBeginAndEndFrame();'),
('    virtual bool BindToThread();','    bool BindToThread();'),
('    virtual bool UnbindFromThread();','    bool UnbindFromThread();'),
('    virtual bool Initialize(Display* display);','    bool Initialize(Display* display);'),
('    virtual RenderKernel* GetRenderKernel();','    RenderKernel* GetRenderKernel();'),
]
for a,b in reps:
    if a not in s: print('MISS',a)
    s=s.replace(a,b)
open(p,'w',encoding='utf-8',newline='\n').write(s)
print('ok')
