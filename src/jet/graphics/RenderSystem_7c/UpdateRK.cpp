// RenderKernel::Update export (RenderSystem_7c). Implemented separately to avoid IListNode conflict.
#include <jet/Task.h>

namespace Jet {
class RenderKernel {
public:
    Task::UpdateCode Update(float dt);
};
}  // namespace Jet

namespace Jet {
Task::UpdateCode RenderKernel::Update(float dt) { (void)dt; return Task::CONTINUE; }
}  // namespace Jet
