#include <php.h>
#include <stdbool.h>

#include "coms.h"
#include "ddtrace.h"
#include "span.h"
#include "configuration.h"
#include "random.h"
#include "handlers_internal.h"  // For 'ddtrace_replace_internal_function'

ZEND_EXTERN_MODULE_GLOBALS(ddtrace);

static void (*dd_pcntl_fork_handler)(INTERNAL_FUNCTION_PARAMETERS) = NULL;

ZEND_FUNCTION(ddtrace_pcntl_fork) {
    dd_pcntl_fork_handler(INTERNAL_FUNCTION_PARAM_PASSTHRU);
    if (Z_LVAL_P(return_value) == 0) {
        // CHILD PROCESS
        ddtrace_coms_clean_background_sender_after_fork();
        ddtrace_coms_curl_shutdown();
        ddtrace_seed_prng();
            if (!get_DD_TRACE_FORKED_PROCESS()) {
                ddtrace_disable_tracing_in_current_request();
            }
            if (get_DD_TRACE_ENABLED()) {
                if (get_DD_DISTRIBUTED_TRACING()) {
                    if (DDTRACE_G(open_spans_top) != NULL) {
                        DDTRACE_G(distributed_parent_trace_id) = DDTRACE_G(open_spans_top)->span.span_id;
                        DDTRACE_G(trace_id) = DDTRACE_G(open_spans_top)->span.trace_id;
                    }
                } else {
                    DDTRACE_G(distributed_parent_trace_id) = 0;
                    DDTRACE_G(trace_id) = 0;
                }
                ddtrace_free_span_stacks(true);
                if (get_DD_TRACE_GENERATE_ROOT_SPAN()) {
                    ddtrace_push_root_span();
                }
            }
            ddtrace_coms_init_and_start_writer();
    }
}

/* This function is called during process startup so all of the memory allocations should be
 * persistent to avoid using the Zend Memory Manager. This will avoid an accidental use after free.
 *
 * "If you use ZendMM out of the scope of a request (like in MINIT()), the allocation will be
 * silently cleared by ZendMM before treating the first request, and you'll probably use-after-free:
 * simply don't."
 *
 * @see http://www.phpinternalsbook.com/php7/memory_management/zend_memory_manager.html#common-errors-and-mistakes
 */
void ddtrace_pcntl_handlers_startup(void) {
    // if we cannot find ext/pcntl then do not instrument it
    zend_string *pcntl = zend_string_init(ZEND_STRL("pcntl"), 1);
    bool pcntl_loaded = zend_hash_exists(&module_registry, pcntl);
    zend_string_release(pcntl);
    if (!pcntl_loaded) {
        return;
    }

    dd_zif_handler handlers[] = {
        {ZEND_STRL("pcntl_fork"), &dd_pcntl_fork_handler, ZEND_FN(ddtrace_pcntl_fork)},
    };
    size_t handlers_len = sizeof handlers / sizeof handlers[0];
    for (size_t i = 0; i < handlers_len; ++i) {
        dd_install_handler(handlers[i]);
    }
}
