/// @file kernel/devices/framebuffer.c
/// @brief Manage the framebuffer given by the firmware

#include "kernel/devices/framebuffer.h"
#include "kernel/memory/vm_object.h"
#include "kernel/object.h"
#include "kernel/process.h"
#include "kernel/handle.h"
#include "kernel/arch/arch.h"
#include "iridium/types.h"
#include "types.h"
#include "arch/debug.h"


vm_object *framebuffer_vm_object;
v_addr_t framebuffer;
int fb_width;
int fb_height;
int fb_pitch;
int fb_bpp;

/// @brief Store framebuffer information for later use
/// @param location Physical address of the framebuffer
/// @param width Framebuffer width in pixels
/// @param height Framebuffer height in pixels
/// @param pitch Width of each row in bytes
/// @param bits_per_pixel
void init_framebuffer(p_addr_t location, int width, int height, int pitch, int bits_per_pixel) {
    debug_printf("Allocating framebuffer\n");
    ir_status_t status = vm_object_create_physical(location, pitch * height, VM_MMIO_FLAGS, &framebuffer_vm_object);
    if (status != IR_OK) {
        debug_printf("Framebuffer reserving returned error %#d\n", status);
        return;
    }

    //status = v_addr_region_map_vm_object(kernel_region, V_ADDR_REGION_READABLE | V_ADDR_REGION_WRITABLE, framebuffer_vm_object, NULL, 0, &framebuffer);
    //if (status != IR_OK) {
    //    debug_printf("Framebuffer mapping error %#d\n", status);
    //}

    fb_width = width;
    fb_height = height;
    fb_pitch = pitch;
    fb_bpp = bits_per_pixel;

    // Prevent the framebuffer from being deleted if a process removes its handle
    framebuffer_vm_object->object.references++;

    debug_printf("Framebuffer at %#p is %d by %d pixels, %#zx bytes large\n", location, width, height, pitch * height);
}

/// @brief SYSCALL_DEBUG_GET_FRAMEBUFFER
/// @return `IR_OK` if a framebuffer exists, otherwise `IR_ERROR_NOT_FOUND`
ir_status_t sys_framebuffer_get(ir_handle_t *framebuffer, int *width, int *height, int *pitch, int *bits_per_pixel) {
    if (!arch_validate_user_pointer(framebuffer) || !arch_validate_user_pointer(width)
        || !arch_validate_user_pointer(height) || !arch_validate_user_pointer(pitch)
        || !arch_validate_user_pointer(bits_per_pixel)) {
        return IR_ERROR_INVALID_ARGUMENTS;
    }
    if (!framebuffer_vm_object) return IR_ERROR_NOT_FOUND;

    struct process *process = (struct process*)this_cpu->current_thread->object.parent;
    // Does not need to obtain handle table lock, because this does not access existing handles or remove any
    struct handle *handle;
    handle_create(process, (object*)framebuffer_vm_object, IR_RIGHT_MAP | IR_RIGHT_WRITE | IR_RIGHT_READ, &handle);
    linked_list_add(&process->handle_table, handle);

    *framebuffer = handle->handle_id;
    *width = fb_width;
    *height = fb_height;
    *pitch = fb_pitch;
    *bits_per_pixel = fb_bpp;
    return IR_OK;
}
