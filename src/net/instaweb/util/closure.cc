#include "net/instaweb/util/public/closure.h"

namespace net_instaweb {

Closure::Closure() {
  base::subtle::Release_Store(&quit_requested_, false);
}

Closure::~Closure() {
}

}  // namespace net_instaweb
