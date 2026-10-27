// Unit U3a2 of RenderSystem_7c: VertexProgramManager (0x10053b70..0x10054880) and TextureCombinerManager
// (0x10040850..0x10041200), with their HashTable helpers. Behaviour follows the TS12 assembly (addresses per
// function). The original's Display makes both managers until U9, so their statics (the manager, the built-in
// lists, the registered program lists) stay the original's; RegisterBuiltInPrograms, called by the other modules,
// fills the original's lists and loads into the original's managers.
#include <jet/GfxLanguage.h>
#include <jet/VCall.h>

#include <cstring>

#include "GfxParse.h"

namespace Jet {

namespace {

using Node = PStringCache::Node;

Display* KernelDisplay(const RenderKernel* k) { return *reinterpret_cast<Display* const*>(reinterpret_cast<const char*>(k) + 0x7c); }

// 0x100411a0 / 0x10054de0: a copy of `a` with `text` appended
PString Concat(const PString& a, const char* text)
{
    PString r(a);
    r.node = PString::Cache()->Ins(r.node, reinterpret_cast<const unsigned char*>(text), r.node ? r.node->length : 0);
    return r;
}
PString Concat(const PString& a, const PString& b) { return Concat(a, b.node ? b.node->text : nullptr); }

// 0x1000ddd0: the old node goes first
void Assign(PString& dst, const PString& src)
{
    if (dst.node) PString::Cache()->Destroy(dst.node);
    ::new (&dst) PString(src);
}

const void* RegisteredKey(const VertexProgram* p) { return p->GetRegisteredName().node; }
const void* NameKey(const TextureCombiner* c) { return c->GetProgramName().node; }

// a vertex type by id from the display (slot 0x110, referenced), none from 99 on
VertexType* TypeById(Display* d, VertexTypeID id) { return id < 99 ? VCall<VertexType*>(d, 0x110, id) : nullptr; }
void Release(VertexType* t) { reinterpret_cast<ReferenceCount*>(t)->ReleaseReference(); }

}  // namespace

// ---------------------------------------------------------------------------------------------------------------
// VertexProgramManager

// 0x10053bd0: the first manager made is the one the built-in programs go to
VertexProgramManager::VertexProgramManager(Display* d) : display(d)
{
    programs.Init();
    if (!theManager) theManager = this;
}

// 0x10053c50
VertexProgramManager::~VertexProgramManager()
{
    programs.ForEach([](VertexProgram* p) { p->ReleaseReference(); });
    if (this == theManager) {
        theManager = nullptr;
        registeredProgramList.ForEach([](RegisteredName*& r) {
            delete r;
            r = nullptr;
        });
        registeredProgramList.Clear();
    }
    programs.Destroy();
}

// 0x10054060 / 0x100540b0
VertexProgramManager::Result __cdecl VertexProgramManager::Create(RenderKernel* kernel, VertexProgram*& out,
                                                                  VertexType* type, bool shared, const PString& name,
                                                                  const PString& file, const ResourceDatabase* db,
                                                                  const PreProcessorArray* defines)
{
    return Create(kernel, out, false, static_cast<VertexTypeID>(99), type, shared, name, file, db, defines);
}

VertexProgramManager::Result __cdecl VertexProgramManager::Create(RenderKernel* kernel, VertexProgram*& out,
                                                                  VertexTypeID id, bool shared, const PString& name,
                                                                  const PString& file, const ResourceDatabase* db,
                                                                  const PreProcessorArray* defines)
{
    return Create(kernel, out, true, id, nullptr, shared, name, file, db, defines);
}

// 0x10054100: registered as "file:program" + the defines; a program already made is shared (or copied), else it
// comes from the built-in lists (no database) or from the `.vtx` resource (the file, or the program's name)
VertexProgramManager::Result __cdecl VertexProgramManager::Create(RenderKernel* kernel, VertexProgram*& out, bool byId,
                                                                  VertexTypeID id, VertexType* type, bool shared,
                                                                  const PString& name, const PString& file,
                                                                  const ResourceDatabase* db,
                                                                  const PreProcessorArray* defines)
{
    out = nullptr;
    PString definesKey;
    {
        const AnsiString d = defines ? defines->ConcatenateStrings() : AnsiString("");
        const char* const t = d;
        if (t) definesKey.node = PString::Cache()->CreateANSI(reinterpret_cast<const unsigned char*>(t), -1);
    }
    const PString key = Concat(Concat(Concat(file, ":"), name), definesKey);
    if (VertexProgram** const slot = programs.Find(key.node, RegisteredKey)) {
        VertexProgram* const p = *slot;
        if (shared) {
            out = p->CreateShared();
        } else {
            p->AddReference();
            out = p;
        }
        return 0;
    }
    if (!db) {
        const Result r = LoadBuiltInProgram(key, name, out, byId, id, type, defines);
        return r.code == 2 ? 4 : r.code;
    }
    PString resName;
    if (file.node && file.node->length) {
        Assign(resName, file);
    } else {
        const PString t = Concat(name, ".vtx");
        Assign(resName, t);
    }
    auto* const res = static_cast<const VertexProgramResource*>(db->Get(resName, 2));
    if (!res) return 2;
    VertexType* const vt = byId ? TypeById(KernelDisplay(kernel), id) : type;
    res->SetParameters(kernel, vt, name, this, defines);
    Result result;
    if (res->Lock().code == 0) {
        out = static_cast<VertexProgram*>(res->CreateInstance(name, defines));
        res->Unlock();
        if (byId && vt) Release(vt);
        if (!out) {
            result = 2;
        } else {
            out->SetRegisteredName(key);
            programs.Insert(out, RegisteredKey(out));
            out->AddReference();
            result = 0;
        }
    } else {
        if (byId && vt) Release(vt);
        result = res->GetErrorType() == 1 ? 6 : 8;
    }
    res->ReleaseReference();
    return result;
}

// 0x10054690: the program `name` of the built-in lists, made by the display and loaded from its list's text
VertexProgramManager::Result __cdecl VertexProgramManager::LoadBuiltInProgram(const PString& key, const PString& name,
                                                                              VertexProgram*& out, bool byId,
                                                                              VertexTypeID id, VertexType* type,
                                                                              const PreProcessorArray* defines)
{
    for (unsigned int i = 0; i < builtInProgramList.count; ++i) {
        const ProgramItem& item = builtInProgramList.data[i];
        if (item.program.node != name.node) continue;
        const char* const text = item.text;
        if (!text) return 2;
        VertexType* const vt = byId ? TypeById(display, id) : type;
        VertexProgram* const p = VCall<VertexProgram*>(display, 0x144, vt);
        out = p;
        if (!p) return 7;
        p->SetProgramManager(this);
        const bool ok = p->LoadProgram(name, text, defines);
        if (byId && vt) Release(vt);
        if (!ok) {
            p->ReleaseReference();
            out = nullptr;
            return 6;
        }
        p->SetRegisteredName(key);
        p->AddReference();
        programs.Insert(p, RegisteredKey(p));
        return 0;
    }
    return 2;
}

// 0x10054620
VertexProgramManager::Result __cdecl VertexProgramManager::Register(VertexProgram* program)
{
    if (programs.Find(RegisteredKey(program), RegisteredKey)) return 3;
    program->SetProgramManager(this);
    programs.Insert(program, RegisteredKey(program));
    return 0;
}

// 0x10054800: every program, even after one fails
VertexProgramManager::Result __cdecl VertexProgramManager::RestorePrograms()
{
    bool failed = false;
    programs.ForEach([&](VertexProgram* p) { failed |= !p->RestoreProgram(); });
    return failed ? 5 : 0;
}

// 0x10053dc0: a list is parsed once; each program it names becomes an item {list name, program, text}
void __cdecl VertexProgramManager::RegisterBuiltInPrograms(const PString& name, const char* text)
{
    if (registeredProgramList.Find(name.node, [](const RegisteredName* r) -> const void* { return r->name.node; }))
        return;
    MemoryFile file(text, static_cast<unsigned int>(std::strlen(text)), false, 0);
    struct Ctx {
        const PString* name;
        const char* text;
    } ctx{&name, text};
    GfxQuickParse(&file, name.Text(),
                  [](const GfxNameList& names, void* p) {
                      const Ctx& c = *static_cast<Ctx*>(p);
                      RegisteredName* const r = new RegisteredName{*c.name};
                      registeredProgramList.Insert(r, r->name.node);
                      const GfxNameList::Chunk* const head = names.head;
                      if (!head) return;
                      const GfxNameList::Chunk* chunk = head;
                      do {
                          for (int i = 0; i < chunk->count; ++i) {
                              const ProgramItem item{*c.name, *reinterpret_cast<const PString*>(&chunk->names[i]),
                                                     c.text};
                              builtInProgramList.Add(item);
                          }
                          chunk = chunk->next;
                      } while (chunk != head);
                  },
                  &ctx);
}

// ---------------------------------------------------------------------------------------------------------------
// TextureCombinerManager

// 0x100408b0: the latest manager made takes the texts registered so far
TextureCombinerManager::TextureCombinerManager(Display* d) : display(d)
{
    combiners.Init();
    theManager = this;
    for (unsigned int i = 0; i < builtInProgramList.count; ++i)
        LoadBuiltInPrograms(builtInProgramList.data[i].name, builtInProgramList.data[i].text);
}

// 0x10040960
TextureCombinerManager::~TextureCombinerManager()
{
    combiners.ForEach([](TextureCombiner* c) { c->ReleaseReference(); });
    combiners.Destroy();
}

// 0x10040a40
void __cdecl TextureCombinerManager::RegisterBuiltInPrograms(const PString& name, const char* text)
{
    if (theManager) {
        theManager->LoadBuiltInPrograms(name, text);
        return;
    }
    const ProgramItem item{name, text};
    builtInProgramList.Add(item);
}

// 0x10040b20: once per name; the display's combiner keeps the manager's reference
void __cdecl TextureCombinerManager::LoadBuiltInPrograms(const PString& name, const char* text)
{
    if (combiners.Find(name.node, NameKey)) return;
    TextureCombiner* const c = VCall<TextureCombiner*>(display, 0x13c);
    if (!c) return;
    c->SetCombinerManager(this);
    if (!c->LoadProgram(name, text, nullptr)) {
        c->ReleaseReference();
        return;
    }
    combiners.Insert(c, NameKey(c));
}

// 0x10040bb0: by name without its ".pxl"; else from the `.pxl` resource. (When the resource does not lock, `out`
// is read as the caller left it, as in the original.)
TextureCombinerManager::Result __cdecl TextureCombinerManager::Create(RenderKernel* kernel, TextureCombiner*& out,
                                                                      const PString& name, const ResourceDatabase* db)
{
    PString key(name);
    {
        const int at = key.node ? PString::Cache()->Find(key.node, reinterpret_cast<const unsigned char*>(".pxl"), 0) : -1;
        const unsigned int len = key.node ? key.node->length : 0;
        if (at == static_cast<int>(len - 4)) {
            const unsigned int from = len - 4;
            if (key.node && from < key.node->length)
                key.node = PString::Cache()->Del(key.node, from, key.node->length - from);
        }
    }
    if (TextureCombiner** const slot = combiners.Find(key.node, NameKey)) {
        (*slot)->AddReference();
        out = *slot;
        return 0;
    }
    if (!db) return 4;
    const PString resName = Concat(key, ".pxl");
    auto* const res = static_cast<const TextureCombinerResource*>(db->Get(resName, 2));
    if (!res) return 2;
    res->kernel = kernel;
    res->manager = this;
    res->defines = nullptr;
    if (res->Lock().code == 0) {
        out = static_cast<TextureCombiner*>(res->CreateInstance());
        res->Unlock();
    }
    Result result;
    if (!out) {
        result = 2;
    } else {
        combiners.Insert(out, NameKey(out));
        out->AddReference();
        result = 0;
    }
    res->ReleaseReference();
    return result;
}

// 0x10040ee0
TextureCombinerManager::Result __cdecl TextureCombinerManager::Register(TextureCombiner* combiner)
{
    if (combiners.Find(NameKey(combiner), NameKey)) return 3;
    combiner->SetCombinerManager(this);
    combiners.Insert(combiner, NameKey(combiner));
    return 0;
}

}  // namespace Jet
