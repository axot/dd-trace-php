#include "compile.h"

#include <php.h>
#include <stdint.h>
#include <time.h>

#include "ddtrace.h"

ZEND_EXTERN_MODULE_GLOBALS(ddtrace);

zend_op_array *(*ddtrace_orig_compile_file)(zend_file_handle *file_handle, int type TSRMLS_DC);

static uint64_t _get_microseconds() {
    struct timespec time;
    if (clock_gettime(CLOCK_MONOTONIC, &time) == 0) {
        return time.tv_sec * 1000000U + time.tv_nsec / 1000U;
    }
    return 0U;
}

static zend_op_array *ddtrace_compile_file(zend_file_handle *file_handle, int type TSRMLS_DC) {
    zend_op_array *res;
    uint64_t start = _get_microseconds();
    res = ddtrace_orig_compile_file(file_handle, type TSRMLS_CC);
    DDTRACE_G(compile_time_microseconds) += (uint32_t)(_get_microseconds() - start);
    return res;
}

void ddtrace_compile_hook() {
    ddtrace_orig_compile_file = zend_compile_file;
    zend_compile_file = ddtrace_compile_file;
}

void ddtrace_compile_unhook() {
    if (zend_compile_file == ddtrace_compile_file) {
        zend_compile_file = ddtrace_orig_compile_file;
    }
}

void ddtrace_compile_time_reset(TSRMLS_D) { DDTRACE_G(compile_time_microseconds) = 0; }

uint32_t ddtrace_compile_time_get(TSRMLS_D) { return DDTRACE_G(compile_time_microseconds); }