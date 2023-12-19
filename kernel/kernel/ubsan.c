
#include "arch/debug.h"
#include <stdint.h>

struct source_location {
    const char *file;
    uint32_t line;
    uint32_t column;
};

struct type_descriptor {
    uint16_t kind;
    uint16_t info;
    char name[];
};

struct type_mismatch_info {
    struct source_location location;
    struct type_descriptor *type;
    uintptr_t alignment;
    uint8_t type_check_kind;
};

struct out_of_bounds_info {
    struct source_location location;
    struct type_descriptor array_type;
    struct type_descriptor index_type;
};

// Alignment must be a power of 2.
#define is_aligned(value, alignment) !(value & (alignment - 1))

const char *Type_Check_Kinds[] = {
    "load of",
    "store to",
    "reference binding to",
    "member access within",
    "member call on",
    "constructor call on",
    "downcast of",
    "downcast of",
    "upcast of",
    "cast to virtual base of",
};

static void log_location(struct source_location *location) {
    debug_printf("\tfile: %s\n\tline: %i\n\tcolumn: %i\n",
         location->file, location->line, location->column);
}


void __ubsan_handle_type_mismatch_v1(struct type_mismatch_info *type_mismatch,
                                  uintptr_t pointer) {
    struct source_location *location = &type_mismatch->location;
    if (pointer == 0) {
        debug_print("Null pointer access\n");
        log_location(location);
        return;
    } else if (type_mismatch->alignment != 0 &&
               is_aligned(pointer, type_mismatch->alignment)) {
        // Most useful on architectures with stricter memory alignment requirements, like ARM.
        debug_print("Unaligned memory access\n");
    } else {
        debug_print("Insufficient size\n");
        debug_printf("%s address %p with insufficient space for object of type %s\n",
             Type_Check_Kinds[type_mismatch->type_check_kind], (void *)pointer,
             type_mismatch->type->name);
    }
    log_location(location);

    debug_print("Panic");
    asm volatile ("int $3");

    while (1);
}

//void __ubsan_handle_type_mismatch_v1() {
//    debug_print("UB: Type mismatch\n");
//}

void __ubsan_handle_pointer_overflow() {
    debug_print("UB: Pointer overflow\n");
    asm volatile ("int $0");
}

void __ubsan_handle_add_overflow() {
    debug_print("Add overflow\n");
}

void __ubsan_handle_out_of_bounds(struct out_of_bounds_info *info, uintptr_t *index) {
    debug_print("UB: Out of bounds\n");
    debug_printf("%#p\n", info->location);
    (void) index;
    asm volatile ("int $0");
}

void __ubsan_handle_mul_overflow() {
    debug_print("UB: Mul overflow\n");
}

void __ubsan_handle_sub_overflow() {
    debug_print("UB: Sub overflow\n");
}

void __ubsan_handle_shift_out_of_bounds() {
    debug_print("UB: Shift out of bounds\n");
    asm volatile ("int $0");
}

void __ubsan_handle_divrem_overflow() {
    debug_print("UB: Divrem overflow\n");
}

void __ubsan_handle_vla_bound_not_positive() {
    debug_print("UB: vla bound not positive\n");
}

void __ubsan_handle_load_invalid_value() {
    debug_print("UB: Load invalid value\n");
    asm volatile ("int $0");
}

void __ubsan_handle_negate_overflow() {
    debug_print("UB: Negate overflow\n");
}
