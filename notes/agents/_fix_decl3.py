p=r'src/jet/include/jet/RenderSystem.h'
s=open(p,encoding='utf-8').read()
reps=[
('    bool GetTextureMode() const;','    virtual bool GetTextureMode() const;'),
('    bool GetGlobalTextureMode() const;','    virtual bool GetGlobalTextureMode() const;'),
('    int GetNumStencilBits() const;','    virtual int GetNumStencilBits() const;'),
('    TextureCombinerManager* GetTextureCombinerManager() const;','    virtual TextureCombinerManager* GetTextureCombinerManager() const;'),
('    VertexProgramManager* GetVertexProgramManager() const;','    virtual VertexProgramManager* GetVertexProgramManager() const;'),
('    bool CheckPlatform(OS* os);','    virtual bool CheckPlatform(OS* os);'),
('    bool GetZBiasEnabled();','    virtual bool GetZBiasEnabled();'),
('    bool IsBetweenBeginAndEndFrame();','    virtual bool IsBetweenBeginAndEndFrame();'),
('    bool BindToThread();','    virtual bool BindToThread();'),
('    bool UnbindFromThread();','    virtual bool UnbindFromThread();'),
('    bool Initialize(Display* display);','    virtual bool Initialize(Display* display);'),
('    RenderKernel* GetRenderKernel();','    virtual RenderKernel* GetRenderKernel();'),
]
for a,b in reps:
    if a not in s: print('MISS',a)
    s=s.replace(a,b)
open(p,'w',encoding='utf-8',newline='\n').write(s)
print('ok')
