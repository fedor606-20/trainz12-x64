// WaterSystem U7a: the four water material factories' lifetime and IsSupported exports. The shared
// MaterialPlugin base stays the original's (called through its imports); only the derived vptr and virtuals
// are local. No U8 material code is touched here: CreateInstance keeps the original table slot.
#include "WaterMaterialInternal.h"

#include <cstdint>
#include <cstring>

namespace {

using BaseCtor = void (__thiscall WsMaterialPluginAbi::*)(const Jet::PString&, const Jet::UId&);
using BaseCopy = void (__thiscall WsMaterialPluginAbi::*)(const WsMaterialPluginAbi&);
using BaseAssign = WsMaterialPluginAbi& (__thiscall WsMaterialPluginAbi::*)(const WsMaterialPluginAbi&);
using BaseDtor = void (__thiscall WsMaterialPluginAbi::*)();
using DeleteFn = void(__cdecl*)(void*);

void* const EmptyPStringAndUId[8] = {};  // two zeroed objects: an empty PString and a null UId

void SetVtable(void* self, void* table)
{
    WsEnsureMirrors();
    *reinterpret_cast<void**>(self) = table;
}

void BaseConstruct(void* self, const Jet::PString& name, const Jet::UId& id)
{
    auto* const base = reinterpret_cast<WsMaterialPluginAbi*>(self);
    (base->*WsBind<BaseCtor>(WsMatPluginCtorImport))(name, id);
}

void BaseCopyConstruct(void* self, const void* other)
{
    auto* const base = reinterpret_cast<WsMaterialPluginAbi*>(self);
    (base->*WsBind<BaseCopy>(WsMatPluginCopyImport))(*static_cast<const WsMaterialPluginAbi*>(other));
}

void* BaseAssignImpl(void* self, const void* other)
{
    auto* const base = reinterpret_cast<WsMaterialPluginAbi*>(self);
    (base->*WsBind<BaseAssign>(WsMatPluginAssignImport))(*static_cast<const WsMaterialPluginAbi*>(other));
    return self;
}

void BaseDestroy(void* self)
{
    auto* const base = reinterpret_cast<WsMaterialPluginAbi*>(self);
    (base->*WsBind<BaseDtor>(WsMatPluginDtorImport))();
}

void VectorDestroy(void* self, unsigned flags, void* vtable)
{
    SetVtable(self, vtable);
    if (flags & 2) {
        unsigned* const base = static_cast<unsigned*>(self);
        const unsigned count = base[-1];
        for (unsigned i = 0; i < count; ++i)
            BaseDestroy(reinterpret_cast<unsigned char*>(self) + i * sizeof(WsMaterialPluginAbi));
    } else {
        BaseDestroy(self);
    }
    if (flags & 3)
        reinterpret_cast<DeleteFn>(WsAllocDeleteImport)(self);
}

void* EmptyName()
{
    return const_cast<void*>(static_cast<const void*>(EmptyPStringAndUId));
}

void* EmptyId()
{
    return const_cast<void*>(static_cast<const void*>(EmptyPStringAndUId + 4));
}

// the names below are alias targets for the original exports; their own mangled names need not match
#define WS_DEFAULT_CTOR(cls, vt)                          \
    cls::cls()                                            \
    {                                                     \
        WsEnsureMirrors();                                \
        BaseConstruct(this, *static_cast<const Jet::PString*>(EmptyName()), \
                      *static_cast<const Jet::UId*>(EmptyId())); \
        SetVtable(this, vt);                              \
    }

#define WS_NAMED_CTOR(cls, vt)                            \
    cls::cls(const Jet::PString& name, const Jet::UId& id) \
    {                                                     \
        WsEnsureMirrors();                                \
        BaseConstruct(this, name, id);                    \
        SetVtable(this, vt);                              \
    }

#define WS_COPY_CTOR(cls, vt)                             \
    cls::cls(const cls& other)                            \
    {                                                     \
        WsEnsureMirrors();                                \
        BaseCopyConstruct(this, &other);                  \
        SetVtable(this, vt);                              \
    }

#define WS_DTOR(cls, vt)                                  \
    cls::~cls()                                           \
    {                                                     \
        WsEnsureMirrors();                                \
        SetVtable(this, vt);                              \
        BaseDestroy(this);                                \
    }

#define WS_ASSIGN(cls)                                    \
    cls& cls::operator=(const cls& other)                 \
    {                                                     \
        WsEnsureMirrors();                                \
        BaseAssignImpl(this, &other);                     \
        return *this;                                     \
    }

}  // namespace

// Export alias targets. They live outside the anonymous namespace so the linker cannot discard unreferenced
// member functions; each one is aliased to a real WaterSystem export in implemented.txt.
class WsBumpDefaultCtor { public: WsBumpDefaultCtor(); };
class WsBumpNamedCtor { protected: WsBumpNamedCtor(const Jet::PString&, const Jet::UId&); };
class WsBumpCopyCtor { public: WsBumpCopyCtor(const WsBumpCopyCtor&); };
class WsBumpDtor { public: virtual ~WsBumpDtor(); };
class WsBumpAssign { public: WsBumpAssign& operator=(const WsBumpAssign&); };

WS_DEFAULT_CTOR(WsBumpDefaultCtor, WsBumpVftable)
WS_NAMED_CTOR(WsBumpNamedCtor, WsBumpVftable)
WS_COPY_CTOR(WsBumpCopyCtor, WsBumpVftable)
WS_DTOR(WsBumpDtor, WsBumpVftable)
WS_ASSIGN(WsBumpAssign)

class WsOneTexCopyCtor { public: WsOneTexCopyCtor(const WsOneTexCopyCtor&); };
class WsOneTexDefaultCtor { public: WsOneTexDefaultCtor(); };
class WsOneTexDtor { public: virtual ~WsOneTexDtor(); };
class WsOneTexAssign { public: WsOneTexAssign& operator=(const WsOneTexAssign&); };

WS_COPY_CTOR(WsOneTexCopyCtor, WsOneTexVftable)
WS_DEFAULT_CTOR(WsOneTexDefaultCtor, WsOneTexVftable)
WS_DTOR(WsOneTexDtor, WsOneTexVftable)
WS_ASSIGN(WsOneTexAssign)

class WsOceanNamedCtor { protected: WsOceanNamedCtor(const Jet::PString&, const Jet::UId&); };
class WsOceanCopyCtor { public: WsOceanCopyCtor(const WsOceanCopyCtor&); };
class WsOceanDefaultCtor { public: WsOceanDefaultCtor(); };
class WsOceanDtor { public: virtual ~WsOceanDtor(); };
class WsOceanAssign { public: WsOceanAssign& operator=(const WsOceanAssign&); };

WS_NAMED_CTOR(WsOceanNamedCtor, WsOceanVftable)
WS_COPY_CTOR(WsOceanCopyCtor, WsOceanVftable)
WS_DEFAULT_CTOR(WsOceanDefaultCtor, WsOceanVftable)
WS_DTOR(WsOceanDtor, WsOceanVftable)
WS_ASSIGN(WsOceanAssign)

// 0x10015a70: both flags in the Display's 0x211/0x212 bytes are needed
bool __cdecl WsVfThunks::SupportedByDisplay(Jet::Display* display, Jet::PString&)
{
    const auto* bytes = reinterpret_cast<const unsigned char*>(display);
    return bytes[0x211] != 0 && bytes[0x212] != 0;
}

// 0x10013f10, shared by FresWave and OneTex
bool __cdecl WsVfThunks::SupportedAlways(Jet::Display*, Jet::PString&) { return true; }

void* __thiscall WsVfThunks::BumpVectorDtor(unsigned flags)
{
    void* const self = this;
    WsEnsureMirrors();
    VectorDestroy(self, flags, WsBumpVftable);
    return self;
}

void* __thiscall WsVfThunks::OneTexVectorDtor(unsigned flags)
{
    void* const self = this;
    WsEnsureMirrors();
    VectorDestroy(self, flags, WsOneTexVftable);
    return self;
}

void* __thiscall WsVfThunks::OceanVectorDtor(unsigned flags)
{
    void* const self = this;
    WsEnsureMirrors();
    VectorDestroy(self, flags, WsOceanVftable);
    return self;
}
