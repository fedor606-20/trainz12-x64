// Usage: diff_rs_u11.exe <bin dir>. Compare data exports, allowing the local Task update slot.
#include <windows.h>

#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "../graphics/RenderSystem_7c/DataMirrorList.h"

static HMODULE original, rebuilt;
static int checked;

static bool Check(const char* name, std::size_t size, bool meta)
{
    ++checked;
    const auto* a = reinterpret_cast<const unsigned char*>(GetProcAddress(original, name));
    const auto* b = reinterpret_cast<const unsigned char*>(GetProcAddress(rebuilt, name));
    if (!a || !b) {
        std::printf("missing export: %s\n", name);
        return false;
    }
    bool match = std::memcmp(a, b, size) == 0;
    if (std::strcmp(name, "??_7RenderKernel@Jet@@6BTask@1@@") == 0) {
        constexpr std::size_t updateSlot = 0x10;
        const char* update;
        std::memcpy(&update, b + updateSlot, sizeof update);
        HMODULE owner = nullptr;
        match = GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                       GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, update, &owner) &&
                owner == rebuilt && std::memcmp(a, b, updateSlot) == 0 &&
                std::memcmp(a + updateSlot + sizeof update, b + updateSlot + sizeof update,
                            size - updateSlot - sizeof update) == 0;
    }
    if (!match) {
        std::printf("data mismatch: %s (%zu bytes)\n", name, size);
        return false;
    }
    if (meta && std::memcmp(a - 4, b - 4, 4) != 0) {
        std::printf("RTTI locator mismatch: %s\n", name);
        return false;
    }
    return true;
}

// The default setters write the rebuilt DLL's own export; the original keeps its value.
static bool CheckIntSetter(const char* setter, const char* data, int value)
{
    const auto set = reinterpret_cast<void(__cdecl*)(int)>(GetProcAddress(rebuilt, setter));
    auto* const local = reinterpret_cast<int*>(GetProcAddress(rebuilt, data));
    auto* const live = reinterpret_cast<const int*>(GetProcAddress(original, data));
    if (!set || !local || !live) {
        std::printf("missing writer/export: %s\n", setter);
        return false;
    }
    const int savedLocal = *local;
    const int savedLive = *live;
    set(value);
    const bool wrote = *local == value && *live == savedLive;
    set(savedLocal);
    const bool restored = *local == savedLocal && *live == savedLive;
    if (!wrote || !restored) std::printf("setter %s crossed module state\n", setter);
    return wrote && restored;
}

static bool CheckWriterSync()
{
    struct IntCase { const char* setter; const char* data; int value; };
    const IntCase intCases[] = {
        {"?SetDefaultWrapS@TextureResource@Jet@@SAXW4TextureWrap@2@@Z", "?WRAP_S_DEFAULT@TextureResource@Jet@@0W4TextureWrap@2@A", 2},
        {"?SetDefaultWrapT@TextureResource@Jet@@SAXW4TextureWrap@2@@Z", "?WRAP_T_DEFAULT@TextureResource@Jet@@0W4TextureWrap@2@A", 1},
        {"?SetDefaultMinFilter@TextureResource@Jet@@SAXW4TextureFilter@2@@Z", "?MIN_FILTER_DEFAULT@TextureResource@Jet@@0W4TextureFilter@2@A", 2},
        {"?SetDefaultMagFilter@TextureResource@Jet@@SAXW4TextureFilter@2@@Z", "?MAG_FILTER_DEFAULT@TextureResource@Jet@@0W4TextureFilter@2@A", 3},
        {"?SetDefaultMipFilter@TextureResource@Jet@@SAXW4TextureFilter@2@@Z", "?MIP_FILTER_DEFAULT@TextureResource@Jet@@0W4TextureFilter@2@A", 1},
        {"?SetDefaultAutoMip@TextureResource@Jet@@SAXW4TextureAutoMip@2@@Z", "?AUTO_MIP_DEFAULT@TextureResource@Jet@@0W4TextureAutoMip@2@A", 2},
        {"?SetDefaultAlphaHint@TextureResource@Jet@@SAXW4TextureAlpha@2@@Z", "?ALPHA_HINT_DEFAULT@TextureResource@Jet@@0W4TextureAlpha@2@A", 1},
        {"?SetDefaultTextureHint@TextureResource@Jet@@SAXW4TextureHint@2@@Z", "?HINT_DEFAULT@TextureResource@Jet@@0W4TextureHint@2@A", 2},
        {"?SetDefaultDisplayFormatId@TextureResource@Jet@@SAXW4DFormatId@2@@Z", "?DISPLAYFORMATID_DEFAULT@TextureResource@Jet@@0W4DFormatId@2@A", 5},
        {"?SetMaxSizeTextureStreaming@TextureManager@Jet@@SAXH@Z", "?MAX_STREAMING_TEXTURE_SIZE@TextureManager@Jet@@2HA", 4096},
    };
    for (const IntCase& c : intCases)
        if (!CheckIntSetter(c.setter, c.data, c.value)) return false;

    const auto setDelayed = reinterpret_cast<void(__cdecl*)(bool)>(
        GetProcAddress(rebuilt, "?SetDelayedLoading@TextureRes@Jet@@SAX_N@Z"));
    auto* const delayed = reinterpret_cast<unsigned char*>(GetProcAddress(rebuilt, "?delayedLoading@TextureRes@Jet@@1_NA"));
    const auto* const delayedLive =
        reinterpret_cast<const unsigned char*>(GetProcAddress(original, "?delayedLoading@TextureRes@Jet@@1_NA"));
    if (!setDelayed || !delayed || !delayedLive) {
        std::puts("missing delayed-loading writer/export");
        return false;
    }
    const unsigned char savedDelayed = *delayed;
    const unsigned char savedDelayedLive = *delayedLive;
    setDelayed(savedDelayed == 0);
    const bool delayedWrote = *delayed != savedDelayed && *delayedLive == savedDelayedLive;
    setDelayed(savedDelayed != 0);
    if (!delayedWrote || *delayed != savedDelayed || *delayedLive != savedDelayedLive) return false;

    const char* const anisoSetter = "?SetDefaultAnisotropyFilterLevel@TextureResource@Jet@@SAXM@Z";
    const char* const anisoData = "?ANISOTROPY_DEFAULT@TextureResource@Jet@@0MA";
    const auto setAniso = reinterpret_cast<void(__cdecl*)(float)>(GetProcAddress(rebuilt, anisoSetter));
    auto* const aniso = reinterpret_cast<float*>(GetProcAddress(rebuilt, anisoData));
    const auto* const anisoLive = reinterpret_cast<const float*>(GetProcAddress(original, anisoData));
    if (!setAniso || !aniso || !anisoLive) {
        std::puts("missing anisotropy writer/export");
        return false;
    }
    const float savedAniso = *aniso;
    const float savedAnisoLive = *anisoLive;
    setAniso(2.5f);
    const bool anisoWrote = *aniso == 2.5f && *anisoLive == savedAnisoLive;
    setAniso(savedAniso);
    if (!anisoWrote || *aniso != savedAniso || *anisoLive != savedAnisoLive) return false;

    const char* const compSetter = "?SetDefaultFileCompression@TextureResource@Jet@@SAXPAD@Z";
    const char* const compData = "?FILE_COMPRESSION_DEFAULT@TextureResource@Jet@@0PADA";
    const auto setComp = reinterpret_cast<void(__cdecl*)(char*)>(GetProcAddress(rebuilt, compSetter));
    auto* const comp = reinterpret_cast<char*>(GetProcAddress(rebuilt, compData));
    const auto* const compLive = reinterpret_cast<const char*>(GetProcAddress(original, compData));
    if (!setComp || !comp || !compLive) {
        std::puts("missing file-compression writer/export");
        return false;
    }
    char savedComp[8];
    char savedCompLive[8];
    char valueComp[9] = "ABCD1234";
    std::memcpy(savedComp, comp, 8);
    std::memcpy(savedCompLive, compLive, 8);
    setComp(valueComp);
    const bool compWrote = std::memcmp(comp, valueComp, 8) == 0 && std::memcmp(compLive, savedCompLive, 8) == 0;
    setComp(savedComp);
    if (!compWrote || std::memcmp(comp, savedComp, 8) != 0 || std::memcmp(compLive, savedCompLive, 8) != 0) return false;

    std::puts("local writers update only their module's exports");
    return true;
}

// The default scalars start at the retail image's values in both modules.
static bool CheckInitialDefaults()
{
    struct Case { const char* name; int value; };
    const Case cases[] = {
        {"?WRAP_S_DEFAULT@TextureResource@Jet@@0W4TextureWrap@2@A", 1},
        {"?WRAP_T_DEFAULT@TextureResource@Jet@@0W4TextureWrap@2@A", 1},
        {"?MIN_FILTER_DEFAULT@TextureResource@Jet@@0W4TextureFilter@2@A", 1},
        {"?MAG_FILTER_DEFAULT@TextureResource@Jet@@0W4TextureFilter@2@A", 1},
        {"?MIP_FILTER_DEFAULT@TextureResource@Jet@@0W4TextureFilter@2@A", 0},
        {"?AUTO_MIP_DEFAULT@TextureResource@Jet@@0W4TextureAutoMip@2@A", 1},
        {"?ALPHA_HINT_DEFAULT@TextureResource@Jet@@0W4TextureAlpha@2@A", 3},
        {"?HINT_DEFAULT@TextureResource@Jet@@0W4TextureHint@2@A", 0},
        {"?DISPLAYFORMATID_DEFAULT@TextureResource@Jet@@0W4DFormatId@2@A", 1},
        {"?MAX_STREAMING_TEXTURE_SIZE@TextureManager@Jet@@2HA", 2048},
        {"?uniqueTexId@Texture@Jet@@1KA", 0},
    };
    for (const Case& c : cases) {
        const auto* local = reinterpret_cast<const int*>(GetProcAddress(rebuilt, c.name));
        const auto* live = reinterpret_cast<const int*>(GetProcAddress(original, c.name));
        if (!local || !live || *local != c.value || *live != c.value) {
            std::printf("unexpected initial value: %s\n", c.name);
            return false;
        }
    }
    const auto* aniso = reinterpret_cast<const float*>(GetProcAddress(rebuilt, "?ANISOTROPY_DEFAULT@TextureResource@Jet@@0MA"));
    if (!aniso || *aniso != 16.0f) return false;
    const auto* registrations = reinterpret_cast<const unsigned*>(GetProcAddress(rebuilt,
        "?registrations@VertexType@Jet@@1V?$HashTable@IPAURegistrationData@VertexType@Jet@@URegistrationDataOp@23@$0N@@2@A"));
    if (!registrations || !registrations[0] || registrations[1] != 0 || registrations[2] != 0x61) return false;
    return !*reinterpret_cast<void* const*>(GetProcAddress(rebuilt, "?display@VertexType@Jet@@1PBVDisplay@2@B"));
}

static bool CheckVisitCounter()
{
    const char* const name = "?currentVisited@Element@Jet@@0IA";
    if (!Check(name, sizeof(unsigned int), false)) return false;
    for (HMODULE module : {original, rebuilt}) {
        auto* const counter = reinterpret_cast<unsigned*>(GetProcAddress(module, name));
        auto* const other = reinterpret_cast<const unsigned*>(GetProcAddress(module == original ? rebuilt : original, name));
        const auto reset = reinterpret_cast<void(__cdecl*)()>(GetProcAddress(module, "?ResetVisited@Element@Jet@@SAXXZ"));
        const auto ctor = reinterpret_cast<void(__thiscall*)(void*)>(GetProcAddress(module, "??0Element@Jet@@QAE@XZ"));
        const auto dtor = reinterpret_cast<void(__thiscall*)(void*)>(GetProcAddress(module, "??1Element@Jet@@UAE@XZ"));
        const auto isVisited = reinterpret_cast<bool(__cdecl*)(const void*)>(
            GetProcAddress(module, "?IsVisited@Element@Jet@@QBA_NXZ"));
        if (!reset || !ctor || !dtor || !isVisited) return false;
        const unsigned before = *counter;
        const unsigned otherBefore = *other;
        alignas(void*) unsigned char element[0xc]{};
        ctor(element);
        reset();
        using Visit = void(__cdecl*)(const void*);
        const auto visit = (*reinterpret_cast<Visit**>(element))[1];
        const bool initiallyClear = !isVisited(element);
        visit(element);
        const bool set = isVisited(element);
        reset();
        const bool passed = initiallyClear && set && !isVisited(element) && *counter == before + 2 && *other == otherBefore;
        dtor(element);
        *counter = before;
        if (!passed) return false;
    }
    return true;
}

int main(int argc, char** argv)
{
    if (argc != 2 || !SetDllDirectoryA(argv[1])) {
        std::puts("usage: diff_rs_u11.exe <bin dir>");
        return 2;
    }
    original = LoadLibraryA("RenderSystem_7c_orig.dll");
    rebuilt = LoadLibraryA("RenderSystem_7c.dll");
    if (!original || !rebuilt) {
        std::printf("cannot load RenderSystem DLLs: %lu\n", GetLastError());
        return 2;
    }
#define X(var, name, size) if (!Check(name, size, false)) return 1;
RS_PLAIN_DATA(X)
#undef X
#define X(var, name, slots) if (!Check(name, slots * 4, true)) return 1;
RS_VFTABLE_DATA(X)
#undef X
#define X(var, name, words) if (!Check(name, words * 4, false)) return 1;
RS_VBTABLE_DATA(X)
#undef X
    if (!Check("?ANISOTROPY_DEFAULT_TOKEN_VALUE@TextureResource@Jet@@2MA", sizeof(float), false)) return 1;
    if (!Check("?NORMAL_MAP_HINT_DEFAULT@TextureResource@Jet@@0W4NormalMapHint@2@A", sizeof(int), false)) return 1;
    if (!Check("?globalSkipLevels@TextureManager@Jet@@1IA", sizeof(unsigned int), false)) return 1;
    if (!Check("?globalSkipThreshold@TextureManager@Jet@@1IA", sizeof(unsigned int), false)) return 1;
    if (!Check("?textureStats@DisplayTexture@Jet@@1UDisplayTextureStats@12@A", 0x30, false)) return 1;
    if (!Check("?MIN_STREAMING_TEXTURE_SIZE@TextureManager@Jet@@2HA", sizeof(int), false)) return 1;
    if (!Check("?MIN_STREAMING_TEXTURE_MIP_COUNT@TextureManager@Jet@@2HA", sizeof(int), false)) return 1;
    if (!Check("?MAX_STREAMING_TEXTURE_MIP_COUNT@TextureManager@Jet@@2HA", sizeof(int), false)) return 1;
    if (!Check("?TEXTURE_STREAMING_ENABLED@TextureManager@Jet@@2_NA", sizeof(bool), false)) return 1;
    if (!CheckInitialDefaults()) return 1;
    if (!CheckWriterSync()) return 1;
    if (!CheckVisitCounter()) {
        std::puts("Element visit counter did not use its own module's state");
        return 1;
    }
    std::printf("data exports: %d checks passed\n", checked);
    return 0;
}
