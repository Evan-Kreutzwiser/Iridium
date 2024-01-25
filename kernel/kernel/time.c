
#include "iridium/errors.h"
#include "iridium/types.h"
#include "kernel/arch/arch.h"
#include <stddef.h>

size_t volatile microseconds_since_boot = 0;

ir_status_t sys_time_microseconds(size_t *out) {
    if (!arch_validate_user_pointer(out)) {
        return IR_ERROR_INVALID_ARGUMENTS;
    }

    *out = microseconds_since_boot;
    return IR_OK;
}
