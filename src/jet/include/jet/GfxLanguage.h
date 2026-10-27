// RenderSystem_7c unit U2 (notes/RenderSystem.md), the shader-language front-end: the symbol records, register
// operands and instructions the vertex/texture program parsers build, and the preprocessor defines. Layouts follow
// the TS12 assembly (addresses per member in RenderSystem_7c/GfxSymbols.cpp).
#pragma once

#include <jet/DynamicArray.h>
#include <jet/FastDynamicArray.h>
#include <jet/KernelStandard.h>
#include <jet/PString.h>
#include <jet/RenderTypes.h>
#include <jet/Resource.h>
#include <jet/Result.h>
#include <jet/VertexFormats.h>

namespace Jet {

// the renderer's per-symbol data, deleted through its virtual destructor (VertexProgramBasic's VPSymbol and the
// API modules' derive from it)
struct ApiDataBase {
    ApiDataBase();
    ApiDataBase(const ApiDataBase& o);
    virtual ~ApiDataBase();
    ApiDataBase& operator=(const ApiDataBase& o);
    JET_HEAP_OBJECT
};

enum SymbolType {};
enum VertexSymbolClass {};
enum TextureSymbolClass {};
enum VertexOpcode {};
enum TextureOpcode {};

// a named or anonymous symbol of a program (0x48 bytes). Symbols chain through `next` (AppendSymbol) and an alias
// points at the symbol it stands for (UnAlias); a symbol owns its chain, its API data and, with bit 2 of `flags`,
// its alias.
struct SymbolEntry {
    SymbolEntry(SymbolType type, bool referenced);
    SymbolEntry(const SymbolEntry& o);
    SymbolEntry& operator=(const SymbolEntry& o);
    ~SymbolEntry();

    void __cdecl SetName(const char* name);  // null: no name; either way the symbol is no longer anonymous
    const PString& __cdecl GetName() const;
    void __cdecl AddRef();  // on the symbol and every alias it stands for
    void __cdecl DecRef();
    bool __cdecl IsAnonymous() const;
    bool __cdecl IsReferenced() const;
    bool __cdecl LikeType(const SymbolEntry* o) const;
    void __cdecl AppendSymbol(SymbolEntry* s);  // at the end of the `next` chain
    SymbolEntry* __cdecl GetNextSymbol() const;
    const SymbolEntry* __cdecl UnAlias() const;
    void __cdecl SetApiData(ApiDataBase* d) const;
    ApiDataBase* __cdecl GetApiData() const;
    ApiDataBase* __cdecl GetAndZeroApiData() const;
    unsigned long __cdecl GetHint() const;
    void __cdecl SetHint(unsigned long h);

    JET_HEAP_OBJECT

    SymbolEntry* alias;  // +0
    SymbolType type;     // +4
    int field08, field0c, field10, field14;  // +8.. (-1 when new; LikeType compares +0xc..+0x14)
    unsigned long hint;  // +0x18
    int field1c;         // +0x1c (5 when new)
    unsigned char field20;  // +0x20
    unsigned char flags;    // +0x21 bit 0 anonymous, bit 1 (compared by LikeType), bit 2 owns the alias
    int field24, field28, field2c, field30;  // +0x24 (left by the constructors)
    int field34;                 // +0x34
    PString name;                // +0x38
    int refs;                    // +0x3c
    SymbolEntry* next;           // +0x40
    mutable ApiDataBase* api;    // +0x44
};
JET_LAYOUT32(sizeof(SymbolEntry) == 0x48);

// the two languages' symbols add their class (+0x48); they share their bodies in the original
struct VertexSymbolEntry : SymbolEntry {
    VertexSymbolEntry(VertexSymbolClass cls, SymbolType type, bool referenced);
    VertexSymbolEntry(const VertexSymbolEntry& o);
    VertexSymbolEntry& operator=(const VertexSymbolEntry& o);
    ~VertexSymbolEntry();
    bool __cdecl LikeType(const VertexSymbolEntry* o) const;  // the same class, then SymbolEntry::LikeType

    VertexSymbolClass symbolClass;  // +0x48
};
JET_LAYOUT32(sizeof(VertexSymbolEntry) == 0x4c);

struct TextureSymbolEntry : SymbolEntry {
    TextureSymbolEntry(TextureSymbolClass cls, SymbolType type, bool referenced);
    TextureSymbolEntry(const TextureSymbolEntry& o);
    TextureSymbolEntry& operator=(const TextureSymbolEntry& o);
    ~TextureSymbolEntry();

    TextureSymbolClass symbolClass;  // +0x48
};
JET_LAYOUT32(sizeof(TextureSymbolEntry) == 0x4c);

// the symbols of a vertex program: an Array<VertexSymbolEntry*, 128> it owns
struct VertexSymbolTable {
    VertexSymbolTable();
    VertexSymbolTable(const VertexSymbolTable& o);  // copies the pointers
    VertexSymbolTable& operator=(const VertexSymbolTable& o);
    ~VertexSymbolTable();  // deletes the symbols

    VertexSymbolEntry* __cdecl Find(const PString& name) const;
    VertexSymbolEntry* __cdecl FindLike(const VertexSymbolEntry* e) const;  // the first of the same class and type
    void __cdecl AddSymbol(VertexSymbolEntry* e);

    unsigned long count, capacity;  // +0, +4 (capacity in steps of 128)
    VertexSymbolEntry** data;       // +8
};
JET_LAYOUT32(sizeof(VertexSymbolTable) == 0xc);

// a modifier of a register operand (negation, write mask...): copies nothing
struct RegisterModifier {
    RegisterModifier& operator=(const RegisterModifier& o);
};

struct RegisterItem;

// an indexed operand: the index register and a constant offset
struct OffsetAddress {
    OffsetAddress(RegisterItem* item, int offset);
    ~OffsetAddress();  // deletes the index register
    OffsetAddress& operator=(const OffsetAddress& o);

    RegisterItem* item;  // +0
    int offset;          // +4
};

// one operand of an instruction (0x18 bytes); it owns its swizzle text, offset and modifiers
struct RegisterItem {
    RegisterItem(SymbolEntry* symbol, char* swizzle, bool flag, RegisterModifier* m1, RegisterModifier* m2);
    explicit RegisterItem(const RegisterItem* o);  // a copy without the modifiers; the offset's register is shared
    // the six fields as they are, on the stack (VertexProgramBasic's macros; the destructor then frees what the
    // original operand owns, as in the original)
    enum ShallowCopy { Shallow };
    RegisterItem(const RegisterItem& o, ShallowCopy)
        : symbol(o.symbol), offset(o.offset), swizzle(o.swizzle), flag(o.flag), m1(o.m1), m2(o.m2) {}
    ~RegisterItem();
    RegisterItem& operator=(const RegisterItem& o);  // a plain copy of the six fields

    SymbolEntry* symbol;     // +0
    OffsetAddress* offset;   // +4
    char* swizzle;           // +8 (copies get 6 bytes)
    bool flag;               // +0xc
    RegisterModifier* m1;    // +0x10
    RegisterModifier* m2;    // +0x14
    JET_HEAP_OBJECT
};
JET_LAYOUT32(sizeof(RegisterItem) == 0x18);

using RegisterList = Array<RegisterItem*, 128>;

// an instruction: its operands (an array it owns, deleted with them). The constructors take the array as is.
struct Instruction {
    explicit Instruction(RegisterList* operands);
    Instruction(const Instruction& o);
    Instruction& operator=(const Instruction& o);
    virtual ~Instruction();

    RegisterList* operands;  // +4
    JET_HEAP_OBJECT
};

struct VertexInstruction : Instruction {
    VertexInstruction(VertexOpcode op, RegisterList* operands);
    VertexInstruction(const VertexInstruction& o);
    VertexInstruction& operator=(const VertexInstruction& o);
    ~VertexInstruction() override;

    VertexOpcode opcode;  // +8
};

struct TextureInstruction : Instruction {
    TextureInstruction(TextureOpcode op, RegisterList* operands);
    TextureInstruction(const TextureInstruction& o);
    TextureInstruction& operator=(const TextureInstruction& o);
    ~TextureInstruction() override;

    TextureOpcode opcode;  // +8
};

// a #define handed to a program: name and value
struct PreProcessorDefine {
    PreProcessorDefine();
    PreProcessorDefine(const AnsiString& name, const AnsiString& value);
    PreProcessorDefine(const PreProcessorDefine& o);
    ~PreProcessorDefine();
    PreProcessorDefine& operator=(const PreProcessorDefine& o);

    AnsiString name, value;
};

struct PreProcessorArray : Array<PreProcessorDefine, 3> {
    PreProcessorArray();
    PreProcessorArray(const PreProcessorArray& o);
    ~PreProcessorArray();
    PreProcessorArray& operator=(const PreProcessorArray& o);

    AnsiString __cdecl ConcatenateStrings() const;  // ":name:value" for each define
};

// ---- U2b (RenderSystem_7c/ExtendedGfxLanguage.cpp)

class Display;
class ResourceDatabase;

// what a program's symbol name stands for (GetSymbolId); `local` symbols are constants numbered by `number`
struct GfxSymbolId {
    bool local;  // +0 (GetLocalConstantNumber wants it false)
    int number;  // +4
    int extra;   // +8
};

// the base of the vertex programs and texture combiners (0x3c bytes with its virtual ReferenceCount at +0x34):
// the source text, names, a generation, the symbol names it declares, a user value and a fallback program.
// LoadProgram/LoadFromFile run the defines over the text and hand it to the derived class's Load.
class ExtendedGfxLanguage : public virtual ReferenceCount {
public:
    struct SymbolName {  // 0x10
        PString name;    // +0
        GfxSymbolId id;  // +4
    };

    virtual bool __cdecl SupportedByPlatform(float generation);  // slot 0: false
    virtual bool __cdecl LoadProgram(const PString& name, const char* source, const PreProcessorArray* defines);
    virtual bool __cdecl LoadFromFile(const ResourceDatabase* db, const PString& name, const PString& file,
                                      const PreProcessorArray* defines);
    virtual bool __cdecl LoadNativeFormat(const PString& name, const char* source);  // true
    virtual bool __cdecl CompileProgram();                                           // true
    virtual bool __cdecl Bind() const;                                               // false
    virtual bool __cdecl End() const;                                                // false
    // slot 7: does the program's API define `name` (#if in the pre-parser)? false in every class seen here
    virtual bool __cdecl Defines(const PString& name) const;

    Display* __cdecl GetDisplay() const;
    const AnsiString& __cdecl GetProgramString() const;
    const PString& __cdecl GetFilename() const;
    const PString& __cdecl GetProgramName() const;
    const PString& __cdecl GetRegisteredName() const;
    void __cdecl SetFilename(const PString& f);  // a trailing ".txt" is cut off
    void __cdecl SetProgramName(const PString& n);
    void __cdecl SetRegisteredName(const PString& n);
    float __cdecl GetGeneration() const;
    void __cdecl SetGeneration(float g);
    unsigned long __cdecl GetUserData() const;
    void __cdecl SetUserData(unsigned long d) const;
    const ExtendedGfxLanguage* __cdecl GetFallbackProgram() const;
    void __cdecl SetFallbackProgram(const ExtendedGfxLanguage* p) const;  // referenced
    bool __cdecl GetSymbolId(const PString& name, GfxSymbolId& id) const;
    int __cdecl GetLocalConstantNumber(const PString& name) const;  // -1 when not a local symbol
    const Array<SymbolName, 3>& GetSymbolNameList() const;

    JET_HEAP_OBJECT

protected:
    explicit ExtendedGfxLanguage(Display* display);
    virtual ~ExtendedGfxLanguage();

    virtual bool __cdecl Load(MemoryFile& file, const PString& name, const PString& fileName);  // slot 8: false

    void __cdecl InitInstance(ExtendedGfxLanguage& copy) const;  // copies everything into `copy`
    void __cdecl ProcessDefines(AnsiString& text, const PreProcessorArray* defines) const;
    void __cdecl AddSymbolName(const PString& name, int number, bool local, int extra);

    Display* display;                             // +8
    AnsiString source;                            // +0xc
    PString filename;                             // +0x10
    PString programName;                          // +0x14
    PString registeredName;                       // +0x18
    float generation;                             // +0x1c (1)
    Array<SymbolName, 3> symbolNames;             // +0x20
    mutable unsigned long userData;               // +0x2c
    mutable const ExtendedGfxLanguage* fallback;  // +0x30
};
JET_LAYOUT32(sizeof(ExtendedGfxLanguage) == 0x3c);

// a singly linked list of parse records through each item's `next` (0xc)
template <class T>
struct ParseList {
    T* head;
    T* tail;
    int count;
    JET_HEAP_OBJECT
};

// ---- U3a (RenderSystem_7c/VertexProgram.cpp, TextureCombiner.cpp)

class TextureCombinerManager;
class VertexProgramManager;
class VertexType;

using DisplayLightList = FastDynamicArray<DisplayLight*, 13, Less<DisplayLight*, DisplayLight*>>;

// A texture combiner (pixel program) of the API modules (0x6c bytes with its virtual ReferenceCount at +0x64): the
// constants its program declares (by register, 16 at most), the bump-environment settings per stage and its
// manager. The API module's class fills the slots that answer 0 here.
class TextureCombiner : public ExtendedGfxLanguage {
public:
    enum TextureOpcode {};
    enum TextureAddressType {};
    enum RegType {};
    enum ModifierType {};

    struct ModifierItem {  // 8
        explicit ModifierItem(ModifierType t);
        ModifierItem& operator=(const ModifierItem& o);
        ModifierType type;
        ModifierItem* next;
        JET_HEAP_OBJECT
    };

    // an operand (0x14): the register and two lists of modifiers it owns
    struct RegisterItem {
        RegisterItem(RegType type, int index);
        explicit RegisterItem(const RegisterItem* o);  // copies both lists, not `next`
        ~RegisterItem();
        RegisterItem& operator=(const RegisterItem& o);  // the five fields as they are

        RegType type;                     // +0
        int index;                        // +4
        ParseList<ModifierItem>* mods1;   // +8
        ParseList<ModifierItem>* mods2;   // +0xc
        RegisterItem* next;               // +0x10
        JET_HEAP_OBJECT
    };

    struct Instruction {  // 0x2c
        Instruction(TextureOpcode op, ParseList<RegisterItem>* operands, ParseList<ModifierItem>* mods,
                    TextureAddressType address, float value, bool flag);
        Instruction(const Instruction& o);           // shallow: the same lists
        explicit Instruction(const Instruction* o);  // deep copy of both lists; +0x18..+0x24 left as they are
        Instruction& operator=(const Instruction& o);
        virtual ~Instruction();

        TextureOpcode opcode;                    // +4
        ParseList<RegisterItem>* operands;       // +8
        ParseList<ModifierItem>* mods;           // +0xc
        TextureAddressType address;              // +0x10
        bool flag;                               // +0x14
        int field18, field1c, field20, field24;  // +0x18 (only the copy constructor and operator= touch them)
        float value;                             // +0x28
        JET_HEAP_OBJECT
    };

    struct Constant {  // a tracked constant register (0x14)
        int index;
        Vector4 value;
    };
    struct BumpEnv {  // a stage's bump scale and bias (0x14; the last two are never set)
        int stage;
        float scale, bias, unused0c, unused10;
    };
    struct BumpMatrix {  // 0x14
        int stage;
        Vector4 matrix;
    };

    static bool __cdecl SupportedByPlatform(Display* display, float generation);
    bool __cdecl SupportedByPlatform(float generation) override;  // the display's capability 0x15, asked once

    virtual void __cdecl SetCombinerManager(TextureCombinerManager* m);  // 9
    virtual TextureCombinerManager* __cdecl GetCombinerManager();         // 10
    virtual bool __cdecl AddInstruction(const Instruction* i);           // 11 (the program parser's lines): false
    virtual bool __cdecl SetConstantRegister(int index, const Vector4& v);  // 12: false when not tracked
    // 13: a new constant register (index < 16, not tracked yet) and its symbol name when it has one
    virtual bool __cdecl TrackConstant(int index, const AnsiString& name, const Vector4& v);
    virtual void __cdecl SetBumpEnvScale(int stage, float scale, bool apply);           // 14
    virtual void __cdecl SetBumpEnvBias(int stage, float bias, bool apply);             // 15
    virtual void __cdecl SetBumpEnvMatrix(int stage, const Vector4& matrix, bool apply);  // 16
    virtual int __cdecl GetApi() const;  // 17: handed to the program parser (0 here)

    bool __cdecl SetConstantRegister(int index, const Color& c);
    bool __cdecl SetConstantRegister(int index, const Vector3& v);  // w = 1

protected:
    explicit TextureCombiner(Display* display);
    ~TextureCombiner() override;

    bool __cdecl Load(MemoryFile& file, const PString& name, const PString& fileName) override;

    Array<Constant*, 128> constants;       // +0x34
    Constant** constantTable;              // +0x40 (16 slots, by register)
    Array<BumpEnv*, 128> bumpEnv;          // +0x44
    Array<BumpMatrix*, 128> bumpMatrices;  // +0x50
    bool bumpChanged;                      // +0x5c
    TextureCombinerManager* manager;       // +0x60

    static __declspec(dllimport) bool supportChecked;  // the original's until U9
    static __declspec(dllimport) bool supported;
};
JET_LAYOUT32(sizeof(TextureCombiner) == 0x6c);

// A vertex program of the API modules (0x50 bytes with its virtual ReferenceCount at +0x48): its vertex type, the
// registers where skinning matrices go and its manager. Load runs the pre-parser, then the vertex program parser
// over its output. The slots that answer 0 here are the API module's.
class VertexProgram : public ExtendedGfxLanguage {
public:
    // a constant register the program asked for (+0 register, +4 kind: 4 is a render state with its id at +8;
    // lights by index +0x10 among those of type +0x1c)
    struct ConstantData {
        int reg;         // +0
        int kind;        // +4
        int state;       // +8
        int field0c;     // +0xc
        int lightIndex;  // +0x10
        int field14, field18;
        int lightType;   // +0x1c
    };
    struct MatrixMap {
        const Matrix4* matrix;  // +0
    };

    static bool __cdecl SupportedByPlatform(Display* display, float generation);  // generations 1..2, asked once
    bool __cdecl SupportedByPlatform(float generation) override;
    bool __cdecl LoadNativeFormat(const PString& name, const char* source) override;

    virtual VertexProgram* __cdecl CreateShared();                       // 9: a program to share (the managers ask it): null
    virtual void __cdecl SetProgramManager(VertexProgramManager* m);     // 10
    virtual VertexProgramManager* __cdecl GetProgramManager();           // 11
    virtual int __cdecl Slot12();                                        // the API module's: 0
    // 13 / 14: the vertex program parser's symbols and instructions (false here; VertexProgramBasic's)
    virtual bool __cdecl LoadSymbolTable(const VertexSymbolTable& table, bool native);
    virtual bool __cdecl LoadInstructions(const Array<VertexInstruction*, 128>& list);
    virtual int __cdecl Slot15();
    virtual bool __cdecl RestoreProgram();                               // 16: true
    virtual void __cdecl SetProgramState(const DisplayLightList& lights, Color ambient,
                                         const DisplayMaterial& material, const Matrix4& view) const;  // 17
    virtual void __cdecl LoadInfluences(const Array<MatrixMap*, 128>& matrices) const;  // 18
    virtual void __cdecl LoadConstantRegisters() const;                  // 19
    virtual int __cdecl Slot20();
    virtual int __cdecl Slot21();
    virtual int __cdecl Slot22();
    virtual bool __cdecl SetLocalConstant(int reg, const Vector4& v) const;        // 23
    virtual bool __cdecl SetLocalConstantDirect(int reg, const Vector4& v) const;  // 24
    virtual int __cdecl Slot25();
    virtual bool __cdecl SetLocalConstants(int reg, const Vector4* v, int count) const;  // 26
    virtual bool __cdecl SetEnvConstant(int reg, const Vector4& v) const;          // 27
    virtual bool __cdecl SetVertexType(VertexType* type);                // 28: false when unchanged
    virtual VertexType* __cdecl GetVertexType() const;                   // 29
    virtual int __cdecl GetMaxLocalRegisters() const;                    // 30
    virtual int __cdecl GetMaxEnvRegisters() const;                      // 31
    virtual int __cdecl GetMaxTotalRegisters() const;                    // 32
    virtual int __cdecl GetApi() const;                                  // 33: handed to the pre-parser
    virtual bool __cdecl LoadNativeProgram(AnsiString& text);           // 34

    // the display's limits, read once from a program it makes
    static int __cdecl GetMaxLocalRegisters(Display* display);
    static int __cdecl GetMaxEnvRegisters(Display* display);
    static int __cdecl GetMaxTotalRegisters(Display* display);

    bool __cdecl SetLocalConstant(int reg, const Vector3& v) const;  // w = 1
    bool __cdecl SetLocalConstant(int reg, const Color& c) const;
    bool __cdecl SetLocalConstantDirect(int reg, const Vector3& v) const;
    bool __cdecl SetLocalConstantDirect(int reg, const Color& c) const;
    bool __cdecl SetEnvConstant(int reg, const Vector3& v) const;
    bool __cdecl SetEnvConstant(int reg, const Color& c) const;
    int __cdecl GetInfluenceBaseRegister() const;
    int __cdecl GetInfluenceMaxRegister() const;

protected:
    VertexProgram(Display* display, VertexType* type);
    ~VertexProgram() override;

    bool __cdecl Load(MemoryFile& file, const PString& name, const PString& fileName) override;
    void __cdecl InitInstance(VertexProgram& copy) const;
    // the render-state constants (material, lights by type, fog...) through SetLocalConstantDirect
    void __cdecl LoadCommonStates(const Array<ConstantData*, 3>& constants, const DisplayLightList& lights,
                                  Color ambient, const DisplayMaterial& material, const Matrix4& view) const;
    static void __cdecl GetMaxRegisters(Display* display);

    VertexType* vertexType;          // +0x34 (referenced)
    int influenceBase;               // +0x38 (-1: none)
    int influenceMax;                // +0x3c
    bool field40;                    // +0x40
    VertexProgramManager* manager;   // +0x44

    static __declspec(dllimport) int maxLocalRegs;  // the original's until U9
    static __declspec(dllimport) int maxEnvRegs;
    static __declspec(dllimport) int maxTotalRegs;
    static __declspec(dllimport) bool supportChecked[4];  // by generation - 1
    static __declspec(dllimport) bool supported[4];
};
JET_LAYOUT32(sizeof(VertexProgram) == 0x50);

// ---- U3a2 (RenderSystem_7c/ProgramManagers.cpp, ProgramResources.cpp)

class RenderKernel;

// Jet::HashTable<Key, T, Hash, SIZ>: 97 FastDynamicArray buckets of items, an item's key being a PString node (the
// bucket is the node's address modulo the size). Only what the program managers use.
template <class K, class T, class H, unsigned int SIZ>
class HashTable {
public:
    using Bucket = FastDynamicArray<T, SIZ, Less<T, T>>;

    void Init();     // 0x10042d80: 97 empty buckets
    void Destroy();  // the buckets from the last to the first, then their block
    void Clear();    // 0x10054a10: every bucket's chunks, from the first bucket on; the block stays
    void Insert(T item, const void* key);  // 0x10040f50 / 0x10054b10 / 0x100549a0: at the end of its bucket
    template <class F>
    T* Find(const void* key, F keyOf) const  // 0x10040fc0 / 0x10054b80 / 0x10054a80: the slot, or null
    {
        Bucket& b = buckets[reinterpret_cast<unsigned int>(key) % size];
        for (typename Bucket::Iterator i = b.First(); i != b.End(); ++i)
            if (keyOf(*i) == key) return &*i;
        return nullptr;
    }
    template <class F>
    void ForEach(F f)  // 0x10054c10 / 0x10054880: buckets in order, each from its head chunk
    {
        for (unsigned int k = 0; k < size; ++k)
            for (typename Bucket::Iterator i = buckets[k].First(); i != buckets[k].End(); ++i) f(*i);
    }

    Bucket* buckets;     // +0 (an Alloc block: the count, then the buckets)
    unsigned int count;  // +4
    unsigned int size;   // +8 (97)
};

template <class K, class T, class H, unsigned int SIZ>
void HashTable<K, T, H, SIZ>::Init()
{
    count = 0;
    size = 0x61;
    void* const block = Alloc::New(4 + 0x61 * sizeof(Bucket));
    if (!block) {
        buckets = nullptr;
        return;
    }
    *static_cast<unsigned int*>(block) = 0x61;
    buckets = reinterpret_cast<Bucket*>(static_cast<char*>(block) + 4);
    for (unsigned int k = 0; k < 0x61; ++k) new (&buckets[k]) Bucket();
}

template <class K, class T, class H, unsigned int SIZ>
void HashTable<K, T, H, SIZ>::Destroy()
{
    if (!buckets) return;
    for (unsigned int k = reinterpret_cast<unsigned int*>(buckets)[-1]; k-- > 0;) buckets[k].~Bucket();
    Alloc::Delete(reinterpret_cast<unsigned int*>(buckets) - 1);
}

template <class K, class T, class H, unsigned int SIZ>
void HashTable<K, T, H, SIZ>::Clear()
{
    for (unsigned int k = 0; k < size; ++k) buckets[k].RemoveAll();
    count = 0;
}

template <class K, class T, class H, unsigned int SIZ>
void HashTable<K, T, H, SIZ>::Insert(T item, const void* key)
{
    const unsigned int k = reinterpret_cast<unsigned int>(key) % size;
    ++count;
    buckets[k].Add(item);
}

// Hands out the vertex programs: shared by their registered name (file:program + defines), made from the built-in
// program texts (RegisterBuiltInPrograms) or from `.vtx` resources (0x18 bytes). Only the original's Display makes
// one until U9, so the statics stay the original's.
class VertexProgramManager : public ReferenceCount {
public:
    enum Results {};
    static __declspec(dllimport) const ResultTable* const results;
    using Result = ResultClass<&results, Results>;

    struct ProgramItem {  // a built-in program: the list's name, the program's name and the list's text (0xc)
        PString name;
        PString program;
        const char* text;
    };
    struct RegisteredName {  // a program list already parsed
        PString name;
        JET_HEAP_OBJECT
    };
    struct NameHash {};

    explicit VertexProgramManager(Display* display);
    ~VertexProgramManager() override;

    // (the vftable holds the three Creates in reverse: slot 2 the protected one, 3 by type, 4 by id)
    virtual Result __cdecl Create(RenderKernel* kernel, VertexProgram*& out, VertexTypeID id, bool shared,
                                  const PString& name, const PString& file, const ResourceDatabase* db,
                                  const PreProcessorArray* defines);
    virtual Result __cdecl Create(RenderKernel* kernel, VertexProgram*& out, VertexType* type, bool shared,
                                  const PString& name, const PString& file, const ResourceDatabase* db,
                                  const PreProcessorArray* defines);

protected:
    virtual Result __cdecl Create(RenderKernel* kernel, VertexProgram*& out, bool byId, VertexTypeID id,
                                  VertexType* type, bool shared, const PString& name, const PString& file,
                                  const ResourceDatabase* db, const PreProcessorArray* defines);

public:
    virtual Result __cdecl Register(VertexProgram* program);  // 5: 3 when its registered name is taken
    virtual Result __cdecl RestorePrograms();                 // 6: 5 when one fails

    // the program list `text` (several programs) under `name`, parsed once (the quick parser) into the list
    static void __cdecl RegisterBuiltInPrograms(const PString& name, const char* text);

    JET_HEAP_OBJECT

protected:
    virtual Result __cdecl LoadBuiltInProgram(const PString& key, const PString& name, VertexProgram*& out,
                                              bool byId, VertexTypeID id, VertexType* type,
                                              const PreProcessorArray* defines);  // 7

    Display* display;                                                  // +8
    HashTable<const PString&, VertexProgram*, NameHash, 13> programs;  // +0xc by registered name

    static __declspec(dllimport) VertexProgramManager* theManager;  // the original's until U9
    static __declspec(dllimport) Array<ProgramItem, 128> builtInProgramList;
    static __declspec(dllimport) HashTable<const PString&, RegisteredName*, NameHash, 13> registeredProgramList;
};
JET_LAYOUT32(sizeof(VertexProgramManager) == 0x18);

// Hands out the texture combiners by program name, made from the built-in texts or from `.pxl` resources (0x18).
class TextureCombinerManager : public ReferenceCount {
public:
    enum Results {};
    static __declspec(dllimport) const ResultTable* const results;
    using Result = ResultClass<&results, Results>;

    struct ProgramItem {  // a built-in text waiting for the manager (8)
        PString name;
        const char* text;
    };

    explicit TextureCombinerManager(Display* display);  // loads the texts registered so far
    ~TextureCombinerManager() override;

    virtual Result __cdecl Create(RenderKernel* kernel, TextureCombiner*& out, const PString& name,
                                  const ResourceDatabase* db);   // 2
    virtual Result __cdecl Register(TextureCombiner* combiner);  // 3

    static void __cdecl RegisterBuiltInPrograms(const PString& name, const char* text);

    JET_HEAP_OBJECT

protected:
    void __cdecl LoadBuiltInPrograms(const PString& name, const char* text);

    Display* display;                                                // +8
    HashTable<const PString&, TextureCombiner*, int, 13> combiners;  // +0xc by program name

    static __declspec(dllimport) TextureCombinerManager* theManager;  // the original's until U9
    static __declspec(dllimport) Array<ProgramItem, 128> builtInProgramList;
};
JET_LAYOUT32(sizeof(TextureCombinerManager) == 0x18);

// A `.vtx` resource: one vertex program file, an instance per program name and defines (0x88 bytes).
class VertexProgramResource : public Resource {
public:
    enum ErrorType {};

    struct Instance {  // 0xc
        VertexProgram* program;
        PString name;
        AnsiString defines;
    };

    VertexProgramResource(const PString& name, const ResourceDatabase* owner);
    VertexProgramResource(const VertexProgramResource& o);
    ~VertexProgramResource() override;
    VertexProgramResource& operator=(const VertexProgramResource& o);

    // the program of that name with those defines, loaded on demand (referenced)
    virtual ExtendedGfxLanguage* __cdecl CreateInstance(const PString& name, const PreProcessorArray* defines) const;  // 18

    void __cdecl SetParameters(RenderKernel* kernel, VertexType* type, const PString& name,
                               VertexProgramManager* manager, const PreProcessorArray* defines) const;
    ErrorType __cdecl GetErrorType() const;
    void __cdecl SetErrorType(ErrorType e) const;

protected:
    Result __cdecl LoadTxt(const char* name) override;

    mutable Array<Instance, 5> instances;           // +0x5c
    mutable VertexProgram* program;                 // +0x68 the last loaded
    mutable PString fileName;                       // +0x6c
    mutable RenderKernel* kernel;                   // +0x70
    mutable VertexType* vertexType;                 // +0x74
    mutable VertexProgramManager* manager;          // +0x78
    mutable PString programName;                    // +0x7c
    mutable Array<PreProcessorDefine, 3>* defines;  // +0x80 a copy
    mutable ErrorType error;                        // +0x84 (1: the program failed to load)
};
JET_LAYOUT32(sizeof(VertexProgramResource) == 0x88);

// A `.pxl` resource: one texture combiner (0x70 bytes).
class TextureCombinerResource : public Resource {
public:
    TextureCombinerResource(const PString& name, const ResourceDatabase* owner);
    TextureCombinerResource(const TextureCombinerResource& o);
    ~TextureCombinerResource() override;
    TextureCombinerResource& operator=(const TextureCombinerResource& o);

    virtual ExtendedGfxLanguage* __cdecl CreateInstance() const;  // 18: the combiner, referenced

    void __cdecl SetParameters(RenderKernel* kernel, TextureCombinerManager* manager,
                               const PreProcessorArray* defines) const;

protected:
    Result __cdecl LoadTxt(const char* name) override;

    friend class TextureCombinerManager;
    mutable TextureCombiner* combiner;         // +0x5c
    mutable PString programName;               // +0x60
    mutable RenderKernel* kernel;              // +0x64
    mutable TextureCombinerManager* manager;   // +0x68
    mutable const PreProcessorArray* defines;  // +0x6c
};
JET_LAYOUT32(sizeof(TextureCombinerResource) == 0x70);

// ---- U3b1 (RenderSystem_7c/VertexProgramBasic.cpp)

// The text program generator under the API modules' vertex programs (0xb4 bytes: its own part 0x60, then the
// virtual ReferenceCount +0x60, the vtordisp +0x68 and the virtual VertexProgram +0x6c). The parser's symbols
// become constant lists and register maps, its instructions assembly text; slots 0, 5 and 6 and the statics are
// the API module's.
class VertexProgramBasic : public virtual VertexProgram {
public:
    // a register's text: the API name, or (indexed) a constant number written as c[...]
    struct VPSymbol : ApiDataBase {
        VPSymbol();
        ~VPSymbol() override;
        bool indexed;     // +4
        AnsiString name;  // +8
        int number;       // +0xc
    };
    struct OpcodeMap {  // by VertexOpcode: the API's name for it (null: a macro or the operands alone)
        const char* name;
        int extra;
    };
    struct VertexMap {  // by vertex input (type - 3)
        int reg;
        bool indexed;
        int extra;
    };
    struct ResultMap {  // by output (type - 0x2c)
        const char* name;
        bool indexed;
        int extra;
    };

    VertexProgramBasic(Display* display, VertexType* type);
    ~VertexProgramBasic() override;

    bool __cdecl CompileProgram() override;
    bool __cdecl LoadSymbolTable(const VertexSymbolTable& table, bool native) override;
    bool __cdecl LoadInstructions(const Array<VertexInstruction*, 128>& list) override;
    void __cdecl SetProgramState(const DisplayLightList& lights, Color ambient, const DisplayMaterial& material,
                                 const Matrix4& view) const override;
    int __cdecl GetMaxLocalRegisters() const override;
    int __cdecl GetMaxEnvRegisters() const override;
    int __cdecl GetMaxTotalRegisters() const override;

protected:
    // its own vftable (+0); 0, 5 and 6 answer 0 here
    virtual void __cdecl GetProgramHeader(AnsiString& header) const;  // 0: the API's program header
    virtual bool __cdecl AddSingleInstruction(const VertexInstruction* in);  // 1
    virtual void __cdecl LoadProgramMatrices() const;                      // 2
    virtual bool __cdecl ProcessLocalConstSymbol(const VertexSymbolEntry* s, bool* used);  // 3
    virtual bool __cdecl ProcessVertexSymbol(const VertexSymbolEntry* s);  // 4
    virtual int __cdecl GetSwizzleConstant();                // 5: sets field48 to the API's constant register
    virtual void __cdecl FinishLine(AnsiString& line) const;  // 6: the API's last touch on each macro line

    void __cdecl InitInstance(VertexProgramBasic& copy) const;
    ConstantData** __cdecl LocalRegMap(int reg) const;
    void __cdecl SetLocalRegMap(int reg, ConstantData* c);
    ConstantData* __cdecl EnvRegMap(int reg) const;
    void __cdecl SetEnvRegMap(int reg, ConstantData* c);

    bool __cdecl DecodeSwizzle(AnsiString& out, const VertexInstruction* in);
    bool __cdecl DecodePower(AnsiString& out, const VertexInstruction* in) const;
    bool __cdecl DecodeAbsolute(AnsiString& out, const VertexInstruction* in) const;
    bool __cdecl DecodeHomoDot(AnsiString& out, const VertexInstruction* in) const;
    bool __cdecl DecodeFloor(AnsiString& out, const VertexInstruction* in) const;
    bool __cdecl DecodeSub(AnsiString& out, const VertexInstruction* in) const;
    bool __cdecl DecodeXProd(AnsiString& out, const VertexInstruction* in) const;
    bool __cdecl DecodeLrp(AnsiString& out, const VertexInstruction* in) const;
    bool __cdecl DecodeNrm(AnsiString& out, const VertexInstruction* in) const;
    bool __cdecl DecodeSign(AnsiString& out, const VertexInstruction* in);
    bool __cdecl DecodeExp(AnsiString& out, const VertexInstruction* in) const;  // (shared "false" bodies)
    bool __cdecl DecodeLog(AnsiString& out, const VertexInstruction* in) const;
    bool __cdecl DecodeFrac(AnsiString& out, const VertexInstruction* in) const;
    bool __cdecl CreateMatrixMacroInstructions(AnsiString& out, const VertexInstruction* in) const;
    void __cdecl DecodeRegister(AnsiString& out, const RegisterItem* r, bool swizzle) const;

    bool __cdecl ProcessProgramSymbol(const VertexSymbolEntry* s, bool local, bool* used);
    bool __cdecl ProcessJetSymbol(const VertexSymbolEntry* s, bool* used);
    bool __cdecl ProcessStateSymbol(const VertexSymbolEntry* s, bool* used);
    bool __cdecl ProcessResultSymbol(const VertexSymbolEntry* s);
    bool __cdecl ProcessAddressSymbol(const VertexSymbolEntry* s) const;
    bool __cdecl ProcessAliasSymbol(const VertexSymbolEntry* s) const;
    bool __cdecl ProcessTempSymbol(const VertexSymbolEntry* s, int& next) const;
    int __cdecl NextFreeTemp() const;
    int __cdecl GetSystemTemp() const;
    int __cdecl GetFreeRegisters(int count, const PString& name, bool* used, bool aligned) const;

    AnsiString declarations;                   // +8 (the "def" lines)
    Array<ConstantData*, 3> constants;         // +0xc local (1) and env (2) parameters
    Array<ConstantData*, 3> stateConstants;    // +0x18 render states
    Array<ConstantData*, 3> matrixConstants;   // +0x24 matrix states (0xc..0x11)
    ConstantData** localRegMap;                // +0x30 by local register - localMin
    int localMin, localMax;                    // +0x34 (999999), +0x38
    ConstantData** envRegMap;                  // +0x3c
    int envMin, envMax;                        // +0x40 (999999), +0x44
    int field48;                               // +0x48 the API's constant register for the macros (-1)
    int field4c;                               // +0x4c added to relative constant addresses
    mutable int nextTemp;                      // +0x50
    mutable int systemTemp;                    // +0x54 (-1 until taken)
    bool* used;                                // +0x58 registers taken (numEnvParameters bytes)
    bool field5c;                              // +0x5c the program writes output 0x2e

    // set by the API module (the original's, written there)
    static __declspec(dllimport) Vector4* envParameters;
    static __declspec(dllimport) int staticParameterInit;
    static __declspec(dllimport) int numTmpRegisters;
    static __declspec(dllimport) int numEnvParameters;
    static __declspec(dllimport) int numLocalParameters;
    static __declspec(dllimport) int numTotalParameters;
    static __declspec(dllimport) bool needsAlignedMatrix;
    static __declspec(dllimport) bool supportsProgramMatrix;
    static __declspec(dllimport) const char* const vertexSymbol;
    static __declspec(dllimport) const char* const resultSymbol;
    static __declspec(dllimport) const char* const addressSymbol;
    static __declspec(dllimport) const char* const tempSymbol;
    static __declspec(dllimport) const char* const instSeperator;
    static __declspec(dllimport) const char* const logInstr;
    static __declspec(dllimport) const char* const expInstr;
    static __declspec(dllimport) const char* const apiName;
    static __declspec(dllimport) const OpcodeMap* const opcodeMap;
    static __declspec(dllimport) const VertexMap* const vertexMap;
    static __declspec(dllimport) const ResultMap* const resultMap;
};
JET_LAYOUT32(sizeof(VertexProgramBasic) == 0xb4);

}  // namespace Jet
