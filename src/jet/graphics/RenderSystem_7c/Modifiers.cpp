// Unit U4 of RenderSystem_7c: Modifier, ModifierHost, ModifierList and ModifierUpdateTask. Behaviour follows the TS12
// assembly (addresses per function). The original's checked-iterator tests (_invalid_parameter_noinfo) only fire
// on misuse and are left out.
#include <jet/Modifier.h>

namespace Jet {

// both tables are only the text "RES_UNKNOWN" (0x10070fac), like Encrypt's in Kernel_7c
struct ResultTable {
    char text[12];
};
static const ResultTable kModifierResults = {"RES_UNKNOWN"};
const ResultTable* const Modifier::results = &kModifierResults;
const ResultTable* const ModifierHost::results = &kModifierResults;

CriticalSection ModifierList::modifierListLock;

// ---------------------------------------------------------------------------------------------------------------
// the VC8 list<Modifier*> (0x1000d470 head, 0x1000dd00 node, 0x1000dc10 erase, 0x1000d490 destructor)

namespace {

// m's priority is read first, as in the original (a derived GetPriority may log or compute)
bool NotAbove(const Modifier* m, const Modifier* n)
{
    float a = m->GetPriority();
    return a <= n->GetPriority();
}
bool Below(const Modifier* m, const Modifier* n)
{
    float a = m->GetPriority();
    return a < n->GetPriority();
}
bool Differ(const Modifier* m, const Modifier* n)
{
    float a = m->GetPriority();
    return a != n->GetPriority();
}

ModifierNode* NewHead()
{
    auto* h = static_cast<ModifierNode*>(Alloc::New(sizeof(ModifierNode)));
    h->next = h;
    h->prev = h;
    return h;
}

void LinkBefore(ModifierListBase& l, ModifierNode* before, Modifier* m)
{
    auto* n = static_cast<ModifierNode*>(Alloc::New(sizeof(ModifierNode)));
    n->next = before;
    n->prev = before->prev;
    n->value = m;
    ++l.size;
    before->prev = n;
    n->prev->next = n;
}

ModifierNode* Erase(ModifierListBase& l, ModifierNode* n)
{
    ModifierNode* next = n->next;
    if (n != l.head) {
        n->prev->next = next;
        next->prev = n->prev;
        Alloc::Delete(n);
        --l.size;
    }
    return next;
}

void Clear(ModifierListBase& l)
{
    ModifierNode* n = l.head->next;
    l.head->next = l.head;
    l.head->prev = l.head;
    l.size = 0;
    while (n != l.head) {
        ModifierNode* next = n->next;
        Alloc::Delete(n);
        n = next;
    }
}

void AppendAll(ModifierListBase& to, const ModifierListBase& from)
{
    for (ModifierNode* n = from.head->next; n != from.head; n = n->next) LinkBefore(to, to.head, n->value);
}

Profile::Info updateInfo = {"ModifierList::UpdateModifiers",
                            "C:/src/trainz/candidate/ts12_sp1/Jet/Src/Jet/RenderSystem/ModifierList.cpp",
                            0, 0, -1, 0x80000000, 0, 0};

}  // namespace

// ---------------------------------------------------------------------------------------------------------------
// Modifier (0x10036300..0x10036710, 0x1000abe0, 0x1000cad0..0x1000ccc0)

Modifier::Modifier() : host(nullptr), priority(0.5f) { complete = 0; }

Modifier::Modifier(const PString& name, float priority) : name(name), priority(priority)
{
    complete = 0;
    host = nullptr;
}

Modifier::Modifier(ModifierHost* host, const PString& name, float priority) : name(name), priority(priority)
{
    complete = 0;
    this->host = nullptr;
    Modifier::SetHost(host);
}

Modifier::Modifier(RenderKernel* kernel, const PString& name, float priority) : name(name), priority(priority)
{
    complete = 0;
    host = nullptr;
    SetHost(kernel);  // (it discards the result)
}

Modifier::Modifier(const Modifier& o)
    : host(o.host), name(o.name), priority(o.priority), frame(o.frame)
{
    complete = o.complete;
}

Modifier& Modifier::operator=(const Modifier& o)
{
    host = o.host;
    name = o.name;
    priority = o.priority;
    complete = o.complete;
    frame = o.frame;
    return *this;
}

Modifier::~Modifier() {}

PString __cdecl Modifier::GetName() const { return name; }
void __cdecl Modifier::SetName(const PString& n) { name = n; }
float __cdecl Modifier::GetPriority() const { return priority; }
void __cdecl Modifier::SetPriority(float p) { priority = p; }
void __cdecl Modifier::SetComplete(bool c) { complete = c; }
ModifierHost* __cdecl Modifier::GetHost() { return host; }
const ModifierHost* __cdecl Modifier::GetHost() const { return host; }

// a new host does not take the modifier from its old one (0x100365d0)
Modifier::Result __cdecl Modifier::SetHost(ModifierHost* h)
{
    if (!h) {
        if (host) host->Remove(this);
        return 4;
    }
    return h->InsertBefore(this).code != 0 ? 5 : 0;
}

// the kernel's scene graph (+0x80) holds the modifier task at +8 (0x10036630)
Modifier::Result __cdecl Modifier::SetHost(RenderKernel* kernel)
{
    if (kernel) {
        const char* graph = *reinterpret_cast<char* const*>(reinterpret_cast<const char*>(kernel) + 0x80);
        if (graph) {
            auto* task = *reinterpret_cast<ModifierHost* const*>(graph + 8);
            if (task) return SetHost(task);
        }
    }
    return 6;
}

Modifier::Result __cdecl Modifier::Update(float) { return 0; }

void __cdecl Modifier::InitInstance(Modifier& to) const
{
    to.complete = complete;
    to.name = name;
    to.priority = priority;
    if (host != to.host) {
        if (to.host) to.SetHost(static_cast<ModifierHost*>(nullptr));
        if (host) to.SetHost(host);
    }
}

// ---------------------------------------------------------------------------------------------------------------
// ModifierHost (0x1000cce0..0x1000cd90; the inserts share 0x1000cd70, operator= 0x10038c50)

ModifierHost::ModifierHost() {}
ModifierHost::ModifierHost(const ModifierHost&) {}
ModifierHost& ModifierHost::operator=(const ModifierHost&) { return *this; }
ModifierHost::~ModifierHost() {}

Modifier* __cdecl ModifierHost::Find(const PString&) { return nullptr; }
ModifierHost::Result __cdecl ModifierHost::Add(Modifier*) { return 0; }
ModifierHost::Result __cdecl ModifierHost::InsertBefore(Modifier*) { return 2; }
ModifierHost::Result __cdecl ModifierHost::InsertBefore(Modifier*, const Modifier*) { return 2; }
ModifierHost::Result __cdecl ModifierHost::InsertAfter(Modifier*) { return 2; }
ModifierHost::Result __cdecl ModifierHost::InsertAfter(Modifier*, const Modifier*) { return 2; }
ModifierHost::Result __cdecl ModifierHost::Remove(const Modifier*) { return 0; }
ModifierHost::Result __cdecl ModifierHost::Remove(const PString&) { return 0; }
void __cdecl ModifierHost::RemoveAll() {}
ModifierHost::Result __cdecl ModifierHost::UpdateModifiers(float) { return 0; }

void ModifierHost::SetModifierHost(ModifierHost* h, const Modifier* m) const { const_cast<Modifier*>(m)->host = h; }

bool ModifierHost::TouchModifier(const Modifier* m, unsigned long frame) const
{
    if (m->frame == frame) return false;
    const_cast<Modifier*>(m)->frame = frame;
    return true;
}

// ---------------------------------------------------------------------------------------------------------------
// ModifierList (0x1000cdb0..0x1000cf30, 0x10036710..0x10037630)

ModifierList::ModifierList() : updating(false), frame(0)
{
    list.head = NewHead();
    list.size = 0;
}

ModifierList::ModifierList(const ModifierList& o) : ModifierHost(o)
{
    list.head = NewHead();
    list.size = 0;
    AppendAll(list, o.list);
    updating = o.updating;
    frame = o.frame;
}

ModifierList& ModifierList::operator=(const ModifierList& o)
{
    if (&list != &o.list) {
        Clear(list);
        AppendAll(list, o.list);
    }
    updating = o.updating;
    frame = o.frame;
    return *this;
}

ModifierList::~ModifierList()
{
    ModifierList::RemoveAll();
    Clear(list);
    Alloc::Delete(list.head);
    list.head = nullptr;
}

ModifierNode* ModifierList::InsertNode(ModifierNode* before, Modifier* m)
{
    LinkBefore(list, before, m);
    m->host = this;
    return before->prev;
}

Modifier* __cdecl ModifierList::Find(const PString& name)
{
    SingleLock lock(&modifierListLock, true);
    for (ModifierNode* n = list.head->next; n != list.head; n = n->next)
        if (name == n->value->GetName()) return n->value;
    return nullptr;
}

ModifierList::Result ModifierList::CheckPresence(const Modifier* m)
{
    SingleLock lock(&modifierListLock, true);
    for (ModifierNode* n = list.head->next; n != list.head; n = n->next)
        if (n->value == m) return 0;
    return 5;
}

ModifierList::Result __cdecl ModifierList::Add(Modifier* m) { return InsertBefore(m); }

ModifierList::Result __cdecl ModifierList::InsertBefore(Modifier* m)
{
    SingleLock lock(&modifierListLock, true);
    ModifierNode* n = list.head->next;
    while (n != list.head && !NotAbove(m, n->value)) n = n->next;
    InsertNode(n, m);
    return 0;
}

ModifierList::Result __cdecl ModifierList::InsertAfter(Modifier* m)
{
    SingleLock lock(&modifierListLock, true);
    ModifierNode* n = list.head->next;
    while (n != list.head && !Below(m, n->value)) n = n->next;
    InsertNode(n, m);
    return 0;
}

ModifierList::Result __cdecl ModifierList::InsertBefore(Modifier* m, const Modifier* other)
{
    SingleLock lock(&modifierListLock, true);
    if (Differ(m, other)) return 4;
    ModifierNode* at = list.head;  // the first of equal or higher priority, while `other` is not found
    for (ModifierNode* n = list.head->next; n != list.head; n = n->next) {
        if (n->value == other) {
            InsertNode(n, m);
            return 0;
        }
        if (at == list.head && NotAbove(m, n->value)) at = n;
    }
    InsertNode(at, m);
    return 0;
}

ModifierList::Result __cdecl ModifierList::InsertAfter(Modifier* m, const Modifier* other)
{
    SingleLock lock(&modifierListLock, true);
    if (Differ(m, other)) return 4;
    ModifierNode* at = list.head;  // the last of equal or higher priority, while `other` is not found
    for (ModifierNode* n = list.head->next; n != list.head; n = n->next) {
        if (n->value == other) {
            InsertNode(n->next, m);
            return 0;
        }
        if (NotAbove(m, n->value)) at = n;
    }
    InsertNode(at == list.head ? list.head : at->next, m);
    return 0;
}

// the host is not compared: any list drops the modifier (0x10037080)
ModifierList::Result __cdecl ModifierList::Remove(const Modifier* m)
{
    if (!m->host) return 5;
    SingleLock lock(&modifierListLock, true);
    for (ModifierNode* n = list.head->next; n != list.head; n = n->next)
        if (n->value == m) {
            Erase(list, n);
            const_cast<Modifier*>(m)->host = nullptr;
            return 0;
        }
    return 5;
}

ModifierList::Result __cdecl ModifierList::Remove(const PString& name)
{
    SingleLock lock(&modifierListLock, true);
    for (ModifierNode* n = list.head->next; n != list.head; n = n->next)
        if (name == n->value->GetName()) {
            QuickRemove(ModifierIterator(&list, n));
            return 0;
        }
    return 5;
}

ModifierIterator ModifierList::QuickRemove(ModifierIterator it)
{
    SingleLock lock(&modifierListLock, true);
    Modifier* m = it.node->value;
    ModifierIterator next(it.container, it.node->next);
    Erase(list, it.node);
    m->host = nullptr;
    Remove(m);  // virtual: a derived list sees the modifier go
    return next;
}

void __cdecl ModifierList::RemoveAll()
{
    SingleLock lock(&modifierListLock, true);
    ModifierIterator it(&list, list.head->next);
    while (it.node != list.head) it = QuickRemove(it);
}

// each modifier once per call (the frame stamp); updates answering 2 or 3 take the modifier out (3 also deletes
// it) and make the result 3
ModifierList::Result __cdecl ModifierList::UpdateModifiers(float dt)
{
    SingleLock lock(&modifierListLock, true);
    Profile profile(&updateInfo);
    ++frame;
    bool all = true;
    ModifierIterator it(&list, list.head->next);
    while (it.node != list.head) {
        Modifier* m = it.node->value;
        if (m->frame != frame) {
            m->frame = frame;
            updating = true;
            int r = m->Update(dt).code;
            updating = false;
            if (r == 3) {
                it = QuickRemove(it);
                delete m;
                all = false;
                continue;
            }
            if (r == 2) {
                it = QuickRemove(it);
                all = false;
                continue;
            }
        }
        it.node = it.node->next;
    }
    return all ? 0 : 3;
}

// ---------------------------------------------------------------------------------------------------------------
// ModifierUpdateTask (0x1000cf50..0x1000cfa0, 0x10037630)

ModifierUpdateTask::ModifierUpdateTask() {}
ModifierUpdateTask::ModifierUpdateTask(const ModifierUpdateTask& o) : ModifierList(o) {}
ModifierUpdateTask& ModifierUpdateTask::operator=(const ModifierUpdateTask& o)
{
    ModifierList::operator=(o);
    return *this;
}
ModifierUpdateTask::~ModifierUpdateTask() {}

void __cdecl ModifierUpdateTask::Update(float dt) { UpdateModifiers(dt); }

}  // namespace Jet
