// Unit U9b of Render3DCore_7c: AttachmentPoint and AttachmentObserver, transcribed from the TS12 assembly.
//
// AttachmentPoint is U9a's Particle plus a host-name PString at +0x12c and a FastDynamicArray<AttachmentObserver*,13>
// at +0x130. Its own virtual slots are 48 AddObserver, 49 RemoveObserver, 50 ClearObservers and 51 NotifyObservers.
// The observer list is the same chunked ring the FastDynamicArray template in FastDynamicArray.h implements; the
// engine's other modules walk it by hand, so the layout (one Chunk* head) is the contract.
#include <jet/Render3DCoreScene.h>

#include <cstring>
#include <string>

namespace Jet {

namespace {

// fld/fstp of one float: a signalling NaN gets its quiet bit, as in the original's member-wise copies.
float X87Copy(float f)
{
    __asm {
        fld dword ptr f
        fstp dword ptr f
    }
    return f;
}

}  // namespace

// ---- AttachmentObserver --------------------------------------------------------------------------------------

AttachmentObserver::AttachmentObserver() : point(nullptr) {}

AttachmentObserver::AttachmentObserver(AttachmentPoint* p) : point(p)
{
    if (p) p->AddObserver(this);
}

AttachmentObserver::AttachmentObserver(const AttachmentObserver& o) : point(o.point) {}

AttachmentObserver& AttachmentObserver::operator=(const AttachmentObserver& o)
{
    point = o.point;
    return *this;
}

AttachmentObserver::~AttachmentObserver()
{
    if (point) point->RemoveObserver(this);
}

// the original's slot 1/2/3 are one shared "xor eax,eax; ret" stub
void __cdecl AttachmentObserver::Added(Particle*) {}
void __cdecl AttachmentObserver::Removed(Particle*) {}
void __cdecl AttachmentObserver::VirtualShutDown() {}

// 0x97d0: clears the point, then calls the observer's own slot 3
void AttachmentObserver::ShutDown()
{
    point = nullptr;
    VirtualShutDown();
}

// ---- AttachmentPoint -----------------------------------------------------------------------------------------

AttachmentPoint::AttachmentPoint() : Particle(), hostName(), observers() {}

AttachmentPoint::AttachmentPoint(const AttachmentPoint& o) : Particle(o), hostName(o.hostName), observers(o.observers)
{
}

AttachmentPoint::AttachmentPoint(const PString& particleName, const PString& host)
    : Particle(), hostName(), observers()
{
    Particle::SetName(particleName);
    hostName = host;
}

AttachmentPoint::AttachmentPoint(const PString& particleName, const PString& host, const Vector3& position,
                                 const Quaternion& orientation)
    : Particle(), hostName(), observers()
{
    Particle::SetName(particleName);
    hostName = host;
    // the original writes the particle's private fields directly, through fld/fstp
    float* const pos = reinterpret_cast<float*>(reinterpret_cast<char*>(this) + 0x2c);
    pos[0] = X87Copy(position.x);
    pos[1] = X87Copy(position.y);
    pos[2] = X87Copy(position.z);
    float* const rot = reinterpret_cast<float*>(reinterpret_cast<char*>(this) + 0x38);
    rot[0] = X87Copy(orientation.x);
    rot[1] = X87Copy(orientation.y);
    rot[2] = X87Copy(orientation.z);
    rot[3] = X87Copy(orientation.w);
}

AttachmentPoint::~AttachmentPoint()
{
    ClearObservers();
}

AttachmentPoint& AttachmentPoint::operator=(const AttachmentPoint& o)
{
    Particle::operator=(o);
    hostName = o.hostName;
    observers = o.observers;
    return *this;
}

const PString& __cdecl AttachmentPoint::GetHostName() const
{
    return hostName;
}

void __cdecl AttachmentPoint::SetHostName(const PString& host)
{
    hostName = host;
}

// 0x94e0: the base Add plus the "added" notification
void __cdecl AttachmentPoint::Add(Particle* child)
{
    Particle::Add(child);
    NotifyObservers(true, child);
}

// 0x9510: an empty name adds directly, otherwise the child goes to the named descendant
bool __cdecl AttachmentPoint::Add(const PString& containerName, Particle* child)
{
    if (!containerName.node) {
        Add(child);
    } else {
        Particle* const container = FindByName(containerName);
        if (!container) return false;
        container->Add(child);
    }
    NotifyObservers(true, child);
    return true;
}

void __cdecl AttachmentPoint::Push(Particle* child)
{
    Particle::Push(child);
    NotifyObservers(true, child);
}

AttachmentPoint::Iterator __cdecl AttachmentPoint::InsertBefore(Iterator& target, Particle* child)
{
    Particle::InsertBefore(target, child);
    NotifyObservers(true, child);
    return Iterator(child);
}

AttachmentPoint::Iterator __cdecl AttachmentPoint::InsertAfter(Iterator& target, Particle* child)
{
    Particle::InsertAfter(target, child);
    NotifyObservers(true, child);
    return Iterator(child);
}

Particle* __cdecl AttachmentPoint::Remove()
{
    Particle* const removed = Particle::Remove();
    if (removed) NotifyObservers(false, removed);
    return removed;
}

bool __cdecl AttachmentPoint::Remove(Particle* child)
{
    const bool removed = Particle::Remove(child);
    if (removed) NotifyObservers(false, child);
    return removed;
}

AttachmentPoint::Iterator __cdecl AttachmentPoint::Remove(Iterator& target)
{
    // the original removes the tail (not the iterator's node) and notifies with the iterator's node; the returned
    // iterator is the removed tail. Copied verbatim, odd as it is.
    Iterator result;
    if (_count) {
        Particle* const node = _tail;
        Iterator tail(node);
        Particle::Remove(tail);
        result.node = node;
    } else {
        result.node = nullptr;
    }
    NotifyObservers(false, target.node);
    return result;
}

Particle* __cdecl AttachmentPoint::Pop()
{
    Particle* const removed = Particle::Pop();
    if (removed) NotifyObservers(false, removed);
    return removed;
}

void AttachmentPoint::InitInstance(AttachmentPoint& instance) const
{
    Particle::InitInstance(instance);
    instance.hostName = hostName;
}

Particle* __cdecl AttachmentPoint::CreateInstance() const
{
    AttachmentPoint* instance = static_cast<AttachmentPoint*>(Alloc::New(sizeof(AttachmentPoint)));
    if (instance) {
        ::new (instance) AttachmentPoint();
        InitInstance(*instance);
    }
    return instance;
}

int __cdecl AttachmentPoint::GetID() const
{
    return 4;
}

bool __cdecl AttachmentPoint::IsOfType(int id) const
{
    return id == 4 || id == 1;
}

// 0xb8e0: bind both ways, then append to the observer ring
void __cdecl AttachmentPoint::AddObserver(AttachmentObserver* observer)
{
    observer->point = this;
    observers.Add(observer);
}

// 0xb900: shut the observer down, clear the back pointer, unlink it
bool __cdecl AttachmentPoint::RemoveObserver(AttachmentObserver* observer)
{
    const FastDynamicArray<AttachmentObserver*, 13>::Iterator found = observers.Find(observer);
    if (found != observers.End()) {
        (*found)->VirtualShutDown();
        (*found)->point = nullptr;
        observers.Remove(found);
        return true;
    }
    return false;
}

// 0xb960: shut every observer down, then empty the ring
void __cdecl AttachmentPoint::ClearObservers()
{
    for (FastDynamicArray<AttachmentObserver*, 13>::Iterator i = observers.First(); i != observers.End(); ++i)
        (*i)->VirtualShutDown();
    observers.RemoveAll();
}

void __cdecl AttachmentPoint::NotifyObservers(bool added, Particle* particle)
{
    if (added) {
        for (FastDynamicArray<AttachmentObserver*, 13>::Iterator i = observers.First(); i != observers.End(); ++i)
            (*i)->Added(particle);
    } else {
        for (FastDynamicArray<AttachmentObserver*, 13>::Iterator i = observers.First(); i != observers.End(); ++i)
            (*i)->Removed(particle);
    }
}

// 0xb5b0: first = the part after '/', second = 'b' + the head's characters after the first one. Without a slash,
// first = the whole name and second = 'b' + name[1..]. (The concatenation helper appends the literal "b" to the
// substring; the decompiler printed its operands in the other order.)
void __cdecl AttachmentPoint::GetSubNames(const PString& name, PString& first, PString& second)
{
    const PStringCache::Node* const node = name.node;
    if (!node) {
        first = name;
        return;
    }
    const int slash = PString::Cache()->Find(const_cast<PStringCache::Node*>(node), '/', 0);
    if (slash >= 0) {
        // out1 = the tail after '/', out2 = 'b' + the head's characters after its first one
        const unsigned int tailStart = static_cast<unsigned int>(slash) + 1;
        PString tail;
        if (tailStart <= node->length) {
            tail.node = PString::Cache()->Create(reinterpret_cast<const unsigned char*>(node->text) + tailStart,
                                                node->length - tailStart);
        }
        first = tail;

        const int headCount = slash >= 1 ? slash - 1 : static_cast<int>(node->length) - 1;
        std::string head = "b";
        if (headCount > 0) head.append(node->text + 1, static_cast<size_t>(headCount));
        PString headString;
        headString.node = PString::Cache()->Create(
            reinterpret_cast<const unsigned char*>(head.c_str()), static_cast<unsigned int>(head.size()));
        second = headString;
        return;
    }
    first = name;
    if (!node->length) return;
    std::string part = "b";
    if (node->length > 1) part.append(node->text + 1, node->length - 1);
    PStringCache::Node* const secondNode =
        PString::Cache()->Create(reinterpret_cast<const unsigned char*>(part.c_str()),
                                 static_cast<unsigned int>(part.size()));
    PString secondTemp;
    secondTemp.node = secondNode;
    second = secondTemp;
}

}  // namespace Jet
