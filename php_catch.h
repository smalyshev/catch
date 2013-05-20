#define JMP_BUF jmp_buf
/*
  +----------------------------------------------------------------------+
  | PHP Version 5                                                        |
  +----------------------------------------------------------------------+
  | Copyright (c) 1997-2010 The PHP Group                                |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
  | Author:                                                              |
  +----------------------------------------------------------------------+
*/

/* $Id: header 297205 2010-03-30 21:09:07Z johannes $ */

#ifndef PHP_CATCH_H
#define PHP_CATCH_H

extern zend_module_entry catch_module_entry;
#define phpext_catch_ptr &catch_module_entry

#ifdef ZTS
#include "TSRM.h"
#endif

PHP_MINIT_FUNCTION(catch);
PHP_MSHUTDOWN_FUNCTION(catch);
PHP_RINIT_FUNCTION(catch);
PHP_RSHUTDOWN_FUNCTION(catch);
PHP_MINFO_FUNCTION(catch);

PHP_FUNCTION(catch_report);

ZEND_BEGIN_MODULE_GLOBALS(catch)
	long on_crash;
	long on_error;
  zend_bool in_execute;
  JMP_BUF *exec_buf;
ZEND_END_MODULE_GLOBALS(catch)

#define CATCH_STACK		  1
#define CATCH_PHP_STACK	2
#define CATCH_STOP		  4
#define CATCH_THROW     8

#ifdef ZTS
#define CATCH_G(v) TSRMG(catch_globals_id, zend_catch_globals *, v)
#else
#define CATCH_G(v) (catch_globals.v)
#endif

#endif	/* PHP_CATCH_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
