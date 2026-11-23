#pragma once

#include <windows.h>
#include <tlhelp32.h>
#include <cwchar>

namespace Jet {

// The game's Win32 CRT preserves the original x87 behavior. Do not resolve its
// entry point through RenderSystem_7c_orig.dll's import address table.
inline FARPROC FindLegacyCiPow()
{
    if (const auto crt = GetModuleHandleA("msvcr80.dll"))
        if (const auto power = GetProcAddress(crt, "_CIpow")) return power;

    // SxS may load the legacy CRT in a different activation context.
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
    if (snapshot == INVALID_HANDLE_VALUE) return nullptr;
    MODULEENTRY32W entry{};
    entry.dwSize = sizeof entry;
    FARPROC power = nullptr;
    for (BOOL ok = Module32FirstW(snapshot, &entry); ok; ok = Module32NextW(snapshot, &entry)) {
        if (_wcsicmp(entry.szModule, L"msvcr80.dll") == 0) {
            power = GetProcAddress(entry.hModule, "_CIpow");
            if (power) break;
        }
    }
    CloseHandle(snapshot);
    return power;
}

} // namespace Jet
