#include <jet/PString.h>
#include <new>

namespace Jet {
void* __cdecl RsNullMaterialType(void* object) { return static_cast<unsigned char*>(object) + 0x9c; }
void __cdecl RsNullMaterialSetType(void* object, const PString*)
{
    auto* target = reinterpret_cast<PString*>(static_cast<unsigned char*>(object) + 0x9c);
    // TS12's implementation reassigns the existing type instead of reading the supplied argument.
    target->~PString();
    ::new (target) PString(*target);
}
void __cdecl RsNullMaterialNoop() {}
void* __cdecl RsNullMaterialTexture() { return nullptr; }
int* __cdecl RsMaterialUnsupported(void*, int* result) { *result = 2; return result; }
}
