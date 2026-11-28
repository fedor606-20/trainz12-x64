// Unit U11 of Render3DCore_7c: Jet::BackgroundResource, the background loading state machine, transcribed from the
// TS12 assembly (0x1000e340..0x1000f71c, 0x1005fec0, 0x1002f240). The layout is in Render3DCoreBackground.h.
//
// The state machine is the original's 10-state table (0x1007cbc0, 16-byte entries): DoAction locks the object's
// CriticalSection, takes the next state and the action from fsm[state][input], stores the next state and runs the
// action. Action 7 (background load) and 11 (abort) run with the lock released, as in the original; the others run
// under it. StateReturn 0 = fine, 1 = error (no action), 2 = failed, 3 = retry, 4 = complete.
#include <jet/Render3DCoreBackground.h>

#include <cstring>
#include <new>

namespace Jet {

namespace {

struct LocalStateTable {
    BackgroundResource::State next;
    BackgroundResource::Action action;
};

// fsm[state][input] from 0x1007cbc0: next state and the action the original dispatches (the table's other two
// words are a constant 0 and a running index)
const LocalStateTable kStateTable[10][9] = {
    //           QueueRequest      ResourceLoaded    QueueBackground   LoadData          LoadComplete
    //           CheckComplete     Unload            LoadFailure       Abort
    {{BackgroundResource::STATE_REQUESTED, BackgroundResource::ACTION_QUEUE_REQUEST},
     {BackgroundResource::STATE_INITIALIZING, BackgroundResource::ACTION_MAIN_THREAD_INITIALIZE},
     {BackgroundResource::STATE_UNLOADED, BackgroundResource::ACTION_ERROR},
     {BackgroundResource::STATE_UNLOADED, BackgroundResource::ACTION_ERROR},
     {BackgroundResource::STATE_UNLOADED, BackgroundResource::ACTION_ERROR},
     {BackgroundResource::STATE_UNLOADED, BackgroundResource::ACTION_NONE},
     {BackgroundResource::STATE_UNLOADED, BackgroundResource::ACTION_ERROR},
     {BackgroundResource::STATE_UNLOADED, BackgroundResource::ACTION_ERROR},
     {BackgroundResource::STATE_UNLOADED, BackgroundResource::ACTION_ABORT_NOW}},

    {{BackgroundResource::STATE_UNLOADED, BackgroundResource::ACTION_ABORT_NOW},
     {BackgroundResource::STATE_UNLOADED, BackgroundResource::ACTION_ABORT_NOW},
     {BackgroundResource::STATE_UNLOADED, BackgroundResource::ACTION_ABORT_NOW},
     {BackgroundResource::STATE_ABORTING, BackgroundResource::ACTION_SEND_LOAD_COMPLETE},
     {BackgroundResource::STATE_ABORTING, BackgroundResource::ACTION_SEND_LOAD_COMPLETE},
     {BackgroundResource::STATE_UNLOADED, BackgroundResource::ACTION_ABORT_NOW},
     {BackgroundResource::STATE_UNLOADED, BackgroundResource::ACTION_ABORT_NOW},
     {BackgroundResource::STATE_UNLOADED, BackgroundResource::ACTION_ABORT_NOW},
     {BackgroundResource::STATE_ABORTING, BackgroundResource::ACTION_NONE}},

    {{BackgroundResource::STATE_FAILURE_PENDING, BackgroundResource::ACTION_ERROR},
     {BackgroundResource::STATE_FAILURE_PENDING, BackgroundResource::ACTION_ERROR},
     {BackgroundResource::STATE_FAILURE_PENDING, BackgroundResource::ACTION_ERROR},
     {BackgroundResource::STATE_FAILURE_PENDING, BackgroundResource::ACTION_ERROR},
     {BackgroundResource::STATE_FAILURE_PENDING, BackgroundResource::ACTION_ERROR},
     {BackgroundResource::STATE_FAILED, BackgroundResource::ACTION_NOTIFY_FAILURE},
     {BackgroundResource::STATE_FAILURE_PENDING, BackgroundResource::ACTION_ERROR},
     {BackgroundResource::STATE_FAILURE_PENDING, BackgroundResource::ACTION_ERROR},
     {BackgroundResource::STATE_ABORTING, BackgroundResource::ACTION_NONE}},

    {{BackgroundResource::STATE_FAILED, BackgroundResource::ACTION_ERROR},
     {BackgroundResource::STATE_FAILED, BackgroundResource::ACTION_ERROR},
     {BackgroundResource::STATE_FAILED, BackgroundResource::ACTION_ERROR},
     {BackgroundResource::STATE_FAILED, BackgroundResource::ACTION_ERROR},
     {BackgroundResource::STATE_FAILED, BackgroundResource::ACTION_ERROR},
     {BackgroundResource::STATE_FAILED, BackgroundResource::ACTION_NONE},
     {BackgroundResource::STATE_FAILED, BackgroundResource::ACTION_ERROR},
     {BackgroundResource::STATE_FAILED, BackgroundResource::ACTION_ERROR},
     {BackgroundResource::STATE_UNLOADED, BackgroundResource::ACTION_ABORT_NOW}},

    {{BackgroundResource::STATE_REQUESTED, BackgroundResource::ACTION_ERROR},
     {BackgroundResource::STATE_INITIALIZING, BackgroundResource::ACTION_MAIN_THREAD_INITIALIZE},
     {BackgroundResource::STATE_REQUESTED, BackgroundResource::ACTION_ERROR},
     {BackgroundResource::STATE_REQUESTED, BackgroundResource::ACTION_ERROR},
     {BackgroundResource::STATE_REQUESTED, BackgroundResource::ACTION_ERROR},
     {BackgroundResource::STATE_REQUESTED, BackgroundResource::ACTION_NONE},
     {BackgroundResource::STATE_REQUESTED, BackgroundResource::ACTION_ERROR},
     {BackgroundResource::STATE_FAILURE_PENDING, BackgroundResource::ACTION_PROCESS_FAILURE},
     {BackgroundResource::STATE_ABORTING, BackgroundResource::ACTION_NONE}},

    {{BackgroundResource::STATE_INITIALIZING, BackgroundResource::ACTION_ERROR},
     {BackgroundResource::STATE_INITIALIZING, BackgroundResource::ACTION_ERROR},
     {BackgroundResource::STATE_QUEUED, BackgroundResource::ACTION_QUEUE_BACKGROUND},
     {BackgroundResource::STATE_INITIALIZING, BackgroundResource::ACTION_ERROR},
     {BackgroundResource::STATE_INITIALIZING, BackgroundResource::ACTION_ERROR},
     {BackgroundResource::STATE_INITIALIZING, BackgroundResource::ACTION_NONE},
     {BackgroundResource::STATE_INITIALIZING, BackgroundResource::ACTION_ERROR},
     {BackgroundResource::STATE_FAILURE_PENDING, BackgroundResource::ACTION_PROCESS_FAILURE},
     {BackgroundResource::STATE_UNLOADED, BackgroundResource::ACTION_ABORT_NOW}},

    {{BackgroundResource::STATE_QUEUED, BackgroundResource::ACTION_ERROR},
     {BackgroundResource::STATE_QUEUED, BackgroundResource::ACTION_ERROR},
     {BackgroundResource::STATE_QUEUED, BackgroundResource::ACTION_ERROR},
     {BackgroundResource::STATE_LOADING, BackgroundResource::ACTION_BACKGROUND_LOAD},
     {BackgroundResource::STATE_QUEUED, BackgroundResource::ACTION_ERROR},
     {BackgroundResource::STATE_QUEUED, BackgroundResource::ACTION_NONE},
     {BackgroundResource::STATE_QUEUED, BackgroundResource::ACTION_ERROR},
     {BackgroundResource::STATE_FAILURE_PENDING, BackgroundResource::ACTION_PROCESS_FAILURE},
     {BackgroundResource::STATE_ABORTING, BackgroundResource::ACTION_NONE}},

    {{BackgroundResource::STATE_LOADING, BackgroundResource::ACTION_ERROR},
     {BackgroundResource::STATE_LOADING, BackgroundResource::ACTION_ERROR},
     {BackgroundResource::STATE_LOADING, BackgroundResource::ACTION_ERROR},
     {BackgroundResource::STATE_LOADING, BackgroundResource::ACTION_ERROR},
     {BackgroundResource::STATE_LOAD_COMPLETE, BackgroundResource::ACTION_SEND_LOAD_COMPLETE},
     {BackgroundResource::STATE_LOADING, BackgroundResource::ACTION_NONE},
     {BackgroundResource::STATE_LOADING, BackgroundResource::ACTION_ERROR},
     {BackgroundResource::STATE_FAILURE_PENDING, BackgroundResource::ACTION_PROCESS_FAILURE},
     {BackgroundResource::STATE_ABORTING, BackgroundResource::ACTION_NONE}},

    {{BackgroundResource::STATE_LOAD_COMPLETE, BackgroundResource::ACTION_ERROR},
     {BackgroundResource::STATE_LOAD_COMPLETE, BackgroundResource::ACTION_ERROR},
     {BackgroundResource::STATE_LOAD_COMPLETE, BackgroundResource::ACTION_ERROR},
     {BackgroundResource::STATE_LOAD_COMPLETE, BackgroundResource::ACTION_ERROR},
     {BackgroundResource::STATE_LOAD_COMPLETE, BackgroundResource::ACTION_ERROR},
     {BackgroundResource::STATE_LOADED, BackgroundResource::ACTION_LOAD_COMPLETE},
     {BackgroundResource::STATE_LOAD_COMPLETE, BackgroundResource::ACTION_ERROR},
     {BackgroundResource::STATE_FAILURE_PENDING, BackgroundResource::ACTION_PROCESS_FAILURE},
     {BackgroundResource::STATE_ABORTING, BackgroundResource::ACTION_NONE}},

    {{BackgroundResource::STATE_LOADED, BackgroundResource::ACTION_ERROR},
     {BackgroundResource::STATE_LOADED, BackgroundResource::ACTION_ERROR},
     {BackgroundResource::STATE_LOADED, BackgroundResource::ACTION_ERROR},
     {BackgroundResource::STATE_LOADED, BackgroundResource::ACTION_ERROR},
     {BackgroundResource::STATE_LOADED, BackgroundResource::ACTION_ERROR},
     {BackgroundResource::STATE_LOADED, BackgroundResource::ACTION_NONE},
     {BackgroundResource::STATE_UNLOADED, BackgroundResource::ACTION_UNLOAD_RESOURCE},
     {BackgroundResource::STATE_FAILURE_PENDING, BackgroundResource::ACTION_PROCESS_FAILURE},
     {BackgroundResource::STATE_UNLOADED, BackgroundResource::ACTION_ABORT_NOW}},
};

// the observer array grows by fives, as 0x1000f720 does: capacity = 5 * ((count + 1 + 4) / 5)
int GrowCapacity(int requested)
{
    return ((requested + 4) / 5) * 5;
}

}  // namespace

unsigned long BackgroundResource::mainThreadId = 0;

BackgroundResource::BackgroundResource(BackgroundLoadingThread* thread)
    : backgroundThread(thread),
      resource(nullptr),
      observerCount(0),
      observerCapacity(0),
      observers(nullptr),
      locked(false),
      deleteOnAbort(false),
      deleteOnFailure(false),
      state(STATE_UNLOADED)
{
    mainThreadId = Thread::GetCurrentId();
}

BackgroundResource::~BackgroundResource()
{
    if (resource) {
        if (locked) resource->Unlock();
        resource->RemoveObserver(this);
        resource->ReleaseReference();
        resource = nullptr;
    }
    observerCount = 0;
    if (observers) {
        Alloc::Delete(observers);
        observers = nullptr;
    }
}

void __cdecl BackgroundResource::DeleteSelf()
{
    delete this;
}

// The database overload is exported from a free helper with a unique name: the linker resolves a .def export of a
// symbol that also exists in Render3DCore_7c_orig.lib to an import thunk (see notes/agents/u11_statics.md).
bool __cdecl BackgroundResourceLoadResourceDatabase(BackgroundResource* self, const ResourceDatabase* db,
                                                    const PString& name)
{
    const Resource* const found = db->Get(name, 2);
    if (!found) return false;
    return self->LoadResource(found);
}

bool __cdecl BackgroundResource::LoadResource(const ResourceDatabase* db, const PString& name)
{
    return BackgroundResourceLoadResourceDatabase(this, db, name);
}

bool __cdecl BackgroundResource::LoadResource(const Resource* found)
{
    resource = const_cast<Resource*>(found);
    found->AddReference();
    found->AddObserver(this);
    if (state != STATE_UNLOADED) return true;
    return DoAction(INPUT_QUEUE_REQUEST) == STATE_RETURN_CONTINUE;
}

void __cdecl BackgroundResource::Abort(bool abort)
{
    deleteOnAbort = abort;
    DoAction(INPUT_ABORT);
}

void __cdecl BackgroundResource::AddObserver(BkgResObserver* observer, unsigned long flags)
{
    const int requested = observerCount + 1;
    if (requested > observerCapacity) {
        const int capacity = GrowCapacity(requested);
        ObserverEntry* const grown = static_cast<ObserverEntry*>(Alloc::New(capacity * sizeof(ObserverEntry)));
        if (observers) {
            std::memcpy(grown, observers, static_cast<size_t>(observerCount) * sizeof(ObserverEntry));
            Alloc::Delete(observers);
        }
        observers = grown;
        observerCapacity = capacity;
    }
    observers[observerCount].flags = flags;
    observers[observerCount].observer = observer;
    ++observerCount;
}

void __cdecl BackgroundResource::RemoveObserver(BkgResObserver* observer)
{
    for (int i = 0; i < observerCount; ++i) {
        if (observers[i].observer != observer) continue;
        --observerCount;
        std::memmove(&observers[i], &observers[i + 1],
                     static_cast<size_t>(observerCount - i) * sizeof(ObserverEntry));
        return;
    }
}

void __cdecl BackgroundResource::BackgroundLoadData()
{
    DoAction(INPUT_LOAD_DATA);
}

bool __cdecl BackgroundResource::CheckComplete()
{
    return DoAction(INPUT_CHECK_COMPLETE) == STATE_RETURN_COMPLETE;
}

const Resource* __cdecl BackgroundResource::GetResource() const
{
    return resource;
}

void __cdecl BackgroundResource::SetBackgroundThread(BackgroundLoadingThread* thread)
{
    backgroundThread = thread;
}

void __cdecl BackgroundResource::UnloadResource()
{
    DoAction(INPUT_UNLOAD);
}

void __cdecl BackgroundResource::OnLoad(const Resource*) const
{
    // the original overwrites its argument with 1 and tail-calls DoAction: the resource itself is not used
    const_cast<BackgroundResource*>(this)->DoAction(INPUT_RESOURCE_LOADED);
}

BackgroundLoadStatus __cdecl BackgroundResource::MainThreadInitialize()
{
    return BACKGROUND_OK;
}

BackgroundLoadStatus __cdecl BackgroundResource::BackgroundLoad()
{
    return BACKGROUND_OK;
}

BackgroundLoadStatus __cdecl BackgroundResource::MainThreadLoadComplete() const
{
    return BACKGROUND_LOADED;
}

BackgroundLoadStatus __cdecl BackgroundResource::MainThreadProcessFailure() const
{
    return BACKGROUND_LOADED;
}

void __cdecl BackgroundResource::QueueBackground()
{
    DoAction(INPUT_QUEUE_BACKGROUND);
}

void __cdecl BackgroundResource::LoadFailed()
{
    DoAction(INPUT_LOAD_FAILURE);
}

BackgroundResource::State __cdecl BackgroundResource::GetCurrentState() const
{
    return state;
}

void __cdecl BackgroundResource::Lock() const
{
    section.Lock(0xffffffffu);
}

void __cdecl BackgroundResource::Unlock() const
{
    section.Unlock();
}

BackgroundResource::StateReturn __cdecl BackgroundResource::CallCallbacks(unsigned long phase) const
{
    // the original ignores its argument and passes the resource to every callback (0x1000efe8: PUSH this)
    (void)phase;
    BackgroundResource* const resource = const_cast<BackgroundResource*>(this);
    for (int i = 0; i < observerCount; ++i) {
        const ObserverEntry& entry = observers[i];
        if (entry.flags & BkgResObserver::FLAG_STARTED) {
            if (!entry.observer->OnStarted(resource)) return STATE_RETURN_FAILED;
        }
        if (entry.flags & BkgResObserver::FLAG_BACKGROUND_LOADED) {
            if (entry.observer->OnBackgroundLoaded(resource)) return STATE_RETURN_FAILED;
        }
        if (entry.flags & BkgResObserver::FLAG_COMPLETED) {
            if (entry.observer->OnCompleted(resource)) return STATE_RETURN_FAILED;
        }
        if (entry.flags & BkgResObserver::FLAG_FAILED) {
            if (entry.observer->OnFailed(resource)) return STATE_RETURN_FAILED;
        }
    }
    return STATE_RETURN_CONTINUE;
}

BackgroundResource::StateReturn __cdecl BackgroundResource::DoAction(Input input)
{
    Lock();
    const LocalStateTable& entry = kStateTable[state][input];
    state = entry.next;
    StateReturn result = STATE_RETURN_ERROR;
    switch (entry.action) {
    case ACTION_NONE:
        result = STATE_RETURN_CONTINUE;
        break;
    case ACTION_PROCESS_FAILURE:
        result = ActionProcessFailure();
        Unlock();
        return result;
    case ACTION_NOTIFY_FAILURE:
        result = ActionNotifyFailure();
        Unlock();
        return result;
    case ACTION_QUEUE_REQUEST:
        result = ActionQueueRequest();
        Unlock();
        return result;
    case ACTION_MAIN_THREAD_INITIALIZE:
        result = ActionMainThreadInitialize();
        Unlock();
        return result;
    case ACTION_QUEUE_BACKGROUND:
        result = ActionQueueBackground();
        Unlock();
        return result;
    case ACTION_BACKGROUND_LOAD:
        Unlock();
        result = ActionBackgroundLoadData();
        return result;
    case ACTION_SEND_LOAD_COMPLETE:
        result = ActionSendLoadComplete();
        Unlock();
        return result;
    case ACTION_LOAD_COMPLETE:
        result = ActionLoadComplete();
        Unlock();
        return result;
    case ACTION_UNLOAD_RESOURCE:
        result = ActionUnloadResource();
        Unlock();
        return result;
    case ACTION_ABORT_NOW:
        Unlock();
        result = ActionAbortNow();
        return result;
    default:
        break;
    }
    Unlock();
    return result;
}

BackgroundResource::StateReturn __cdecl BackgroundResource::ActionProcessFailure()
{
    if (Thread::GetCurrentId() == mainThreadId) {
        DoAction(INPUT_CHECK_COMPLETE);
        return STATE_RETURN_FAILED;
    }
    backgroundThread->SignalComplete(this);
    return STATE_RETURN_FAILED;
}

BackgroundResource::StateReturn __cdecl BackgroundResource::ActionNotifyFailure()
{
    MainThreadProcessFailure();
    CallCallbacks(8);
    if (locked) {
        resource->Unlock();
        locked = false;
    }
    resource->RemoveObserver(this);
    resource->ReleaseReference();
    resource = nullptr;
    if (deleteOnFailure) backgroundThread->QueueForDelete(this);
    return STATE_RETURN_CONTINUE;
}

BackgroundResource::StateReturn __cdecl BackgroundResource::ActionQueueRequest()
{
    if (resource->Status() != Resource::STATUS_AVAILABLE) {
        resource->Cache(false);
        return STATE_RETURN_CONTINUE;
    }
    return DoAction(INPUT_RESOURCE_LOADED);
}

BackgroundResource::StateReturn __cdecl BackgroundResource::ActionMainThreadInitialize()
{
    resource->Lock();
    locked = true;
    const BackgroundLoadStatus status = MainThreadInitialize();
    if (status == BACKGROUND_FAILED) {
        LoadFailed();
        return STATE_RETURN_FAILED;
    }
    const StateReturn callbacks = CallCallbacks(1);
    if (callbacks != STATE_RETURN_CONTINUE) {
        LoadFailed();
        return callbacks;
    }
    DoAction(INPUT_QUEUE_BACKGROUND);
    return STATE_RETURN_CONTINUE;
}

BackgroundResource::StateReturn __cdecl BackgroundResource::ActionQueueBackground() const
{
    BackgroundLoadingThread::Result result = backgroundThread->QueueResource(const_cast<BackgroundResource*>(this));
    return result.code == 0 ? STATE_RETURN_CONTINUE : STATE_RETURN_FAILED;
}

BackgroundResource::StateReturn __cdecl BackgroundResource::ActionBackgroundLoadData()
{
    const BackgroundLoadStatus status = BackgroundLoad();
    if (status == BACKGROUND_FAILED) {
        LoadFailed();
        return STATE_RETURN_FAILED;
    }
    if (status == BACKGROUND_RETRY) return STATE_RETURN_RETRY;
    const StateReturn callbacks = CallCallbacks(2);
    if (callbacks != STATE_RETURN_CONTINUE) {
        LoadFailed();
        return callbacks;
    }
    return DoAction(INPUT_LOAD_COMPLETE);
}

BackgroundResource::StateReturn __cdecl BackgroundResource::ActionSendLoadComplete()
{
    backgroundThread->SignalComplete(this);
    return STATE_RETURN_CONTINUE;
}

BackgroundResource::StateReturn __cdecl BackgroundResource::ActionLoadComplete()
{
    const BackgroundLoadStatus status = MainThreadLoadComplete();
    if (status == BACKGROUND_FAILED) {
        LoadFailed();
        return STATE_RETURN_FAILED;
    }
    const StateReturn callbacks = CallCallbacks(4);
    if (callbacks != STATE_RETURN_CONTINUE) {
        LoadFailed();
        return callbacks;
    }
    return STATE_RETURN_COMPLETE;
}

BackgroundResource::StateReturn __cdecl BackgroundResource::ActionUnloadResource()
{
    if (locked) resource->Unlock();
    resource->RemoveObserver(this);
    resource->ReleaseReference();
    resource = nullptr;
    locked = false;
    return STATE_RETURN_CONTINUE;
}

BackgroundResource::StateReturn __cdecl BackgroundResource::ActionAbortNow()
{
    if (deleteOnAbort) backgroundThread->QueueForDelete(this);
    return STATE_RETURN_CONTINUE;
}

// the original's `default constructor closure' (0x1000e340): construct with no thread. Exported under the original
// name by the .def's alias (a __fastcall's object pointer arrives in ECX, like the __thiscall it stands in for).
void __fastcall BackgroundResourceDefaultClosure(void* self, void*)
{
    ::new (self) BackgroundResource(nullptr);
}

}  // namespace Jet

