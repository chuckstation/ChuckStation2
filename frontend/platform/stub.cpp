#include "chuckstation2.hpp"

// Note: This is a stub implementation for platforms that
//       do not need special initialization

namespace chuckstation2::platform {

bool init(chuckstation2::instance* iris) {
    return true;
}

bool apply_settings(chuckstation2::instance* iris) {
    return true;
}

void destroy(chuckstation2::instance* iris) {}

}