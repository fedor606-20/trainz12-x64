// Render3DCore_7c wave 6 tails (Tails3): SplineDefinition's ConfigData and text-file paths. The original's config
// key order is Tension/Continuity/Bias, so +0x10 (the header's `bias`) is "Continuity" and +0x14 (`continuity`) is
// "Bias". WriteConfigData builds one anonymous node per point with three children named by the "%f" coordinates;
// ReadConfigData expects each point child to carry a 3-float array instead (the shipped writer/reader do not
// round-trip points; the read path is reproduced as it is).
#include <jet/Render3DCoreSplines.h>

#include <jet/Config.h>
#include <jet/KernelStandard.h>
#include <jet/PString.h>
#include <jet/Resource.h>

#include <cstdio>

namespace Jet {

// 0x1006d7c0
bool __cdecl SplineDefinition::ReadConfigData(const ConfigData& config)
{
    const ConfigData& spline = config(PString("Spline"));
    if (!spline)
        return false;

    RemoveAllPoints();
    closed = true;
    tension = 0.0f;
    bias = 0.0f;        // +0x10, read from "Continuity"
    continuity = 0.0f;  // +0x14, read from "Bias"
    type = Unknown;

    PString typeName;
    spline >> typeName;
    if (typeName.CompareIgnoreCase("hermite") == 0) {
        type = Hermite;
    } else if (typeName.CompareIgnoreCase("cardinal") == 0) {
        type = Cardinal;
    } else if (typeName.CompareIgnoreCase("tcb") == 0) {
        type = TCB;
    } else if (typeName.CompareIgnoreCase("bezier") == 0) {
        type = Bezier;
    } else if (typeName.CompareIgnoreCase("bspline") == 0) {
        type = BSpline;
        tension = 1.0f;
    } else {
        type = Unknown;
    }

    spline(PString("Loop")) >> closed;
    spline(PString("Tension")) >> tension;
    spline(PString("Continuity")) >> bias;
    spline(PString("Bias")) >> continuity;

    const ConfigData& points = spline(PString("Points"));
    if (points) {
        const int count = points.GetChildCount();
        for (int i = 0; i < count; ++i) {
            const ConfigData& child = points[i];
            const int kind = child.Kind();
            if (kind == 0 || kind < 4)
                return false;
            const int* data = child.Data();
            if (data == nullptr || data[0] != 3)
                return false;
            Vector3 point;
            child >> point;
            AddPoint(point);
        }
    }
    return true;
}

// 0x1006db80
bool __cdecl SplineDefinition::WriteConfigData(ConfigData& config, bool includeVersion) const
{
    if (points == nullptr || count == 0)
        return false;

    if (includeVersion) {
        ConfigData* version = new ConfigData(PString("version"));
        *version << 1.0f;
        config << version;
    }

    ConfigData* spline = new ConfigData(PString("Spline"));
    const char* typeName = "Unknown";
    switch (type) {
    case Hermite: typeName = "Hermite"; break;
    case Cardinal: typeName = "Cardinal"; break;
    case TCB: typeName = "TCB"; break;
    case Bezier: typeName = "Bezier"; break;
    case BSpline: typeName = "Bspline"; break;
    default: typeName = "Unknown"; break;
    }
    *spline << PString(typeName);
    config << spline;

    {
        ConfigData* node = new ConfigData(PString("Loop"));
        *node << closed;
        *spline << node;
    }
    if (type == Cardinal || type == TCB || type == BSpline || type == Unknown) {
        ConfigData* node = new ConfigData(PString("Tension"));
        *node << tension;
        *spline << node;
    }
    if (type == TCB || type == Unknown) {
        ConfigData* node = new ConfigData(PString("Continuity"));
        *node << bias;  // +0x10
        *spline << node;
    }
    if (type == TCB || type == Unknown) {
        ConfigData* node = new ConfigData(PString("Bias"));
        *node << continuity;  // +0x14
        *spline << node;
    }

    ConfigData* pointsNode = new ConfigData(PString("Points"));
    *spline << pointsNode;
    for (unsigned int i = 0; i < count; ++i) {
        const Vector3& p = points[i];
        ConfigData* pointNode = new ConfigData(PString());
        *pointsNode << pointNode;
        const float values[3] = {p.x, p.y, p.z};
        for (float value : values) {
            char text[32];
            std::snprintf(text, sizeof text, "%f", static_cast<double>(value));
            ConfigData* component = new ConfigData(PString(), PString(text));
            *pointNode << component;
        }
    }
    return true;
}

// 0x1006e3e0
bool __cdecl SplineDefinition::ReadTextFile(const ResourceDatabase* database, const char* name)
{
    if (database == nullptr || name == nullptr)
        return false;
    ConfigParser parser;
    const PString fileName = database->GetFileName(PString(name));
    const ConfigParser::Result result = parser.Parse(fileName.Text());
    if (result.code != 0)
        return false;
    return ReadConfigData(parser.Root());
}

// 0x1006e530
bool __cdecl SplineDefinition::WriteTextFile(const ResourceDatabase* database, const char* name) const
{
    File file;
    if (database != nullptr) {
        if (name == nullptr)
            return false;
        const PString fileName = database->GetFileName(PString(name));
        if (file.Open(fileName.Text(), File::CREATE_WRITE, file.GetDefaultEncoding()).code != 0)
            return false;
    } else {
        if (name == nullptr)
            return false;
        if (file.Open(name, File::CREATE_WRITE, file.GetDefaultEncoding()).code != 0)
            return false;
    }

    ConfigData config(PString("SplineFile"));
    if (!WriteConfigData(config, true)) {
        file.Close();
        return false;
    }
    const bool written = config.Write(file, 0);
    file.Close();
    return written;
}

}  // namespace Jet
