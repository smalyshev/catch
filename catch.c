#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_catch.h"
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>

ZEND_DECLARE_MODULE_GLOBALS(catch);

int nullfd;
static void (*old_error_handler)(int error_num, const char *error_filename, const uint error_lineno, const char *format, va_list args);
	
/* {{{ catch_functions[]
 *
 * Every user visible function must have an entry in catch_functions[].
 */
const zend_function_entry catch_functions[] = {
	{NULL, NULL, NULL}	/* Must be the last line in catch_functions[] */
};
/* }}} */

/* {{{ catch_module_entry
 */
zend_module_entry catch_module_entry = {
	STANDARD_MODULE_HEADER,
	"catch",
	catch_functions,
	PHP_MINIT(catch),
	PHP_MSHUTDOWN(catch),
	PHP_RINIT(catch),		/* Replace with NULL if there's nothing to do at request start */
	PHP_RSHUTDOWN(catch),	/* Replace with NULL if there's nothing to do at request end */
	PHP_MINFO(catch),
	"0.1", /* Replace with version number for your extension */
	STANDARD_MODULE_PROPERTIES
};
/* }}} */

#ifdef COMPILE_DL_CATCH
ZEND_GET_MODULE(catch)
#endif

static PHP_INI_MH(OnUpdateHandler)
{
	long *p;
#ifndef ZTS
	char *base = (char *) mh_arg2;
#else
	char *base;
	base = (char *) ts_resource(*((int *) mh_arg2));
#endif
	p = (long *) (base+(size_t) mh_arg1);

	if(!new_value) {
		*p = 0;
	}
	while(new_value) {
		char *next = strchr(new_value, ',');
		int len;
		if(next) {
			len = next-new_value;
		} else {
			len = strlen(new_value);
		}
		if(strncmp(new_value, "stop", len) == 0) {
			*p |= CATCH_STOP;
		} else 	if(strncmp(new_value, "trace", len) == 0) {
			*p |= CATCH_STACK;
		} else if(strncmp(new_value, "phptrace", len) == 0) {
			*p |= CATCH_PHP_STACK;
		}
		if(next) {
			new_value = next+1;
		} else {
			break;
		}
	}
}

/* {{{ PHP_INI
 */
PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("catch.oncrash", "stop,trace,phptrace", PHP_INI_ALL, OnUpdateHandler, on_crash, zend_catch_globals, catch_globals)
    STD_PHP_INI_ENTRY("catch.onerror", "phptrace", PHP_INI_ALL, OnUpdateHandler, on_error, zend_catch_globals, catch_globals)
PHP_INI_END()
/* }}} */

void print_trace()
{
  void           *array[32];    /* Array to store backtrace symbols */
  size_t          size;     /* To store the exact no of values stored */

  size = backtrace(array, 32);

  backtrace_symbols_fd(array, size, fileno(stderr));
}

int safe_pointer(void *pointer)
{
	if (write(nullfd, pointer, 1) < 0)
	{
		return 0;
	}
	return 1;	
}

int makes_sense(const char *pointer)
{
	if(!safe_pointer((void *)pointer)) return 0;
	if(*pointer < ' ' || *((unsigned char *)pointer) > 0x80) return 0;
	return 1;
}

const char * safe_string(const char *ptr)
{
	if(ptr == NULL) return "[NULL]";
	return makes_sense(ptr)?ptr:"[???]";
}

/*
dumps the current execution stack. usage: dump_bt executor_globals.current_execute_data
define dump_bt
        set $t = $arg0
        while $t
                printf "[0x%08x] ", $t
                if $t->function_state.function->common.function_name
                        printf "%s() ", $t->function_state.function->common.function_name
                else
                        printf "??? "
                end
                if $t->op_array != 0
                        printf "%s:%d ", $t->op_array->filename, $t->opline->lineno
                end
                set $t = $t->prev_execute_data
                printf "\n"
        end
end
*/
void print_php_trace()
{
	struct _zend_execute_data *ed = executor_globals.current_execute_data;
	fprintf(stderr, "PHP backtrace:\n");
	while(ed && safe_pointer(ed))  {
		zend_function *func = ed->function_state.function;
		char *funcname, *filename;
		zend_op_array *op_array = ed->op_array;
		struct _zend_op *opline = ed->opline;
		int lineno = 0;
		
		if(safe_pointer(func)) {
			funcname = func->common.function_name;
		} else {
			funcname = "[???]";
		}
		if(safe_pointer(op_array)) {
			filename = op_array->filename;
		} else {
			filename = "[???]";
		}
		if(safe_pointer(opline)) {
			lineno = opline->lineno;
		} 
		fprintf(stderr, "%s() %s:%d\n", safe_string(funcname), safe_string(filename), lineno);
		ed = ed->prev_execute_data;
	}
}

void segv_handler(int sig_num, siginfo_t * info, void * ucontext)
{
	TSRMLS_FETCH();
	fflush(stderr);
	fprintf(stderr, "CRASH(%d): signal %d (%s), address is %p\n", getpid(), sig_num, strsignal(sig_num), info->si_addr);
	if(CATCH_G(on_crash) & CATCH_STACK) {
		print_trace();
	}
	if(CATCH_G(on_crash) & CATCH_PHP_STACK) {
		print_php_trace();
	}
	fflush(stderr);
	if(CATCH_G(on_crash) & CATCH_STOP) {
		pause();
	}
}

void err_handler(zend_execute_data *ed TSRMLS_DC)
{
	fflush(stderr);
	if(CATCH_G(on_error)) {
		fprintf(stderr, "ERROR(%d)\n", getpid());
	}
	if(CATCH_G(on_error) & CATCH_STACK) {
		print_trace();
	}
	if(CATCH_G(on_error) & CATCH_PHP_STACK) {
		EG(current_execute_data) = ed;
		print_php_trace();
		EG(current_execute_data) = NULL;
	}
	fflush(stderr);
	if(CATCH_G(on_error) & CATCH_STOP) {
		pause();
	}
}

static void catch_error_handler(int error_num, const char *error_filename, const uint error_lineno, const char *format, va_list args)
{
	zend_execute_data *ed;
	TSRMLS_FETCH();

	ed = EG(current_execute_data);
	zend_try {
#ifdef va_copy
        va_list copy; 
        va_copy(copy, args); 
        old_error_handler(error_num, error_filename, error_lineno, format, copy); 
        va_end(copy); 
#else		
        old_error_handler(error_num, error_filename, error_lineno, format, args); 
#endif
	} zend_catch {
		err_handler(ed TSRMLS_CC);
		zend_bailout();
	} zend_end_try();
}

/* {{{ php_catch_init_globals
 */
static void php_catch_init_globals(zend_catch_globals *catch_globals)
{
	catch_globals->on_crash = 0;
	catch_globals->on_error = 0;
}
/* }}} */

static void setsignals()
{
	struct sigaction act = {0}; 
	act.sa_sigaction = segv_handler;
 	act.sa_flags = SA_RESTART | SA_SIGINFO;

	sigaction(SIGBUS, &act, NULL);
	sigaction(SIGILL, &act, NULL);
	sigaction(SIGSEGV, &act, NULL);
}

/* {{{ PHP_MINIT_FUNCTION
 */
PHP_MINIT_FUNCTION(catch)
{
	REGISTER_INI_ENTRIES();
	setsignals();
	nullfd = open("/dev/null", O_WRONLY);
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MSHUTDOWN_FUNCTION
 */
PHP_MSHUTDOWN_FUNCTION(catch)
{
	UNREGISTER_INI_ENTRIES();
	close(nullfd);
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_RINIT_FUNCTION
 */
PHP_RINIT_FUNCTION(catch)
{
	setsignals();
	old_error_handler = zend_error_cb;
	zend_error_cb = catch_error_handler;
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_RSHUTDOWN_FUNCTION
 */
PHP_RSHUTDOWN_FUNCTION(catch)
{
	setsignals();
	zend_error_cb = old_error_handler;
	return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION
 */
PHP_MINFO_FUNCTION(catch)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "catch support", "enabled");
	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();
}
/* }}} */

PHP_FUNCTION(catch_report)
{
	long options = 0;
	
  	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|l", &options) == FAILURE) {
		return;
	}
	if(options&CATCH_STACK) {
		print_trace();
	}
	if(options&CATCH_PHP_STACK) {
		print_php_trace();
	}
	if(options&CATCH_STOP) {
		fflush(stderr);
		fprintf(stderr, "STOP(%d): waiting...\n", getpid());
		fflush(stderr);
		pause();
	}
}


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
