// Unit U11 of Render3DCore_7c: the exported static data of BackgroundResource and BackgroundLoadingThread.
//
// Only the original names are exported; the contents are the original's tables, transcribed from the image
// (0x1007cbb8 fsm, 0x1007d158 stateNames, 0x1007d1a8 inputNames, 0x1007d1f0 actionNames, 0x1007d250 results).
// The debug tables are arrays of {int value; const char* name}; MSVC mangles an array of const like a pointer,
// so the exports demangle as `NameCheck const * const` and `ResultTable const * const`.
//
// `fsm` is a pointer variable in the original and is null there (nothing reads it; the state machine uses its
// own table at 0x1007cbc0). `results` is the exporter's ResultTable array; the local ResultClass symbol points
// at the same array and the .def exports the array under the original name (see implemented.d/u11_rem.txt).
#include <jet/Render3DCoreBackground.h>

namespace Jet {

struct ResultTable {
    const char* name;
    const char* desc;
};

BackgroundResource::StateTable const (* const BackgroundResource::fsm)[9] = nullptr;

BackgroundResource::NameCheck const BackgroundResource::actionNames[12] = {
    {0, "Error"},
    {1, "NoAction"},
    {2, "ProcessFailure"},
    {3, "NotifyFailure"},
    {4, "QueueRequest"},
    {5, "MainThreadInitialize"},
    {6, "QueueBackground"},
    {7, "BackgroundLoad"},
    {8, "SendLoadComplete"},
    {9, "LoadComplete"},
    {10, "UnloadResource"},
    {11, "AbortNow"},
};

BackgroundResource::NameCheck const BackgroundResource::inputNames[9] = {
    {0, "QueueRequest"},
    {1, "ResourceLoaded"},
    {2, "QueueBackground"},
    {3, "LoadData"},
    {4, "LoadComplete"},
    {5, "CheckComplete"},
    {6, "Unload"},
    {7, "LoadFailure"},
    {8, "Abort"},
};

// the original's "Requesed" typo is part of the exported table
BackgroundResource::NameCheck const BackgroundResource::stateNames[10] = {
    {0, "Unloaded"},
    {1, "Aborting"},
    {2, "FailurePending"},
    {3, "Failed"},
    {4, "Requesed"},
    {5, "Initializing"},
    {6, "Queued"},
    {7, "Loading"},
    {8, "LoadComplete"},
    {9, "Loaded"},
};

const ResultTable BackgroundLoadingThread::resultsTable[3] = {
    {"RES_PROCESSING", "The resource is currently being processed"},
    {"RES_NOT_QUEUED", "The resource is not in the queue"},
    {"RES_QUEUE_FULL", "The queue was full and the item could not be added"},
};

const ResultTable* const BackgroundLoadingThread::results = resultsTable;

}  // namespace Jet
