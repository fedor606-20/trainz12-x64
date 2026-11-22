// Usage: diff_rs_u11.exe <bin dir>. Compare every local data mirror with the original export bytes.
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
    if (std::memcmp(a, b, size) != 0) {
        std::printf("data mismatch: %s (%zu bytes)\n", name, size);
        return false;
    }
    if (meta && std::memcmp(a - 4, b - 4, 4) != 0) {
        std::printf("RTTI locator mismatch: %s\n", name);
        return false;
    }
    return true;
}

static bool CheckIntSetter(const char* setter, const char* data, int value)
{
    const auto set = reinterpret_cast<void(__cdecl*)(int)>(GetProcAddress(rebuilt, setter));
    auto* const live = reinterpret_cast<int*>(GetProcAddress(original, data));
    if (!set || !live) {
        std::printf("missing writer/export: %s\n", setter);
        return false;
    }
    const int saved = *live;
    set(value);
    if (*live != value || !Check(data, 4, false)) return false;
    set(saved);
    return *live == saved && Check(data, 4, false);
}

static bool CheckBoolSetter(const char* setter, const char* data)
{
    const auto set = reinterpret_cast<void(__cdecl*)(bool)>(GetProcAddress(rebuilt, setter));
    auto* const live = reinterpret_cast<unsigned char*>(GetProcAddress(original, data));
    if (!set || !live) {
        std::printf("missing writer/export: %s\n", setter);
        return false;
    }
    const unsigned char saved = *live;
    set(saved == 0);
    if (*live == saved || !Check(data, 1, false)) return false;
    set(saved != 0);
    return *live == saved && Check(data, 1, false);
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
        {"?SetMinSizeTextureStreaming@TextureManager@Jet@@SAXH@Z", "?MIN_STREAMING_TEXTURE_SIZE@TextureManager@Jet@@2HA", 128},
        {"?SetMaxSizeTextureStreaming@TextureManager@Jet@@SAXH@Z", "?MAX_STREAMING_TEXTURE_SIZE@TextureManager@Jet@@2HA", 4096},
    };
    for (const IntCase& c : intCases)
        if (!CheckIntSetter(c.setter, c.data, c.value)) return false;

    if (!CheckBoolSetter("?SetUsePath@AsciiTextureResource@Jet@@SAX_N@Z",
                         "?noPath@AsciiTextureResource@Jet@@1_NA")) return false;
    if (!CheckBoolSetter("?SetDelayedLoading@TextureRes@Jet@@SAX_N@Z",
                         "?delayedLoading@TextureRes@Jet@@1_NA")) return false;

    const char* const anisoSetter = "?SetDefaultAnisotropyFilterLevel@TextureResource@Jet@@SAXM@Z";
    const char* const anisoData = "?ANISOTROPY_DEFAULT@TextureResource@Jet@@0MA";
    const auto setAniso = reinterpret_cast<void(__cdecl*)(float)>(GetProcAddress(rebuilt, anisoSetter));
    auto* const aniso = reinterpret_cast<float*>(GetProcAddress(original, anisoData));
    if (!setAniso || !aniso) {
        std::puts("missing anisotropy writer/export");
        return false;
    }
    const float savedAniso = *aniso;
    setAniso(2.5f);
    if (*aniso != 2.5f || !Check(anisoData, 4, false)) return false;
    setAniso(savedAniso);
    if (*aniso != savedAniso || !Check(anisoData, 4, false)) return false;

    const char* const compSetter = "?SetDefaultFileCompression@TextureResource@Jet@@SAXPAD@Z";
    const char* const compData = "?FILE_COMPRESSION_DEFAULT@TextureResource@Jet@@0PADA";
    const auto setComp = reinterpret_cast<void(__cdecl*)(char*)>(GetProcAddress(rebuilt, compSetter));
    auto* const comp = reinterpret_cast<char*>(GetProcAddress(original, compData));
    if (!setComp || !comp) {
        std::puts("missing file-compression writer/export");
        return false;
    }
    char savedComp[8];
    char valueComp[9] = "ABCD1234";
    std::memcpy(savedComp, comp, 8);
    setComp(valueComp);
    if (std::memcmp(comp, valueComp, 8) != 0 || !Check(compData, 8, false)) return false;
    setComp(savedComp);
    if (std::memcmp(comp, savedComp, 8) != 0 || !Check(compData, 8, false)) return false;

    const char* const streamSetter = "?SetEnableTextureStreaming@TextureManager@Jet@@SAX_N@Z";
    const char* const streamData = "?TEXTURE_STREAMING_ENABLED@TextureManager@Jet@@2_NA";
    const auto setStream = reinterpret_cast<void(__cdecl*)(bool)>(GetProcAddress(rebuilt, streamSetter));
    auto* const stream = reinterpret_cast<unsigned char*>(GetProcAddress(original, streamData));
    if (!setStream || !stream) {
        std::puts("missing streaming writer/export");
        return false;
    }
    const unsigned char savedStream = *stream;
    setStream(savedStream == 0);
    if (*stream == savedStream || !Check(streamData, 1, false)) return false;
    setStream(savedStream);
    if (*stream != savedStream || !Check(streamData, 1, false)) return false;

    using Reset = void(__cdecl*)();
    const auto reset = reinterpret_cast<Reset>(GetProcAddress(rebuilt, "?ResetVisited@Element@Jet@@SAXXZ"));
    auto* const visited = reinterpret_cast<unsigned int*>(GetProcAddress(original, "?currentVisited@Element@Jet@@0IA"));
    if (!reset || !visited) {
        std::puts("missing currentVisited writer/export");
        return false;
    }
    const unsigned int before = *visited;
    reset();
    if (*visited != before + 1 || !Check("?currentVisited@Element@Jet@@0IA", 4, false)) return false;

    std::puts("mutable mirror writers refresh their exports");
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
    if (!CheckWriterSync()) return 1;
    std::printf("data mirrors: %d entries matching\n", checked);
    return 0;
}
