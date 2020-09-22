#include "engine_hooks.h"

#include <php.h>
#include <time.h>

#include "configuration.h"
#include "ddtrace.h"
#include "span.h"

ZEND_EXTERN_MODULE_GLOBALS(ddtrace);

static zend_op_array *(*_prev_compile_file)(zend_file_handle *file_handle, int type TSRMLS_DC);
static void (*dd_prev_error_cb)(DDTRACE_ERROR_CB_PARAMETERS);

void ddtrace_execute_internal_minit(void);
void ddtrace_execute_internal_mshutdown(void);

static void _compile_minit(void);
static void _compile_mshutdown(void);

void ddtrace_opcode_minit(void);
void ddtrace_opcode_mshutdown(void);

void ddtrace_engine_hooks_minit(void) {
    ddtrace_execute_internal_minit();
    ddtrace_opcode_minit();
    _compile_minit();

    dd_prev_error_cb = zend_error_cb;
    zend_error_cb = ddtrace_error_cb;
}

void ddtrace_engine_hooks_mshutdown(void) {
    if (dd_prev_error_cb == ddtrace_error_cb) {
        zend_error_cb = dd_prev_error_cb;
    }

    _compile_mshutdown();
    ddtrace_opcode_mshutdown();
    ddtrace_execute_internal_mshutdown();
}

static uint64_t _get_microseconds() {
    struct timespec time;
    if (clock_gettime(CLOCK_MONOTONIC, &time) == 0) {
        return time.tv_sec * 1000000U + time.tv_nsec / 1000U;
    }
    return 0U;
}

static zend_op_array *_dd_compile_file(zend_file_handle *file_handle, int type TSRMLS_DC) {
    zend_op_array *res;
    uint64_t start = _get_microseconds();
    res = _prev_compile_file(file_handle, type TSRMLS_CC);
    DDTRACE_G(compile_time_microseconds) += (int64_t)(_get_microseconds() - start);
    return res;
}

static void _compile_minit(void) {
    if (get_dd_trace_measure_compile_time()) {
        _prev_compile_file = zend_compile_file;
        zend_compile_file = _dd_compile_file;
    }
}

static void _compile_mshutdown(void) {
    if (get_dd_trace_measure_compile_time() && zend_compile_file == _dd_compile_file) {
        zend_compile_file = _prev_compile_file;
    }
}

void ddtrace_compile_time_reset(TSRMLS_D) { DDTRACE_G(compile_time_microseconds) = 0; }

int64_t ddtrace_compile_time_get(TSRMLS_D) { return DDTRACE_G(compile_time_microseconds); }

extern inline void ddtrace_backup_error_handling(ddtrace_error_handling *eh, zend_error_handling_t mode TSRMLS_DC);
#if PHP_VERSION_ID < 80000
extern inline void ddtrace_restore_error_handling(ddtrace_error_handling *eh TSRMLS_DC);
#else
void ddtrace_restore_error_handling(ddtrace_error_handling *eh) {
    if (PG(last_error_message)) {
        if (PG(last_error_message) != eh->message) {
            zend_string_release(PG(last_error_message));
        }
        if (PG(last_error_file) != eh->file) {
            free(PG(last_error_file));
        }
    }
    zend_restore_error_handling(&eh->error_handling TSRMLS_CC);
    PG(last_error_type) = eh->type;
    PG(last_error_message) = eh->message;
    PG(last_error_file) = eh->file;
    PG(last_error_lineno) = eh->lineno;
    EG(error_reporting) = eh->error_reporting;
}
#endif
extern inline void ddtrace_sandbox_end(ddtrace_sandbox_backup *backup TSRMLS_DC);
#if PHP_VERSION_ID < 70000
extern inline ddtrace_sandbox_backup ddtrace_sandbox_begin(zend_op *opline_before_exception TSRMLS_DC);
extern inline void ddtrace_maybe_clear_exception(TSRMLS_D);
#else
extern inline ddtrace_sandbox_backup ddtrace_sandbox_begin(void);
extern inline void ddtrace_maybe_clear_exception(void);
extern inline zend_class_entry *ddtrace_get_exception_base(zval *object);
#endif

void ddtrace_error_cb(DDTRACE_ERROR_CB_PARAMETERS) {
    TSRMLS_FETCH();

    ddtrace_span_fci *span = DDTRACE_G(open_spans_top);
    if (span && (type == E_ERROR || type == E_CORE_ERROR || type == E_USER_ERROR)) {
        span->exception = ddtrace_make_exception_from_error(DDTRACE_ERROR_CB_PARAM_PASSTHRU TSRMLS_CC);
        ddtrace_close_all_open_spans(TSRMLS_C);
    }
    dd_prev_error_cb(DDTRACE_ERROR_CB_PARAM_PASSTHRU);
}
