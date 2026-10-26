cmake_minimum_required(VERSION 3.28)
project(trainz12_jet CXX)

# Drop-in replacements for the Jet engine modules of Trainz 2012.
# Built 32-bit (-A Win32) to run inside the original game; the x64 build comes when nothing is forwarded.

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")  # static CRT: no runtime DLL to ship next to the game
set(CMAKE_CXX_STANDARD 20)
set(PY python)
set(TOOLS "D:/universal modder/trainz12-x64/tools")
set(EXPORTS "D:/trainz12-decomp/exports")
set(IMPLIB "D:/trainz12-decomp/implib")  # import libs of the original Jet modules (tools/gen_implib.py)

# module sources sit in a folder per README group: core/, input/, graphics/ (later ui/, debug/)
function(jet_module name)
    file(GLOB dir LIST_DIRECTORIES true "${CMAKE_CURRENT_SOURCE_DIR}/*/${name}")
    set(def "${CMAKE_CURRENT_BINARY_DIR}/${name}.def")
    set(impl "${dir}/implemented.txt")
    set(fwd "${CMAKE_CURRENT_BINARY_DIR}/${name}_forwards.cpp")
    add_custom_command(
        OUTPUT ${def} ${fwd}
        COMMAND ${PY} -I "${TOOLS}/gen_def.py" "${EXPORTS}/${name}.tsv" ${name} "${impl}" ${def}
        DEPENDS "${TOOLS}/gen_def.py" "${EXPORTS}/${name}.tsv" "${impl}"
        VERBATIM)
    file(GLOB_RECURSE srcs CONFIGURE_DEPENDS "${dir}/*.cpp")
    add_library(${name} SHARED ${srcs} ${def} ${fwd})
    target_include_directories(${name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
    # same base and layout habits as the original MSVC 2005 build
    target_compile_options(${name} PRIVATE /W3 /EHsc /GR /Zc:wchar_t- /Zp8)
    target_link_options(${name} PRIVATE /DYNAMICBASE:NO /BASE:0x10000000)
endfunction()

if(CMAKE_SIZEOF_VOID_P EQUAL 4)  # 32-bit: drop-in modules for the game and tests against the originals

jet_module(Kernel_7c)
target_compile_definitions(Kernel_7c PRIVATE _CRT_SECURE_NO_WARNINGS)
target_link_libraries(Kernel_7c PRIVATE "${IMPLIB}/KernelStandard_7c.lib" ole32)

# KernelStandard_7c: replaced top-down, one unit at a time (notes/KernelStandard.md); the rest forwards to _orig
jet_module(KernelStandard_7c)
target_link_options(KernelStandard_7c PRIVATE /MAP)
target_compile_definitions(KernelStandard_7c PRIVATE _CRT_SECURE_NO_WARNINGS JET_KS_BUILD JET_KS_IMPLEMENTATION)
# since U14b only the crash report writers and HardwareException::GetCallStack are the original's (called there)
target_link_libraries(KernelStandard_7c PRIVATE "${IMPLIB}/KernelStandard_7c_orig.lib")
# Bitmap::SaveJPG: libjpeg-turbo in place of the original's static IJG encoder (vcpkg x86-windows-static)
set(VCPKG_X86 "D:/trainz12-decomp/thirdparty/vcpkg/installed/x86-windows-static")
target_include_directories(KernelStandard_7c PRIVATE "${VCPKG_X86}/include")
target_link_libraries(KernelStandard_7c PRIVATE "${VCPKG_X86}/lib/jpeg.lib")
# tables with no known formula are read from the installed game's original module at build time (never committed)
set(ORIG_KS "D:/games/trainz 2012 VDK/bin/KernelStandard_7c.dll")
set(GEN "${CMAKE_CURRENT_BINARY_DIR}/generated")
add_custom_command(
    OUTPUT "${GEN}/StdMathLUT.h"
    COMMAND ${PY} -I "${TOOLS}/extract_data.py" "${ORIG_KS}" 72078 512 u32 STDMATH_SQRT_LUT "${GEN}/StdMathLUT.h"
    DEPENDS "${TOOLS}/extract_data.py" VERBATIM)
target_sources(KernelStandard_7c PRIVATE "${GEN}/StdMathLUT.h")
# Console's 8x8 bitmap font (256 characters)
add_custom_command(
    OUTPUT "${GEN}/ConsoleFont.h"
    COMMAND ${PY} -I "${TOOLS}/extract_data.py" "${ORIG_KS}" 72d98 2048 u8 CONSOLE_F50_FONT "${GEN}/ConsoleFont.h"
    DEPENDS "${TOOLS}/extract_data.py" VERBATIM)
target_sources(KernelStandard_7c PRIVATE "${GEN}/ConsoleFont.h")
target_include_directories(KernelStandard_7c PRIVATE "${GEN}")
# ConfigParser's scanner (flex) and parser (Lemon) tables: generated output with no source, read the same way
set(SCFG_TABLES "")
foreach(t "acclist;52360;432;u16" "accept;526c0;208;u16" "ec;52860;128;u8" "meta;528e0;44;u32"
          "base;52990;228;u16" "def;52b58;228;u16" "nxt;52d20;1016;u16" "chk;53510;1016;u16"
          "actions;735a0;522;u32" "states;73dc8;195;u32" "rules;74148;68;u8" "hexlo;74444;103;u8"
          "hexhi;7447c;103;u8")
    list(GET t 0 n)
    list(GET t 1 rva)
    list(GET t 2 cnt)
    list(GET t 3 kind)
    string(TOUPPER "SCFG_${n}" macro)
    add_custom_command(
        OUTPUT "${GEN}/scfg_${n}.h"
        COMMAND ${PY} -I "${TOOLS}/extract_data.py" "${ORIG_KS}" ${rva} ${cnt} ${kind} ${macro} "${GEN}/scfg_${n}.h"
        DEPENDS "${TOOLS}/extract_data.py" VERBATIM)
    list(APPEND SCFG_TABLES "${GEN}/scfg_${n}.h")
endforeach()
target_sources(KernelStandard_7c PRIVATE ${SCFG_TABLES})

# step 5, small Jet modules: drop-in proxies, replaced module by module (the rest forwards to <module>_orig.dll)
foreach(m KernelResource_7c KernelResources_7c Render2DCore_7c InputSystem_7c InputWin32_7c RenderSystem_7c WaterSystem_7c)
    jet_module(${m})
    target_compile_definitions(${m} PRIVATE _CRT_SECURE_NO_WARNINGS)
    # each calls the other Jet modules by their original names (our proxies or the originals)
    target_link_libraries(${m} PRIVATE "${IMPLIB}/KernelStandard_7c.lib" "${IMPLIB}/Kernel_7c.lib")
endforeach()
target_link_libraries(KernelResources_7c PRIVATE "${IMPLIB}/KernelResource_7c.lib")
target_link_libraries(Render2DCore_7c PRIVATE "${IMPLIB}/KernelResource_7c.lib" "${IMPLIB}/RenderSystem_7c.lib")
target_link_libraries(InputWin32_7c PRIVATE "${IMPLIB}/InputSystem_7c.lib")
# RenderSystem_7c: replaced unit by unit (notes/RenderSystem.md); its module state stays the original's until U9
target_compile_definitions(RenderSystem_7c PRIVATE JET_RS_BUILD)
target_link_libraries(RenderSystem_7c PRIVATE "${IMPLIB}/RenderSystem_7c_orig.lib")
# WaterSystem_7c: replaced unit by unit (notes/WaterSystem.md); its module state stays the original's until U10
target_compile_definitions(WaterSystem_7c PRIVATE JET_WS_BUILD)
# the map lets the diff tests find the parsers' internal entry points (RsTest*)
target_link_options(RenderSystem_7c PRIVATE /MAP)
# the shader-language scanners' DFA tables and their Lemon parsers' tables (generated output with no source), read
# from the installed original at build time like KernelStandard's ConfigParser
set(ORIG_RS "D:/games/trainz 2012 VDK/bin/RenderSystem_7c.dll")
set(RS_TABLES "")
foreach(t "gfxqk_acclist;6cdb0;88;u16" "gfxqk_accept;6ce60;96;u16" "gfxqk_ec;6cf20;128;u8" "gfxqk_meta;6cfa0;42;u32"
          "gfxqk_base;6d048;100;u16" "gfxqk_def;6d110;100;u16" "gfxqk_nxt;6d1d8;208;u16" "gfxqk_chk;6d378;208;u16"
          "gfxpp_acclist;6d518;356;u16" "gfxpp_accept;6d7e0;288;u16" "gfxpp_ec;6da20;128;u8" "gfxpp_meta;6daa0;56;u32"
          "gfxpp_base;6db80;304;u16" "gfxpp_def;6dde0;304;u16" "gfxpp_nxt;6e040;636;u16" "gfxpp_chk;6e538;636;u16"
          "gfxtp_acclist;69438;424;u16" "gfxtp_accept;69788;328;u16" "gfxtp_ec;69a18;128;u8" "gfxtp_meta;69a98;68;u32"
          "gfxtp_base;69ba8;344;u16" "gfxtp_def;69e58;344;u16" "gfxtp_nxt;6a108;840;u16" "gfxtp_chk;6a798;840;u16"
          "gfxvp_acclist;62be8;492;u16" "gfxvp_accept;62fc0;612;u16" "gfxvp_ec;63488;128;u8" "gfxvp_meta;63508;74;u32"
          "gfxvp_base;63630;616;u16" "gfxvp_def;63b00;616;u16" "gfxvp_nxt;63fd0;956;u16" "gfxvp_chk;64748;956;u16")
    list(GET t 0 n)
    list(GET t 1 rva)
    list(GET t 2 cnt)
    list(GET t 3 kind)
    string(TOUPPER "${n}" macro)
    add_custom_command(
        OUTPUT "${GEN}/${n}.h"
        COMMAND ${PY} -I "${TOOLS}/extract_data.py" "${ORIG_RS}" ${rva} ${cnt} ${kind} ${macro} "${GEN}/${n}.h"
        DEPENDS "${TOOLS}/extract_data.py" VERBATIM)
    list(APPEND RS_TABLES "${GEN}/${n}.h")
endforeach()
foreach(t "gfxqk;100b5de0;32;100b5fc8;19;" "gfxpp;100b66c0;95;100b6c28;54;" "gfxtp;100b14a8;119;100b1b58;69;"
          "gfxvp;100aef10;480;100b0950;268;wide")
    list(GET t 0 n)
    list(GET t 1 states)
    list(GET t 2 nstates)
    list(GET t 3 rules)
    list(GET t 4 nrules)
    list(GET t 5 mode)
    string(TOUPPER "${n}" macro)
    add_custom_command(
        OUTPUT "${GEN}/${n}_lemon.h"
        COMMAND ${PY} -I "${TOOLS}/extract_lemon.py" "${ORIG_RS}" ${states} ${nstates} ${rules} ${nrules} ${macro}
                "${GEN}/${n}_lemon.h" ${mode}
        DEPENDS "${TOOLS}/extract_lemon.py" VERBATIM)
    list(APPEND RS_TABLES "${GEN}/${n}_lemon.h")
endforeach()
target_sources(RenderSystem_7c PRIVATE ${RS_TABLES})
target_include_directories(RenderSystem_7c PRIVATE "${GEN}")
target_compile_definitions(InputSystem_7c PRIVATE JET_IS_BUILD)
# (all 327 exports are ours: nothing is called in InputSystem_7c_orig.dll any more)
target_link_libraries(InputSystem_7c PRIVATE "${IMPLIB}/KernelResource_7c.lib")
# the key binding scanner's DFA tables (flex output, no known source) come from the installed original at build time
set(ORIG_IS "D:/games/trainz 2012 VDK/bin/InputSystem_7c.dll")
set(LEX_TABLES "")
foreach(t "acclist;159b0;84;u16" "accept;15a58;136;u16" "ec;15b68;128;u8" "meta;15be8;50;u32"
          "base;15cb0;136;u16" "def;15dc0;136;u16" "nxt;15ed0;264;u16" "chk;160e0;264;u16")
    list(GET t 0 n)
    list(GET t 1 rva)
    list(GET t 2 cnt)
    list(GET t 3 kind)
    string(TOUPPER "ICFG_${n}" macro)
    add_custom_command(
        OUTPUT "${GEN}/icfg_${n}.h"
        COMMAND ${PY} -I "${TOOLS}/extract_data.py" "${ORIG_IS}" ${rva} ${cnt} ${kind} ${macro} "${GEN}/icfg_${n}.h"
        DEPENDS "${TOOLS}/extract_data.py" VERBATIM)
    list(APPEND LEX_TABLES "${GEN}/icfg_${n}.h")
endforeach()
target_sources(InputSystem_7c PRIVATE ${LEX_TABLES})
target_include_directories(InputSystem_7c PRIVATE "${GEN}")
# the scanner alone, against the original's (called inside InputSystem_7c_orig.dll by address)
add_executable(diff_is_lex tests/diff_is_lex.cpp input/InputSystem_7c/Lexer.cpp ${LEX_TABLES})
target_include_directories(diff_is_lex PRIVATE include "${GEN}")
target_compile_options(diff_is_lex PRIVATE /EHsc /GR /Zc:wchar_t- /Zp8)
target_compile_definitions(diff_is_lex PRIVATE _CRT_SECURE_NO_WARNINGS)
target_link_libraries(diff_is_lex PRIVATE "${IMPLIB}/KernelStandard_7c.lib")

# KernelStandard unit tests: ours vs KernelStandard_7c_orig.dll in one process
add_executable(diff_ks_math tests/diff_ks_math.cpp)
add_executable(diff_ks_small tests/diff_ks_small.cpp)
add_executable(diff_ks_sync tests/diff_ks_sync.cpp)
add_executable(diff_ks_platform tests/diff_ks_platform.cpp)
add_executable(diff_ks_options tests/diff_ks_options.cpp)
add_executable(diff_ks_configdata tests/diff_ks_configdata.cpp)
add_executable(cfgp_probe tests/cfgp_probe.cpp)
add_executable(diff_ks_configparser tests/diff_ks_configparser.cpp)
add_executable(diff_ks_stringtables tests/diff_ks_stringtables.cpp)
add_executable(diff_ks_dynbuf tests/diff_ks_dynbuf.cpp)
add_executable(diff_ks_ioslave tests/diff_ks_ioslave.cpp)
add_executable(diff_ks_jirf tests/diff_ks_jirf.cpp)
add_executable(diff_ks_bitmap_draw tests/diff_ks_bitmap_draw.cpp)
add_executable(diff_ks_bitmap_io tests/diff_ks_bitmap_io.cpp)
add_executable(diff_ks_file tests/diff_ks_file.cpp)
add_executable(diff_ks_filesub tests/diff_ks_filesub.cpp)
add_executable(diff_ks_window tests/diff_ks_window.cpp)
add_executable(diff_ks_rand tests/diff_ks_rand.cpp)
add_executable(diff_ks_math2 tests/diff_ks_math2.cpp)
add_executable(diff_ks_thread tests/diff_ks_thread.cpp)
target_link_libraries(diff_ks_thread PRIVATE winmm)
add_executable(diff_ks_log tests/diff_ks_log.cpp)
add_executable(diff_ks_profile tests/diff_ks_profile.cpp)
add_executable(diff_ks_eh tests/diff_ks_eh.cpp)
add_executable(diff_ks_string tests/diff_ks_string.cpp)
add_executable(diff_ks_pstring tests/diff_ks_pstring.cpp)
add_executable(diff_ks_alloc tests/diff_ks_alloc.cpp)
add_executable(diff_krs tests/diff_krs.cpp)
add_executable(diff_r2d tests/diff_r2d.cpp)
# RenderSystem U1: ours vs RenderSystem_7c_orig.dll (vertex records by signature, the small display types)
add_executable(diff_rs_u1 tests/diff_rs_u1.cpp)
# RenderSystem U4: modifiers and lists, ours vs RenderSystem_7c_orig.dll on the same scripts
add_executable(diff_rs_u4 tests/diff_rs_u4.cpp)
# RenderSystem U2a: shader-language data (symbols, registers, instructions, defines), heap hooked on both sides
add_executable(diff_rs_u2 tests/diff_rs_u2.cpp)
# RenderSystem U2b: ExtendedGfxLanguage and the texture combiner records (+ the shader-language parsers)
add_executable(diff_rs_u2b tests/diff_rs_u2b.cpp)
# RenderSystem U3a: VertexProgram and TextureCombiner (stand-in Display, lights and API slots)
add_executable(diff_rs_u3 tests/diff_rs_u3.cpp)
# WaterSystem U1: FrameAnimation, PNoise and WaterGrid, ours vs WaterSystem_7c_orig.dll on the same inputs
add_executable(diff_ws_u1 tests/diff_ws_u1.cpp)
add_executable(diff_iw32 tests/diff_iw32.cpp)
add_executable(diff_is_obs tests/diff_is_obs.cpp)
add_executable(diff_is_dev tests/diff_is_dev.cpp)
add_executable(diff_is_str tests/diff_is_str.cpp)
add_executable(diff_is_emu tests/diff_is_emu.cpp)
add_executable(diff_is_input tests/diff_is_input.cpp)
add_executable(diff_is_uiext tests/diff_is_uiext.cpp)
add_executable(diff_is_force tests/diff_is_force.cpp)
add_executable(diff_is_emuload tests/diff_is_emuload.cpp)
add_executable(diff_is_mapper tests/diff_is_mapper.cpp)
add_executable(diff_kr_system tests/diff_kr_system.cpp)
target_compile_options(diff_kr_system PRIVATE /Zi)
target_link_options(diff_kr_system PRIVATE /DEBUG /MAP)

add_executable(diff_blowfish tests/diff_blowfish.cpp core/Kernel_7c/AuranBlowfish.cpp)
add_executable(diff_lzss tests/diff_lzss.cpp core/Kernel_7c/JetLzss.cpp)
add_executable(diff_fda tests/diff_fda.cpp)

# differential tests: our module vs the original, both loaded in one 32-bit process
add_executable(diff_kernel_uid_version tests/diff_kernel_uid_version.cpp)
target_compile_definitions(diff_kernel_uid_version PRIVATE _CRT_SECURE_NO_WARNINGS)

# plugin system dump: links Kernel_7c.dll by name, run once next to the original module and once next to ours
add_executable(plugin_probe tests/plugin_probe.cpp)
target_include_directories(plugin_probe PRIVATE include)
target_compile_options(plugin_probe PRIVATE /EHa /GR /Zc:wchar_t- /Zp8)
target_compile_definitions(plugin_probe PRIVATE _CRT_SECURE_NO_WARNINGS)
target_link_libraries(plugin_probe PRIVATE Kernel_7c "${IMPLIB}/KernelStandard_7c.lib")

# task system walk-through: same idea as plugin_probe
add_executable(task_probe tests/task_probe.cpp)
target_include_directories(task_probe PRIVATE include)
target_compile_options(task_probe PRIVATE /EHsc /GR /Zc:wchar_t- /Zp8)
target_compile_definitions(task_probe PRIVATE _CRT_SECURE_NO_WARNINGS)
target_link_libraries(task_probe PRIVATE Kernel_7c "${IMPLIB}/KernelStandard_7c.lib")

# EditItem block walk-through: same idea as task_probe
add_executable(edititem_probe tests/edititem_probe.cpp)
target_include_directories(edititem_probe PRIVATE include)
target_compile_options(edititem_probe PRIVATE /EHsc /GR /Zc:wchar_t- /Zp8)
target_compile_definitions(edititem_probe PRIVATE _CRT_SECURE_NO_WARNINGS)
target_link_libraries(edititem_probe PRIVATE Kernel_7c "${IMPLIB}/KernelStandard_7c.lib")

else()  # x64 harness: no game, no original modules (tests/x64)

# KernelStandard_7c has no x64 build yet: a stand-in with just enough behaviour for Kernel_7c and the probes
add_library(KernelStandardStub STATIC tests/x64/KernelStandardStub.cpp
            core/KernelStandard_7c/SyncObjects.cpp core/KernelStandard_7c/Rand.cpp
            core/KernelStandard_7c/Math2.cpp core/KernelStandard_7c/Bitmap.cpp)  # reconstructed units replace their part of the stand-in
target_compile_definitions(KernelStandardStub PRIVATE JET_KS_IMPLEMENTATION)
# Kernel_7c as an object library, so every unit (and its statics) is in each harness exe, as in the DLL
file(GLOB kernel_srcs CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/core/Kernel_7c/*.cpp")
add_library(Kernel_7c_x64 OBJECT ${kernel_srcs})
foreach(t KernelStandardStub Kernel_7c_x64)
    target_include_directories(${t} PUBLIC include)
    target_compile_definitions(${t} PUBLIC JET_KS_STATIC _CRT_SECURE_NO_WARNINGS)
    target_compile_options(${t} PUBLIC /W3 /EHsc /GR /Zc:wchar_t-)
endforeach()

foreach(p task_probe edititem_probe)
    add_executable(${p}_x64 tests/${p}.cpp)
    target_link_libraries(${p}_x64 PRIVATE Kernel_7c_x64 KernelStandardStub ole32)
endforeach()

endif()

# deterministic codec and container results, built for both: the 32-bit output (codecs proven equal to the
# originals) is the reference for the x64 one
add_executable(golden_kernel tests/golden_kernel.cpp core/Kernel_7c/JetLzss.cpp core/Kernel_7c/AuranBlowfish.cpp
               core/Kernel_7c/UId.cpp core/Kernel_7c/Version.cpp)
target_include_directories(golden_kernel PRIVATE include)
target_compile_options(golden_kernel PRIVATE /EHsc /GR /Zc:wchar_t-)
target_compile_definitions(golden_kernel PRIVATE _CRT_SECURE_NO_WARNINGS)
if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    target_link_libraries(golden_kernel PRIVATE "${IMPLIB}/KernelStandard_7c.lib" ole32)
else()
    target_link_libraries(golden_kernel PRIVATE KernelStandardStub ole32)
endif()
