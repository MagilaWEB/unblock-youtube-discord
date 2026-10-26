module;

#include <coco/utils/utils.hpp>

export module ext.coco;

import std;

// saucer's coroutine helpers, used by dom and engine.

export namespace coco {

using ::coco::await;
using ::coco::stray;

} // namespace coco
