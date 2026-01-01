#include "ModuleEvents.h"

namespace M1 {

EventBus& EventBus::instance() {
    static EventBus bus;
    return bus;
}

} // namespace M1
