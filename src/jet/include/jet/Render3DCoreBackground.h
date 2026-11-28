// Render3DCore_7c unit U11 (notes/agents/u11_statics.md): the background loading state machine and its worker thread.
//
// Layouts, vtable order and behaviour follow the TS12 assembly (vftables 0x1007e1c0 BackgroundResource,
// 0x1007e038 BackgroundLoadingThread, 0x1007e1ec ResourceObserver; behaviour in
// graphics/Render3DCore_7c/Background.cpp and BackgroundThread.cpp).
//
// BackgroundResource is 0x2c bytes: a ResourceObserver (vftable +0x00, slot 0 OnLoad) with the virtuals DeleteSelf,
// the two LoadResource overloads, Abort and four hooks at slots 6..9, then the loading thread, the resource, an
// observer array and a CriticalSection. Ordinary members are __cdecl (`this` on the stack), constructors and
// destructors __thiscall, as the mangled QAE/UAE/IAA/IBA names show.
#pragma once

#include <jet/KernelStandard.h>
#include <jet/Resource.h>
#include <jet/Result.h>

namespace Jet {

class BackgroundLoadingThread;
class BackgroundResource;

// What a BackgroundResource reports to the objects that watch it. AddObserver takes a mask of the Flags; each
// callback whose flag is set is called with the resource. The first one (flag 1) aborts the load when it returns
// false; the others abort when they return true.
class BkgResObserver {
public:
    enum Flags {
        FLAG_STARTED = 1,
        FLAG_BACKGROUND_LOADED = 2,
        FLAG_COMPLETED = 4,
        FLAG_FAILED = 8,
    };

    virtual bool __cdecl OnStarted(BackgroundResource* resource) = 0;
    virtual bool __cdecl OnBackgroundLoaded(BackgroundResource* resource) = 0;
    virtual bool __cdecl OnCompleted(BackgroundResource* resource) = 0;
    virtual bool __cdecl OnFailed(BackgroundResource* resource) = 0;
};

// what the state machine's actions can ask for; the caller of DoAction only uses 0 (none), 2 (failed), 3 (retry,
// the background thread keeps trying) and 4 (complete)
enum BackgroundLoadStatus {
    BACKGROUND_OK = 0,
    BACKGROUND_LOADED = 1,
    BACKGROUND_FAILED = 2,
    BACKGROUND_RETRY = 3,
    BACKGROUND_COMPLETE = 4,
};

// a resource loaded on the loading thread: states Unloaded, Aborting, FailurePending, Failed, Requested,
// Initializing, Queued, Loading, LoadComplete, Loaded; inputs QueueRequest, ResourceLoaded, QueueBackground,
// LoadData, LoadComplete, CheckComplete, Unload, LoadFailure, Abort.
class BackgroundResource : public ResourceObserver {
public:
    enum State {
        STATE_UNLOADED = 0,
        STATE_ABORTING = 1,
        STATE_FAILURE_PENDING = 2,
        STATE_FAILED = 3,
        STATE_REQUESTED = 4,
        STATE_INITIALIZING = 5,
        STATE_QUEUED = 6,
        STATE_LOADING = 7,
        STATE_LOAD_COMPLETE = 8,
        STATE_LOADED = 9,
    };
    enum Input {
        INPUT_QUEUE_REQUEST = 0,
        INPUT_RESOURCE_LOADED = 1,
        INPUT_QUEUE_BACKGROUND = 2,
        INPUT_LOAD_DATA = 3,
        INPUT_LOAD_COMPLETE = 4,
        INPUT_CHECK_COMPLETE = 5,
        INPUT_UNLOAD = 6,
        INPUT_LOAD_FAILURE = 7,
        INPUT_ABORT = 8,
    };
    enum Action {
        ACTION_ERROR = 0,
        ACTION_NONE = 1,
        ACTION_PROCESS_FAILURE = 2,
        ACTION_NOTIFY_FAILURE = 3,
        ACTION_QUEUE_REQUEST = 4,
        ACTION_MAIN_THREAD_INITIALIZE = 5,
        ACTION_QUEUE_BACKGROUND = 6,
        ACTION_BACKGROUND_LOAD = 7,
        ACTION_SEND_LOAD_COMPLETE = 8,
        ACTION_LOAD_COMPLETE = 9,
        ACTION_UNLOAD_RESOURCE = 10,
        ACTION_ABORT_NOW = 11,
    };
    enum StateReturn {
        STATE_RETURN_CONTINUE = 0,
        STATE_RETURN_NO_ACTION = 0,
        STATE_RETURN_ERROR = 1,
        STATE_RETURN_FAILED = 2,
        STATE_RETURN_RETRY = 3,
        STATE_RETURN_COMPLETE = 4,
    };

    explicit BackgroundResource(BackgroundLoadingThread* thread);
    ~BackgroundResource() override;

    virtual void __cdecl DeleteSelf();                                                // slot 2
    virtual bool __cdecl LoadResource(const Resource* resource);                      // slot 4 (declared first:
    // MSVC puts overloaded virtuals in reverse order, so this one lands after the database overload)
    virtual bool __cdecl LoadResource(const ResourceDatabase* db, const PString& name);  // slot 3
    virtual void __cdecl Abort(bool abort);                                           // slot 5

    void __cdecl AddObserver(BkgResObserver* observer, unsigned long flags);
    void __cdecl RemoveObserver(BkgResObserver* observer);
    void __cdecl BackgroundLoadData();
    bool __cdecl CheckComplete();
    const Resource* __cdecl GetResource() const;
    void __cdecl SetBackgroundThread(BackgroundLoadingThread* thread);
    void __cdecl UnloadResource();

    JET_HEAP_OBJECT

protected:
    virtual void __cdecl OnLoad(const Resource* resource) const override;            // slot 0
    // slots 6 and 7 (the original's shared "return 0" stubs) are the hooks the main and the background thread run
    // before the state machine continues; not exported, so only the slot position matters
    virtual BackgroundLoadStatus __cdecl MainThreadInitialize();
    virtual BackgroundLoadStatus __cdecl BackgroundLoad();
    virtual BackgroundLoadStatus __cdecl MainThreadLoadComplete() const;             // slot 8
    virtual BackgroundLoadStatus __cdecl MainThreadProcessFailure() const;           // slot 9

    void __cdecl QueueBackground();
    void __cdecl LoadFailed();
    State __cdecl GetCurrentState() const;
    void __cdecl Lock() const;
    void __cdecl Unlock() const;

    StateReturn __cdecl CallCallbacks(unsigned long phase) const;
    StateReturn __cdecl DoAction(Input input);

    StateReturn __cdecl ActionProcessFailure();
    StateReturn __cdecl ActionNotifyFailure();
    StateReturn __cdecl ActionQueueRequest();
    StateReturn __cdecl ActionMainThreadInitialize();
    StateReturn __cdecl ActionQueueBackground() const;
    StateReturn __cdecl ActionBackgroundLoadData();
    StateReturn __cdecl ActionSendLoadComplete();
    StateReturn __cdecl ActionLoadComplete();
    StateReturn __cdecl ActionUnloadResource();
    StateReturn __cdecl ActionAbortNow();

    // the debug name tables and the exported FSM pointer. The tables are arrays (MSVC mangles an array of
    // `const T` like a pointer to `const T`, which is why the export table demangles them as pointers); the FSM
    // is read from the table the state machine owns, so the exported pointer is the original's null.
    struct NameCheck {
        int value;
        const char* name;
    };
    struct StateTable {
        State state;
        Action action;
        unsigned int reserved;
        unsigned int index;
    };

    static unsigned long mainThreadId;
    static StateTable const (* const fsm)[9];
    static NameCheck const actionNames[12];
    static NameCheck const inputNames[9];
    static NameCheck const stateNames[10];

private:
    friend class BackgroundLoadingThread;

    struct ObserverEntry {
        unsigned long flags;
        BkgResObserver* observer;
    };

    BackgroundLoadingThread* backgroundThread;  // +0x04
    Resource* resource;                         // +0x08
    int observerCount;                          // +0x0c
    int observerCapacity;                       // +0x10
    ObserverEntry* observers;                   // +0x14
    bool locked;                                // +0x18 the resource is locked and must be unlocked
    bool deleteOnAbort;                         // +0x19
    bool deleteOnFailure;                       // +0x1a
    State state;                                // +0x1c
    mutable CriticalSection section;             // +0x20
};
JET_LAYOUT32(sizeof(BackgroundResource) == 0x2c);

// The background loading thread: a ReferenceCount with two priority queues (resources to load and resources that
// finished loading, ordered by Resource::GetPriority) and a list of resources to delete. 0x44 bytes.
class BackgroundLoadingThread : public ReferenceCount {
public:
    enum Results {
        RESULT_OK = 0,
        RESULT_IS_CURRENT = 1,
        RESULT_NOT_QUEUED = 2,
        RESULT_QUEUE_FULL = 3,
    };
    static const ResultTable* const results;  // the ResultClass symbol; the exported name aliases resultsTable
    using Result = ResultClass<&results, Results>;

    BackgroundLoadingThread(int maxQueued, Thread::Priority priority);

    virtual Result __cdecl QueueResource(BackgroundResource* resource);    // slot 2
    virtual Result __cdecl DequeueResource(BackgroundResource* resource);  // slot 3
    virtual Result __cdecl SignalComplete(BackgroundResource* resource);   // slot 4
    virtual void __cdecl CheckForComplete();                               // slot 5
    virtual void __cdecl QueueForDelete(BackgroundResource* resource);     // slot 6

    unsigned long __cdecl GetThreadID() const;
    void __cdecl SetThreadPriority(Thread::Priority priority);

    JET_HEAP_OBJECT

protected:
    ~BackgroundLoadingThread() override;

    static unsigned int __stdcall MainThread(void* self);
    unsigned int __cdecl ProcessThread();
    static unsigned long mainThreadId;
    static const ResultTable resultsTable[3];

private:
    // a block of the delete list, as the original's std::list pool: 13 pointers per 0x40-byte block, circular
    struct DeleteBlock {
        DeleteBlock* next;   // +0x00
        DeleteBlock* prev;   // +0x04 (the newest block, on the head)
        unsigned int count;  // +0x08
        BackgroundResource* items[13];  // +0x0c
    };

    static void SiftUp(BackgroundResource** data, int index);
    static void SiftDown(BackgroundResource** data, int count, int index);
    static void RemoveAt(BackgroundResource** data, int& count, int index);

    void RemoveDeleteList();

    int queueCount;                 // +0x08
    int queueCapacity;              // +0x0c
    BackgroundResource** queue;     // +0x10
    int completeCount;              // +0x14
    int completeCapacity;           // +0x18
    BackgroundResource** complete;  // +0x1c
    DeleteBlock* deleteList;        // +0x20
    Thread* thread;                 // +0x24
    BackgroundResource* current;    // +0x28
    Semaphore queued;               // +0x2c
    bool done;                      // +0x34
    unsigned long threadId;         // +0x38
    Mutex mutex;                    // +0x3c
};
JET_LAYOUT32(sizeof(BackgroundLoadingThread) == 0x44);

// The module's facility interface: one virtual (GetName) and nothing else. TS12's table at 0x1007d420 has exactly
// one slot, the shared `xor eax,eax; ret` stub, so a locally compiled one-slot table is the same shape. The class
// is only a base for the facilities other modules define; the constructor writes its own table, as the original
// does (the copy constructor and assignment are the original's trivial ones).
class Facility {
public:
    Facility();
    Facility(const Facility& other);
    Facility& operator=(const Facility& other);
    virtual const PString& __cdecl GetName() const;
};
JET_LAYOUT32(sizeof(Facility) == 4);

}  // namespace Jet
