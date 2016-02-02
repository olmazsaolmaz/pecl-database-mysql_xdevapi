/*
  +----------------------------------------------------------------------+
  | PHP Version 7                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 2006-2016 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Authors: Andrey Hristov <andrey@php.net>                             |
  +----------------------------------------------------------------------+
*/
#include <php.h>
#include <ext/mysqlnd/mysqlnd.h>
#include <ext/mysqlnd/mysqlnd_debug.h>
#include <ext/mysqlnd/mysqlnd_alloc.h>
#include <xmysqlnd/xmysqlnd.h>
#include <xmysqlnd/xmysqlnd_node_stmt.h>
#include <xmysqlnd/xmysqlnd_node_stmt_result.h>
#include <xmysqlnd/xmysqlnd_node_stmt_result_meta.h>
#include "php_mysqlx.h"
#include "mysqlx_class_properties.h"
#include "mysqlx_execution_status.h"
#include "mysqlx_exception.h"
#include "mysqlx_field_metadata.h"
#include "mysqlx_node_sql_statement_result.h"
#include "mysqlx_node_sql_statement.h"
#include "mysqlx_node_session.h"

static zend_class_entry *mysqlx_node_sql_statement_class_entry;

#define MAX_STMT_PARAMS 1000

#define DONT_ALLOW_NULL 0
#define NO_PASS_BY_REF 0

ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqlx_node_sql_statement__bind, 0, ZEND_RETURN_VALUE, 2)
	ZEND_ARG_TYPE_INFO(NO_PASS_BY_REF, param_no, IS_LONG, DONT_ALLOW_NULL)
	ZEND_ARG_INFO(NO_PASS_BY_REF, param)
ZEND_END_ARG_INFO()


//ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqlx_node_sql_statement__execute, 0, ZEND_RETURN_VALUE, 0)
//	ZEND_ARG_TYPE_INFO(NO_PASS_BY_REF, flags, IS_LONG, DONT_ALLOW_NULL)
//ZEND_END_ARG_INFO()


ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqlx_node_sql_statement__has_more_results, 0, ZEND_RETURN_VALUE, 0)
ZEND_END_ARG_INFO()


ZEND_BEGIN_ARG_INFO_EX(arginfo_mysqlx_node_sql_statement__get_result, 0, ZEND_RETURN_VALUE, 0)
ZEND_END_ARG_INFO()

struct st_mysqlx_node_sql_statement
{
	XMYSQLND_NODE_STMT * stmt;
	zend_long execute_flags;
	enum_func_status send_query_status;
	zend_bool in_execution;
	zend_bool has_more_results;
	zend_bool has_more_rows_in_set;
};


#define MYSQLX_FETCH_NODE_SQL_STATEMENT_FROM_ZVAL(_to, _from) \
{ \
	struct st_mysqlx_object * mysqlx_object = Z_MYSQLX_P((_from)); \
	(_to) = (struct st_mysqlx_node_sql_statement *) mysqlx_object->ptr; \
	if (!(_to) || !(_to)->stmt) { \
		php_error_docref(NULL, E_WARNING, "invalid object of class %s", ZSTR_VAL(mysqlx_object->zo.ce->name)); \
		DBG_VOID_RETURN; \
	} \
} \

/* {{{ mysqlx_node_sql_statement::__construct */
static
PHP_METHOD(mysqlx_node_sql_statement, __construct)
{
}
/* }}} */

#ifdef MYSQLX_EXPERIMENTAL_FEATURES
struct st_xmysqlnd_exec_with_cb_ctx
{
	struct {
		zend_fcall_info fci;
		zend_fcall_info_cache fci_cache;
	} on_row;
	struct {
		zend_fcall_info fci;
		zend_fcall_info_cache fci_cache;
	} on_warning;
	struct {
		zend_fcall_info fci;
		zend_fcall_info_cache fci_cache;
	} on_error;
	struct {
		zend_fcall_info fci;
		zend_fcall_info_cache fci_cache;
	} on_rset_end;
	struct {
		zend_fcall_info fci;
		zend_fcall_info_cache fci_cache;
	} on_stmt_ok;
	zval * ctx;
};

/* {{{ exec_with_cb_handle_on_row */
static const enum_hnd_func_status
exec_with_cb_handle_on_row(void * context,
						   XMYSQLND_NODE_STMT * const stmt,
						   const struct st_xmysqlnd_node_stmt_result_meta * const meta,
						   const zval * const row,
						   MYSQLND_STATS * const stats,
						   MYSQLND_ERROR_INFO * const error_info)
{
	enum_hnd_func_status ret = HND_AGAIN;
	struct st_xmysqlnd_exec_with_cb_ctx * ctx = (struct st_xmysqlnd_exec_with_cb_ctx *) context;
	DBG_ENTER("exec_with_cb_handle_on_row");
	if (ctx && row) {
		zval params[3];
		zval * user_context = &params[0];
		zval * row_container = &params[1];
		zval * meta_container = &params[2];
		zval return_value;
		unsigned int i = 0;
		const unsigned int col_count = meta->m->get_field_count(meta);

		array_init_size(row_container, col_count);
		array_init_size(meta_container, col_count);
		for (; i < col_count; ++i) {
			const XMYSQLND_RESULT_FIELD_META * field_meta = meta->m->get_field(meta, i);
			zval meta_field_zv;
			ZVAL_UNDEF(&meta_field_zv);
			mysqlx_new_field_metadata(&meta_field_zv, meta->m->get_field(meta, i));
			zend_hash_next_index_insert(Z_ARRVAL_P(meta_container), &meta_field_zv);

			if (field_meta->zend_hash_key.is_numeric == FALSE) {
				zend_hash_update(Z_ARRVAL_P(row_container), field_meta->zend_hash_key.sname, (zval *) &row[i]);
			} else {
				zend_hash_index_update(Z_ARRVAL_P(row_container), field_meta->zend_hash_key.key, (zval *) &row[i]);
			}
		}
		ZVAL_COPY(user_context, ctx->ctx);

		ZVAL_UNDEF(&return_value);
		ctx->on_row.fci.retval = &return_value;
		ctx->on_row.fci.params = params;
		ctx->on_row.fci.param_count = 3;

		if (zend_call_function(&ctx->on_row.fci, &ctx->on_row.fci_cache) == SUCCESS) {
			if (Z_TYPE(return_value) != IS_UNDEF) {
				if (Z_TYPE(return_value) == IS_LONG) {
					DBG_INF_FMT("retval=%ld", Z_LVAL(return_value));
					switch (Z_LVAL(return_value)) {
						case HND_PASS:
						case HND_FAIL:
						case HND_PASS_RETURN_FAIL:
						case HND_AGAIN:
							ret = Z_LVAL(return_value);
							break;
						default:
							break;
					}
				}
				zval_ptr_dtor(&return_value);
			}
		} else {
			ret = HND_FAIL;
		}
		zval_ptr_dtor(&params[0]);
		zval_ptr_dtor(&params[1]);
		zval_ptr_dtor(&params[2]);
	}
#ifdef PHP_DEBUG
	DBG_INF_FMT("ret=%d", ret);
	switch (ret) {
		case HND_PASS:
			DBG_INF("HND_PASS");
			break;
		case HND_FAIL:
			DBG_INF("HND_FAIL");
			break;
		case HND_PASS_RETURN_FAIL:
			DBG_INF("HND_PASS_RETURN_FAIL");
			break;
		case HND_AGAIN:
			DBG_INF("HND_AGAIN");
			break;
		default:
			break;
	}
#endif
	DBG_RETURN(ret);
}
/* }}} */


/* {{{ exec_with_cb_handle_on_warning */
static const enum_hnd_func_status
exec_with_cb_handle_on_warning(void * context, XMYSQLND_NODE_STMT * const stmt, const enum xmysqlnd_stmt_warning_level level, const unsigned int code, const MYSQLND_CSTRING message)
{
	enum_hnd_func_status ret = HND_AGAIN;
	struct st_xmysqlnd_exec_with_cb_ctx * ctx = (struct st_xmysqlnd_exec_with_cb_ctx *) context;
	DBG_ENTER("exec_with_cb_handle_on_warning");
	if (ctx) {
		zval params[3];
		zval * user_context = &params[0];
		zval * code_zv = &params[1];
		zval * message_zv = &params[2];
		zval return_value;

		ZVAL_LONG(code_zv, code);
		ZVAL_STRINGL(message_zv, message.s, message.l);

		ZVAL_COPY(user_context, ctx->ctx);

		ZVAL_UNDEF(&return_value);
		ctx->on_warning.fci.retval = &return_value;
		ctx->on_warning.fci.params = params;
		ctx->on_warning.fci.param_count = 3;

		if (zend_call_function(&ctx->on_warning.fci, &ctx->on_warning.fci_cache) == SUCCESS) {
			if (Z_TYPE(return_value) != IS_UNDEF) {
				zval_ptr_dtor(&return_value);
			}
		} else {
			ret = HND_FAIL;
		}
		zval_ptr_dtor(&params[0]);
		zval_ptr_dtor(&params[1]);
		zval_ptr_dtor(&params[2]);
	}
	DBG_RETURN(ret);
}
/* }}} */


/* {{{ exec_with_cb_handle_on_error */
static const enum_hnd_func_status
exec_with_cb_handle_on_error(void * context, XMYSQLND_NODE_STMT * const stmt, const unsigned int code, const MYSQLND_CSTRING sql_state, const MYSQLND_CSTRING message)
{
	enum_hnd_func_status ret = HND_PASS_RETURN_FAIL;
	struct st_xmysqlnd_exec_with_cb_ctx * ctx = (struct st_xmysqlnd_exec_with_cb_ctx *) context;
	DBG_ENTER("exec_with_cb_handle_on_error");
	if (ctx) {
		zval params[4];
		zval * user_context = &params[0];
		zval * code_zv = &params[1];
		zval * sql_state_zv = &params[2];
		zval * message_zv = &params[3];
		zval return_value;

		ZVAL_LONG(code_zv, code);
		ZVAL_STRINGL(sql_state_zv, sql_state.s, sql_state.l);
		ZVAL_STRINGL(message_zv, message.s, message.l);

		ZVAL_COPY(user_context, ctx->ctx);

		ZVAL_UNDEF(&return_value);
		ctx->on_error.fci.retval = &return_value;
		ctx->on_error.fci.params = params;
		ctx->on_error.fci.param_count = 4;

		if (zend_call_function(&ctx->on_error.fci, &ctx->on_error.fci_cache) == SUCCESS) {
			if (Z_TYPE(return_value) != IS_UNDEF) {
				zval_ptr_dtor(&return_value);
			}
		} else {
			ret = HND_FAIL;
		}
		zval_ptr_dtor(&params[0]);
		zval_ptr_dtor(&params[1]);
		zval_ptr_dtor(&params[2]);
		zval_ptr_dtor(&params[3]);
	}
	DBG_RETURN(ret);
}
/* }}} */


/* {{{ exec_with_cb_handle_on_resultset_end */
static const enum_hnd_func_status
exec_with_cb_handle_on_resultset_end(void * context, XMYSQLND_NODE_STMT * const stmt, const zend_bool has_more)
{
	enum_hnd_func_status ret = HND_PASS_RETURN_FAIL;
	struct st_xmysqlnd_exec_with_cb_ctx * ctx = (struct st_xmysqlnd_exec_with_cb_ctx *) context;
	DBG_ENTER("exec_with_cb_handle_on_resultset_end");
	if (ctx) {
		zval params[2];
		zval * user_context = &params[0];
		zval * has_more_zv = &params[1];
		zval return_value;

		ZVAL_COPY(user_context, ctx->ctx);
		ZVAL_BOOL(has_more_zv, has_more);

		/* Add statement status here and pass it to the function */
		ZVAL_UNDEF(&return_value);
		ctx->on_rset_end.fci.retval = &return_value;
		ctx->on_rset_end.fci.params = params;
		ctx->on_rset_end.fci.param_count = 2;

		if (zend_call_function(&ctx->on_rset_end.fci, &ctx->on_rset_end.fci_cache) == SUCCESS) {
			if (Z_TYPE(return_value) != IS_UNDEF) {
				zval_ptr_dtor(&return_value);
			}
		} else {
			ret = HND_FAIL;
		}
		zval_ptr_dtor(&params[0]);
	}
	DBG_RETURN(ret);
}
/* }}} */


/* {{{ exec_with_cb_handle_on_statement_ok */
static const enum_hnd_func_status
exec_with_cb_handle_on_statement_ok(void * context,
									XMYSQLND_NODE_STMT * const stmt,
									const struct st_xmysqlnd_stmt_execution_state * const exec_state)
{
	enum_hnd_func_status ret = HND_PASS;
	struct st_xmysqlnd_exec_with_cb_ctx * ctx = (struct st_xmysqlnd_exec_with_cb_ctx *) context;
	DBG_ENTER("exec_with_cb_handle_on_statement_ok");
	if (ctx) {
		zval params[2];
		zval * user_context = &params[0];
		zval * exec_status_zv = &params[1];
		zval return_value;

		ZVAL_COPY(user_context, ctx->ctx);

		ZVAL_UNDEF(exec_status_zv);
		mysqlx_new_execution_status(exec_status_zv, exec_state);
		if (Z_TYPE_P(exec_status_zv) != IS_UNDEF) {

			ZVAL_UNDEF(&return_value);
			ctx->on_stmt_ok.fci.retval = &return_value;
			ctx->on_stmt_ok.fci.params = params;
			ctx->on_stmt_ok.fci.param_count = 1;

			if (zend_call_function(&ctx->on_stmt_ok.fci, &ctx->on_stmt_ok.fci_cache) == SUCCESS) {
				if (Z_TYPE(return_value) != IS_UNDEF) {
					zval_ptr_dtor(&return_value);
				}
			} else {
				ret = HND_FAIL;
			}
		}
		zval_ptr_dtor(&params[0]);
		zval_ptr_dtor(&params[1]);
	}
	DBG_RETURN(ret);
}
/* }}} */


#if 0
#ifndef FAST_ZPP
	if (zend_parse_parameters(ZEND_NUM_ARGS(), "f*", &fci, &fci_cache, &fci.params, &fci.param_count) == FAILURE) {
		return;
	}
#else
	ZEND_PARSE_PARAMETERS_START(1, -1)
		Z_PARAM_FUNC(fci, fci_cache)
		Z_PARAM_VARIADIC('*', fci.params, fci.param_count)
	ZEND_PARSE_PARAMETERS_END();
#endif

#endif


/* {{{ mysqlx_fetch_data_with_callback */
static enum_func_status
mysqlx_fetch_data_with_callback(struct st_mysqlx_node_sql_statement * object, struct st_xmysqlnd_exec_with_cb_ctx * xmysqlnd_exec_with_cb_ctx)
{
	enum_func_status ret;
	zend_bool has_more_results = FALSE;
	XMYSQLND_NODE_STMT * stmt = object->stmt;
	const zend_bool on_rset_end_passed = ZEND_FCI_INITIALIZED(xmysqlnd_exec_with_cb_ctx->on_rset_end.fci);
	const zend_bool on_stmt_ok_passed = ZEND_FCI_INITIALIZED(xmysqlnd_exec_with_cb_ctx->on_stmt_ok.fci);

	const struct st_xmysqlnd_node_stmt_on_row_bind on_row = { exec_with_cb_handle_on_row, xmysqlnd_exec_with_cb_ctx };
	const struct st_xmysqlnd_node_stmt_on_warning_bind on_warning = { exec_with_cb_handle_on_warning, xmysqlnd_exec_with_cb_ctx };
	const struct st_xmysqlnd_node_stmt_on_error_bind on_error = { exec_with_cb_handle_on_error, xmysqlnd_exec_with_cb_ctx };
	const struct st_xmysqlnd_node_stmt_on_result_end_bind on_resultset_end = { on_rset_end_passed? exec_with_cb_handle_on_resultset_end : NULL, xmysqlnd_exec_with_cb_ctx };
	const struct st_xmysqlnd_node_stmt_on_statement_ok_bind on_statement_ok = { on_stmt_ok_passed? exec_with_cb_handle_on_statement_ok : NULL, xmysqlnd_exec_with_cb_ctx };

	DBG_ENTER("mysqlx_fetch_data_with_callback");

	xmysqlnd_exec_with_cb_ctx->on_error.fci.params = xmysqlnd_exec_with_cb_ctx->on_row.fci.params;
	xmysqlnd_exec_with_cb_ctx->on_error.fci.param_count = xmysqlnd_exec_with_cb_ctx->on_row.fci.param_count;

	ret = stmt->data->m.read_one_result(stmt,
										on_row,
										on_warning,
										on_error,
										on_resultset_end,
										on_statement_ok,
										&has_more_results,
										NULL,
										NULL);
	object->has_more_results = has_more_results;
	object->in_execution = has_more_results;
	DBG_RETURN(ret);
}
/* }}} */

#endif /* #ifdef MYSQLX_EXPERIMENTAL_FEATURES */

/* {{{ mysqlx_node_sql_statement_bind_one_param */
void
mysqlx_node_sql_statement_bind_one_param(zval * object_zv, zval * param_zv, const zend_long param_no, zval * return_value)
{
	struct st_mysqlx_node_sql_statement * object;
	DBG_ENTER("mysqlx_node_sql_statement_bind_one_param");
	MYSQLX_FETCH_NODE_SQL_STATEMENT_FROM_ZVAL(object, object_zv);
	RETVAL_TRUE;
	if (TRUE == object->in_execution) {
		php_error_docref(NULL, E_WARNING, "Statement in execution. Please fetch all data first.");
	} else if (object->stmt) {
		object->stmt->data->m.bind_one_param(object->stmt, param_no, param_zv);
	}
	DBG_VOID_RETURN;
}
/* }}} */


/* {{{ proto mixed mysqlx_node_sql_statement::bind(object statement, int param_no, mixed value) */
static
PHP_METHOD(mysqlx_node_sql_statement, bind)
{
	zval * object_zv;
	zend_long param_no;
	zval * param_zv;

	DBG_ENTER("mysqlx_node_sql_statement::bind");
	if (FAILURE == zend_parse_method_parameters(ZEND_NUM_ARGS(), getThis(), "Olz",
												&object_zv, mysqlx_node_sql_statement_class_entry,
												&param_no, &param_zv))
	{
		DBG_VOID_RETURN;
	}
	if (param_no >= MAX_STMT_PARAMS) {
		php_error_docref(NULL, E_WARNING, "param_no too big. Allowed are %", MAX_STMT_PARAMS);
		DBG_VOID_RETURN;
	}
	mysqlx_node_sql_statement_bind_one_param(object_zv, param_zv, param_no, return_value);

	DBG_VOID_RETURN;
}
/* }}} */


/* {{{ mysqlx_node_sql_stmt_on_warning */
static const enum_hnd_func_status
mysqlx_node_sql_stmt_on_warning(void * context, XMYSQLND_NODE_STMT * const stmt, const enum xmysqlnd_stmt_warning_level level, const unsigned int code, const MYSQLND_CSTRING message)
{
	DBG_ENTER("mysqlx_node_sql_stmt_on_warning");
	php_error_docref(NULL, E_WARNING, "[%d] %*s", code, message.l, message.s);
	DBG_RETURN(HND_AGAIN);
}
/* }}} */


/* {{{ mysqlx_node_sql_stmt_on_error */
static const enum_hnd_func_status
mysqlx_node_sql_stmt_on_error(void * context, XMYSQLND_NODE_STMT * const stmt, const unsigned int code, const MYSQLND_CSTRING sql_state, const MYSQLND_CSTRING message)
{
	DBG_ENTER("mysqlx_node_sql_stmt_on_error");
	mysqlx_new_exception(code, sql_state, message);
	DBG_RETURN(HND_PASS_RETURN_FAIL);
}
/* }}} */


#define MYSQLX_EXECUTE_FWD_PREFETCH_COUNT 3
void
mysqlx_node_sql_statement_execute(zval * object_zv, const zend_long flags, zval * return_value)
{
	struct st_mysqlx_node_sql_statement * object = NULL;
	DBG_ENTER("mysqlx_node_sql_statement_execute");

	MYSQLX_FETCH_NODE_SQL_STATEMENT_FROM_ZVAL(object, object_zv);
	RETVAL_FALSE;

	if ((flags | MYSQLX_EXECUTE_ALL_FLAGS) != MYSQLX_EXECUTE_ALL_FLAGS) {
		php_error_docref(NULL, E_WARNING, "Invalid flags. Unknown %lu", flags - (flags | MYSQLX_EXECUTE_ALL_FLAGS));
		DBG_VOID_RETURN;
	}
	DBG_INF_FMT("flags=%lu", flags);
	DBG_INF_FMT("%sSYNC", (flags & MYSQLX_EXECUTE_FLAG_ASYNC)? "A":"");
	DBG_INF_FMT("%s", (flags & MYSQLX_EXECUTE_FLAG_BUFFERED)? "BUFFERED":"FWD");

	if (TRUE == object->in_execution) {
		php_error_docref(NULL, E_WARNING, "Statement in execution. Please fetch all data first.");
	} else {
		XMYSQLND_NODE_STMT * stmt = object->stmt;
		object->execute_flags = flags;
		object->has_more_rows_in_set = FALSE;
		object->has_more_results = FALSE;
		object->send_query_status = stmt->data->m.send_query(stmt, NULL, NULL);

		if (PASS == object->send_query_status) {
			if (object->execute_flags & MYSQLX_EXECUTE_FLAG_ASYNC) {
				DBG_INF("ASYNC");
				RETVAL_BOOL(PASS == object->send_query_status);
			} else {
				const struct st_xmysqlnd_node_stmt_on_warning_bind on_warning = { mysqlx_node_sql_stmt_on_warning, NULL };
				const struct st_xmysqlnd_node_stmt_on_error_bind on_error = { mysqlx_node_sql_stmt_on_error, NULL };
				XMYSQLND_NODE_STMT_RESULT * result;
				if (object->execute_flags & MYSQLX_EXECUTE_FLAG_BUFFERED) {
					result = object->stmt->data->m.get_buffered_result(stmt, &object->has_more_results, on_warning, on_error, NULL, NULL);
				} else {
					result = object->stmt->data->m.get_fwd_result(stmt, MYSQLX_EXECUTE_FWD_PREFETCH_COUNT, &object->has_more_rows_in_set, &object->has_more_results, on_warning, on_error, NULL, NULL);
				}

				DBG_INF_FMT("has_more_results=%s   has_more_rows_in_set=%s",
							object->has_more_results? "TRUE":"FALSE",
							object->has_more_rows_in_set? "TRUE":"FALSE");

				if (result) {
					mysqlx_new_sql_stmt_result(return_value, result);
				} else {
					/* Or we should close the connection, rendering it unusable at this point ?*/
					object->send_query_status = FAIL;
				}
			}
		}
	}
	DBG_VOID_RETURN;
}
/* }}} */


/* {{{ proto mixed mysqlx_node_sql_statement::execute(object statement, int flags) */
static
PHP_METHOD(mysqlx_node_sql_statement, execute)
{
	zend_long flags = 0;
	zval * object_zv;

	DBG_ENTER("mysqlx_node_sql_statement::execute");
	if (FAILURE == zend_parse_method_parameters(ZEND_NUM_ARGS(), getThis(), "O|l",
												&object_zv, mysqlx_node_sql_statement_class_entry,
												&flags))
	{
		DBG_VOID_RETURN;
	}

	mysqlx_node_sql_statement_execute(object_zv, flags, return_value);

	DBG_VOID_RETURN;
}
/* }}} */


/* {{{ proto mixed mysqlx_node_sql_statement::hasMoreResults(object statement) */
static
PHP_METHOD(mysqlx_node_sql_statement, hasMoreResults)
{
	struct st_mysqlx_node_sql_statement * object;
	zval * object_zv;

	DBG_ENTER("mysqlx_node_sql_statement::hasMoreResults");
	if (FAILURE == zend_parse_method_parameters(ZEND_NUM_ARGS(), getThis(), "O",
												&object_zv, mysqlx_node_sql_statement_class_entry))
	{
		DBG_VOID_RETURN;
	}

	MYSQLX_FETCH_NODE_SQL_STATEMENT_FROM_ZVAL(object, object_zv);

	RETVAL_BOOL(object->stmt->data->m.has_more_results(object->stmt));
	DBG_INF_FMT("%s", Z_TYPE_P(return_value) == IS_TRUE? "YES":"NO");

	DBG_VOID_RETURN;
}
/* }}} */


/* {{{ mysqlx_node_sql_statement_read_result */
static void mysqlx_node_sql_statement_read_result(INTERNAL_FUNCTION_PARAMETERS)
{
	struct st_mysqlx_node_sql_statement * object;
	zval * object_zv;
	zend_bool use_callbacks = FALSE;
	struct st_xmysqlnd_exec_with_cb_ctx xmysqlnd_exec_with_cb_ctx;
	memset(&xmysqlnd_exec_with_cb_ctx, sizeof(struct st_xmysqlnd_exec_with_cb_ctx), 0);

	DBG_ENTER("mysqlx_node_sql_statement::getResult");
	if (ZEND_NUM_ARGS() == 0) {
		if (FAILURE == zend_parse_method_parameters(ZEND_NUM_ARGS(), getThis(), "O",
													&object_zv, mysqlx_node_sql_statement_class_entry))
		{
			DBG_VOID_RETURN;
		}
#ifdef MYSQLX_EXPERIMENTAL_FEATURES
	} else {
		if (FAILURE == zend_parse_method_parameters(ZEND_NUM_ARGS(), getThis(), "Offff!f!z",
													&object_zv, mysqlx_node_sql_statement_class_entry,
													&xmysqlnd_exec_with_cb_ctx.on_row.fci, &xmysqlnd_exec_with_cb_ctx.on_row.fci_cache,
													&xmysqlnd_exec_with_cb_ctx.on_warning.fci, &xmysqlnd_exec_with_cb_ctx.on_warning.fci_cache,
													&xmysqlnd_exec_with_cb_ctx.on_error.fci, &xmysqlnd_exec_with_cb_ctx.on_error.fci_cache,
													&xmysqlnd_exec_with_cb_ctx.on_rset_end.fci, &xmysqlnd_exec_with_cb_ctx.on_rset_end.fci_cache,
													&xmysqlnd_exec_with_cb_ctx.on_stmt_ok.fci, &xmysqlnd_exec_with_cb_ctx.on_stmt_ok.fci_cache,
													&xmysqlnd_exec_with_cb_ctx.ctx))
		{
			DBG_VOID_RETURN;
		}
		use_callbacks = TRUE;
#endif
	}

	MYSQLX_FETCH_NODE_SQL_STATEMENT_FROM_ZVAL(object, object_zv);

	RETVAL_FALSE;
	if (PASS == object->send_query_status) {
		XMYSQLND_NODE_STMT * stmt = object->stmt;

		if (use_callbacks) {
			RETVAL_BOOL(PASS == mysqlx_fetch_data_with_callback(object, &xmysqlnd_exec_with_cb_ctx));
		} else {
			const struct st_xmysqlnd_node_stmt_on_warning_bind on_warning = { mysqlx_node_sql_stmt_on_warning, NULL };
			const struct st_xmysqlnd_node_stmt_on_error_bind on_error = { mysqlx_node_sql_stmt_on_error, NULL };
			XMYSQLND_NODE_STMT_RESULT * result;

			if (object->execute_flags & MYSQLX_EXECUTE_FLAG_BUFFERED) {
				result = object->stmt->data->m.get_buffered_result(stmt, &object->has_more_results, on_warning, on_error, NULL, NULL);
			} else {
				result = object->stmt->data->m.get_fwd_result(stmt, MYSQLX_EXECUTE_FWD_PREFETCH_COUNT, &object->has_more_rows_in_set, &object->has_more_results, on_warning, on_error, NULL, NULL);
			}

			DBG_INF_FMT("result=%p  has_more_results=%s", result, object->has_more_results? "TRUE":"FALSE");
			if (result) {
				mysqlx_new_sql_stmt_result(return_value, result);
			}
		}
	}

	DBG_VOID_RETURN;
}
/* }}} */

/* {{{ proto mixed mysqlx_node_sql_statement::readResult(object statement) */
/*     proto mixed mysqlx_node_sql_statement::readResult(object statement, callable on_row_cb, callable on_error_cb, callable on_rset_end, callable on_stmt_ok[, mixed cb_param]]) */
static
PHP_METHOD(mysqlx_node_sql_statement, getResult)
{
	mysqlx_node_sql_statement_read_result(INTERNAL_FUNCTION_PARAM_PASSTHRU);
}
/* }}} */


/* {{{ proto mixed mysqlx_node_sql_statement::getNextResult(object statement) */
static
PHP_METHOD(mysqlx_node_sql_statement, getNextResult)
{
	mysqlx_node_sql_statement_read_result(INTERNAL_FUNCTION_PARAM_PASSTHRU);
}
/* }}} */



/* {{{ mysqlx_node_sql_statement_methods[] */
static const zend_function_entry mysqlx_node_sql_statement_methods[] = {
	PHP_ME(mysqlx_node_sql_statement, __construct,		NULL,													ZEND_ACC_PRIVATE)
	PHP_ME(mysqlx_node_sql_statement, bind,				arginfo_mysqlx_node_sql_statement__bind,				ZEND_ACC_PUBLIC)
	PHP_ME(mysqlx_node_sql_statement, execute,			NULL,													ZEND_ACC_PUBLIC)
	PHP_ME(mysqlx_node_sql_statement, hasMoreResults,	arginfo_mysqlx_node_sql_statement__has_more_results,	ZEND_ACC_PUBLIC)
	PHP_ME(mysqlx_node_sql_statement, getResult,		arginfo_mysqlx_node_sql_statement__get_result, 			ZEND_ACC_PUBLIC)
	PHP_ME(mysqlx_node_sql_statement, getNextResult,	arginfo_mysqlx_node_sql_statement__get_result, 			ZEND_ACC_PUBLIC)
	{NULL, NULL, NULL}
};
/* }}} */


/* {{{ mysqlx_node_sql_statement_property__statement */
static zval *
mysqlx_node_sql_statement_property__statement(const struct st_mysqlx_object * obj, zval * return_value)
{
	const struct st_mysqlx_node_sql_statement * object = (const struct st_mysqlx_node_sql_statement *) (obj->ptr);
	DBG_ENTER("mysqlx_node_sql_statement_property__statement");
	if (object->stmt && object->stmt->data->query.s) {
		ZVAL_STRINGL(return_value, object->stmt->data->query.s, object->stmt->data->query.l);
	} else {
		/*
		  This means EG(uninitialized_value). If we return just return_value, this is an UNDEF-ed value
		  and ISSET will say 'true' while for EG(unin) it is false.
		  In short:
		  return NULL; -> isset()===false, value is NULL
		  return return_value; (without doing ZVAL_XXX)-> isset()===true, value is NULL
		*/
		return_value = NULL;
	}
	DBG_RETURN(return_value);
}
/* }}} */


static zend_object_handlers mysqlx_object_node_sql_statement_handlers;
static HashTable mysqlx_node_sql_statement_properties;

const struct st_mysqlx_property_entry mysqlx_node_sql_statement_property_entries[] =
{
	{{"statement",	sizeof("statement") - 1}, mysqlx_node_sql_statement_property__statement,	NULL},
	{{NULL,	0}, NULL, NULL}
};

/* {{{ mysqlx_node_sql_statement_free_storage */
static void
mysqlx_node_sql_statement_free_storage(zend_object * object)
{
	struct st_mysqlx_object * mysqlx_object = mysqlx_fetch_object_from_zo(object);
	struct st_mysqlx_node_sql_statement * inner_obj = (struct st_mysqlx_node_sql_statement *) mysqlx_object->ptr;

	if (inner_obj) {
		if (inner_obj->stmt) {
			xmysqlnd_node_stmt_free(inner_obj->stmt, NULL, NULL);
		}
		mnd_efree(inner_obj);
	}
	mysqlx_object_free_storage(object); 
}
/* }}} */


/* {{{ php_mysqlx_node_sql_statement_object_allocator */
static zend_object *
php_mysqlx_node_sql_statement_object_allocator(zend_class_entry * class_type)
{
	struct st_mysqlx_object * mysqlx_object = mnd_ecalloc(1, sizeof(struct st_mysqlx_object) + zend_object_properties_size(class_type));
	struct st_mysqlx_node_sql_statement * object = mnd_ecalloc(1, sizeof(struct st_mysqlx_node_sql_statement));

	DBG_ENTER("php_mysqlx_node_sql_statement_object_allocator");
	if (!mysqlx_object || !object) {
		DBG_RETURN(NULL);	
	}
	mysqlx_object->ptr = object;

	zend_object_std_init(&mysqlx_object->zo, class_type);
	object_properties_init(&mysqlx_object->zo, class_type);

	mysqlx_object->zo.handlers = &mysqlx_object_node_sql_statement_handlers;
	mysqlx_object->properties = &mysqlx_node_sql_statement_properties;


	DBG_RETURN(&mysqlx_object->zo);
}
/* }}} */


/* {{{ mysqlx_register_node_sql_statement_class */
void
mysqlx_register_node_sql_statement_class(INIT_FUNC_ARGS, zend_object_handlers * mysqlx_std_object_handlers)
{
	mysqlx_object_node_sql_statement_handlers = *mysqlx_std_object_handlers;
	mysqlx_object_node_sql_statement_handlers.free_obj = mysqlx_node_sql_statement_free_storage;

	{
		zend_class_entry tmp_ce;
		INIT_NS_CLASS_ENTRY(tmp_ce, "Mysqlx", "NodeSqlStatement", mysqlx_node_sql_statement_methods);
		tmp_ce.create_object = php_mysqlx_node_sql_statement_object_allocator;
		mysqlx_node_sql_statement_class_entry = zend_register_internal_class(&tmp_ce);
	}

	zend_hash_init(&mysqlx_node_sql_statement_properties, 0, NULL, mysqlx_free_property_cb, 1);

	/* Add name + getter + setter to the hash table with the properties for the class */
	mysqlx_add_properties(&mysqlx_node_sql_statement_properties, mysqlx_node_sql_statement_property_entries);

	/* The following is needed for the Reflection API */
	zend_declare_property_null(mysqlx_node_sql_statement_class_entry, "statement",	sizeof("statement") - 1, ZEND_ACC_PUBLIC);

	zend_declare_class_constant_long(mysqlx_node_sql_statement_class_entry, "EXECUTE_ASYNC", sizeof("EXECUTE_ASYNC") - 1, MYSQLX_EXECUTE_FLAG_ASYNC);
	zend_declare_class_constant_long(mysqlx_node_sql_statement_class_entry, "BUFFERED", sizeof("BUFFERED") - 1, MYSQLX_EXECUTE_FLAG_BUFFERED);
}
/* }}} */


/* {{{ mysqlx_unregister_node_sql_statement_class */
void
mysqlx_unregister_node_sql_statement_class(SHUTDOWN_FUNC_ARGS)
{
	zend_hash_destroy(&mysqlx_node_sql_statement_properties);
}
/* }}} */


/* {{{ mysqlx_new_sql_stmt */
void
mysqlx_new_sql_stmt(zval * return_value, XMYSQLND_NODE_STMT * stmt)
{
	struct st_mysqlx_object * mysqlx_object;
	struct st_mysqlx_node_sql_statement * object = NULL;
	DBG_ENTER("mysqlx_new_sql_stmt");

	object_init_ex(return_value, mysqlx_node_sql_statement_class_entry);

	mysqlx_object = Z_MYSQLX_P(return_value);
	object = (struct st_mysqlx_node_sql_statement *) mysqlx_object->ptr;
	if (!object) {
		php_error_docref(NULL, E_WARNING, "invalid object of class %s", ZSTR_VAL(mysqlx_object->zo.ce->name));
		DBG_VOID_RETURN;
	}

	object->stmt = stmt;
	object->execute_flags = 0;
	object->send_query_status = FAIL;
	object->in_execution = FALSE;
	object->has_more_results = FALSE;
	object->has_more_rows_in_set = FALSE;

	DBG_VOID_RETURN;
}
/* }}} */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
