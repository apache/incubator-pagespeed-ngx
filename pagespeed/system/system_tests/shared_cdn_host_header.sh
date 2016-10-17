# Test a scenario where a multi-domain installation is using a
# single CDN for all hosts, and uses a subdirectory in the CDN to
# distinguish hosts.  Some of the resources may already be mapped to
# the CDN in the origin HTML, but we want to fetch them directly
# from localhost.  If we do this successfully (see the MapOriginDomain
# command in customhostheader.example.com in the configuration), we will
# inline a small image.
start_test shared CDN short-circuit back to origin via host-header override
URL="http://customhostheader.example.com/map_origin_host_header.html"
http_proxy=$SECONDARY_HOSTNAME fetch_until -save "$URL" \
    "grep -c data:image/png;base64" 1
