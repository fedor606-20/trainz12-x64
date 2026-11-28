// Render3DCore_7c unit U9b2: the bone/skeleton modifiers left forwarded by U9b.
//
// Layouts and vtable order follow the TS12 assembly (notes/agents/r3d_workstreams/u9b_scene_asm.txt) and the
// original vftables at RVAs 0x7e36c (BoneModifier), 0x7e3d0/0x7e400 (SkeletonModifier for ModifierList/Modifier) and
// 0x7e428 (BoneBlender). The SDK RC1 headers give the source-level shape; TS12 changed some signatures (BoneBlender
// no longer derives from ModifierList and its methods take BoneModifier*/bool, SkeletonModifier is the multiple
// inheritance of ModifierList and Modifier).
//
// Ordinary members and virtuals are __cdecl (this on the stack) except where the export is __thiscall:
// BoneBlender::RemoveAll is __thiscall like the original's U A E mangling. Constructors, assignments and destructors
// are __thiscall.
//
// BoneModifier's vtable slots 1..7 are import thunks to RenderSystem_7c's Modifier methods, slot 0 is the deleting
// destructor, slots 8/9 and 17..20/23 are the original's shared "return 0" stub (EnumerateBones, the SetInfluence
// family and UpdateBone) or fldz (the influence getters). The derived modifiers that override EnumerateBones live in
// other modules; the base stays instantiable as in the original.
#pragma once

#include <jet/DynamicArray.h>
#include <jet/FastDynamicArray.h>
#include <jet/KernelStandard.h>
#include <jet/Modifier.h>
#include <jet/PString.h>
#include <jet/Render3DCoreParticle.h>

namespace Jet {

class BoneBlender;
class RenderKernel;
class SkeletonModifier;

// ---- BoneModifier (0x20 bytes: Modifier 0x18, root-bone PString +0x18, update flags +0x1c) ---------------------

class BoneModifier : public Modifier {
public:
    enum UpdateFlag {
        NONE = 0x0,
        FIXED = 0x1,
        FINAL = 0x2,
        ALWAYS = 0x4,
    };

    static PString& nullString;

    BoneModifier();
    BoneModifier(const PString& name, float priority, const PString& rootBone, unsigned long updateFlags);
    BoneModifier(ModifierHost* host, const PString& name, float priority, const PString& rootBone,
                 unsigned long updateFlags);
    BoneModifier(RenderKernel* kernel, const PString& name, float priority, const PString& rootBone,
                 unsigned long updateFlags);
    BoneModifier(const BoneModifier& o);
    ~BoneModifier() override;
    BoneModifier& operator=(const BoneModifier& o);

    // Modifier's slot 8 is the original's shared "return 0" stub in the BoneModifier vtable (0x100079e0), not the
    // RenderSystem import thunk, so the class overrides it.
    Modifier::Result __cdecl Update(float dt) override;

    virtual void __cdecl EnumerateBones(DynamicArray<PString, 13, Less<PString, PString>>& boneList) const;  // 9
    virtual SkeletonModifier* __cdecl GetSkeleton();                             // 10
    virtual PString __cdecl GetRootBone() const;                                 // 11
    virtual void __cdecl SetRootBone(const PString& rootBone);                   // 12
    virtual float __cdecl GetInfluence(const PString& boneName) const;           // 13
    virtual float __cdecl GetInfluenceLimit(const PString& boneName) const;      // 14
    virtual float __cdecl GetInfluenceInTime(const PString& boneName) const;     // 15
    virtual float __cdecl GetInfluenceOutTime(const PString& boneName) const;    // 16
    virtual void __cdecl SetInfluence(const PString& boneName, float influence);  // 17
    virtual void __cdecl SetInfluenceLimit(const PString& boneName, float limit);  // 18
    virtual void __cdecl SetInfluenceInTime(const PString& boneName, float time);  // 19
    virtual void __cdecl SetInfluenceOutTime(const PString& boneName, float time);  // 20
    virtual bool __cdecl GetUpdateFlag(unsigned long flag) const;                // 21
    virtual void __cdecl SetUpdateFlag(unsigned long flag, bool set);            // 22
    virtual void __cdecl UpdateBone(const PString& boneName, Particle* bone, float dt, bool visible,
                                    float influence);                            // 23

    JET_HEAP_OBJECT

protected:
    PString rootBone;          // +0x18
    unsigned long updateFlags; // +0x1c
};
JET_LAYOUT32(sizeof(BoneModifier) == 0x20);

// ---- BoneBlender (0x14 bytes: vftable, host +4, bone name +8, bone +0xc, sorted BoneModifier* array +0x10) -----

// The original's vftable starts with Add (no virtual destructor) and its overloads are in MSVC's reverse order:
// 0 Add, 1 InsertBefore(m, before), 2 InsertBefore(m), 3 InsertAfter(m, after), 4 InsertAfter(m), 5 Remove(name),
// 6 Remove(modifier), 7 RemoveAll, 8 UpdateBoneModifiers. The exported names keep the plain declaration order.
class BoneBlender {
public:
    BoneBlender(SkeletonModifier* host, const PString& boneName, Particle* bone);
    BoneBlender(const BoneBlender& o);
    ~BoneBlender();
    BoneBlender& operator=(const BoneBlender& o);

    virtual bool __cdecl Add(BoneModifier* modifier);                                  // 0
    virtual bool __cdecl InsertBefore(BoneModifier* modifier);                         // 2
    virtual bool __cdecl InsertBefore(BoneModifier* modifier, const BoneModifier* before);  // 1
    virtual bool __cdecl InsertAfter(BoneModifier* modifier);                          // 4
    virtual bool __cdecl InsertAfter(BoneModifier* modifier, const BoneModifier* after);    // 3
    // MSVC puts each overload set in reverse declaration order, as the original's vtable shows: 5 Remove(name),
    // 6 Remove(modifier).
    virtual bool __cdecl Remove(const Modifier* modifier);                             // 6
    virtual bool __cdecl Remove(const PString& name);                                  // 5
    virtual void __thiscall RemoveAll();                                               // 7
    virtual bool __cdecl UpdateBoneModifiers(float dt);                                // 8

    const PString& __cdecl GetBoneName() const;
    Particle* __cdecl GetBone();
    const Particle* __cdecl GetBone() const;
    void __cdecl SetBone(Particle* bone);
    BoneModifier* __cdecl GetFirstReplaceable(float priority);

    JET_HEAP_OBJECT

private:
    SkeletonModifier* host;                       // +0x04
    PString boneName;                             // +0x08
    Particle* bone;                               // +0x0c
    FastDynamicArray<BoneModifier*> modifiers;    // +0x10
};
JET_LAYOUT32(sizeof(BoneBlender) == 0x14);

// ---- SkeletonModifier (0x34 bytes: ModifierList, Modifier at +0x18, BoneBlender* array at +0x30) ---------------

// Multiple inheritance, ModifierList first and Modifier second: the two exported vftables are
// ??_7SkeletonModifier@Jet@@6BModifierList@1@@ and ??_7SkeletonModifier@Jet@@6BModifier@1@@.
class SkeletonModifier : public ModifierList, public Modifier {
public:
    SkeletonModifier(Particle* particle, float priority);
    SkeletonModifier(Particle* particle, ModifierHost* host, float priority);
    SkeletonModifier(Particle* particle, RenderKernel* kernel, float priority);
    SkeletonModifier(const SkeletonModifier& o);
    ~SkeletonModifier() override;
    SkeletonModifier& operator=(const SkeletonModifier& o);

    // ModifierList
    ModifierHost::Result __cdecl Add(Modifier* modifier) override;
    ModifierHost::Result __cdecl InsertBefore(Modifier* modifier) override;
    ModifierHost::Result __cdecl InsertBefore(Modifier* modifier, const Modifier* before) override;
    ModifierHost::Result __cdecl InsertAfter(Modifier* modifier) override;
    ModifierHost::Result __cdecl InsertAfter(Modifier* modifier, const Modifier* after) override;
    ModifierHost::Result __cdecl Remove(const Modifier* modifier) override;
    ModifierHost::Result __cdecl Remove(const PString& name) override;
    void __cdecl RemoveAll() override;
    ModifierHost::Result __cdecl UpdateModifiers(float dt) override;

    // Modifier
    Modifier::Result __cdecl Update(float dt) override;

    PString __cdecl GetRootBone() const;
    int __cdecl GetNumBones() const;
    bool __cdecl RemoveBlender(BoneBlender* blender, bool deleteBlender);
    unsigned int __cdecl RemoveBlenders(const FastDynamicArray<BoneBlender*>& blenders, bool deleteBlenders);
    void __cdecl SwitchParticles(Particle* particle, FastDynamicArray<BoneBlender*>* old,
                                 FastDynamicArray<BoneBlender*>* current);

    JET_HEAP_OBJECT

protected:
    ModifierHost::Result __cdecl AddToBoneBlenders(Modifier* modifier, const Modifier* other, bool after);
    void __cdecl CreateBlenders(Particle* particle);
    BoneBlender* __cdecl GetBoneBlender(const PString& name);
    void __cdecl UnlinkBlenders();

private:
    FastDynamicArray<BoneBlender*> blenders;  // +0x30
};
JET_LAYOUT32(sizeof(SkeletonModifier) == 0x34);

}  // namespace Jet
