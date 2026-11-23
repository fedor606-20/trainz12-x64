// RenderSystem_7c unit U4 (notes/RenderSystem.md): modifiers, the objects a scene updates once per frame (Render3DCore,
// WaterSystem and trainz.exe derive from them). Layouts and vtable order follow the TS12 assembly (vftables
// 0x1006f22c ModifierHost, 0x1006f25c Modifier, 0x1006f284 ModifierList, 0x1006f2b4 ModifierUpdateTask); behaviour in
// graphics/RenderSystem_7c/Modifiers.cpp.
#pragma once

#include <jet/KernelStandard.h>
#include <jet/PString.h>
#include <jet/Result.h>

namespace Jet {

class Modifier;
class RenderKernel;

// the owner of modifiers. The base answers 2 to the inserts and nothing else (its other slots are the original's
// shared "xor eax,eax; ret" stub, i.e. abstract); ModifierList is the real one.
class ModifierHost {
public:
    enum Results {};  // 2 not supported, 3 a modifier was removed by its update, 4 priority mismatch, 5 not found
    static const ResultTable* const results;
    using Result = ResultClass<&results, Results>;

    ModifierHost();
    ModifierHost(const ModifierHost& o);
    ModifierHost& operator=(const ModifierHost&);
    virtual ~ModifierHost();  // 0

    virtual Modifier* __cdecl Find(const PString& name);  // 1
    virtual Result __cdecl Add(Modifier* m);              // 2
    // overloads go in reverse: 4 InsertBefore(m), 3 InsertBefore(m, next to), 6 / 5 the same for InsertAfter
    virtual Result __cdecl InsertBefore(Modifier* m);
    virtual Result __cdecl InsertBefore(Modifier* m, const Modifier* other);
    virtual Result __cdecl InsertAfter(Modifier* m);
    virtual Result __cdecl InsertAfter(Modifier* m, const Modifier* other);
    virtual Result __cdecl Remove(const Modifier* m);   // 8
    virtual Result __cdecl Remove(const PString& name);  // 7
    virtual void __cdecl RemoveAll();                     // 9
    virtual Result __cdecl UpdateModifiers(float dt);     // 10

    JET_HEAP_OBJECT

protected:
    void SetModifierHost(ModifierHost* host, const Modifier* m) const;
    bool TouchModifier(const Modifier* m, unsigned long frame) const;  // false when already stamped with `frame`
};
JET_LAYOUT32(sizeof(ModifierHost) == 4);

// 0x18 bytes: +4 host, +8 name, +0xc priority (lower first; 0.5 by default), +0x10 bit 0 complete, +0x14 the frame
// stamp of its last update (left uninitialised by the constructors, as in the original)
class Modifier {
public:
    enum Results {};  // 4 host removed, 5 the host refused it, 6 no modifier task in the kernel
    static const ResultTable* const results;
    using Result = ResultClass<&results, Results>;

    Modifier();
    Modifier(const PString& name, float priority);
    Modifier(ModifierHost* host, const PString& name, float priority);
    Modifier(RenderKernel* kernel, const PString& name, float priority);
    Modifier(const Modifier& o);
    Modifier& operator=(const Modifier& o);
    virtual ~Modifier();  // 0

    virtual PString __cdecl GetName() const;                // 1
    virtual void __cdecl SetName(const PString& name);      // 2
    virtual float __cdecl GetPriority() const;              // 3
    virtual void __cdecl SetPriority(float priority);       // 4
    virtual Result __cdecl SetHost(ModifierHost* host);     // 6
    virtual Result __cdecl SetHost(RenderKernel* kernel);   // 5: the kernel's scene graph's modifier task
    virtual void __cdecl SetComplete(bool complete);        // 7
    // 8: called by ModifierList::UpdateModifiers once per frame; 2 detaches the modifier, 3 detaches and deletes it.
    // Not exported: the original's base slot is the shared "xor eax,eax; ret" stub (it writes no result).
    virtual Result __cdecl Update(float dt);

    ModifierHost* __cdecl GetHost();
    const ModifierHost* __cdecl GetHost() const;

    JET_HEAP_OBJECT

protected:
    void __cdecl InitInstance(Modifier& to) const;  // copies complete, name and priority, then moves `to` to our host

private:
    friend class ModifierHost;
    friend class ModifierList;

    ModifierHost* host;        // +4
    PString name;              // +8
    float priority;            // +0xc
    unsigned char complete : 1;  // +0x10 (the other bits are kept)
    unsigned long frame;       // +0x14
};
JET_LAYOUT32(sizeof(Modifier) == 0x18);

// the VC8 std::list<Modifier*> of the original (checked iterators, release): {allocator (empty, 4 bytes), head,
// size}; nodes {next, prev, value} of 0xc bytes from Jet's heap. Laid out by hand: the VS2026 list differs.
struct ModifierNode {
    ModifierNode* next;
    ModifierNode* prev;
    Modifier* value;
};
struct ModifierListBase {
    unsigned char allocator[4];
    ModifierNode* head;
    unsigned int size;
};
// std::list<Modifier*>::_Iterator<1>: {container, node}
// (a constructor, so that it is returned in memory like the original's class)
struct ModifierIterator {
    ModifierIterator(ModifierListBase* c = nullptr, ModifierNode* n = nullptr) : container(c), node(n) {}
    ModifierListBase* container;
    ModifierNode* node;
};

// the modifiers of a scene in priority order. 0x18 bytes: +4 the list, +0x10 set while a modifier updates, +0x14
// the frame counter UpdateModifiers stamps the modifiers with. Every walk holds `modifierListLock`.
class ModifierList : public ModifierHost {
public:
    ModifierList();
    ModifierList(const ModifierList& o);  // copies the pointers (the modifiers keep their host)
    ModifierList& operator=(const ModifierList& o);
    ~ModifierList() override;

    Modifier* __cdecl Find(const PString& name) override;
    Result __cdecl Add(Modifier* m) override;  // InsertBefore(m)
    // before the first one of equal or higher priority
    Result __cdecl InsertBefore(Modifier* m) override;
    // `m` must have the priority of `other`: right before it, else where InsertBefore(m) would put it
    Result __cdecl InsertBefore(Modifier* m, const Modifier* other) override;
    // after the last one of equal priority
    Result __cdecl InsertAfter(Modifier* m) override;
    Result __cdecl InsertAfter(Modifier* m, const Modifier* other) override;
    Result __cdecl Remove(const Modifier* m) override;
    Result __cdecl Remove(const PString& name) override;
    void __cdecl RemoveAll() override;
    Result __cdecl UpdateModifiers(float dt) override;

    Result CheckPresence(const Modifier* m);

protected:
    // unlinks the node (the modifier's host becomes null, then Remove(m) is called on us) and returns the next one.
    // Exported under the original's name, std::list<Modifier*>::_Iterator<1> (implemented.txt alias).
    ModifierIterator QuickRemove(ModifierIterator it);

    static CriticalSection modifierListLock;

private:
    ModifierNode* InsertNode(ModifierNode* before, Modifier* m);  // before `before`, then m's host is us

    ModifierListBase list;  // +4
    bool updating;          // +0x10
    unsigned long frame;    // +0x14
};
JET_LAYOUT32(sizeof(ModifierList) == 0x18);

// the scene graph's list (SceneGraph +8): a task whose Update runs UpdateModifiers
class ModifierUpdateTask : public ModifierList {
public:
    ModifierUpdateTask();
    ModifierUpdateTask(const ModifierUpdateTask& o);
    ModifierUpdateTask& operator=(const ModifierUpdateTask& o);
    ~ModifierUpdateTask() override;

    virtual void __cdecl Update(float dt);  // 11
};
JET_LAYOUT32(sizeof(ModifierUpdateTask) == 0x18);

}  // namespace Jet
