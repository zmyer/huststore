#include "hustdb_ha_handler_base.h"

static hustdb_ha_ctx_t * __create_ctx(ngx_http_request_t *r)
{
    hustdb_ha_ctx_t * ctx = ngx_palloc(r->pool, sizeof(hustdb_ha_ctx_t));
    if (!ctx)
    {
        return NULL;
    }
    ngx_http_set_addon_module_ctx(r, ctx);
    memset(ctx, 0, sizeof(hustdb_ha_ctx_t));
    return ctx;
}

ngx_http_subrequest_peer_t * __get_peer_by_args(ngx_http_request_t *r)
{
    char * key = ngx_http_get_param_val(&r->args, "key", r->pool);
    if (!key)
    {
        return NULL;
    }
    return ngx_http_get_first_peer(hustdb_ha_get_readlist(key));
}

static ngx_int_t __start_read(ngx_str_t * backend_uri, ngx_http_request_t *r)
{
	ngx_http_subrequest_peer_t * peer = __get_peer_by_args(r);
	if (!peer)
	{
		return NGX_ERROR;
	}

	hustdb_ha_ctx_t * ctx = __create_ctx(r);
	if (!ctx)
	{
		return NGX_ERROR;
	}

	ctx->peer = peer;
	return ngx_http_gen_subrequest(
	        backend_uri,
			r,
			peer->peer,
			&ctx->base,
			hustdb_ha_on_subrequest_complete);
}

static ngx_http_subrequest_peer_t * __get_readable_peer(ngx_bool_t key_in_body, ngx_http_request_t *r)
{
    if (key_in_body)
    {
        char * key = hustdb_ha_get_key_from_body(r);
        if (!key)
        {
            return NULL;
        }
        return ngx_http_get_first_peer(hustdb_ha_get_readlist(key));
    }
    return __get_peer_by_args(r);
}

static void __post_handler(ngx_http_request_t *r)
{
    --r->main->count;
    hustdb_ha_ctx_t * ctx = ngx_http_get_addon_module_ctx(r);

    ngx_http_subrequest_peer_t * peer = __get_readable_peer(ctx->key_in_body, r);
    if (!peer)
    {
        hustdb_ha_send_response(NGX_HTTP_NOT_FOUND, NULL, NULL, r);
    }
    else
    {
        ctx->peer = peer;

        ngx_http_gen_subrequest(
            ctx->base.backend_uri,
            r,
            peer->peer,
            &ctx->base,
            hustdb_ha_on_subrequest_complete);
    }
}

static ngx_int_t __post_read(ngx_bool_t key_in_body, ngx_str_t * backend_uri, ngx_http_request_t *r)
{
    hustdb_ha_ctx_t * ctx = __create_ctx(r);
    if (!ctx)
    {
        return NGX_ERROR;
    }
    ctx->base.backend_uri = backend_uri;
    ctx->key_in_body = key_in_body;

    ngx_int_t rc = ngx_http_read_client_request_body(r, __post_handler);
    if ( rc >= NGX_HTTP_SPECIAL_RESPONSE )
    {
        return rc;
    }
    return NGX_DONE;
}

ngx_int_t hustdb_ha_read_handler(
    ngx_bool_t read_body,
    ngx_bool_t key_in_body,
    hustdb_ha_check_parameter_t check_parameter,
    ngx_str_t * backend_uri,
    ngx_http_request_t *r)
{
	hustdb_ha_ctx_t * ctx = ngx_http_get_addon_module_ctx(r);
	if (!ctx)
	{
	    if (read_body && !(r->method & NGX_HTTP_POST))
	    {
	        return NGX_HTTP_NOT_ALLOWED;
	    }
	    if (check_parameter && !check_parameter(backend_uri, r))
        {
            return NGX_ERROR;
        }
	    return read_body ? __post_read(key_in_body, backend_uri, r) : __start_read(backend_uri, r);
	}
	if (NGX_HTTP_OK != r->headers_out.status)
	{
		ctx->peer = ngx_http_get_next_peer(ctx->peer);
		return ctx->peer ? ngx_http_run_subrequest(
				r, &ctx->base, ctx->peer->peer) : hustdb_ha_send_response(
						NGX_HTTP_NOT_FOUND, NULL, NULL, r);
	}
	return hustdb_ha_send_response(NGX_HTTP_OK, ctx->version, &ctx->base.response, r);
}
