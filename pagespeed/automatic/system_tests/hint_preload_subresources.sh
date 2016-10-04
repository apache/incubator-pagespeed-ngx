test_filter hint_preload_subresources works, and finds indirects

# Expect 5 resources to be hinted
fetch_until -save $URL 'grep -c ^Link:' 5 --save-headers
OUT=$(cat $FETCH_UNTIL_OUTFILE)

check_from "$OUT" fgrep \
  'Link: </mod_pagespeed_example/styles/all_using_imports.css>; rel=preload; as=style; nopush'
check_from "$OUT" fgrep \
  'Link: </mod_pagespeed_example/styles/yellow.css>; rel=preload; as=style; nopush'
check_from "$OUT" fgrep \
  'Link: </mod_pagespeed_example/styles/blue.css>; rel=preload; as=style; nopush'
check_from "$OUT" fgrep \
  'Link: </mod_pagespeed_example/styles/bold.css>; rel=preload; as=style; nopush'
check_from "$OUT" fgrep \
  'Link: </mod_pagespeed_example/inline_javascript.js>; rel=preload; as=script; nopush'
