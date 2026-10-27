// Unit U3a2 of RenderSystem_7c: VertexProgramResource (0x1003aa60.., 0x10055840..0x10055e90) and
// TextureCombinerResource (0x1003a800.., 0x10041260..0x100413c0), the `.vtx` / `.pxl` resources the program
// managers load. Behaviour follows the TS12 assembly (addresses per function).
#include <jet/GfxLanguage.h>
#include <jet/VCall.h>

#include <new>

namespace Jet {

namespace {

Display* KernelDisplay(const RenderKernel* k) { return *reinterpret_cast<Display* const*>(reinterpret_cast<const char*>(k) + 0x7c); }

// the defines as one string (":name:value"...), "" without any
AnsiString DefinesText(const PreProcessorArray* defines) { return defines ? defines->ConcatenateStrings() : AnsiString(""); }

}  // namespace

// ---------------------------------------------------------------------------------------------------------------
// VertexProgramResource

// 0x1003acc0 (the kernel and the vertex type are left as they are)
VertexProgramResource::VertexProgramResource(const PString& name, const ResourceDatabase* owner)
    : Resource(name, owner), program(nullptr), manager(nullptr), defines(nullptr), error(static_cast<ErrorType>(0))
{
}

// 0x1003aa60
VertexProgramResource::VertexProgramResource(const VertexProgramResource& o)
    : Resource(o), instances(o.instances), program(o.program), fileName(o.fileName), kernel(o.kernel),
      vertexType(o.vertexType), manager(o.manager), programName(o.programName), defines(o.defines), error(o.error)
{
}

// 0x1003ab70
VertexProgramResource& VertexProgramResource::operator=(const VertexProgramResource& o)
{
    Resource::operator=(o);
    instances = o.instances;
    program = o.program;
    fileName = o.fileName;
    kernel = o.kernel;
    vertexType = o.vertexType;
    manager = o.manager;
    programName = o.programName;
    defines = o.defines;
    error = o.error;
    return *this;
}

// 0x10055840: the programs, then the defines' copy; the names and the instances go with the members
VertexProgramResource::~VertexProgramResource()
{
    for (unsigned int i = 0; i < instances.count; ++i) instances.data[i].program->ReleaseReference();
    if (defines) {
        defines->RemoveAll();
        if (defines->data) Alloc::Delete(defines->data);
        Alloc::Delete(defines);
    }
    defines = nullptr;
}

// 0x10055c20: the program's name and a copy of the defines (when given) for the next load
void __cdecl VertexProgramResource::SetParameters(RenderKernel* k, VertexType* type, const PString& name,
                                                  VertexProgramManager* m, const PreProcessorArray* d) const
{
    kernel = k;
    vertexType = type;
    programName = name;
    manager = m;
    if (d) {
        auto* const copy = static_cast<Array<PreProcessorDefine, 3>*>(Alloc::New(sizeof(Array<PreProcessorDefine, 3>)));
        if (copy) {
            copy->count = 0;
            copy->capacity = 0;
            copy->data = nullptr;
            *copy = *d;
        }
        defines = copy;
    }
}

VertexProgramResource::ErrorType __cdecl VertexProgramResource::GetErrorType() const { return error; }
void __cdecl VertexProgramResource::SetErrorType(ErrorType e) const { error = e; }

// 0x10055ce0: a program from the display, loaded from the file with the current name and defines
VertexProgramResource::Result __cdecl VertexProgramResource::LoadTxt(const char* name)
{
    if (fileName.node) PString::Cache()->Destroy(fileName.node);
    fileName.node = PString::Cache()->Create(reinterpret_cast<const unsigned char*>(name));
    error = static_cast<ErrorType>(0);
    VertexProgram* const p = VCall<VertexProgram*>(KernelDisplay(kernel), 0x144, vertexType);
    p->SetProgramManager(manager);
    bool ok;
    {
        const PString file(name);
        ok = p->LoadFromFile(Owner(), programName, file, static_cast<const PreProcessorArray*>(defines));
    }
    if (!ok) {
        p->ReleaseReference();
        error = static_cast<ErrorType>(1);
        return 1;
    }
    const Instance instance{p, programName, DefinesText(static_cast<const PreProcessorArray*>(defines))};
    instances.Add(instance);
    program = p;
    return 0;
}

// 0x100559e0: the last program when its name matches, else an instance of that name and defines, else a new load
// of the file (keeping a copy of the new defines; the old copy is left behind, as in the original)
ExtendedGfxLanguage* __cdecl VertexProgramResource::CreateInstance(const PString& name,
                                                                   const PreProcessorArray* d) const
{
    if (program->GetProgramName().node == name.node) {
        program->AddReference();
        return program;
    }
    const AnsiString key(DefinesText(d));
    for (unsigned int i = 0; i < instances.count; ++i) {
        const Instance& in = instances.data[i];
        if (in.name.node == name.node && key == in.defines) {
            in.program->AddReference();
            return in.program;
        }
    }
    if (d) {
        auto* const copy = static_cast<Array<PreProcessorDefine, 3>*>(Alloc::New(sizeof(Array<PreProcessorDefine, 3>)));
        if (copy) {
            copy->count = 0;
            copy->capacity = 0;
            copy->data = nullptr;
            *copy = *d;
        }
        defines = copy;
    }
    auto* const self = const_cast<VertexProgramResource*>(this);
    if (self->LoadTxt(fileName.node ? fileName.node->text : nullptr).code != 0 ||
        program->GetProgramName().node != name.node)
        return nullptr;
    program->AddReference();
    return program;
}

// ---------------------------------------------------------------------------------------------------------------
// TextureCombinerResource

// 0x1003a990 (the kernel is left as it is)
TextureCombinerResource::TextureCombinerResource(const PString& name, const ResourceDatabase* owner)
    : Resource(name, owner), combiner(nullptr), manager(nullptr), defines(nullptr)
{
}

// 0x1003a800: the combiner is shared without a reference
TextureCombinerResource::TextureCombinerResource(const TextureCombinerResource& o)
    : Resource(o), combiner(o.combiner), programName(o.programName), kernel(o.kernel), manager(o.manager),
      defines(o.defines)
{
}

// 0x1003a8b0
TextureCombinerResource& TextureCombinerResource::operator=(const TextureCombinerResource& o)
{
    Resource::operator=(o);
    combiner = o.combiner;
    programName = o.programName;
    kernel = o.kernel;
    manager = o.manager;
    defines = o.defines;
    return *this;
}

// 0x1003a9c0
TextureCombinerResource::~TextureCombinerResource()
{
    if (combiner) combiner->ReleaseReference();
}

// 0x10041260
ExtendedGfxLanguage* __cdecl TextureCombinerResource::CreateInstance() const
{
    combiner->AddReference();
    return combiner;
}

// 0x10041280
void __cdecl TextureCombinerResource::SetParameters(RenderKernel* k, TextureCombinerManager* m,
                                                    const PreProcessorArray* d) const
{
    kernel = k;
    manager = m;
    defines = d;
}

// 0x100412a0: a combiner from the display, loaded from the file
TextureCombinerResource::Result __cdecl TextureCombinerResource::LoadTxt(const char* name)
{
    combiner = VCall<TextureCombiner*>(KernelDisplay(kernel), 0x13c);
    if (combiner) {
        combiner->SetCombinerManager(manager);
        bool ok;
        {
            const PString file(name);
            ok = combiner->LoadFromFile(Owner(), programName, file, defines);
        }
        if (ok) return 0;
        combiner->ReleaseReference();
        combiner = nullptr;
    }
    return 1;
}

}  // namespace Jet
