
/*
 * Copyright (C) Kirill A. Korinskiy
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

typedef struct {
    ngx_str_t         by;
    ngx_array_t      *by_lengths;
    ngx_array_t      *by_values;
    ngx_rbtreehash_t  data;
} ngx_http_is_bot_loc_conf_t;

typedef struct {
    ngx_str_t         bot;
} ngx_http_is_bot_ctx_t;

static ngx_int_t ngx_http_is_bot_add_variable(ngx_conf_t *cf);

static void *ngx_http_is_bot_create_loc_conf(ngx_conf_t *cf);
static char *ngx_http_is_bot_merge_loc_conf(ngx_conf_t *cf,
    void *parent, void *child);

static char *ngx_http_is_bot_set_by_slot(ngx_conf_t *cf,
    ngx_command_t *cmd, void *conf);

static ngx_http_is_bot_ctx_t *ngx_http_is_bot_get_ctx(ngx_http_request_t *r);

static ngx_command_t  ngx_http_is_bot_commands[] = {

    { ngx_string("is_bot_by"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_http_is_bot_set_by_slot,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("is_bot_data"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_CONF_TAKE1,
      ngx_rbtreehash_from_path,
      NGX_HTTP_LOC_CONF_OFFSET,
      offsetof(ngx_http_is_bot_loc_conf_t, data),
      NULL },

      ngx_null_command
};


static ngx_http_module_t  ngx_http_is_bot_module_ctx = {
    ngx_http_is_bot_add_variable,        /* preconfiguration */
    NULL,                                /* postconfiguration */

    NULL,                                /* create main configuration */
    NULL,                                /* init main configuration */

    NULL,	                         /* create server configuration */
    NULL,			         /* merge server configuration */

    ngx_http_is_bot_create_loc_conf,     /* create location configuration */
    ngx_http_is_bot_merge_loc_conf       /* merge location configuration */
};


ngx_module_t  ngx_http_is_bot_module = {
    NGX_MODULE_V1,
    &ngx_http_is_bot_module_ctx,           /* module context */
    ngx_http_is_bot_commands,              /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};

static ngx_str_t  ngx_http_is_bot_variable_name = ngx_string("is_bot");

static ngx_int_t
ngx_http_is_bot_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    ngx_http_is_bot_ctx_t          *ctx;

    ctx = ngx_http_is_bot_get_ctx(r);

    if (ctx == NULL) {
	return NGX_ERROR;
    }


    if (!ctx->bot.data || (ngx_int_t)ctx->bot.len < 0) {
	v->not_found = 1;
	return NGX_OK;
    }

    v->len = ctx->bot.len;
    v->data = ctx->bot.data;

    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;

    return NGX_OK;
}

static ngx_int_t
ngx_http_is_bot_add_variable(ngx_conf_t *cf)
{
  ngx_http_variable_t  *var;

  var = ngx_http_add_variable(cf, &ngx_http_is_bot_variable_name, NGX_HTTP_VAR_NOHASH);
  if (var == NULL) {
    return NGX_ERROR;
  }

  var->get_handler = ngx_http_is_bot_variable;

  return NGX_OK;
}

static ngx_http_is_bot_ctx_t *
ngx_http_is_bot_get_ctx(ngx_http_request_t *r)
{
    ngx_str_t                       by;
    ngx_http_script_code_pt         code;
    ngx_http_script_engine_t        e;
    ngx_http_script_len_code_pt     lcode;
    ngx_http_is_bot_loc_conf_t     *blcf;
    ngx_http_is_bot_ctx_t          *ctx;

    ctx = ngx_http_get_module_ctx(r, ngx_http_is_bot_module);

    if (ctx != NULL) {
	return ctx;
    }

    ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_is_bot_ctx_t));
    if (ctx == NULL) {
	return NULL;
    }

    blcf = ngx_http_get_module_loc_conf(r, ngx_http_is_bot_module);

    if (blcf->data.data == NULL || blcf->data.data->nodes == 0) {
	return ctx;
    }

    if (blcf->by_lengths == NULL &&
	blcf->by_values == NULL) {
	by = blcf->by;
    } else {
	ngx_memzero(&e, sizeof(ngx_http_script_engine_t));
	e.ip = blcf->by_lengths->elts;
	e.request = r;
	e.flushed = 1;

	by.len = 1;		/* 1 byte for terminating '\0' */

	while (*(uintptr_t *) e.ip) {
	    lcode = *(ngx_http_script_len_code_pt *) e.ip;
	    by.len += lcode(&e);
	}

	by.data = ngx_pcalloc(r->pool, by.len);
	if (by.data == NULL) {
	    return NULL;
	}

	e.pos = by.data;
	e.ip = blcf->by_values->elts;

	while (*(uintptr_t *) e.ip) {
	    code = *(ngx_http_script_code_pt *) e.ip;
	    code((ngx_http_script_engine_t *) &e);
	}

	by.len = e.pos - by.data;
    }

    ctx->bot.data = ngx_rbtreehash_find(&blcf->data, &by, &ctx->bot.len);

    ngx_http_set_ctx(r, ctx, ngx_http_is_bot_module);

    return ctx;
}


static void *
ngx_http_is_bot_create_loc_conf(ngx_conf_t *cf)
{
    ngx_http_is_bot_loc_conf_t  *conf;

    conf = ngx_pcalloc(cf->pool, sizeof(ngx_http_is_bot_loc_conf_t));
    if (conf == NULL) {
        return NGX_CONF_ERROR;
    }

    return conf;
}


static char *
ngx_http_is_bot_merge_loc_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_http_is_bot_loc_conf_t  *prev = parent;
    ngx_http_is_bot_loc_conf_t  *conf = child;

    if (prev->by.len) {
	conf->by = prev->by;
	conf->by_values = prev->by_values;
	conf->by_lengths = prev->by_lengths;
    }

    return NGX_CONF_OK;
}


static char *
ngx_http_is_bot_set_by_slot(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_is_bot_loc_conf_t      *blcf = conf;
    ngx_str_t                       *value;
    ngx_uint_t                       n;
    ngx_http_script_compile_t        sc; 

    if (blcf->by.data) {
	return "is duplicate";
    }

    value = cf->args->elts;

    if (value[1].len == 0) {
	ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
			   "region \"%V\" in \"%V\" directive is invalid",
			   &value[1], &value[0]);
	return NGX_CONF_ERROR;
    }

    blcf->by = value[1];

    n = ngx_http_script_variables_count(&blcf->by);

    if (n == 0) {
	/* include the terminating '\0' to the length to use ngx_copy()*/
	blcf->by.len++;

	return NGX_CONF_OK;
    }

    ngx_memzero(&sc, sizeof(ngx_http_script_compile_t));

    sc.cf = cf;
    sc.source = &blcf->by;
    sc.lengths = &blcf->by_lengths;
    sc.values = &blcf->by_values;
    sc.variables = n;
    sc.complete_lengths = 1;
    sc.complete_values = 1;

    if (ngx_http_script_compile(&sc) != NGX_OK) {
	return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}
