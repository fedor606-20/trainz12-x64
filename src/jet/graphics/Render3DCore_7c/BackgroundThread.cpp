// Unit U11 of Render3DCore_7c: Jet::BackgroundLoadingThread, the worker that loads resources off the main thread,
// transcribed from the TS12 assembly (0x1000e420..0x1000ec8c, 0x10058b60). The layout is in
// Render3DCoreBackground.h.
//
// The two queues are binary heaps keyed by the watched resource's Resource::GetPriority: the higher priority comes
// out first (the original's COMISS/JBE comparisons, so a NaN priority never moves an item up). The complete queue is
// drained by CheckForComplete on the main thread, which runs the resource's "check complete" action. The delete list
// is the original's pooled std::list: 13 pointers per 0x40-byte block, the head block holding the first 13.
//
// QueueResource, DequeueResource and SignalComplete construct and destroy their SingleLock immediately, as the
// original does (0x1000e750, 0x1000e7d0, 0x1000e870); their synchronisation is the semaphore and the mutex the
// thread holds while it pops.
#include <jet/Render3DCoreBackground.h>

#include <new>

namespace Jet {

unsigned long BackgroundLoadingThread::mainThreadId = 0;

namespace {

enum { DELETE_BLOCK_ITEMS = 13 };

}  // namespace

BackgroundLoadingThread::BackgroundLoadingThread(int maxQueued, Thread::Priority priority)
    : queueCount(0),
      queueCapacity(maxQueued),
      queue(nullptr),
      completeCount(0),
      completeCapacity(maxQueued),
      complete(nullptr),
      deleteList(nullptr),
      thread(nullptr),
      current(nullptr),
      queued(0, 0x7fffffff, nullptr),
      done(false),
      threadId(0),
      mutex(false, nullptr)
{
    queue = static_cast<BackgroundResource**>(Alloc::New(static_cast<unsigned int>(maxQueued) * sizeof(*queue)));
    complete =
        static_cast<BackgroundResource**>(Alloc::New(static_cast<unsigned int>(maxQueued) * sizeof(*complete)));
    mainThreadId = Thread::GetCurrentId();
    Thread* const worker = new Thread(MainThread, this, false);
    thread = worker;
    worker->SetPriority(priority);
    worker->Resume();
}

BackgroundLoadingThread::~BackgroundLoadingThread()
{
    RemoveDeleteList();
    done = true;
    queued.Signal();
    if (thread) {
        delete thread;
        thread = nullptr;
    }
    completeCount = 0;
    if (complete) Alloc::Delete(complete);
    queueCount = 0;
    if (queue) Alloc::Delete(queue);
}

unsigned int __stdcall BackgroundLoadingThread::MainThread(void* self)
{
    return static_cast<BackgroundLoadingThread*>(self)->ProcessThread();
}

unsigned int __cdecl BackgroundLoadingThread::ProcessThread()
{
    threadId = Thread::GetCurrentId();
    SharedScratchPad* const scratch = SharedScratchPad::GetAccess();
    while (!thread->IsDone() && !done) {
        if (!queued.Lock(0xffffffffu)) continue;
        mutex.Lock(0xffffffffu);
        if (queueCount == 0) {
            mutex.Unlock();
            continue;
        }
        BackgroundResource* const resource = queue[0];
        --queueCount;
        queue[0] = queue[queueCount];
        SiftDown(queue, queueCount, 0);
        mutex.Unlock();
        current = resource;
        resource->DoAction(BackgroundResource::INPUT_LOAD_DATA);
        current = nullptr;
    }
    scratch->ReleaseReference();
    return 0;
}

unsigned long __cdecl BackgroundLoadingThread::GetThreadID() const
{
    return threadId;
}

void __cdecl BackgroundLoadingThread::SetThreadPriority(Thread::Priority priority)
{
    thread->SetPriority(priority);
}

BackgroundLoadingThread::Result __cdecl BackgroundLoadingThread::QueueResource(BackgroundResource* resource)
{
    {
        SingleLock lock(&mutex, true);
    }
    if (queueCount == queueCapacity) return Result(RESULT_QUEUE_FULL);
    queue[queueCount] = resource;
    SiftUp(queue, queueCount);
    ++queueCount;
    queued.Signal();
    return Result(RESULT_OK);
}

BackgroundLoadingThread::Result __cdecl BackgroundLoadingThread::DequeueResource(BackgroundResource* resource)
{
    {
        SingleLock lock(&mutex, true);
    }
    if (current == resource) return Result(RESULT_IS_CURRENT);
    for (int i = 0; i < queueCount; ++i) {
        if (queue[i] != resource) continue;
        RemoveAt(queue, queueCount, i);
        queued.Lock(0xffffffffu);
        return Result(RESULT_OK);
    }
    return Result(RESULT_NOT_QUEUED);
}

BackgroundLoadingThread::Result __cdecl BackgroundLoadingThread::SignalComplete(BackgroundResource* resource)
{
    {
        SingleLock lock(&mutex, true);
    }
    if (completeCount == completeCapacity) return Result(RESULT_QUEUE_FULL);
    complete[completeCount] = resource;
    SiftUp(complete, completeCount);
    ++completeCount;
    return Result(RESULT_OK);
}

void __cdecl BackgroundLoadingThread::CheckForComplete()
{
    RemoveDeleteList();
    mutex.Lock(0xffffffffu);
    while (completeCount != 0) {
        BackgroundResource* const resource = complete[0];
        --completeCount;
        complete[0] = complete[completeCount];
        SiftDown(complete, completeCount, 0);
        mutex.Unlock();
        resource->DoAction(BackgroundResource::INPUT_CHECK_COMPLETE);
        mutex.Lock(0xffffffffu);
    }
    mutex.Unlock();
}

void __cdecl BackgroundLoadingThread::QueueForDelete(BackgroundResource* resource)
{
    if (!deleteList) {
        DeleteBlock* const first = static_cast<DeleteBlock*>(Alloc::New(sizeof(DeleteBlock)));
        first->next = first;
        first->prev = first;
        first->count = 0;
        deleteList = first;
    }
    DeleteBlock* block = deleteList->prev;
    if (block->count == DELETE_BLOCK_ITEMS) {
        DeleteBlock* const grown = static_cast<DeleteBlock*>(Alloc::New(sizeof(DeleteBlock)));
        grown->next = deleteList;
        grown->prev = block;
        block->next = grown;
        deleteList->prev = grown;
        block = grown;
    }
    block->items[block->count] = resource;
    ++block->count;
}

void BackgroundLoadingThread::RemoveDeleteList()
{
    DeleteBlock* const head = deleteList;
    if (!head) return;
    DeleteBlock* block = head;
    do {
        for (unsigned int i = 0; i < block->count; ++i) block->items[i]->DeleteSelf();
        block = block->next;
    } while (block != head);
    block = head->next;
    while (block != head) {
        DeleteBlock* const next = block->next;
        Alloc::Delete(block);
        block = next;
    }
    Alloc::Delete(head);
    deleteList = nullptr;
}

// the original's 0x1000eb90: move data[index] up while it is strictly above its parent
void BackgroundLoadingThread::SiftUp(BackgroundResource** data, int index)
{
    if (index == 0) return;
    BackgroundResource* const item = data[index];
    int parent = (index - 1) / 2;
    if (!(item->resource->GetPriority() > data[parent]->resource->GetPriority())) return;
    for (;;) {
        data[index] = data[parent];
        index = parent;
        if (index == 0) break;
        parent = (index - 1) / 2;
        if (!(item->resource->GetPriority() > data[parent]->resource->GetPriority())) break;
    }
    data[index] = item;
}

// the original's 0x1000ec00: move data[index] down while the greater child is not below it
void BackgroundLoadingThread::SiftDown(BackgroundResource** data, int count, int index)
{
    BackgroundResource* const item = data[index];
    if (index >= count / 2) {
        data[index] = item;
        return;
    }
    int child = 2 * index + 1;
    for (;;) {
        if (child < count - 1) {
            if (!(data[child]->resource->GetPriority() > data[child + 1]->resource->GetPriority())) ++child;
        }
        if (item->resource->GetPriority() > data[child]->resource->GetPriority()) break;
        data[index] = data[child];
        index = child;
        if (index >= count / 2) break;
        child = 2 * index + 1;
    }
    data[index] = item;
}

// the original's 0x1000eb40: move the last item into the hole, then restore the heap
void BackgroundLoadingThread::RemoveAt(BackgroundResource** data, int& count, int index)
{
    BackgroundResource* const removed = data[index];
    BackgroundResource* const last = data[count - 1];
    const float removedPriority = removed->resource->GetPriority();
    const float lastPriority = last->resource->GetPriority();
    data[index] = last;
    --count;
    if (removedPriority > lastPriority) {
        SiftDown(data, count, index);
    } else {
        SiftUp(data, index);
    }
}

}  // namespace Jet
