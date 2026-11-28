// Unit U3 of Render3DCore_7c: Jet::ParticleBoundingVolume, the base of the particle bounding volumes. Transcribed
// from the TS12 assembly (0x10006710..0x10006aa0, 0x10051e70..0x10052da0).
//
// The dirty bits the original uses: 0x01 global sphere centre, 0x02 sphere volume, 0x04 oriented box, 0x08
// axis-aligned box, 0x10 global position, 0x20 global orientation. A fresh volume has 0xfffff. The setters mark the
// volumes and also walk the host's parent chain and children, as the assembly does.
#include <jet/Render3DCoreBounds.h>
#include <jet/Render3DCoreParticle.h>

#include <windows.h>

#include <cstring>
#include <new>

namespace Jet {

namespace BoundsDetail {

float __cdecl AddSS(float a, float b)
{
    return SolidsDetail::AddSS(a, b);
}

float __cdecl SubSS(float a, float b)
{
    return SolidsDetail::SubSS(a, b);
}

float __cdecl MulSS(float a, float b)
{
    return SolidsDetail::MulSS(a, b);
}

float __cdecl DivSS(float a, float b)
{
    return SolidsDetail::DivSS(a, b);
}

void __cdecl CopyX87(float& dst, const float& src)
{
    SolidsDetail::X87Copy(dst, src);
}

// 0x1004be00: out = q applied to v, with the original's grouped products (NaN payload parity).
void __cdecl RotateByQuaternion(float* out, const float* v, const float* q)
{
    const float fVar1 = q[1];
    const float fVar9 = MulSS(q[2], 2.0f);
    const float fVar10 = MulSS(fVar9, q[2]);
    const float fVar8 = MulSS(fVar1, 2.0f);
    const float fVar5 = MulSS(q[0], fVar8);
    const float fVar13 = MulSS(q[3], fVar8);
    const float fVar7 = MulSS(q[0], fVar9);
    const float fVar6 = MulSS(q[3], fVar9);
    const float fVar2 = q[0];
    const float fVar12 = q[3];
    const float fVar11 = MulSS(fVar2, 2.0f);

    out[0] = AddSS(AddSS(MulSS(SubSS(1.0f, AddSS(MulSS(fVar8, fVar1), fVar10)), v[0]),
                         MulSS(AddSS(fVar7, fVar13), v[2])),
                   MulSS(SubSS(fVar5, fVar6), v[1]));
    const float fVar12b = MulSS(fVar11, fVar12);
    const float fVar11b = MulSS(fVar11, fVar2);
    const float fVar2b = v[1];
    const float fVar3 = v[2];
    const float fVar4 = v[0];
    out[1] = AddSS(AddSS(MulSS(SubSS(1.0f, AddSS(fVar11b, fVar10)), fVar2b),
                         MulSS(SubSS(MulSS(fVar1, fVar9), fVar12b), fVar3)),
                   MulSS(AddSS(fVar6, fVar5), fVar4));
    out[2] = AddSS(AddSS(MulSS(SubSS(1.0f, AddSS(fVar11b, MulSS(fVar8, fVar1))), fVar3),
                         MulSS(AddSS(fVar12b, MulSS(fVar1, fVar9)), fVar2b)),
                   MulSS(SubSS(fVar7, fVar13), fVar4));
}

// 0x1004fe60: out = a * b.
void __cdecl MultiplyQuaternions(float* out, const float* a, const float* b)
{
    out[0] = AddSS(AddSS(SubSS(MulSS(b[1], a[2]), MulSS(b[2], a[1])), MulSS(a[0], b[3])), MulSS(b[0], a[3]));
    out[1] = AddSS(AddSS(SubSS(MulSS(a[0], b[2]), MulSS(b[0], a[2])), MulSS(a[1], b[3])), MulSS(a[3], b[1]));
    out[2] = AddSS(AddSS(SubSS(MulSS(b[0], a[1]), MulSS(a[0], b[1])), MulSS(a[2], b[3])), MulSS(b[2], a[3]));
    out[3] = SubSS(MulSS(b[3], a[3]), AddSS(AddSS(MulSS(a[1], b[1]), MulSS(a[2], b[2])), MulSS(a[0], b[0])));
}

// 0x1004eb70 / 0x1004f320 matrix build: same values as the original's inlined quaternion->3x3 conversion.
void __cdecl QuaternionToMatrix3(float* m, const float* q)
{
    const float qz2 = MulSS(q[2], 2.0f);
    const float qy2 = MulSS(q[1], 2.0f);
    const float qx2 = MulSS(q[0], 2.0f);
    const float qzsq = MulSS(q[2], qz2);
    const float qysq = MulSS(q[1], qy2);
    const float qxsq = MulSS(q[0], qx2);
    const float xy = MulSS(q[0], qy2);
    const float wz = MulSS(q[3], qz2);
    const float wy = MulSS(q[3], qy2);
    const float wx = MulSS(q[3], qx2);
    const float xz = MulSS(q[0], qz2);
    const float yz = MulSS(q[1], qz2);

    m[0] = SubSS(1.0f, AddSS(qzsq, qysq));
    m[1] = AddSS(wz, xy);
    m[2] = SubSS(xz, wy);
    m[3] = SubSS(xy, wz);
    m[4] = SubSS(1.0f, AddSS(qzsq, qxsq));
    m[5] = AddSS(wx, yz);
    m[6] = AddSS(wy, xz);
    m[7] = SubSS(yz, wx);
    m[8] = SubSS(1.0f, AddSS(qysq, qxsq));
}

}  // namespace BoundsDetail

namespace BoundsDetail {

Particle* __cdecl Parent(Particle* particle)
{
    return particle->_parent;
}

Particle* __cdecl FirstChild(Particle* particle)
{
    return particle->_head;
}

Particle* __cdecl NextSibling(Particle* particle)
{
    return particle->_next;
}

ParticleBoundingVolume* __cdecl Volume(Particle* particle)
{
    return particle->GetBoundingVolume();
}

const Vector3& __cdecl Position(Particle* particle)
{
    return particle->GetPosition();
}

const Quaternion& __cdecl Orientation(Particle* particle)
{
    return particle->GetOrientation();
}

const Vector3& __cdecl GlobalPosition(Particle* particle)
{
    return particle->GetGlobalPosition();
}

const Quaternion& __cdecl GlobalOrientation(Particle* particle)
{
    return particle->GetGlobalOrientation();
}

void __cdecl LocalToGlobal(Particle* particle, Vector3& value)
{
    particle->LocalToGlobal(value);
}

void __cdecl LocalToGlobal(Particle* particle, OrientedBox& box)
{
    particle->LocalToGlobal(box);
}

void __cdecl GlobalToLocal(Particle* particle, Vector3& value)
{
    particle->GlobalToLocal(value);
}

void __cdecl GlobalToLocal(Particle* particle, OrientedBox& box)
{
    particle->GlobalToLocal(box);
}

}  // namespace BoundsDetail

namespace {

using BoundsDetail::AddSS;
using BoundsDetail::CopyX87;
using BoundsDetail::DivSS;
using BoundsDetail::MulSS;
using BoundsDetail::MultiplyQuaternions;
using BoundsDetail::QuaternionToMatrix3;
using BoundsDetail::RotateByQuaternion;
using BoundsDetail::SubSS;

// U1c's OrientedBox owns the transform, init and maximize bodies.
void TransformOrig(OrientedBox& box, const Matrix3& m, const Vector3& t)
{
    box.Transform(m, t);
}

void InitOrig(OrientedBox& box, int count, VertexItemPtr<const Vector3, VIPConstHelper> items)
{
    OrientedBoxAccess::Init(box, count, items);
}

void MaximizeOrig(OrientedBox& box, const OrientedBox& other)
{
    box.Maximize(other);
}

void CopyAabbX87(AxisAlignedBox& dst, const AxisAlignedBox& src)
{
    CopyX87(dst.min.x, src.min.x);
    CopyX87(dst.min.y, src.min.y);
    CopyX87(dst.min.z, src.min.z);
    CopyX87(dst.max.x, src.max.x);
    CopyX87(dst.max.y, src.max.y);
    CopyX87(dst.max.z, src.max.z);
}

void CopyQuatX87(Quaternion& dst, const Quaternion& src)
{
    CopyX87(dst.x, src.x);
    CopyX87(dst.y, src.y);
    CopyX87(dst.z, src.z);
    CopyX87(dst.w, src.w);
}

// the copy of the local oriented box inside GetChildBoundingOrientedBox: centre and extents are plain moves, the
// axes go through the x87 stack
void CopyObbPlainCentre(OrientedBox& dst, const OrientedBox& src)
{
    dst.center = src.center;
    for (int i = 0; i < 3; ++i) {
        CopyX87(dst.axis[i].x, src.axis[i].x);
        CopyX87(dst.axis[i].y, src.axis[i].y);
        CopyX87(dst.axis[i].z, src.axis[i].z);
    }
    dst.extents = src.extents;
}

}  // namespace

// ==== 0x10051e70 ParticleBoundingVolume::ParticleBoundingVolume(Particle*)
ParticleBoundingVolume::ParticleBoundingVolume(Particle* host_)
    : dirtyFlags(0xfffff),
      sphere(),
      globalSphereCenter(),
      axisAlignedBox(),
      orientedBox(),
      globalPosition(),
      globalOrientation(),
      host(host_),
      primaryVolumeType(PRIMARY_BOUNDING_VOLUME)
{
    sphere.center = Vector3{0.0f, 0.0f, 0.0f};
    sphere.radius = 0.0f;
    globalSphereCenter = Vector3{0.0f, 0.0f, 0.0f};
    axisAlignedBox.min = Vector3{0.0f, 0.0f, 0.0f};
    axisAlignedBox.max = Vector3{0.0f, 0.0f, 0.0f};
    orientedBox = OrientedBox(axisAlignedBox);
    globalPosition = Vector3{0.0f, 0.0f, 0.0f};
    // the original computes 1 / StdMath(1.0) and multiplies 0 by it: +0 for the three parts, 1 for w
    globalOrientation = Quaternion{0.0f, 0.0f, 0.0f, 1.0f};
}

// ==== 0x10006710 ParticleBoundingVolume::ParticleBoundingVolume(const ParticleBoundingVolume&)
ParticleBoundingVolume::ParticleBoundingVolume(const ParticleBoundingVolume& o)
    : dirtyFlags(o.dirtyFlags),
      sphere(),
      globalSphereCenter(),
      axisAlignedBox(),
      orientedBox(o.orientedBox),
      globalPosition(),
      globalOrientation(),
      host(o.host),
      primaryVolumeType(o.primaryVolumeType)
{
    // centre copies are plain moves in this constructor, radius and the frames go through the x87 stack
    sphere.center = o.sphere.center;
    CopyX87(sphere.radius, o.sphere.radius);
    globalSphereCenter = o.globalSphereCenter;
    CopyAabbX87(axisAlignedBox, o.axisAlignedBox);
    globalPosition = o.globalPosition;
    CopyQuatX87(globalOrientation, o.globalOrientation);
}

// ==== 0x10051f60 ParticleBoundingVolume::~ParticleBoundingVolume()
ParticleBoundingVolume::~ParticleBoundingVolume()
{
}

// ==== 0x10006800 ParticleBoundingVolume::operator=
ParticleBoundingVolume& ParticleBoundingVolume::operator=(const ParticleBoundingVolume& o)
{
    dirtyFlags = o.dirtyFlags;
    CopyX87(sphere.center.x, o.sphere.center.x);
    CopyX87(sphere.center.y, o.sphere.center.y);
    CopyX87(sphere.center.z, o.sphere.center.z);
    CopyX87(sphere.radius, o.sphere.radius);
    CopyX87(globalSphereCenter.x, o.globalSphereCenter.x);
    CopyX87(globalSphereCenter.y, o.globalSphereCenter.y);
    CopyX87(globalSphereCenter.z, o.globalSphereCenter.z);
    CopyAabbX87(axisAlignedBox, o.axisAlignedBox);
    orientedBox = o.orientedBox;
    CopyX87(globalPosition.x, o.globalPosition.x);
    CopyX87(globalPosition.y, o.globalPosition.y);
    CopyX87(globalPosition.z, o.globalPosition.z);
    CopyQuatX87(globalOrientation, o.globalOrientation);
    host = o.host;
    primaryVolumeType = o.primaryVolumeType;
    return *this;
}

// ==== 0x10006940 ParticleBoundingVolume::GetHost()
Particle* ParticleBoundingVolume::GetHost()
{
    return host;
}

const Particle* ParticleBoundingVolume::GetHost() const
{
    return host;
}

// ==== 0x10006950 ParticleBoundingVolume::IsFlagDirty(unsigned long) const
bool ParticleBoundingVolume::IsFlagDirty(unsigned long flag) const
{
    return (dirtyFlags & flag) != 0;
}

// ==== 0x10006970 ParticleBoundingVolume::GetDirtyFlags() const
unsigned long ParticleBoundingVolume::GetDirtyFlags() const
{
    return dirtyFlags;
}

// ==== 0x10006960 ParticleBoundingVolume::SetDirtyFlags(unsigned long)
void ParticleBoundingVolume::SetDirtyFlags(unsigned long flags)
{
    dirtyFlags |= flags;
}

// ==== 0x10006980 ParticleBoundingVolume::SetRawDirtyFlags(bool, unsigned long) const
void ParticleBoundingVolume::SetRawDirtyFlags(bool set, unsigned long flags) const
{
    unsigned long& value = const_cast<ParticleBoundingVolume*>(this)->dirtyFlags;
    if (set)
        value |= flags;
    else
        value &= ~flags;
}

// ==== 0x1005fec0 ParticleBoundingVolume::SetRawDirtyFlags(unsigned long) const
void ParticleBoundingVolume::SetRawDirtyFlags(unsigned long flags) const
{
    const_cast<ParticleBoundingVolume*>(this)->dirtyFlags = flags;
}

// ==== 0x100069a0 ParticleBoundingVolume::GetBoundingSphere(bool) const
const Sphere& ParticleBoundingVolume::GetBoundingSphere(bool recalculate) const
{
    if (recalculate && (dirtyFlags & 2)) RecalculateSphereVolume();
    return sphere;
}

// ==== 0x100069d0 ParticleBoundingVolume::GetGlobalBoundingSphereCenter(bool) const
const Vector3& ParticleBoundingVolume::GetGlobalBoundingSphereCenter(bool recalculate) const
{
    if (recalculate && (dirtyFlags & 3)) RecalculateSphereGlobalCenter();
    return globalSphereCenter;
}

// ==== 0x10006a00 ParticleBoundingVolume::GetOrientedBoundingBox(bool) const
const OrientedBox& ParticleBoundingVolume::GetOrientedBoundingBox(bool recalculate) const
{
    if (recalculate && (dirtyFlags & 4)) RecalculateOrientedBoxVolume();
    return orientedBox;
}

// ==== 0x10006a30 ParticleBoundingVolume::GetAxisAlignedBoundingBox(bool) const
const AxisAlignedBox& ParticleBoundingVolume::GetAxisAlignedBoundingBox(bool recalculate) const
{
    if (recalculate && (dirtyFlags & 8)) RecalculateAxisAlignedBoxVolume();
    return axisAlignedBox;
}

// ==== 0x10006a60 ParticleBoundingVolume::GetGlobalPosition() const
const Vector3& ParticleBoundingVolume::GetGlobalPosition() const
{
    if (dirtyFlags & 0x10) RecalculateGlobalPosition();
    return globalPosition;
}

// ==== 0x10006a80 ParticleBoundingVolume::GetGlobalOrientation() const
const Quaternion& ParticleBoundingVolume::GetGlobalOrientation() const
{
    if (dirtyFlags & 0x20) RecalculateGlobalOrientation();
    return globalOrientation;
}

// ==== 0x10006aa0 ParticleBoundingVolume::GetPrimaryVolumeType() const
ParticleBoundingVolume::PrimaryVolumeType ParticleBoundingVolume::GetPrimaryVolumeType() const
{
    return primaryVolumeType;
}

// ---- default virtuals of the base: the original's slots 1..18 are `xor eax,eax; ret` --------------

bool __cdecl ParticleBoundingVolume::IsWithinView(const ViewPoint&) const
{
    return false;
}

bool __cdecl ParticleBoundingVolume::IsWithinView(const Camera*) const
{
    return false;
}

bool __cdecl ParticleBoundingVolume::TestCollision(const Ray&, bool& hit) const
{
    hit = false;
    return false;
}

bool __cdecl ParticleBoundingVolume::TestCollision(const LineSegment&, bool& hit) const
{
    hit = false;
    return false;
}

void __cdecl ParticleBoundingVolume::SetLocalVolume(const OrientedBox&)
{
}

void __cdecl ParticleBoundingVolume::SetLocalVolume(const AxisAlignedBox&)
{
}

void __cdecl ParticleBoundingVolume::SetLocalVolume(const Sphere&)
{
}

void __cdecl ParticleBoundingVolume::GetLocalVolume(OrientedBox&) const
{
}

void __cdecl ParticleBoundingVolume::GetLocalVolume(AxisAlignedBox&) const
{
}

void __cdecl ParticleBoundingVolume::GetLocalVolume(Sphere&) const
{
}

ParticleBoundingVolume* __cdecl ParticleBoundingVolume::CreateInstance(Particle*) const
{
    return nullptr;
}

Render3DPrimitive* __cdecl ParticleBoundingVolume::CreateRenderableObject(RenderKernel*, const Color&) const
{
    return nullptr;
}

Render3DPrimitive* __cdecl ParticleBoundingVolume::CheckRenderableObjectPosition(RenderKernel*,
                                                                                 Render3DPrimitive*) const
{
    return nullptr;
}

void __cdecl ParticleBoundingVolume::RecalculateSphereVolume() const
{
}

void __cdecl ParticleBoundingVolume::RecalculateAxisAlignedBoxVolume() const
{
}

void __cdecl ParticleBoundingVolume::RecalculateOrientedBoxVolume() const
{
}

// ==== 0x10051f70 ParticleBoundingVolume::SetParentDirtyFlags(unsigned long)
void __cdecl ParticleBoundingVolume::SetParentDirtyFlags(unsigned long flags)
{
    for (Particle* p = host->_parent; p; p = p->_parent) p->GetBoundingVolume()->dirtyFlags |= flags;
}

// ==== 0x10051fa0 ParticleBoundingVolume::SetChildDirtyFlags(unsigned long)
void __cdecl ParticleBoundingVolume::SetChildDirtyFlags(unsigned long flags)
{
    for (Particle* child = host->_head; child; child = child->_next) {
        ParticleBoundingVolume* const volume = child->GetBoundingVolume();
        volume->dirtyFlags |= flags;
        volume->SetChildDirtyFlags(flags);
    }
}

// ==== 0x10051fe0 ParticleBoundingVolume::OnChildAdded / OnChildRemoved
void __cdecl ParticleBoundingVolume::OnChildAdded(Particle* child)
{
    dirtyFlags |= 0xf;
    for (Particle* p = host->_parent; p; p = p->_parent) p->GetBoundingVolume()->dirtyFlags |= 0xf;
    ParticleBoundingVolume* const volume = child->GetBoundingVolume();
    volume->dirtyFlags |= 0x39;
    volume->SetChildDirtyFlags(0x39);
}

void __cdecl ParticleBoundingVolume::OnChildRemoved(Particle* child)
{
    OnChildAdded(child);
}

// ==== 0x10052030 ParticleBoundingVolume::SetPositionChanged(const Vector3&, const Vector3&)
void __cdecl ParticleBoundingVolume::SetPositionChanged(const Vector3& oldPosition, const Vector3& newPosition)
{
    if ((dirtyFlags & 2) == 0) {
        sphere.center.x = AddSS(newPosition.x, SubSS(sphere.center.x, oldPosition.x));
        sphere.center.y = AddSS(newPosition.y, SubSS(sphere.center.y, oldPosition.y));
        sphere.center.z = AddSS(newPosition.z, SubSS(sphere.center.z, oldPosition.z));
    }
    for (Particle* p = host->_parent; p; p = p->_parent) p->GetBoundingVolume()->dirtyFlags |= 0xf;
    dirtyFlags |= 0x19;
    SetChildDirtyFlags(0x19);
}

// ==== 0x10052100 ParticleBoundingVolume::SetOrientationChanged(const Quaternion&, const Quaternion&)
void __cdecl ParticleBoundingVolume::SetOrientationChanged(const Quaternion& oldOrientation,
                                                           const Quaternion& newOrientation)
{
    if ((dirtyFlags & 2) == 0) {
        const Vector3& position = host->GetPosition();
        Vector3 delta{SubSS(sphere.center.x, position.x), SubSS(sphere.center.y, position.y),
                      SubSS(sphere.center.z, position.z)};

        const float sum = AddSS(AddSS(AddSS(MulSS(oldOrientation.y, oldOrientation.y),
                                            MulSS(oldOrientation.w, oldOrientation.w)),
                                      MulSS(oldOrientation.z, oldOrientation.z)),
                                MulSS(oldOrientation.x, oldOrientation.x));
        const float inv = DivSS(1.0f, sum);
        Quaternion inverse;
        inverse.x = MulSS(inv, SubSS(-0.0f, oldOrientation.x));
        inverse.y = MulSS(inv, SubSS(-0.0f, oldOrientation.y));
        inverse.z = MulSS(inv, SubSS(-0.0f, oldOrientation.z));
        inverse.w = MulSS(inv, oldOrientation.w);

        float rotated[3];
        RotateByQuaternion(rotated, &delta.x, &inverse.x);
        float finalRotated[3];
        RotateByQuaternion(finalRotated, rotated, &newOrientation.x);
        sphere.center.x = AddSS(position.x, finalRotated[0]);
        sphere.center.y = AddSS(position.y, finalRotated[1]);
        sphere.center.z = AddSS(position.z, finalRotated[2]);
    }
    for (Particle* p = host->_parent; p; p = p->_parent) p->GetBoundingVolume()->dirtyFlags |= 0xf;
    dirtyFlags |= 0x29;
    SetChildDirtyFlags(0x29);
}

// ==== 0x100522d0 ParticleBoundingVolume::SetPositionOrientationChanged
void __cdecl ParticleBoundingVolume::SetPositionOrientationChanged(const Vector3& oldPosition,
                                                                   const Vector3& newPosition,
                                                                   const Quaternion& oldOrientation,
                                                                   const Quaternion& newOrientation)
{
    if ((dirtyFlags & 2) == 0) {
        Vector3 delta{SubSS(sphere.center.x, oldPosition.x), SubSS(sphere.center.y, oldPosition.y),
                      SubSS(sphere.center.z, oldPosition.z)};

        const float sum = AddSS(AddSS(AddSS(MulSS(oldOrientation.y, oldOrientation.y),
                                            MulSS(oldOrientation.w, oldOrientation.w)),
                                      MulSS(oldOrientation.z, oldOrientation.z)),
                                MulSS(oldOrientation.x, oldOrientation.x));
        const float inv = DivSS(1.0f, sum);
        Quaternion inverse;
        inverse.x = MulSS(inv, SubSS(-0.0f, oldOrientation.x));
        inverse.y = MulSS(inv, SubSS(-0.0f, oldOrientation.y));
        inverse.z = MulSS(inv, SubSS(-0.0f, oldOrientation.z));
        inverse.w = MulSS(inv, oldOrientation.w);

        float rotated[3];
        RotateByQuaternion(rotated, &delta.x, &inverse.x);
        float finalRotated[3];
        RotateByQuaternion(finalRotated, rotated, &newOrientation.x);
        sphere.center.x = AddSS(newPosition.x, finalRotated[0]);
        sphere.center.y = AddSS(newPosition.y, finalRotated[1]);
        sphere.center.z = AddSS(newPosition.z, finalRotated[2]);
    }
    for (Particle* p = host->_parent; p; p = p->_parent) p->GetBoundingVolume()->dirtyFlags |= 0xf;
    dirtyFlags |= 0x39;
    SetChildDirtyFlags(0x39);
}

// ==== 0x100524a0 ParticleBoundingVolume::RecalculateGlobalPosition() const
void __cdecl ParticleBoundingVolume::RecalculateGlobalPosition() const
{
    Particle* const owner = host;
    Particle* const parent = owner->_parent;
    if (parent) {
        const Vector3& parentPosition = parent->GetGlobalPosition();
        ParticleBoundingVolume* const parentVolume = parent->GetBoundingVolume();
        if (parentVolume->dirtyFlags & 0x20) parentVolume->RecalculateGlobalOrientation();
        float rotated[3];
        RotateByQuaternion(rotated, &owner->GetPosition().x, &parentVolume->globalOrientation.x);
        const_cast<ParticleBoundingVolume*>(this)->globalPosition.x = AddSS(rotated[0], parentPosition.x);
        const_cast<ParticleBoundingVolume*>(this)->globalPosition.y = AddSS(rotated[1], parentPosition.y);
        const_cast<ParticleBoundingVolume*>(this)->globalPosition.z = AddSS(rotated[2], parentPosition.z);
    } else {
        const Vector3& position = owner->GetPosition();
        CopyX87(const_cast<ParticleBoundingVolume*>(this)->globalPosition.x, position.x);
        CopyX87(const_cast<ParticleBoundingVolume*>(this)->globalPosition.y, position.y);
        CopyX87(const_cast<ParticleBoundingVolume*>(this)->globalPosition.z, position.z);
    }
    const_cast<ParticleBoundingVolume*>(this)->dirtyFlags &= ~0x10u;
}

// ==== 0x10052570 ParticleBoundingVolume::RecalculateGlobalOrientation() const
void __cdecl ParticleBoundingVolume::RecalculateGlobalOrientation() const
{
    Particle* const owner = host;
    if (owner->_parent) {
        const Quaternion& parentOrientation = owner->_parent->GetGlobalOrientation();
        Quaternion result;
        MultiplyQuaternions(&result.x, &owner->GetOrientation().x, &parentOrientation.x);
        CopyQuatX87(const_cast<ParticleBoundingVolume*>(this)->globalOrientation, result);
    } else {
        CopyQuatX87(const_cast<ParticleBoundingVolume*>(this)->globalOrientation, owner->GetOrientation());
    }
    const_cast<ParticleBoundingVolume*>(this)->dirtyFlags &= ~0x20u;
}

// ==== 0x10052d50 ParticleBoundingVolume::RecalculateSphereGlobalCenter() const
void __cdecl ParticleBoundingVolume::RecalculateSphereGlobalCenter() const
{
    if (dirtyFlags & 2) RecalculateSphereVolume();
    const_cast<ParticleBoundingVolume*>(this)->globalSphereCenter =
        const_cast<const ParticleBoundingVolume*>(this)->sphere.center;
    if (host->_parent) host->_parent->LocalToGlobal(const_cast<ParticleBoundingVolume*>(this)->globalSphereCenter);
    const_cast<ParticleBoundingVolume*>(this)->dirtyFlags &= ~1u;
}

// ==== 0x10052600 ParticleBoundingVolume::GetChildBoundingSphere(Sphere&) const
void __cdecl ParticleBoundingVolume::GetChildBoundingSphere(Sphere& out) const
{
    out.center = Vector3{0.0f, 0.0f, 0.0f};
    out.radius = 0.0f;
    for (Particle* child = host->_head; child; child = child->_next) {
        ParticleBoundingVolume* const volume = child->GetBoundingVolume();
        if (volume->dirtyFlags & 2) volume->RecalculateSphereVolume();
        if (volume->sphere.radius > 0.0f) {
            if (!(out.radius > 0.0f)) {
                CopyX87(out.center.x, volume->sphere.center.x);
                CopyX87(out.center.y, volume->sphere.center.y);
                CopyX87(out.center.z, volume->sphere.center.z);
                CopyX87(out.radius, volume->sphere.radius);
            } else {
                out.Maximize(volume->sphere);
            }
        }
    }
    if (out.radius > 0.0f) {
        const Vector3& position = host->GetPosition();
        const Quaternion& orientation = host->GetOrientation();
        float rotated[3];
        RotateByQuaternion(rotated, &out.center.x, &orientation.x);
        out.center.x = AddSS(position.x, rotated[0]);
        out.center.y = AddSS(position.y, rotated[1]);
        out.center.z = AddSS(position.z, rotated[2]);
    }
}

// ==== 0x10052880 ParticleBoundingVolume::GetChildBoundingOrientedBox(OrientedBox&) const
void __cdecl ParticleBoundingVolume::GetChildBoundingOrientedBox(OrientedBox& out) const
{
    out.center = Vector3{0.0f, 0.0f, 0.0f};
    out.axis[0] = Vector3{1.0f, 0.0f, 0.0f};
    out.axis[1] = Vector3{0.0f, 1.0f, 0.0f};
    out.axis[2] = Vector3{0.0f, 0.0f, 1.0f};
    out.extents = Vector3{0.0f, 0.0f, 0.0f};

    for (Particle* child = host->_head; child; child = child->_next) {
        ParticleBoundingVolume* const volume = child->GetBoundingVolume();
        if (volume->dirtyFlags & 4) volume->RecalculateOrientedBoxVolume();
        const OrientedBox& childBox = volume->orientedBox;
        if (childBox.extents.x != 0.0f || childBox.extents.y != 0.0f || childBox.extents.z != 0.0f) {
            OrientedBox transformed = childBox;
            Matrix3 rotation;
            QuaternionToMatrix3(rotation.m, &child->GetOrientation().x);
            TransformOrig(transformed, rotation, child->GetPosition());

            if (out.IsEmpty()) {
                CopyObbPlainCentre(out, transformed);
            } else {
                Vector3 corners[16];
                out.CalcCorners(corners);
                transformed.CalcCorners(corners + 8);
                InitOrig(out, 16, VertexItemPtr<const Vector3, VIPConstHelper>{0xc, corners});
            }
        }
    }
}

// ==== 0x10052bf0 ParticleBoundingVolume::GetChildBoundingAxisAlignBox(AxisAlignedBox&) const
void __cdecl ParticleBoundingVolume::GetChildBoundingAxisAlignBox(AxisAlignedBox& out) const
{
    out.min = Vector3{0.0f, 0.0f, 0.0f};
    out.max = Vector3{0.0f, 0.0f, 0.0f};
    for (Particle* child = host->_head; child; child = child->_next) {
        ParticleBoundingVolume* const volume = child->GetBoundingVolume();
        if (volume->dirtyFlags & 8) volume->RecalculateAxisAlignedBoxVolume();
        const AxisAlignedBox& box = volume->axisAlignedBox;
        if (box.min.x != box.max.x || box.min.y != box.max.y || box.min.z != box.max.z) {
            if (out.IsEmpty()) {
                out = box;
            } else {
                if (out.min.x > box.min.x) out.min.x = box.min.x;
                if (out.min.y > box.min.y) out.min.y = box.min.y;
                if (out.min.z > box.min.z) out.min.z = box.min.z;
                if (box.max.x > out.max.x) out.max.x = box.max.x;
                if (box.max.y > out.max.y) out.max.y = box.max.y;
                if (box.max.z > out.max.z) out.max.z = box.max.z;
            }
        }
    }
}

// ==== 0x10052da0 ParticleBoundingVolume::InitInstance(ParticleBoundingVolume&) const
void __cdecl ParticleBoundingVolume::InitInstance(ParticleBoundingVolume& instance) const
{
    instance.dirtyFlags = 0xfffff;
}

}  // namespace Jet
