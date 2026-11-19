// Trivial/small copy-assign operators sharing 0x10038c50 body (return this).
// Evidence: TS12 decomp/exports show folded body returning the object unchanged.
namespace Jet {

class SystemResourceManager {
public:
    SystemResourceManager& operator=(const SystemResourceManager&);
};

SystemResourceManager& SystemResourceManager::operator=(const SystemResourceManager&)
{
    return *this;
}

}  // namespace Jet
