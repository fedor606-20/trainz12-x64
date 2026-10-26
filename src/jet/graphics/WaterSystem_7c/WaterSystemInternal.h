// Shared helpers of the WaterSystem_7c units. Nothing here is exported; the original inlined these patterns.
#pragma once

#include <jet/PString.h>

namespace Jet {

class ResourceDatabase;

// The original calls db's vtable slot 3 through its raw shape: a __cdecl function taking the object, an out
// node and the name on the stack (this matches ResourceDatabase's exported virtuals, which are __cdecl with
// `this` pushed; slot 3 is GetFileName, whose by-value PString return slot the caller reuses). Both the return
// value and *out are used, and *out is released like a PString node.
using DbSlot3 = const PString*(__cdecl*)(const ResourceDatabase* db, PStringCache::Node** out, const PString& name);

inline const PString* CallDbSlot3(const ResourceDatabase* db, PStringCache::Node** out, const PString& name)
{
    return (*reinterpret_cast<DbSlot3* const*>(db))[3](db, out, name);
}


// the original passes the MSVC 2005 array-new overflow guard through to Jet::Alloc::New: the byte count wraps as
// u32 and becomes -1 when it hits the top bits (4-byte elements fail at 2^30 bytes, 8-byte at 2^29)
inline unsigned int GridNewBytes(unsigned int count, unsigned int elementSize)
{
    unsigned int bytes = count * elementSize;
    if (static_cast<unsigned long long>(bytes) >> (elementSize == 8 ? 29 : 30))
        bytes = ~0u;
    return bytes;
}

}  // namespace Jet
