/*
  +----------------------------------------------------------------------+
  | PHP Version 7                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 2006-2017 The PHP Group                                |
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
#ifndef MYSQLX_BASE_SESSION_H
#define MYSQLX_BASE_SESSION_H

#include "xmysqlnd/xmysqlnd_node_session.h"
#include "phputils/allocator.h"

namespace mysqlx {

namespace devapi {

extern zend_class_entry *mysqlx_base_session_class_entry;

struct st_mysqlx_session : public phputils::custom_allocable
{
	drv::XMYSQLND_NODE_SESSION * session;
	zend_bool closed;
};

void mysqlx_register_base_session_class(INIT_FUNC_ARGS, zend_object_handlers * mysqlx_std_object_handlers);
void mysqlx_unregister_base_session_class(SHUTDOWN_FUNC_ARGS);

#define SAVEPOINT_NAME_PREFIX "SAVEPOINT"

} // namespace devapi

} // namespace mysqlx

#endif /* MYSQLX_BASE_SESSION_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
