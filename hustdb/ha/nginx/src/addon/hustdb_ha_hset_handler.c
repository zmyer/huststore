#include "hustdb_ha_write_handler.h"

ngx_int_t hustdb_ha_hset_handler(ngx_str_t * backend_uri, ngx_http_request_t *r)
{
    return hustdb_ha_write_handler(HUSTDB_METHOD_HSET, false, false, true, false, backend_uri, r, hustdb_ha_start_post);
}
