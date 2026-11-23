// U16: the local owner of the SharedTextureCollection data exports and the format-name table
// TextureManager::Init reads. Neither is copied from the original module any more.
#include "TextureData.h"

#include <jet/PString.h>

#include <windows.h>

#include <new>

#pragma warning(disable : 4073)  // initializers put in library initialization area
#pragma init_seg(lib)

// Exported as ?textureArray@... / ?textureNameArray@... by implemented.txt. Keep the exact
// unsigned char[44] type: the .def alias names the mangled array symbol.
alignas(4) unsigned char RsRsD068[44];
alignas(4) unsigned char RsRsD069[44];

namespace Jet {
namespace TextureData {

namespace {

constexpr int kSharedTextureCount = 11;
static_assert(sizeof(RsRsD068) == kSharedTextureCount * 4);
static_assert(sizeof(RsRsD069) == kSharedTextureCount * 4);

// The eleven names the retail static initializer builds, in SharedTextureID order.
const char* const kSharedTextureNames[kSharedTextureCount] = {
    "AuranJetTexture:Default", "AuranJetTexture:DefaultBump", "AuranJetTexture:SubstituteLoadTexture",
    "AuranJetTexture:D2Attenuation", "AuranJetTexture:SpotCubeMap", "AuranJetTexture:NormCubeMap",
    "AuranJetTexture:Pow8", "AuranJetTexture:Pow16", "AuranJetTexture:Pow32",
    "AuranJetTexture:SpecPow16", "AuranJetTexture:SpecPow32"};

// 0 = no names yet, 1 = one thread constructing, 2 = ready.
volatile LONG sharedNamesState = 0;

}  // namespace

const FormatName kFormatNames[16] = {
    {"DXT1", 10}, {"DXT3", 11},  {"DXT5", 12},    {"ATC", 25},     {"ATCE", 26},    {"ATCI", 27},
    {"ETC1", 28}, {"ARGB8888", 1}, {"ABGR8888", 2}, {"RGB0888", 3}, {"BGR0888", 4}, {"RGB888", 5},
    {"BGR888", 6}, {"ARGB4444", 7}, {"RGB565", 8}, {"RGB555", 9},
};
const char kDefaultFormatName[] = "ARGB8888";

// The names are interned PStrings, so they need the KernelStandard cache, which exists once that
// module is loaded. The load-time attempt below normally does the work; a later first use retries.
void EnsureSharedNames()
{
    for (;;) {
        const LONG state = sharedNamesState;
        if (state == 2) return;
        if (state == 0) {
            if (InterlockedCompareExchange(&sharedNamesState, 1, 0) != 0) continue;
            if (!PString::Cache()) {
                InterlockedExchange(&sharedNamesState, 0);
                return;
            }
            auto* const names = reinterpret_cast<PString*>(RsRsD069);
            for (int i = 0; i < kSharedTextureCount; ++i) ::new (names + i) PString(kSharedTextureNames[i]);
            InterlockedExchange(&sharedNamesState, 2);
            return;
        }
        Sleep(0);
    }
}

Texture*& SharedTextureSlot(int id)
{
    return reinterpret_cast<Texture**>(RsRsD068)[id];
}

const PString& SharedTextureName(int id)
{
    EnsureSharedNames();
    return reinterpret_cast<const PString*>(RsRsD069)[id];
}

namespace {

struct SharedNamesAtLoad {
    SharedNamesAtLoad() { EnsureSharedNames(); }
} sharedNamesAtLoad;

}  // namespace

}  // namespace TextureData
}  // namespace Jet
