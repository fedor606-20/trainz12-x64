// Unit U9a of Render3DCore_7c: Jet::Particle and the ITree<Particle> helpers, transcribed from the TS12 assembly.
//
// The object is 0x12c bytes; see Render3DCoreParticle.h for the offsets. Most members are __cdecl (this on the stack),
// constructors/assignment are __thiscall and the virtuals are __cdecl as their U** names and the call sites show.
// The embedded ParticleAxisAlignedBoundingBox is U3's class: its constructors, dirty-flag recalculation and virtual
// slots are still the original's, called through Render3DCore_7c_orig.dll. Float expressions keep the assembly's
// operand order (SSE for the vector maths, x87 for the member-wise copies that quiet a signalling NaN).
#include <jet/Render3DCoreBounds.h>
#include <jet/Render3DCoreParticle.h>

#include <windows.h>

#include <cstddef>
#include <cstring>
#include <new>

namespace Jet {

namespace {

using uint = unsigned int;

// addss/subss/mulss/divss with the original's destination (first operand) order.
float fadd(float a, float b)
{
    __asm {
        movss xmm0, a
        addss xmm0, b
        movss a, xmm0
    }
    return a;
}

float fsub(float a, float b)
{
    __asm {
        movss xmm0, a
        subss xmm0, b
        movss a, xmm0
    }
    return a;
}

float fmul(float a, float b)
{
    __asm {
        movss xmm0, a
        mulss xmm0, b
        movss a, xmm0
    }
    return a;
}

float fdiv(float a, float b)
{
    __asm {
        movss xmm0, a
        divss xmm0, b
        movss a, xmm0
    }
    return a;
}

// 0.0f - x (the original writes its negations as subss from zero)
float fneg(float x)
{
    return fsub(0.0f, x);
}

// a float through fld/fstp: a signalling NaN gets its quiet bit, no other bit changes
float X87Copy(float f)
{
    unsigned int u;
    std::memcpy(&u, &f, 4);
    if ((u & 0x7f800000u) == 0x7f800000u && (u & 0x007fffffu) && !(u & 0x00400000u))
        u |= 0x00400000u;
    std::memcpy(&f, &u, 4);
    return f;
}

void CopyX87(float& d, float s)
{
    d = X87Copy(s);
}

HMODULE OrigModule()
{
    static HMODULE module = [] {
        HMODULE m = GetModuleHandleA("Render3DCore_7c_orig.dll");
        return m ? m : LoadLibraryA("Render3DCore_7c_orig.dll");
    }();
    return module;
}

template <class F>
F OrigProc(const char* name)
{
    HMODULE m = OrigModule();
    return m ? reinterpret_cast<F>(GetProcAddress(m, name)) : nullptr;
}

// ---- the embedded ParticleBoundingVolume (U3) ---------------------------------------------------------------

// The bounding volume is U3's class; its constructors, assignment and change notifications are local bodies.
void BvConstruct(void* bv, Particle* host)
{
    ::new (bv) ParticleBoundingVolume(host);
}

void BvConstructCopy(void* bv, const void* source)
{
    ::new (bv) ParticleBoundingVolume(*static_cast<const ParticleBoundingVolume*>(source));
}

void BvAssign(void* bv, const void* source)
{
    *static_cast<ParticleBoundingVolume*>(bv) = *static_cast<const ParticleBoundingVolume*>(source);
}

void BvRecalculateGlobalPosition(void* bv)
{
    BoundsAccess::RecalculateGlobalPosition(static_cast<ParticleBoundingVolume*>(bv));
}

void BvRecalculateGlobalOrientation(void* bv)
{
    BoundsAccess::RecalculateGlobalOrientation(static_cast<ParticleBoundingVolume*>(bv));
}

void BvRecalculateSphereGlobalCenter(void* bv)
{
    BoundsAccess::RecalculateSphereGlobalCenter(static_cast<ParticleBoundingVolume*>(bv));
}

void BvSetPositionChanged(void* bv, const Vector3& oldPosition, const Vector3& newPosition)
{
    static_cast<ParticleBoundingVolume*>(bv)->SetPositionChanged(oldPosition, newPosition);
}

void BvSetOrientationChanged(void* bv, const Quaternion& oldOrientation, const Quaternion& newOrientation)
{
    static_cast<ParticleBoundingVolume*>(bv)->SetOrientationChanged(oldOrientation, newOrientation);
}

void BvSetPositionOrientationChanged(void* bv, const Vector3& oldPosition, const Vector3& newPosition,
                                     const Quaternion& oldOrientation, const Quaternion& newOrientation)
{
    static_cast<ParticleBoundingVolume*>(bv)->SetPositionOrientationChanged(oldPosition, newPosition, oldOrientation,
                                                                             newOrientation);
}

void* AabbVftable()
{
    static void* table = [] {
        HMODULE m = OrigModule();
        return m ? GetProcAddress(m, "??_7ParticleAxisAlignedBoundingBox@Jet@@6B@") : nullptr;
    }();
    return table;
}

void* BvBaseVftable()
{
    static void* table = [] {
        HMODULE m = OrigModule();
        return m ? GetProcAddress(m, "??_7ParticleBoundingVolume@Jet@@6B@") : nullptr;
    }();
    return table;
}

// the bounding volume's virtual slots are __cdecl with `this` as the first argument
inline void** BvTable(void* bv)
{
    return *reinterpret_cast<void***>(bv);
}

void BvOnChildAdded(void* bv, Particle* child)
{
    using Fn = void(__cdecl*)(void*, Particle*);
    reinterpret_cast<Fn>(BvTable(bv)[0x18 / 4])(bv, child);
}

void BvOnChildRemoved(void* bv, Particle* child)
{
    using Fn = void(__cdecl*)(void*, Particle*);
    reinterpret_cast<Fn>(BvTable(bv)[0x14 / 4])(bv, child);
}

void BvRecalculateSphereVolume(void* bv)
{
    using Fn = void(__cdecl*)(void*);
    reinterpret_cast<Fn>(BvTable(bv)[0x40 / 4])(bv);
}

void BvRecalculateAxisAlignedBoxVolume(void* bv)
{
    using Fn = void(__cdecl*)(void*);
    reinterpret_cast<Fn>(BvTable(bv)[0x44 / 4])(bv);
}

void BvRecalculateOrientedBoxVolume(void* bv)
{
    using Fn = void(__cdecl*)(void*);
    reinterpret_cast<Fn>(BvTable(bv)[0x48 / 4])(bv);
}

void* BvCreateInstance(void* bv, Particle* host)
{
    using Fn = void*(__cdecl*)(void*, Particle*);
    return reinterpret_cast<Fn>(BvTable(bv)[0x34 / 4])(bv, host);
}

void BvDelete(void* bv)
{
    using Fn = void(__thiscall*)(void*, int);
    reinterpret_cast<Fn>(BvTable(bv)[0])(bv, 1);
}

bool BvIsWithinView(void* bv, const ViewPoint& view)
{
    using Fn = bool(__cdecl*)(void*, const ViewPoint*);
    return reinterpret_cast<Fn>(BvTable(bv)[8 / 4])(bv, &view);
}

// U1c's OrientedBox owns both transforms. The local bodies differ from the original only in NaN payload bits.
void TransformBox(OrientedBox& box, const Matrix3& m, const Vector3& t)
{
    box.Transform(m, t);
}

void InverseTransformBox(OrientedBox& box, const Matrix3& m, const Vector3& t)
{
    box.InverseTransform(m, t);
}

// ---- the original's small maths helpers (0x1004be00, 0x1004fe60, 0x1001af00, 0x1001ab30) ---------------------

// 0x1004be00: out = quaternion q applied to vector v
void RotateByQuaternion(float* out, const float* v, const float* q)
{
    const float fVar1 = q[1];
    const float fVar9 = fmul(q[2], 2.0f);
    const float fVar10 = fmul(fVar9, q[2]);
    const float fVar8 = fmul(fVar1, 2.0f);
    const float fVar5 = fmul(q[0], fVar8);
    const float fVar13 = fmul(q[3], fVar8);
    const float fVar7 = fmul(q[0], fVar9);
    const float fVar6 = fmul(q[3], fVar9);
    const float fVar2 = q[0];
    const float fVar12 = q[3];
    const float fVar11 = fmul(fVar2, 2.0f);

    out[0] = fadd(fadd(fmul(fsub(1.0f, fadd(fmul(fVar8, fVar1), fVar10)), v[0]), fmul(fadd(fVar7, fVar13), v[2])),
                 fmul(fsub(fVar5, fVar6), v[1]));
    const float fVar12b = fmul(fVar11, fVar12);
    const float fVar11b = fmul(fVar11, fVar2);
    const float fVar2b = v[1];
    const float fVar3 = v[2];
    const float fVar4 = v[0];
    out[1] = fadd(fadd(fmul(fsub(1.0f, fadd(fVar11b, fVar10)), fVar2b), fmul(fsub(fmul(fVar1, fVar9), fVar12b), fVar3)),
                 fmul(fadd(fVar6, fVar5), fVar4));
    out[2] = fadd(fadd(fmul(fsub(1.0f, fadd(fVar11b, fmul(fVar8, fVar1))), fVar3), fmul(fadd(fVar12b, fmul(fVar1, fVar9)), fVar2b)),
                 fmul(fsub(fVar7, fVar13), fVar4));
}

// 0x1004fe60: out = a * b
void MultiplyQuaternions(float* out, const float* a, const float* b)
{
    out[0] = fadd(fadd(fsub(fmul(b[1], a[2]), fmul(b[2], a[1])), fmul(a[0], b[3])), fmul(b[0], a[3]));
    out[1] = fadd(fadd(fsub(fmul(a[0], b[2]), fmul(b[0], a[2])), fmul(a[1], b[3])), fmul(a[3], b[1]));
    out[2] = fadd(fadd(fsub(fmul(b[0], a[1]), fmul(a[0], b[1])), fmul(a[2], b[3])), fmul(b[2], a[3]));
    out[3] = fsub(fmul(b[3], a[3]), fadd(fadd(fmul(a[1], b[1]), fmul(a[2], b[2])), fmul(a[0], b[0])));
}

// 0x1001af00: a row-major 4x4 rotation matrix with translation left at 0 (the caller stores position at +0x30)
void QuaternionToMatrix(float* m, const float* q)
{
    const float fVar1 = q[0];
    const float fVar2 = q[1];
    const float fVar4 = fmul(q[2], 2.0f);
    const float fVar7 = fmul(fVar2, 2.0f);
    const float fVar5 = fmul(fmul(fVar1, fVar1), 2.0f);
    const float fVar8 = fmul(q[2], fVar4);
    const float fVar3 = q[3];
    const float fVar6 = fmul(fmul(fVar3, fVar1), 2.0f);

    m[0] = fsub(1.0f, fadd(fVar8, fmul(fVar2, fVar7)));
    m[1] = fadd(fmul(fVar3, fVar4), fmul(fVar1, fVar7));
    m[4] = fsub(fmul(fVar1, fVar7), fmul(fVar3, fVar4));
    m[2] = fsub(fmul(fVar1, fVar4), fmul(fVar3, fVar7));
    m[5] = fsub(1.0f, fadd(fVar8, fVar5));
    m[9] = fsub(fmul(fVar2, fVar4), fVar6);
    m[6] = fadd(fVar6, fmul(fVar2, fVar4));
    m[10] = fsub(1.0f, fadd(fmul(fVar2, fVar7), fVar5));
    m[8] = fadd(fmul(fVar3, fVar7), fmul(fVar1, fVar4));
    m[12] = 0.0f;
    m[13] = 0.0f;
    m[14] = 0.0f;
    m[15] = 1.0f;
    m[3] = 0.0f;
    m[7] = 0.0f;
    m[11] = 0.0f;
}

// the OBB matrix build of 0x4eb70 / 0x4f320: same values as the 4x4 helper, but the products are grouped and
// ordered as that inlined code (this matters for NaN payloads)
void QuaternionToMatrix3(float* m, const float* q)
{
    const float qz2 = fmul(q[2], 2.0f);
    const float qy2 = fmul(q[1], 2.0f);
    const float qx2 = fmul(q[0], 2.0f);
    const float qzsq = fmul(q[2], qz2);
    const float qysq = fmul(q[1], qy2);
    const float qxsq = fmul(q[0], qx2);
    const float xy = fmul(q[0], qy2);
    const float wz = fmul(q[3], qz2);
    const float wy = fmul(q[3], qy2);
    const float wx = fmul(q[3], qx2);
    const float xz = fmul(q[0], qz2);
    const float yz = fmul(q[1], qz2);

    m[0] = fsub(1.0f, fadd(qzsq, qysq));
    m[1] = fadd(wz, xy);
    m[2] = fsub(xz, wy);
    m[3] = fsub(xy, wz);
    m[4] = fsub(1.0f, fadd(qzsq, qxsq));
    m[5] = fadd(wx, yz);
    m[6] = fadd(wy, xz);
    m[7] = fsub(yz, wx);
    m[8] = fsub(1.0f, fadd(qysq, qxsq));
}

// 0x1001ab30: result (stored into b) = a * b, row-major, additions in the original's order
void MultiplyMatrices(float* b, const float* a)
{
    float v[16];
    std::memcpy(v, b, sizeof v);
    float p[16];
    p[0] = fadd(fadd(fadd(fmul(a[1], v[4]), fmul(a[3], v[12])), fmul(a[2], v[8])), fmul(a[0], v[0]));
    p[1] = fadd(fadd(fadd(fmul(a[1], v[5]), fmul(a[3], v[13])), fmul(a[2], v[9])), fmul(a[0], v[1]));
    p[2] = fadd(fadd(fadd(fmul(a[1], v[6]), fmul(a[3], v[14])), fmul(a[2], v[10])), fmul(a[0], v[2]));
    p[3] = fadd(fadd(fadd(fmul(a[1], v[7]), fmul(a[3], v[15])), fmul(a[2], v[11])), fmul(a[0], v[3]));
    p[4] = fadd(fadd(fadd(fmul(a[7], v[12]), fmul(a[6], v[8])), fmul(a[4], v[0])), fmul(a[5], v[4]));
    p[5] = fadd(fadd(fadd(fmul(a[7], v[13]), fmul(a[6], v[9])), fmul(a[4], v[1])), fmul(a[5], v[5]));
    p[6] = fadd(fadd(fadd(fmul(a[7], v[14]), fmul(a[6], v[10])), fmul(a[4], v[2])), fmul(a[5], v[6]));
    p[7] = fadd(fadd(fadd(fmul(a[7], v[15]), fmul(a[6], v[11])), fmul(a[4], v[3])), fmul(a[5], v[7]));
    p[8] = fadd(fadd(fadd(fmul(a[11], v[12]), fmul(a[10], v[8])), fmul(a[8], v[0])), fmul(a[9], v[4]));
    p[9] = fadd(fadd(fadd(fmul(a[11], v[13]), fmul(a[10], v[9])), fmul(a[8], v[1])), fmul(a[9], v[5]));
    p[10] = fadd(fadd(fadd(fmul(a[11], v[14]), fmul(a[10], v[10])), fmul(a[8], v[2])), fmul(a[9], v[6]));
    p[11] = fadd(fadd(fadd(fmul(a[11], v[15]), fmul(a[10], v[11])), fmul(a[8], v[3])), fmul(a[9], v[7]));
    p[12] = fadd(fadd(fadd(fmul(a[15], v[12]), fmul(a[14], v[8])), fmul(a[12], v[0])), fmul(a[13], v[4]));
    p[13] = fadd(fadd(fadd(fmul(a[15], v[13]), fmul(a[14], v[9])), fmul(a[12], v[1])), fmul(a[13], v[5]));
    p[14] = fadd(fadd(fadd(fmul(a[15], v[14]), fmul(a[14], v[10])), fmul(a[12], v[2])), fmul(a[13], v[6]));
    p[15] = fadd(fadd(fadd(fmul(a[15], v[15]), fmul(a[14], v[11])), fmul(a[12], v[3])), fmul(a[13], v[7]));
    std::memcpy(b, p, sizeof p);
}

// 0x10008930: Jet::ITree<Particle>::IRemove
void TreeRemove(ITree<Particle>& tree, Particle* node)
{
    if (tree._head == node) {
        Particle* next = node->_next;
        tree._head = next;
        if (next) next->_prev = nullptr;
    } else {
        node->_prev->_next = node->_next;
    }
    if (tree._tail == node) {
        Particle* prev = node->_prev;
        tree._tail = prev;
        if (prev) {
            prev->_next = nullptr;
            node->_prev = nullptr;
            node->_next = nullptr;
            node->_parent = nullptr;
            --tree._count;
            return;
        }
    } else {
        node->_next->_prev = node->_prev;
    }
    node->_prev = nullptr;
    node->_next = nullptr;
    node->_parent = nullptr;
    --tree._count;
}

float* BvPosition(void* bv)
{
    return reinterpret_cast<float*>(static_cast<char*>(bv) + 0x7c);
}

float* BvOrientation(void* bv)
{
    return reinterpret_cast<float*>(static_cast<char*>(bv) + 0x88);
}

}  // namespace

// ==== 0x1004def0 Particle::Particle()
Particle::Particle()
{
    // the base constructors zero visited/lastCulledFrame and the six tree links; the original writes them again
    cellNode = nullptr;
    name = PString();
    flags |= 3;  // the original only ORs the two low bits, whatever the padding held

    BvConstruct(volumeStore, this);
    *reinterpret_cast<void**>(volumeStore) = AabbVftable();

    AxisAlignedBox empty;
    empty.min = Vector3{0.0f, 0.0f, 0.0f};
    empty.max = Vector3{0.0f, 0.0f, 0.0f};
    new (&orientedBox) OrientedBox(empty);

    *reinterpret_cast<int*>(volumeStore + 0x9c) = 1;
    orientation = Quaternion{0.0f, 0.0f, 0.0f, 1.0f};
    position = Vector3{0.0f, 0.0f, 0.0f};
    boundingVolume = reinterpret_cast<ParticleBoundingVolume*>(volumeStore);
}

// ==== 0x10006d80 Particle::Particle(const Particle&)
Particle::Particle(const Particle& o) : Element(o), ITree<Particle>()
{
    cellNode = o.cellNode;
    name = o.name;
    position = o.position;
    orientation = o.orientation;
    flags = static_cast<unsigned char>((flags ^ ((flags ^ o.flags) & 1)));
    flags = static_cast<unsigned char>((o.flags ^ flags) & 2 ^ flags);
    boundingVolume = o.boundingVolume;  // the original copies the source pointer, it is not re-pointed here

    BvConstructCopy(volumeStore, o.volumeStore);
    *reinterpret_cast<void**>(volumeStore) = AabbVftable();
    new (&orientedBox) OrientedBox(o.orientedBox);
}

// ==== 0x4dfb0 Particle::~Particle()
Particle::~Particle()
{
    DeleteAll();

    if (_parent) _parent->Remove(this);

    ParticleBoundingVolume* const embedded = reinterpret_cast<ParticleBoundingVolume*>(volumeStore);
    if (boundingVolume != embedded) {
        if (boundingVolume) BvDelete(boundingVolume);
        boundingVolume = nullptr;
    }
    *reinterpret_cast<void**>(volumeStore) = BvBaseVftable();

    ITree<Particle>::DeleteAll();  // anything left (normally nothing) goes through the virtual destructor
}

// ==== 0x6ec0 Particle::operator=(const Particle&)
Particle& Particle::operator=(const Particle& o)
{
    Element::operator=(o);
    _count = o._count;
    _parent = o._parent;
    _head = o._head;
    _tail = o._tail;
    _next = o._next;
    _prev = o._prev;
    cellNode = o.cellNode;
    name = o.name;
    position = o.position;
    orientation = o.orientation;
    flags = static_cast<unsigned char>((flags ^ ((flags ^ o.flags) & 1)));
    flags = static_cast<unsigned char>((o.flags ^ flags) & 2 ^ flags);
    boundingVolume = o.boundingVolume;
    BvAssign(volumeStore, o.volumeStore);
    orientedBox = o.orientedBox;
    return *this;
}

// ==== 0x7020 Particle::CreateInstance() const
Particle* Particle::CreateInstance() const
{
    Particle* instance = static_cast<Particle*>(Alloc::New(sizeof(Particle)));
    if (instance) {
        ::new (instance) Particle();
        InitInstance(*instance);
    }
    return instance;
}

// ==== 0x7070 Particle::GetID() const
int Particle::GetID() const
{
    return 1;
}

// ==== 0x7080 Particle::IsOfType(int) const
bool Particle::IsOfType(int id) const
{
    return id == 1;
}

// ==== 0x7090 Particle::GetName() const
const PString& Particle::GetName() const
{
    return name;
}

// ==== 0x70a0 Particle::SetName(const PString&)
void Particle::SetName(const PString& value)
{
    name = value;
}

// ==== 0x7100 Particle::GetParent()
Particle* Particle::GetParent()
{
    return _parent;
}

const Particle* Particle::GetParent() const
{
    return _parent;
}

// 0x7110 Particle::GetGlobalPosition() const
const Vector3& Particle::GetGlobalPosition() const
{
    void* const bv = boundingVolume;
    if (*reinterpret_cast<unsigned char*>(static_cast<char*>(bv) + 4) & 0x10) BvRecalculateGlobalPosition(bv);
    return *reinterpret_cast<const Vector3*>(static_cast<char*>(bv) + 0x7c);
}

// 0x7130 Particle::GetGlobalOrientation() const
const Quaternion& Particle::GetGlobalOrientation() const
{
    void* const bv = boundingVolume;
    if (*reinterpret_cast<unsigned char*>(static_cast<char*>(bv) + 4) & 0x20) BvRecalculateGlobalOrientation(bv);
    return *reinterpret_cast<const Quaternion*>(static_cast<char*>(bv) + 0x88);
}

// 0x7150 Particle::GetPosition() const
const Vector3& Particle::GetPosition() const
{
    return position;
}

// 0x7160 Particle::SetPosition(const Vector3&)
void Particle::SetPosition(const Vector3& value)
{
    BvSetPositionChanged(boundingVolume, position, value);
    CopyX87(position.x, value.x);
    CopyX87(position.y, value.y);
    CopyX87(position.z, value.z);
}

// 0x7190 Particle::GetPositionXY() const
Vector2 Particle::GetPositionXY() const
{
    Vector2 r;
    CopyX87(r.x, position.x);
    CopyX87(r.y, position.y);
    return r;
}

// 0x71b0 Particle::SetPositionXY(const Vector2&)
void Particle::SetPositionXY(const Vector2& value)
{
    const Vector3 p{value.x, value.y, 0.0f};
    SetPosition(p);
}

// 0x71f0 Particle::SetRawPosition(const Vector3&)
void Particle::SetRawPosition(const Vector3& value)
{
    position = value;
}

// 0x7210 Particle::GetOrientation() const
const Quaternion& Particle::GetOrientation() const
{
    return orientation;
}

// 0x7220 Particle::SetOrientation(const Quaternion&)
void Particle::SetOrientation(const Quaternion& value)
{
    BvSetOrientationChanged(boundingVolume, orientation, value);
    CopyX87(orientation.x, value.x);
    CopyX87(orientation.y, value.y);
    CopyX87(orientation.z, value.z);
    CopyX87(orientation.w, value.w);
}

// 0x7260 Particle::SetReferenceFrame(const Vector3&, const Quaternion&)
void Particle::SetReferenceFrame(const Vector3& pos, const Quaternion& rot)
{
    BvSetPositionOrientationChanged(boundingVolume, position, pos, orientation, rot);
    CopyX87(position.x, pos.x);
    CopyX87(position.y, pos.y);
    CopyX87(position.z, pos.z);
    CopyX87(orientation.x, rot.x);
    CopyX87(orientation.y, rot.y);
    CopyX87(orientation.z, rot.z);
    CopyX87(orientation.w, rot.w);
}

// 0x72c0 Particle::SetRawOrientation(const Quaternion&)
void Particle::SetRawOrientation(const Quaternion& value)
{
    orientation = value;
}

// 0x72e0 Particle::GetEnabled() const
bool Particle::GetEnabled() const
{
    return (flags >> 1) & 1;
}

// 0x72f0 Particle::SetEnabled(bool)
void Particle::SetEnabled(bool enabled)
{
    flags = static_cast<unsigned char>(flags ^ ((static_cast<unsigned char>(enabled) * 2) ^ flags) & 2);
}

// 0x7310 Particle::GetOpacity() const
float Particle::GetOpacity() const
{
    return 1.0f;
}

// 0x7320 Particle::Add(Particle*)
void Particle::Add(Particle* child)
{
    if (child->_parent != this) {
        if (child->_parent) child->_parent->Remove(child);
        child->_parent = this;
        child->_next = nullptr;
        child->_prev = _tail;
        if (!_count)
            _head = child;
        else
            _tail->_next = child;
        ++_count;
        _tail = child;
        BvOnChildAdded(boundingVolume, child);
    }
}

// 0x7390 Particle::Push(Particle*)
void Particle::Push(Particle* child)
{
    if (child->_parent != this) {
        if (child->_parent) child->_parent->Remove(child);
        child->_parent = this;
        child->_next = _head;
        child->_prev = nullptr;
        if (!_count)
            _tail = child;
        else
            _head->_prev = child;
        ++_count;
        _head = child;
        BvOnChildAdded(boundingVolume, child);
    }
}

// 0x7400 Particle::InsertBefore(Iterator&, Particle*)
Particle::Iterator Particle::InsertBefore(Iterator& target, Particle* child)
{
    Iterator result;
    if (child->_parent != this) {
        if (child->_parent) child->_parent->Remove(child);
        if (_head == target.node) _head = child;
        child->_parent = this;
        child->_next = target.node;
        child->_prev = target.node->_prev;
        if (target.node->_prev) target.node->_prev->_next = child;
        target.node->_prev = child;
        ++_count;
        BvOnChildAdded(boundingVolume, child);
        result.node = child;
    }
    return result;
}

// 0x7480 Particle::InsertAfter(Iterator&, Particle*)
Particle::Iterator Particle::InsertAfter(Iterator& target, Particle* child)
{
    Iterator result;
    if (child->_parent != this) {
        if (child->_parent) child->_parent->Remove(child);
        if (_tail == target.node) _tail = child;
        child->_parent = this;
        child->_prev = target.node;
        child->_next = target.node->_next;
        if (target.node->_next) target.node->_next->_prev = child;
        target.node->_next = child;
        ++_count;
        BvOnChildAdded(boundingVolume, child);
        result.node = child;
    }
    return result;
}

// 0x7500 Particle::Remove()
Particle* Particle::Remove()
{
    if (!_count) return nullptr;
    Particle* const node = _tail;
    TreeRemove(*this, node);
    if (node) BvOnChildRemoved(boundingVolume, node);
    return node;
}

// 0x7540 Particle::Remove(Particle*)
bool Particle::Remove(Particle* child)
{
    const bool found = child->_parent == this;
    if (found) {
        TreeRemove(*this, child);
        BvOnChildRemoved(boundingVolume, child);
    }
    return found;
}

// 0x7580 Particle::Remove(Iterator&)
Particle::Iterator Particle::Remove(Iterator& target)
{
    Iterator result;
    Particle* const node = target.node;
    result.node = node->_next;
    TreeRemove(*this, node);
    if (target.node) BvOnChildRemoved(boundingVolume, target.node);
    return result;
}

// 0x75c0 Particle::Pop()
Particle* Particle::Pop()
{
    if (!_count) return nullptr;
    Particle* const node = _head;
    TreeRemove(*this, node);
    if (node) BvOnChildRemoved(boundingVolume, node);
    return node;
}

// 0x7600 Particle::GetBoundingSphere(bool) const
const Sphere& Particle::GetBoundingSphere(bool recalculate) const
{
    void* const bv = boundingVolume;
    if (recalculate && (*reinterpret_cast<unsigned char*>(static_cast<char*>(bv) + 4) & 2))
        BvRecalculateSphereVolume(bv);
    return *reinterpret_cast<const Sphere*>(static_cast<char*>(bv) + 8);
}

// 0x7630 Particle::GetAxisAlignedBoundingBox(bool) const
const AxisAlignedBox& Particle::GetAxisAlignedBoundingBox(bool recalculate) const
{
    void* const bv = boundingVolume;
    if (recalculate && (*reinterpret_cast<unsigned char*>(static_cast<char*>(bv) + 4) & 8))
        BvRecalculateAxisAlignedBoxVolume(bv);
    return *reinterpret_cast<const AxisAlignedBox*>(static_cast<char*>(bv) + 0x28);
}

// 0x7660 Particle::GetOrientedBoundingBox(bool) const
const OrientedBox& Particle::GetOrientedBoundingBox(bool recalculate) const
{
    void* const bv = boundingVolume;
    if (recalculate && (*reinterpret_cast<unsigned char*>(static_cast<char*>(bv) + 4) & 4))
        BvRecalculateOrientedBoxVolume(bv);
    return *reinterpret_cast<const OrientedBox*>(static_cast<char*>(bv) + 0x40);
}

// 0x7690 Particle::GetGlobalBoundingSphere(bool) const
Sphere Particle::GetGlobalBoundingSphere(bool recalculate) const
{
    void* bv = boundingVolume;
    if (recalculate && (*reinterpret_cast<unsigned char*>(static_cast<char*>(bv) + 4) & 2))
        BvRecalculateSphereVolume(bv);

    Sphere result;
    result.center.x = *reinterpret_cast<float*>(static_cast<char*>(bv) + 0x0c);
    result.center.y = *reinterpret_cast<float*>(static_cast<char*>(bv) + 0x10);
    result.center.z = *reinterpret_cast<float*>(static_cast<char*>(bv) + 0x14);
    result.radius = X87Copy(*reinterpret_cast<float*>(static_cast<char*>(bv) + 0x18));

    bv = boundingVolume;
    if (recalculate && (*reinterpret_cast<unsigned char*>(static_cast<char*>(bv) + 4) & 3))
        BvRecalculateSphereGlobalCenter(bv);

    CopyX87(result.center.x, *reinterpret_cast<float*>(static_cast<char*>(bv) + 0x1c));
    CopyX87(result.center.y, *reinterpret_cast<float*>(static_cast<char*>(bv) + 0x20));
    CopyX87(result.center.z, *reinterpret_cast<float*>(static_cast<char*>(bv) + 0x24));
    return result;
}

// 0x7710 Particle::GetGlobalBoundingSphere(Sphere&, bool) const
void Particle::GetGlobalBoundingSphere(Sphere& result, bool recalculate) const
{
    void* bv = boundingVolume;
    if (recalculate && (*reinterpret_cast<unsigned char*>(static_cast<char*>(bv) + 4) & 2))
        BvRecalculateSphereVolume(bv);

    result.radius = X87Copy(*reinterpret_cast<float*>(static_cast<char*>(bv) + 0x18));
    bv = boundingVolume;
    if (recalculate && (*reinterpret_cast<unsigned char*>(static_cast<char*>(bv) + 4) & 3))
        BvRecalculateSphereGlobalCenter(bv);

    CopyX87(result.center.x, *reinterpret_cast<float*>(static_cast<char*>(bv) + 0x1c));
    CopyX87(result.center.y, *reinterpret_cast<float*>(static_cast<char*>(bv) + 0x20));
    CopyX87(result.center.z, *reinterpret_cast<float*>(static_cast<char*>(bv) + 0x24));
}

// 0x7770 Particle::GetGlobalBoundingSphereCenter(bool) const
const Vector3& Particle::GetGlobalBoundingSphereCenter(bool recalculate) const
{
    void* const bv = boundingVolume;
    if (recalculate && (*reinterpret_cast<unsigned char*>(static_cast<char*>(bv) + 4) & 3))
        BvRecalculateSphereGlobalCenter(bv);
    return *reinterpret_cast<const Vector3*>(static_cast<char*>(bv) + 0x1c);
}

// 0x77a0/0x7860/0x7920 Particle::PointXAt/PointYAt/PointZAt
void Particle::PointXAt(const Vector3& target, const Vector3& up)
{
    Quaternion q = orientation;
    Vector3 delta;
    delta.x = fsub(target.x, position.x);
    delta.y = fsub(target.y, position.y);
    delta.z = fsub(target.z, position.z);
    q.SetXAxis(delta, up);
    SetOrientation(q);
}

void Particle::PointYAt(const Vector3& target, const Vector3& up)
{
    Quaternion q = orientation;
    Vector3 delta;
    delta.x = fsub(target.x, position.x);
    delta.y = fsub(target.y, position.y);
    delta.z = fsub(target.z, position.z);
    q.SetYAxis(delta, up);
    SetOrientation(q);
}

void Particle::PointZAt(const Vector3& target, const Vector3& up)
{
    Quaternion q = orientation;
    Vector3 delta;
    delta.x = fsub(target.x, position.x);
    delta.y = fsub(target.y, position.y);
    delta.z = fsub(target.z, position.z);
    q.SetZAxis(delta, up);
    SetOrientation(q);
}

// 0x79f0 Particle::IsWithin(const PlaneList&) const
bool Particle::IsWithin(const PlaneList& planes) const
{
    void* bv = boundingVolume;
    if (*reinterpret_cast<unsigned char*>(static_cast<char*>(bv) + 4) & 2) BvRecalculateSphereVolume(bv);
    const float radius = *reinterpret_cast<float*>(static_cast<char*>(bv) + 0x18);

    bv = boundingVolume;
    if (*reinterpret_cast<unsigned char*>(static_cast<char*>(bv) + 4) & 3) BvRecalculateSphereGlobalCenter(bv);

    const Vector3 center = *reinterpret_cast<const Vector3*>(static_cast<char*>(bv) + 0x1c);
    const float negRadius = fneg(radius);
    for (PlaneList::ConstIterator i = planes.First(); i != planes.End(); ++i) {
        const Plane& plane = *i;
        const float distance =
            fsub(fadd(fadd(fmul(plane.normal.y, center.y), fmul(plane.normal.z, center.z)), fmul(plane.normal.x, center.x)),
                plane.d);
        if (!(distance > radius) && (negRadius > distance)) return false;
    }
    return true;
}

// 0x7ac0 Particle::SetCullSizeCheck(bool)
void Particle::SetCullSizeCheck(bool state)
{
    flags = static_cast<unsigned char>(flags ^ ((flags ^ static_cast<unsigned char>(state)) & 1));
}

// 0x7ae0 Particle::GetCullSizeCheck() const
bool Particle::GetCullSizeCheck() const
{
    return (flags & 1) != 0;
}

// 0x7af0 Particle::GetBoundingVolume()
ParticleBoundingVolume* Particle::GetBoundingVolume()
{
    return boundingVolume;
}

const ParticleBoundingVolume* Particle::GetBoundingVolume() const
{
    return boundingVolume;
}

// 0x7b00 Particle::SetBoundingVolume(ParticleBoundingVolume*)
bool Particle::SetBoundingVolume(ParticleBoundingVolume* volume)
{
    if (!volume) return false;
    if (*reinterpret_cast<Particle**>(reinterpret_cast<char*>(volume) + 0x98) != this) return false;

    ParticleBoundingVolume* const embedded = reinterpret_cast<ParticleBoundingVolume*>(volumeStore);
    if (boundingVolume != embedded) {
        if (boundingVolume) BvDelete(boundingVolume);
        boundingVolume = nullptr;
    }
    boundingVolume = volume;
    return true;
}

// 0x7b50 Particle::SetCellNode(CellNode*)
void Particle::SetCellNode(CellNode* node)
{
    cellNode = node;
}

// 0x4e080 Particle::SetParent(Particle*)
void Particle::SetParent(Particle* parent)
{
    if (_parent != parent) {
        if (_parent) _parent->Remove(this);
        if (parent) parent->Add(this);
    }
}

// 0x4e0c0 Particle::CheckVisible(const ViewPoint&)
bool Particle::CheckVisible(const ViewPoint& view)
{
    if (flags & 2) {
        if (BvIsWithinView(boundingVolume, view)) {
            SetVisited();
            SetLastCulledFrame(*reinterpret_cast<unsigned int*>(
                reinterpret_cast<char*>(*reinterpret_cast<void* const*>(reinterpret_cast<char const*>(&view) + 0x7c)) + 0x3c));
            return true;
        }
    }
    return false;
}

// 0x4e110 Particle::Cull(ViewPoint&, const Matrix4&)
void Particle::Cull(ViewPoint& view, const Matrix4& transform)
{
    for (Particle* child = _head; child; child = child->_next) {
        if (child->CheckVisible(view)) {
            Matrix4 modelView;
            child->GetModelView(modelView, transform);
            child->Cull(view, modelView);
        }
    }
}

// 0x4e170 Particle::SetOpacity(float)
void Particle::SetOpacity(float opacity)
{
    for (Particle* p = _head; p; p = p->_next) p->SetOpacity(opacity);
}

// 0x4e1a0 Particle::Add(const PString&, Particle*)
bool Particle::Add(const PString& hostName, Particle* child)
{
    if (hostName.node == nullptr) {
        Add(child);
        return true;
    }
    Particle* const host = FindByName(hostName);
    if (host) {
        host->Add(child);
        return true;
    }
    return false;
}

// 0x4e1f0 Particle::Remove(const PString&)
Particle* Particle::Remove(const PString& value)
{
    Particle* found = FindByName(value);
    if (found) {
        found->SetParent(nullptr);
        return found;
    }
    return nullptr;
}

// 0x4e250 Particle::GetModelView(Matrix4&, const Matrix4&) const
void Particle::GetModelView(Matrix4& result, const Matrix4& view) const
{
    float model[16];
    QuaternionToMatrix(model, &orientation.x);
    model[12] = position.x;
    model[13] = position.y;
    model[14] = position.z;

    Matrix4 out = view;
    MultiplyMatrices(out.m, model);
    result = out;
}

// 0x4e370 Particle::GetModelView(const Matrix4&) const
Matrix4 Particle::GetModelView(const Matrix4& view) const
{
    Matrix4 result;
    GetModelView(result, view);
    return result;
}

// 0x4e390 Particle::Translate(const Vector3&)
void Particle::Translate(const Vector3& delta)
{
    Vector3 p;
    p.x = fadd(delta.x, position.x);
    p.y = fadd(delta.y, position.y);
    p.z = fadd(delta.z, position.z);
    SetPosition(p);
}

// 0x4e410 Particle::Rotate(const Matrix3&)
void Particle::Rotate(const Matrix3& rot)
{
    const float x = position.x;
    const float y = position.y;
    const float z = position.z;
    Vector3 p;
    p.x = fadd(fadd(fmul(rot.m[3], y), fmul(rot.m[6], z)), fmul(rot.m[0], x));
    p.y = fadd(fadd(fmul(rot.m[1], x), fmul(rot.m[4], y)), fmul(rot.m[7], z));
    p.z = fadd(fadd(fmul(rot.m[2], x), fmul(rot.m[5], y)), fmul(rot.m[8], z));
    SetPosition(p);

    Quaternion q;
    q.Set(rot);
    Quaternion r;
    MultiplyQuaternions(&r.x, &orientation.x, &q.x);
    SetOrientation(r);
}

// 0x4e4f0 Particle::Rotate(const Quaternion&)
void Particle::Rotate(const Quaternion& rot)
{
    const float fVar1 = rot.x;
    const float fVar2 = rot.y;
    const float fVar10 = fmul(fVar2, 2.0f);
    const float fVar7 = fmul(rot.z, 2.0f);
    const float fVar8 = fmul(fmul(fVar1, fVar1), 2.0f);
    const float fVar11 = fmul(rot.z, fVar7);
    const float fVar3 = rot.w;
    const float fVar9 = fmul(fmul(fVar3, fVar1), 2.0f);

    const float local_20 = fadd(fmul(fVar3, fVar7), fmul(fVar1, fVar10));
    const float local_18 = fsub(fmul(fVar1, fVar10), fmul(fVar3, fVar7));
    const float local_1c = fsub(fmul(fVar1, fVar7), fmul(fVar3, fVar10));
    const float local_c = fadd(fmul(fVar3, fVar10), fmul(fVar1, fVar7));
    const float local_4 = fsub(1.0f, fadd(fmul(fVar2, fVar10), fVar8));
    const float local_8 = fsub(fmul(fVar2, fVar7), fVar9);

    const float px = position.x;
    const float py = position.y;
    const float pz = position.z;
    Vector3 p;
    p.x = fadd(fadd(fmul(px, fsub(1.0f, fadd(fVar11, fmul(fVar2, fVar10)))), fmul(py, local_18)), fmul(pz, local_c));
    p.y = fadd(fadd(fmul(px, local_20), fmul(pz, local_8)), fmul(py, fsub(1.0f, fadd(fVar11, fVar8))));
    p.z = fadd(fadd(fmul(pz, local_4), fmul(py, fadd(fVar9, fmul(fVar2, fVar7)))), fmul(px, local_1c));

    Quaternion q;
    MultiplyQuaternions(&q.x, &orientation.x, &rot.x);
    SetReferenceFrame(p, q);
}

// 0x4e6b0 Particle::SelfRotate(const Matrix3&)
void Particle::SelfRotate(const Matrix3& rot)
{
    Quaternion q;
    q.Set(rot);
    Quaternion r;
    MultiplyQuaternions(&r.x, &orientation.x, &q.x);
    SetOrientation(r);
}

// 0x4e6f0 Particle::SelfRotate(const Quaternion&)
void Particle::SelfRotate(const Quaternion& rot)
{
    Quaternion r;
    MultiplyQuaternions(&r.x, &orientation.x, &rot.x);
    SetOrientation(r);
}

// 0x4e720 Particle::LocalToGlobal(Plane&) const
void Particle::LocalToGlobal(Plane& plane) const
{
    void* bv = boundingVolume;
    if (*reinterpret_cast<unsigned char*>(static_cast<char*>(bv) + 4) & 0x20) BvRecalculateGlobalOrientation(bv);

    const float fVar6 = *reinterpret_cast<float*>(static_cast<char*>(bv) + 0x88);  // q.x
    const float fVar3 = *reinterpret_cast<float*>(static_cast<char*>(bv) + 0x8c);  // q.y
    const float fVar5 = fmul(*reinterpret_cast<float*>(static_cast<char*>(bv) + 0x90), 2.0f);
    const float fVar14 = fmul(fVar3, 2.0f);
    const float fVar16 = fmul(*reinterpret_cast<float*>(static_cast<char*>(bv) + 0x90), fVar5);
    const float fVar9 = fmul(fmul(fVar6, fVar6), 2.0f);
    const float fVar13 = *reinterpret_cast<float*>(static_cast<char*>(bv) + 0x94);  // q.w
    const float fVar12 = fmul(fmul(fVar13, fVar6), 2.0f);
    const float fVar19 = fsub(1.0f, fadd(fVar16, fmul(fVar3, fVar14)));
    const float fVar17 = fadd(fmul(fVar13, fVar5), fmul(fVar6, fVar14));
    const float fVar10 = fsub(fmul(fVar6, fVar14), fmul(fVar13, fVar5));
    const float fVar16b = fsub(1.0f, fadd(fVar16, fVar9));
    const float fVar18 = fsub(fmul(fVar6, fVar5), fmul(fVar13, fVar14));
    const float fVar13b = fadd(fmul(fVar13, fVar14), fmul(fVar6, fVar5));
    const float fVar15 = fadd(fVar12, fmul(fVar3, fVar5));
    const float fVar12b = fsub(fmul(fVar3, fVar5), fVar12);
    const float fVar6b = fsub(1.0f, fadd(fmul(fVar3, fVar14), fVar9));

    bv = boundingVolume;
    if (*reinterpret_cast<unsigned char*>(static_cast<char*>(bv) + 4) & 0x10) BvRecalculateGlobalPosition(bv);

    const float nx = plane.normal.x;
    const float ny = plane.normal.y;
    const float nz = plane.normal.z;
    const float d = plane.d;
    const float dnx = fmul(d, nx);
    const float dny = fmul(ny, d);
    const float dnz = fmul(nz, d);
    const float px = *reinterpret_cast<float*>(static_cast<char*>(bv) + 0x7c);
    const float py = *reinterpret_cast<float*>(static_cast<char*>(bv) + 0x80);
    const float pz = *reinterpret_cast<float*>(static_cast<char*>(bv) + 0x84);

    const float rnx = fadd(fadd(fmul(nz, fVar13b), fmul(nx, fVar19)), fmul(ny, fVar10));
    const float rny = fadd(fadd(fmul(nx, fVar17), fmul(nz, fVar12b)), fmul(ny, fVar16b));
    const float rnz = fadd(fadd(fmul(nz, fVar6b), fmul(nx, fVar18)), fmul(fVar15, ny));

    plane.normal.x = rnx;
    plane.normal.y = rny;
    plane.normal.z = rnz;
    const float tx = fadd(fadd(fmul(dnz, fVar13b), fmul(fVar19, dnx)), fmul(fVar10, dny));
    const float ty = fadd(fadd(fmul(dnz, fVar12b), fmul(fVar16b, dny)), fmul(fVar17, dnx));
    const float tz = fadd(fadd(fmul(dnz, fVar6b), fmul(fVar15, dny)), fmul(fVar18, dnx));
    plane.d = fadd(fadd(fmul(rnz, fadd(pz, tz)), fmul(rny, fadd(py, ty))), fmul(rnx, fadd(px, tx)));
}

// 0x4ea20 Particle::LocalToGlobal(Vector3&) const
void Particle::LocalToGlobal(Vector3& value) const
{
    void* bv = boundingVolume;
    if (*reinterpret_cast<unsigned char*>(static_cast<char*>(bv) + 4) & 0x20) BvRecalculateGlobalOrientation(bv);

    bv = boundingVolume;
    if (*reinterpret_cast<unsigned char*>(static_cast<char*>(bv) + 4) & 0x10) BvRecalculateGlobalPosition(bv);

    float rotated[3];
    RotateByQuaternion(rotated, &value.x, BvOrientation(bv));
    value.x = fadd(rotated[0], *reinterpret_cast<float*>(static_cast<char*>(bv) + 0x7c));
    value.y = fadd(rotated[1], *reinterpret_cast<float*>(static_cast<char*>(bv) + 0x80));
    value.z = fadd(rotated[2], *reinterpret_cast<float*>(static_cast<char*>(bv) + 0x84));
}

// 0x4eac0 Particle::LocalToGlobal(Quaternion&) const
void Particle::LocalToGlobal(Quaternion& value) const
{
    void* bv = boundingVolume;
    if (*reinterpret_cast<unsigned char*>(static_cast<char*>(bv) + 4) & 0x20) BvRecalculateGlobalOrientation(bv);
    Quaternion result;
    MultiplyQuaternions(&result.x, &value.x, BvOrientation(bv));
    value = result;
}

// 0x4eb10 Particle::LocalToGlobal(Vector3&, Quaternion&) const
void Particle::LocalToGlobal(Vector3& value, Quaternion& rot) const
{
    LocalToGlobal(value);
    void* bv = boundingVolume;
    if (*reinterpret_cast<unsigned char*>(static_cast<char*>(bv) + 4) & 0x20) BvRecalculateGlobalOrientation(bv);
    Quaternion result;
    MultiplyQuaternions(&result.x, &rot.x, BvOrientation(bv));
    rot = result;
}

// 0x4eb70 Particle::LocalToGlobal(OrientedBox&) const
void Particle::LocalToGlobal(OrientedBox& box) const
{
    void* bv = boundingVolume;
    if (*reinterpret_cast<unsigned char*>(static_cast<char*>(bv) + 4) & 0x20) BvRecalculateGlobalOrientation(bv);

    Matrix3 rot;
    QuaternionToMatrix3(rot.m, BvOrientation(bv));

    bv = boundingVolume;
    if (*reinterpret_cast<unsigned char*>(static_cast<char*>(bv) + 4) & 0x10) BvRecalculateGlobalPosition(bv);
    TransformBox(box, rot, *reinterpret_cast<const Vector3*>(static_cast<char*>(bv) + 0x7c));
}

// 0x4ecf0 Particle::GlobalToLocal(Plane&) const
void Particle::GlobalToLocal(Plane& plane) const
{
    void* bv = boundingVolume;
    if (*reinterpret_cast<unsigned char*>(static_cast<char*>(bv) + 4) & 0x20) BvRecalculateGlobalOrientation(bv);

    const float qx = *reinterpret_cast<float*>(static_cast<char*>(bv) + 0x88);
    const float qy = *reinterpret_cast<float*>(static_cast<char*>(bv) + 0x8c);
    const float qz2 = fmul(*reinterpret_cast<float*>(static_cast<char*>(bv) + 0x90), 2.0f);
    const float qy2 = fmul(qy, 2.0f);
    const float qzqz = fmul(*reinterpret_cast<float*>(static_cast<char*>(bv) + 0x90), qz2);
    const float qw = *reinterpret_cast<float*>(static_cast<char*>(bv) + 0x94);
    const float qxqx = fmul(fmul(qx, qx), 2.0f);
    const float qwqx = fmul(fmul(qw, qx), 2.0f);
    const float m00 = fsub(1.0f, fadd(qzqz, fmul(qy, qy2)));
    const float m01 = fadd(fmul(qw, qz2), fmul(qx, qy2));
    const float m02 = fsub(fmul(qx, qy2), fmul(qw, qz2));
    const float m10 = fsub(fmul(qx, qz2), fmul(qw, qy2));
    const float m11 = fsub(1.0f, fadd(qzqz, qxqx));
    const float m12 = fadd(qwqx, fmul(qy, qz2));
    const float m20 = fadd(fmul(qw, qy2), fmul(qx, qz2));
    const float m21 = fsub(fmul(qy, qz2), qwqx);
    const float m22 = fsub(1.0f, fadd(fmul(qy, qy2), qxqx));

    bv = boundingVolume;
    if (*reinterpret_cast<unsigned char*>(static_cast<char*>(bv) + 4) & 0x10) BvRecalculateGlobalPosition(bv);

    const float d = plane.d;
    const float nx = plane.normal.x;
    const float ny = plane.normal.y;
    const float nz = plane.normal.z;
    const float dnx = fsub(fmul(d, nx), *reinterpret_cast<float*>(static_cast<char*>(bv) + 0x7c));
    const float dnz = fsub(fmul(d, nz), *reinterpret_cast<float*>(static_cast<char*>(bv) + 0x84));
    const float dny = fsub(fmul(d, ny), *reinterpret_cast<float*>(static_cast<char*>(bv) + 0x80));

    const float rnx = fadd(fadd(fmul(nz, m10), fmul(nx, m00)), fmul(ny, m01));
    const float rny = fadd(fadd(fmul(nz, m12), fmul(ny, m11)), fmul(nx, m02));
    const float rnz = fadd(fadd(fmul(nx, m20), fmul(nz, m22)), fmul(m21, ny));

    plane.normal.x = rnx;
    plane.normal.y = rny;
    plane.normal.z = rnz;
    plane.d = fadd(fadd(fmul(rnz, fadd(fadd(fmul(dnz, m22), fmul(dny, m21)), fmul(m20, dnx))),
                      fmul(rny, fadd(fadd(fmul(dnz, m12), fmul(dny, m11)), fmul(dnx, m02)))),
                  fmul(rnx, fadd(fadd(fmul(dnz, m10), fmul(dnx, m00)), fmul(dny, m01))));
}

// 0x4efe0 Particle::GlobalToLocal(Vector3&) const
void Particle::GlobalToLocal(Vector3& value) const
{
    void* bv = boundingVolume;
    if (*reinterpret_cast<unsigned char*>(static_cast<char*>(bv) + 4) & 0x10) BvRecalculateGlobalPosition(bv);

    const float vx = fsub(value.x, *reinterpret_cast<float*>(static_cast<char*>(bv) + 0x7c));
    const float vy = fsub(value.y, *reinterpret_cast<float*>(static_cast<char*>(bv) + 0x80));
    const float vz = fsub(value.z, *reinterpret_cast<float*>(static_cast<char*>(bv) + 0x84));

    bv = boundingVolume;
    if (*reinterpret_cast<unsigned char*>(static_cast<char*>(bv) + 4) & 0x20) BvRecalculateGlobalOrientation(bv);

    const float qx = *reinterpret_cast<float*>(static_cast<char*>(bv) + 0x88);
    const float qy = *reinterpret_cast<float*>(static_cast<char*>(bv) + 0x8c);
    const float qz = *reinterpret_cast<float*>(static_cast<char*>(bv) + 0x90);
    const float qw = *reinterpret_cast<float*>(static_cast<char*>(bv) + 0x94);
    const float inv =
        fdiv(1.0f, fadd(fadd(fadd(fmul(qw, qw), fmul(qx, qx)), fmul(qz, qz)), fmul(qy, qy)));
    const float nx = fmul(inv, fneg(qx));
    const float nz = fmul(inv, fneg(qz));
    const float twoNz = fmul(nz, 2.0f);
    const float ny = fmul(inv, fneg(qy));
    const float nw = fmul(inv, qw);
    const float nz2 = fmul(twoNz, nz);
    const float twoNy = fmul(ny, 2.0f);
    const float nxw = fmul(fmul(nx, 2.0f), nw);
    const float nx2 = fmul(fmul(nx, 2.0f), nx);
    const float twoNx = fmul(nx, 2.0f);

    value.x = fadd(fadd(fmul(fsub(1.0f, fadd(fmul(twoNy, ny), nz2)), vx),
                        fmul(fadd(fmul(twoNz, nx), fmul(twoNy, nw)), vz)),
                   fmul(fsub(fmul(twoNy, nx), fmul(twoNz, nw)), vy));
    value.y = fadd(fadd(fmul(fsub(1.0f, fadd(nx2, nz2)), vy),
                        fmul(fsub(fmul(twoNz, ny), nxw), vz)),
                   fmul(fadd(fmul(twoNz, nw), fmul(twoNy, nx)), vx));
    value.z = fadd(fadd(fmul(fsub(1.0f, fadd(nx2, fmul(twoNy, ny))), vz),
                        fmul(fadd(nxw, fmul(twoNz, ny)), vy)),
                   fmul(fsub(fmul(twoNz, nx), fmul(twoNy, nw)), vx));
}

// 0x4f250 Particle::GlobalToLocal(Quaternion&) const
void Particle::GlobalToLocal(Quaternion& value) const
{
    void* bv = boundingVolume;
    if (*reinterpret_cast<unsigned char*>(static_cast<char*>(bv) + 4) & 0x20) BvRecalculateGlobalOrientation(bv);

    Quaternion inverse;
    inverse.x = fneg(*reinterpret_cast<float*>(static_cast<char*>(bv) + 0x88));
    inverse.y = fneg(*reinterpret_cast<float*>(static_cast<char*>(bv) + 0x8c));
    inverse.z = fneg(*reinterpret_cast<float*>(static_cast<char*>(bv) + 0x90));
    inverse.w = *reinterpret_cast<float*>(static_cast<char*>(bv) + 0x94);
    Quaternion result;
    MultiplyQuaternions(&result.x, &value.x, &inverse.x);
    value = result;
}

// 0x4f2f0 Particle::GlobalToLocal(Vector3&, Quaternion&) const
void Particle::GlobalToLocal(Vector3& value, Quaternion& rot) const
{
    GlobalToLocal(value);
    GlobalToLocal(rot);
}

// 0x4f320 Particle::GlobalToLocal(OrientedBox&) const
void Particle::GlobalToLocal(OrientedBox& box) const
{
    void* bv = boundingVolume;
    if (*reinterpret_cast<unsigned char*>(static_cast<char*>(bv) + 4) & 0x20) BvRecalculateGlobalOrientation(bv);

    const float* q = BvOrientation(bv);
    Matrix3 rot;
    QuaternionToMatrix3(rot.m, q);

    bv = boundingVolume;
    if (*reinterpret_cast<unsigned char*>(static_cast<char*>(bv) + 4) & 0x10) BvRecalculateGlobalPosition(bv);
    InverseTransformBox(box, rot, *reinterpret_cast<const Vector3*>(static_cast<char*>(bv) + 0x7c));
}

// 0x4f4a0..0x4f580 Particle::LocalToLocal
void Particle::LocalToLocal(const Particle* target, Plane& plane) const
{
    LocalToGlobal(plane);
    target->GlobalToLocal(plane);
}

void Particle::LocalToLocal(const Particle* target, Vector3& value) const
{
    LocalToGlobal(value);
    target->GlobalToLocal(value);
}

void Particle::LocalToLocal(const Particle* target, Quaternion& rot) const
{
    LocalToGlobal(rot);
    target->GlobalToLocal(rot);
}

void Particle::LocalToLocal(const Particle* target, Vector3& value, Quaternion& rot) const
{
    LocalToGlobal(value, rot);
    target->GlobalToLocal(value);
    target->GlobalToLocal(rot);
}

void Particle::LocalToLocal(const Particle* target, OrientedBox& box) const
{
    LocalToGlobal(box);
    target->GlobalToLocal(box);
}

// 0x4f5a0 Particle::FindParentContainer()
Particle* Particle::FindParentContainer()
{
    for (Particle* p = _parent; p; p = p->_parent)
        if (p->IsContainer()) return p;
    return nullptr;
}

const Particle* Particle::FindParentContainer() const
{
    for (Particle* p = _parent; p; p = p->_parent)
        if (p->IsContainer()) return p;
    return nullptr;
}

// 0x4f5d0 Particle::RemoveAll()
void Particle::RemoveAll()
{
    while (_count) Pop();
}

// 0x4f600 Particle::RemoveAllChildren()
void Particle::RemoveAllChildren()
{
    if (!_count) return;
    Particle* child = Pop();
    while (child) {
        child->RemoveAllChildren();
        child = Pop();
    }
}

// 0x4f640 Particle::DeleteAll()
void Particle::DeleteAll()
{
    if (!_count) return;
    Particle* child = Pop();
    while (child) {
        using Fn = void(__thiscall*)(Particle*, int);
        reinterpret_cast<Fn>((*reinterpret_cast<void***>(child))[0])(child, 1);
        child = Pop();
    }
}

// 0x4f680 Particle::FindByName(const PString&)
Particle* Particle::FindByName(const PString& value)
{
    Particle* child = _head;
    if (child) {
        do {
            if (child->name.node == value.node) return child;
            child = child->_next;
        } while (child);
    }
    for (child = _head; child; child = child->_next) {
        Particle* found = child->FindByName(value);
        if (found) return found;
    }
    return nullptr;
}

const Particle* Particle::FindByName(const PString& value) const
{
    const Particle* child = _head;
    if (child) {
        do {
            if (child->name.node == value.node) return child;
            child = child->_next;
        } while (child);
    }
    for (child = _head; child; child = child->_next) {
        const Particle* found = child->FindByName(value);
        if (found) return found;
    }
    return nullptr;
}

// 0x4f6c0 Particle::EnumerateParticles(ParticlePtrList&)
bool Particle::EnumerateParticles(ParticlePtrList& list)
{
    Particle* node = _head;
    bool added = false;
    while (node) {
        list.Add(node);
        added = true;
        if (node->_count) {
            node = node->_head;
        } else if (node->_next) {
            node = node->_next;
        } else {
            Particle* parent = node->_parent;
            while (parent) {
                if (parent == this || parent->_next == this) {
                    parent = nullptr;
                } else {
                    if (parent->_next) {
                        node = parent->_next;
                        break;
                    }
                    parent = parent->_parent;
                }
            }
            if (!parent) node = nullptr;
        }
        if (node == this) node = nullptr;
    }
    return added;
}

bool Particle::EnumerateParticles(ConstParticlePtrList& list) const
{
    const Particle* node = _head;
    bool added = false;
    while (node) {
        list.Add(node);
        added = true;
        if (node->_count) {
            node = node->_head;
        } else if (node->_next) {
            node = node->_next;
        } else {
            const Particle* parent = node->_parent;
            while (parent) {
                if (parent == this || parent->_next == this) {
                    parent = nullptr;
                } else {
                    if (parent->_next) {
                        node = parent->_next;
                        break;
                    }
                    parent = parent->_parent;
                }
            }
            if (!parent) node = nullptr;
        }
        if (node == this) node = nullptr;
    }
    return added;
}

// 0x4f940 Particle::GetFacilities / IsContainer / TestCollisionGeometry
bool Particle::GetFacilities(FacilityPtrList&)
{
    return false;
}

bool Particle::GetFacilities(ConstFacilityPtrList&) const
{
    return false;
}

bool Particle::IsContainer() const
{
    return false;
}

// 0x79e0 Particle::GetFacility returns NULL in the base class
Facility* Particle::GetFacility(const PString&)
{
    return nullptr;
}

const Facility* Particle::GetFacility(const PString&) const
{
    return nullptr;
}

bool Particle::TestCollisionGeometry(const Ray& ray, float& t, ConstFaceDescriptor& face) const
{
    using Fn = bool(__cdecl*)(const Particle*, const Ray&, float&, ConstFaceDescriptor&);
    static Fn fn = OrigProc<Fn>("?TestCollisionGeometry@Particle@Jet@@UBA_NABVRay@2@AAMAAVConstFaceDescriptor@2@@Z");
    return fn ? fn(this, ray, t, face) : false;
}

// 0x4f950 Particle::GetAllWithin(const PlaneList&, ParticlePtrList&)
bool Particle::GetAllWithin(const PlaneList& planes, ParticlePtrList& found)
{
    bool any = IsWithin(planes);
    if (any) found.Add(this);
    Particle* child = _head;
    if (child) {
        any = true;
        do {
            child->GetAllWithin(planes, found);
            child = child->_next;
        } while (child);
    }
    return any;
}

// 0x4f9b0 Particle::GetAllWithin(const PlaneList&, ConstParticlePtrList&) const
bool Particle::GetAllWithin(const PlaneList& planes, ConstParticlePtrList& found) const
{
    bool any = IsWithin(planes);
    if (any) found.Add(this);
    const Particle* child = _head;
    if (child) {
        any = true;
        do {
            child->GetAllWithin(planes, found);
            child = child->_next;
        } while (child);
    }
    return any;
}

// 0x4fa10 Particle::GetAllWithin(const Sphere&, ParticlePtrList&)
bool Particle::GetAllWithin(const Sphere& sphere, ParticlePtrList& found)
{
    void* bv = boundingVolume;
    if (*reinterpret_cast<unsigned char*>(static_cast<char*>(bv) + 4) & 2) BvRecalculateSphereVolume(bv);
    const float ownRadius = *reinterpret_cast<float*>(static_cast<char*>(bv) + 0x18);

    bv = boundingVolume;
    if (*reinterpret_cast<unsigned char*>(static_cast<char*>(bv) + 4) & 3) BvRecalculateSphereGlobalCenter(bv);

    const float dx = fsub(sphere.center.x, *reinterpret_cast<float*>(static_cast<char*>(bv) + 0x1c));
    const float dz = fsub(sphere.center.z, *reinterpret_cast<float*>(static_cast<char*>(bv) + 0x24));
    const float dy = fsub(sphere.center.y, *reinterpret_cast<float*>(static_cast<char*>(bv) + 0x20));
    const float sum = fadd(sphere.radius, ownRadius);
    const bool inside = fsub(fadd(fadd(fmul(dx, dx), fmul(dz, dz)), fmul(dy, dy)), fmul(sum, sum)) < 0.0001f;

    if (inside) found.Add(this);
    Particle* child = _head;
    if (!child) return inside;
    do {
        child->GetAllWithin(sphere, found);
        child = child->_next;
    } while (child);
    return true;
}

// 0x4fb20 Particle::GetAllWithin(const Sphere&, ConstParticlePtrList&) const
bool Particle::GetAllWithin(const Sphere& sphere, ConstParticlePtrList& found) const
{
    void* bv = boundingVolume;
    if (*reinterpret_cast<unsigned char*>(static_cast<char*>(bv) + 4) & 2) BvRecalculateSphereVolume(bv);
    const float ownRadius = *reinterpret_cast<float*>(static_cast<char*>(bv) + 0x18);

    bv = boundingVolume;
    if (*reinterpret_cast<unsigned char*>(static_cast<char*>(bv) + 4) & 3) BvRecalculateSphereGlobalCenter(bv);

    const float dx = fsub(sphere.center.x, *reinterpret_cast<float*>(static_cast<char*>(bv) + 0x1c));
    const float dz = fsub(sphere.center.z, *reinterpret_cast<float*>(static_cast<char*>(bv) + 0x24));
    const float dy = fsub(sphere.center.y, *reinterpret_cast<float*>(static_cast<char*>(bv) + 0x20));
    const float sum = fadd(sphere.radius, ownRadius);
    const bool inside = fsub(fadd(fadd(fmul(dx, dx), fmul(dz, dz)), fmul(dy, dy)), fmul(sum, sum)) < 0.0001f;

    if (inside) found.Add(this);
    const Particle* child = _head;
    if (!child) return inside;
    do {
        child->GetAllWithin(sphere, found);
        child = child->_next;
    } while (child);
    return true;
}

// 0x4f710 Particle::TestCollision (Ray/CollisionResult internals belong to other units: forward)
bool Particle::TestCollision(const Ray& ray, CollisionResult& result, const Particle* ignoreThis)
{
    using Fn = bool(__cdecl*)(Particle*, const Ray&, CollisionResult&, const Particle*);
    static Fn fn = OrigProc<Fn>("?TestCollision@Particle@Jet@@UAA_NABVRay@2@AAVCollisionResult@2@PBV12@@Z");
    return fn ? fn(this, ray, result, ignoreThis) : false;
}

// 0x4fc30 Particle::InitInstance(Particle&) const
void Particle::InitInstance(Particle& instance) const
{
    instance.flags = static_cast<unsigned char>(instance.flags ^ ((instance.flags ^ flags) & 1));
    instance.flags = static_cast<unsigned char>((flags ^ instance.flags) & 2 ^ instance.flags);

    instance.name = name;
    CopyX87(instance.orientation.x, orientation.x);
    CopyX87(instance.orientation.y, orientation.y);
    CopyX87(instance.orientation.z, orientation.z);
    CopyX87(instance.orientation.w, orientation.w);
    CopyX87(instance.position.x, position.x);
    CopyX87(instance.position.y, position.y);
    CopyX87(instance.position.z, position.z);

    void* const newVolume = BvCreateInstance(boundingVolume, &instance);
    instance.SetBoundingVolume(reinterpret_cast<ParticleBoundingVolume*>(newVolume));

    for (Particle* child = _head; child; child = child->_next) {
        Particle* const clone = child->CreateInstance();
        if (clone) instance.Add(clone);
    }
}

// 0x4fd20 Particle::RenderDebug: the original forwards to Element::Render (its own vtable slot 4), not to the
// Display's slot 0x10
void Particle::RenderDebug(Display* display, DebugViewPort& port, Matrix4& m, unsigned long flags_)
{
    using Fn = void(__cdecl*)(Particle*, Display*, DebugViewPort&, Matrix4&, unsigned long);
    reinterpret_cast<Fn>((*reinterpret_cast<void***>(this))[4])(this, display, port, m, flags_);
}

// 0x4fd40 Particle::SetCell
void Particle::SetCell(Cell* cell)
{
    using Fn = void(__cdecl*)(Particle*, Cell*);
    static Fn fn = OrigProc<Fn>("?SetCell@Particle@Jet@@QAAXPAVCell@2@@Z");
    if (fn) fn(this, cell);
}

// 0x4fd90 Particle::GetCell
Cell* Particle::GetCell()
{
    return cellNode ? *reinterpret_cast<Cell**>(reinterpret_cast<char*>(cellNode) + 0x0c) : nullptr;
}

const Cell* Particle::GetCell() const
{
    return cellNode ? *reinterpret_cast<Cell* const*>(reinterpret_cast<char const*>(cellNode) + 0x0c) : nullptr;
}

// 0x4fdb0 Particle::PushToCellRoot
void Particle::PushToCellRoot()
{
    if (!cellNode) return;
    Cell* const cell = *reinterpret_cast<Cell**>(reinterpret_cast<char*>(cellNode) + 0x0c);
    using Fn = void*(__cdecl*)(Cell*);
    cellNode = reinterpret_cast<CellNode*>(reinterpret_cast<Fn>((*reinterpret_cast<void***>(cell))[0xc0 / 4])(cell));
}

}  // namespace Jet
