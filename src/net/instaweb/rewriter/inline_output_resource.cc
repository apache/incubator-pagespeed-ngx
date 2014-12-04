#include "net/instaweb/rewriter/public/inline_output_resource.h"

#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "pagespeed/kernel/base/hasher.h"

namespace net_instaweb {

InlineOutputResource::InlineOutputResource(const RewriteDriver* driver)
    : OutputResource(driver,
                     // TODO(sligocki): Modify OutputResource so that it does
                     // not depend upon having these dummy fields.
                     "dummy:/" /* resolved_base */,
                     "dummy:/" /* unmapped_base */,
                     "dummy:/" /* original_base */,
                     ResourceNamer(),
                     kInlineResource) {
}

GoogleString InlineOutputResource::url() const {
  LOG(DFATAL) << "Attempt to check inline resource URL.";
  return "";
}

GoogleString InlineOutputResource::cache_key() const {
  CHECK(loaded());
  const Hasher* hasher = server_context()->contents_hasher();
  return hasher->Hash(contents());
}

}  // namespace net_instaweb
