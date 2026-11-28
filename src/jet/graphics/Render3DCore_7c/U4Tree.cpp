// U4 wave 4: the CacheBucket red-black tree. The three functions are exact assembly transcriptions in
// U4TreeAsm.cpp; these wrappers give them the class names and ABI.
#include <jet/Render3DCoreCaches.h>

namespace Jet {

typedef AlphaBlendCache::CacheRecord CacheRecord;

}  // namespace Jet

extern "C" void* __cdecl U4TreeRotateTree(void* bucket, Jet::AlphaBlendCache::CacheRecord* a,
                                          Jet::AlphaBlendCache::CacheRecord* b);
extern "C" void __cdecl U4TreeSplitTree(void* bucket, Jet::AlphaBlendCache::CacheRecord* p1,
                                        Jet::AlphaBlendCache::CacheRecord* p2, Jet::AlphaBlendCache::CacheRecord* p3,
                                        Jet::AlphaBlendCache::CacheRecord* p4, Jet::AlphaBlendCache::CacheRecord* p5);
extern "C" void* __cdecl U4TreeInsert(void* bucket, Jet::AlphaBlendCache::CacheRecord* record);

namespace Jet {

AlphaBlendCache::CacheRecord* __cdecl AlphaBlendCache::CacheBucket::RotateTree(CacheRecord* a, CacheRecord* b)
{
    return static_cast<CacheRecord*>(U4TreeRotateTree(this, a, b));
}

void __cdecl AlphaBlendCache::CacheBucket::SplitTree(CacheRecord* p1, CacheRecord* p2, CacheRecord* p3,
                                                     CacheRecord* p4, CacheRecord* p5)
{
    U4TreeSplitTree(this, p1, p2, p3, p4, p5);
}

AlphaBlendCache::CacheRecord* __cdecl AlphaBlendCache::CacheBucket::Insert(CacheRecord* newRecord)
{
    return static_cast<CacheRecord*>(U4TreeInsert(this, newRecord));
}

}  // namespace Jet
