// Jet's main loop (Kernel_7c): Task, TaskManager, App, EntryPoint, TimeEventSystem. Layouts and members follow
// TS12 (the SDK 1.0 headers plus TS12's lockFPS); members with __cdecl are exported that way.
#pragma once

#include <jet/FastDynamicArray.h>
#include <jet/KernelStandard.h>
#include <jet/PString.h>

namespace Jet {

class TaskManager;

// intrusive list link (KernelStandard's IListNode: {next, prev})
struct IListNode {
    IListNode* next = nullptr;
    IListNode* prev = nullptr;
};

// A periodically updated process. MSVC pads the vfptr to the class's 8-byte alignment, so the links sit at +8.
struct TaskList;

class Task : public IListNode {
    friend class TaskManager;
    friend struct TaskList;

public:
    enum UpdateCode { CONTINUE, STOP, EXIT };

    Task(const PString& name, float priority = 0.0f, bool updated = true);
    Task(const Task& o);
    Task& operator=(const Task& o);
    virtual ~Task();
    virtual const PString& __cdecl GetName() const;  // slot 1
    bool __cdecl IsUpdating() const;
    bool __cdecl IsOnUpdateList() const;
    int __cdecl operator<(const Task& o) const;
    int __cdecl operator==(const Task& o) const;
    JET_HEAP_OBJECT

protected:
    virtual bool __cdecl Init(OS* os);            // slot 2
    virtual void __cdecl Done();                  // slot 3
    virtual UpdateCode __cdecl Update(float dt);  // slot 4
    void __cdecl Suspend();
    void __cdecl Resume();
    void __cdecl SetOnUpdateList(bool onList);

private:
    static TaskManager* taskManager;
    bool __cdecl IsInitialized() const;
    void __cdecl SetInitialized(bool initialized);

    PString taskName;        // +0x10
    double updatedTime;      // +0x18
    float priority;          // +0x20
    bool isUpdating : 1;     // +0x24
    bool isOnUpdateList : 1;
    bool isInitialized : 1;
};

using TaskArray = FastDynamicArray<Task*, 13, Less<Task*, Task*>>;

// IList<Task>: a circular list through Task's links, with the head node standing in as the end
struct TaskList {
    IListNode head;          // {first, last}
    unsigned long count = 0;

    TaskList() { head.next = head.prev = &head; }
    static Task* Of(IListNode* n) { return n ? static_cast<Task*>(n) : nullptr; }
    Task* End() { return static_cast<Task*>(&head); }
    Task* First() { return Of(head.next); }
    void Unlink(Task* t)
    {
        t->prev->next = t->next;
        t->next->prev = t->prev;
        t->prev = nullptr;
        t->next = nullptr;
        --count;
    }
    void InsertBefore(Task* before, Task* t)
    {
        t->next = before ? static_cast<IListNode*>(before) : nullptr;
        t->prev = before->prev;
        before->prev->next = t;
        before->prev = t;
        ++count;
    }
    void Append(Task* t) { InsertBefore(End(), t); }
    Task* PopFirst()
    {
        if (!count) return nullptr;
        Task* t = First();
        Unlink(t);
        return t;
    }
    // before the first task whose priority is not lower (TS12 0x1000d570)
    void InsertSorted(Task* t)
    {
        Task* cur = First();
        while (cur != End() && t->priority > cur->priority) cur = Of(cur->next);
        InsertBefore(cur, t);
    }
};

class TaskManager {
    friend class Task;

public:
    TaskManager();
    ~TaskManager();
    Task* __cdecl GetTask(const PString& name);
    Task::UpdateCode __cdecl UpdateTask(const PString& name, float dt);
    OS* __cdecl GetOS() const;
    int __cdecl Main(OS* os);
    static void __cdecl Shutdown();
    static bool __cdecl IsRunning();
    bool __cdecl Start(OS* os);
    bool __cdecl Update(float dt);
    void __cdecl Stop();
    void __cdecl ResetTime();
    int __cdecl GetFrameRate() const;
    JET_HEAP_OBJECT

private:
    bool Init();
    bool CheckTaskChanges();
    void Loop();
    void Done();
    void InitWave();
    void DoneWave();
    void __cdecl Add(Task* task);
    void __cdecl Remove(Task* task);
    void __cdecl SetTaskChanged(Task* task);
    const TaskArray& __cdecl GetTaskArr() const;

    static bool shutdown;
    static bool running;

    OS* os;                        // +0x00
    double lastTime;               // +0x08
    Task* frameEndTask;            // +0x10
    TaskArray taskArray;           // +0x14
    TaskArray taskChangeArray;     // +0x18
    TaskList taskUpdateList;       // +0x1c (count +0x24)
    bool initialised : 1;          // +0x28
    bool taskChanges : 1;
    int frameRate;                 // +0x2c
    float lockFPS;                 // +0x30: TS12, seconds per locked frame once read
    DynamicLibrary* waveServer;    // +0x34
};

TaskManager* __cdecl GetTaskManager();

// The kernel's TimeEventSystem is a Jet::Events object made on first use.
class TimeEventSystem {
public:
    static TimeEventSystem& __cdecl Get();
};

class App {
public:
    App();
    App(const App& o);
    App& operator=(const App& o);
    virtual ~App();
    virtual bool __cdecl Init(const char* appName, const char* commandLine, void* instance, HWND__* window);
    int __cdecl Main();
    const char* __cdecl GetAppName() const;
    OS* __cdecl GetOS() const;
    JET_HEAP_OBJECT

protected:
    virtual int __cdecl ProtectedMain();
    virtual int __cdecl CriticalShutdown();

private:
    static int __cdecl MainCallBack(void* app);
    static int __cdecl CriticalShutdownCallBack(void* app);

    OS* os;          // +4
    void* reserved;  // +8 (copied, never set)
};

class EntryPoint {
public:
    EntryPoint();
    EntryPoint(const EntryPoint& o);
    EntryPoint& operator=(const EntryPoint& o);
    virtual bool __cdecl Init(OS* os);  // TS12: returns false (pure in the SDK)
    virtual void __cdecl Done();
    virtual ~EntryPoint();
    JET_HEAP_OBJECT
};

}  // namespace Jet
