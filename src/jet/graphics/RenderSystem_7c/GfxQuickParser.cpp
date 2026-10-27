// Unit U2b2 of RenderSystem_7c: GfxLanguageQuickParser, the scan VertexProgramManager::RegisterBuiltInPrograms runs
// over a text of built-in programs to collect their names (scanner gfxqk 0x1002efd0, Lemon driver 0x1002ed30 with
// reduce 0x1002e990, run 0x1002f930). The DFA and parser tables are the original's (gfxqk_*.h, read from the installed
// module at build time).
#include "GfxParse.h"

#include <cstdlib>
#include <new>

#include "gfxqk_accept.h"
#include "gfxqk_acclist.h"
#include "gfxqk_base.h"
#include "gfxqk_chk.h"
#include "gfxqk_def.h"
#include "gfxqk_ec.h"
#include "gfxqk_lemon.h"
#include "gfxqk_meta.h"
#include "gfxqk_nxt.h"

namespace Jet {

namespace {

const unsigned short accList[] = {GFXQK_ACCLIST_DATA};
const unsigned short accept[] = {GFXQK_ACCEPT_DATA};
const unsigned char ec[] = {GFXQK_EC_DATA};
const unsigned int meta[] = {GFXQK_META_DATA};
const unsigned short base[] = {GFXQK_BASE_DATA};
const unsigned short def[] = {GFXQK_DEF_DATA};
const unsigned short nxt[] = {GFXQK_NXT_DATA};
const unsigned short chk[] = {GFXQK_CHK_DATA};
const FlexTables tables = {accList, accept, ec, meta, base, def, nxt, chk, 0x5b, 22, true};

}  // namespace

class GfxLanguageQuickParser;

struct QuickContext {  // the parse's state (0x1002f930 locals)
    GfxNameList* names;
    bool error;
    GfxLanguageQuickParser* parser;
};

struct QuickTraits {
    static constexpr int kNoCode = 0x1b;
    static constexpr int kStates = 0x20;
    static constexpr int kRules = 0x13;
    static constexpr int kErrorSymbol = 0xc;
    static constexpr int kTerminals = 0xb;
    static const LemonAction pool[];
    static const LemonState states[];
    static const LemonRule rules[];
    static void Reduce(LemonStack* p, int rule, QuickContext* ctx, void*& gotoMinor);
    static void SyntaxError(QuickContext* ctx) { ctx->error = true; }
    static void Failed(QuickContext* ctx) { ctx->error = true; }
};
const LemonAction QuickTraits::pool[] = {GFXQK_POOL_DATA};
const LemonState QuickTraits::states[] = {GFXQK_STATES_DATA};
const LemonRule QuickTraits::rules[] = {GFXQK_RULES_DATA};
using QuickLemon = LemonParser<QuickTraits, QuickContext>;

class GfxLanguageQuickParser : public FlexScanner {
public:
    explicit GfxLanguageQuickParser(MemoryFile* in) : FlexScanner(tables, in) {}
    ~GfxLanguageQuickParser() override { names.Clear(); }

    bool Run(const AnsiString& programName);  // 0x1002f930: true without a syntax error
    int Lex();                                // 0x1002efd0

    void* value = nullptr;          // +0x78 the token's text (Jet::Alloc)
    String name;                    // +0x7c
    GfxNameList names;              // +0x80
    int line = 1;                   // +0x84
    QuickContext* context = nullptr;  // +0x88
};

int GfxLanguageQuickParser::Lex()
{
    for (;;) {
        switch (const int act = Scan()) {
        case 1: case 19: ++line; break;
        case 2:
            --gfxLex.qkDepth;
            gfxLex.qkCurrent = gfxLex.qkStack[gfxLex.qkDepth];
            start = 2 * gfxLex.qkCurrent + 1;
            break;
        case 3: case 18: case 20: break;
        case 4: return 0xb;
        case 5:
            gfxLex.qkStack[0] = gfxLex.qkCurrent;
            gfxLex.qkBraces = 0;
            gfxLex.qkDepth = 1;
            gfxLex.qkCurrent = 2;
            start = 5;
            return 9;
        case 6: case 15: value = CopyText(yytext); return 5;
        case 7: return 8;
        case 8: return 2;
        case 9: ++gfxLex.qkBraces; return 7;
        case 10:
            if (--gfxLex.qkBraces == 0) {  // (the original also stores the top of the stack back into place)
                gfxLex.qkCurrent = 3;
                start = 7;
            }
            return 1;
        case 11: return 4;
        case 12: return 0xa;
        case 13: return 3;
        case 14: return 9;
        case 16: value = CopyText(yytext); return 6;
        case 17:
            gfxLex.qkStack[gfxLex.qkDepth++] = gfxLex.qkCurrent;
            gfxLex.qkCurrent = 1;
            start = 3;
            break;
        case 21: Output(yytext, yyleng); break;
        case 23: case 24: case 25: case 26: return 0;
        default: (void)act; Error("fatal flex scanner internal error--no action found");
        }
    }
}

namespace {

void* Minor(LemonStack* p, int k) { return (p->top - k)->minor; }
void FreeToken(LemonStack* p, int k)  // a token's text (small values are not pointers)
{
    LemonEntry* const e = p->top - k;
    if (reinterpret_cast<uintptr_t>(e->minor) > 0x100) {
        if (e->minor) Alloc::Delete(e->minor);
        e->minor = nullptr;
    }
}
void AddName(LemonStack* p, int k, QuickContext* ctx)  // the name in token k, then the token freed
{
    PString name;
    if (const char* text = static_cast<const char*>(Minor(p, k)))
        name.node = PString::Cache()->CreateANSI(reinterpret_cast<const unsigned char*>(text), -1);
    ctx->names->Append(name);
    LemonEntry* const e = p->top - k;
    if (e->minor) Alloc::Delete(e->minor);
    e->minor = nullptr;
}

}  // namespace

void QuickTraits::Reduce(LemonStack* p, int rule, QuickContext* ctx, void*& gotoMinor)
{
    switch (rule) {
    case 1: case 13: case 15: FreeToken(p, 1); break;
    case 2: FreeToken(p, 4); FreeToken(p, 2); FreeToken(p, 0); break;
    case 3: AddName(p, 1, ctx); FreeToken(p, 2); FreeToken(p, 0); break;
    case 10: AddName(p, 3, ctx); FreeToken(p, 4); FreeToken(p, 0); break;
    case 11: FreeToken(p, 2); FreeToken(p, 1); FreeToken(p, 0); break;
    case 17: FreeToken(p, 0); break;
    case 18: {
        const float f = static_cast<float>(std::atof(static_cast<const char*>(Minor(p, 0))));
        gotoMinor = *reinterpret_cast<void* const*>(&f);
        LemonEntry* const e = p->top;
        if (e->minor) Alloc::Delete(e->minor);
        e->minor = nullptr;
        break;
    }
    default: break;
    }
}

bool GfxLanguageQuickParser::Run(const AnsiString& programName)
{
    static_cast<AnsiString&>(name) = programName;
    QuickContext ctx{&names, false, this};
    context = &ctx;
    LemonStack* const p = QuickLemon::New();
    value = nullptr;
    int token = Lex();
    while (token > 0) {
        QuickLemon::Parse(p, token, value, &ctx);
        value = nullptr;
        if (ctx.error) break;
        token = Lex();
    }
    if (!ctx.error) QuickLemon::Parse(p, 0, value, &ctx);
    QuickLemon::Free(p);
    return !ctx.error;
}

// ---- VertexProgramManager::RegisterBuiltInPrograms (0x10053dc0): the program names reach `then` while the parser
// still holds them
bool GfxQuickParse(MemoryFile* in, const char* name, void (*then)(const GfxNameList& names, void* ctx), void* ctx)
{
    GfxLanguageQuickParser parser(in);
    bool ok;
    {
        String s(name);
        ok = parser.Run(s);
    }
    if (ok) then(parser.names, ctx);
    return ok;
}

// ---- entry points for the differential test (found through the linker map; nothing else calls them)

extern "C" void __cdecl RsTestQuickCtor(void* mem, MemoryFile* in) { ::new (mem) GfxLanguageQuickParser(in); }
extern "C" bool __cdecl RsTestQuickRun(void* p, const AnsiString* name)
{
    return static_cast<GfxLanguageQuickParser*>(p)->Run(*name);
}
extern "C" const GfxNameList* __cdecl RsTestQuickNames(void* p) { return &static_cast<GfxLanguageQuickParser*>(p)->names; }
extern "C" void __cdecl RsTestQuickDtor(void* p) { static_cast<GfxLanguageQuickParser*>(p)->~GfxLanguageQuickParser(); }
// the Lemon driver alone, on token streams the test makes up (context {names, error, parser}, as the original's)
extern "C" void __cdecl RsTestQuickParse(LemonStack* p, int major, void* minor, void* ctx)
{
    QuickLemon::Parse(p, major, minor, static_cast<QuickContext*>(ctx));
}
extern "C" void __cdecl RsTestQuickFree(LemonStack* p) { QuickLemon::Free(p); }
#pragma comment(linker, "/include:_RsTestQuickParse")
#pragma comment(linker, "/include:_RsTestQuickFree")
#pragma comment(linker, "/include:_RsTestQuickCtor")
#pragma comment(linker, "/include:_RsTestQuickRun")
#pragma comment(linker, "/include:_RsTestQuickNames")
#pragma comment(linker, "/include:_RsTestQuickDtor")

}  // namespace Jet
