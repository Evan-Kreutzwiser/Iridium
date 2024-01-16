/// @file kernel/devices/framebuffer.c
/// @brief Manage the framebuffer given by the firmware, and support writing text to the display

#include "kernel/devices/framebuffer.h"
#include "kernel/memory/vm_object.h"
#include "kernel/memory/physical_map.h"
#include "kernel/object.h"
#include "kernel/handle.h"
#include "kernel/process.h"
#include "kernel/string.h"
#include "kernel/arch/arch.h"
#include "iridium/types.h"
#include "types.h"
#include "arch/debug.h"

#define FONT_START _binary____public_fonts_Tamsyn8x16r_psf_start
#define FONT_END _binary____public_fonts_Tamsyn8x16r_psf_end

#define PSF_FONT_MAGIC 0x864ab572

struct psf_font_header {
    uint32_t magic;
    uint32_t version;
    uint32_t header_size; // Offset of glyph array into file
    uint32_t flags;
    uint32_t glyph_count;
    uint32_t bytes_per_glyph;
    uint32_t height;
    uint32_t width;
};

extern char FONT_START;
extern char FONT_END;

vm_object *framebuffer_vm_object;
v_addr_t framebuffer;
int fb_width;
int fb_height;
int fb_pitch;
int fb_bits_per_pixel;

// Measured in characters
int cursor_x;
int cursor_y;

int max_x;
int max_y;

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

    // Map into kernel space so we can render panics and display debugging information
    status = v_addr_region_map_vm_object(kernel_region, V_ADDR_REGION_READABLE | V_ADDR_REGION_WRITABLE | V_ADDR_REGION_DISABLE_CACHE, framebuffer_vm_object, NULL, 0, &framebuffer);
    if (status != IR_OK) {
        debug_printf("Framebuffer mapping error %#d\n", status);
    }
    debug_printf("Mapped framebuffer to %#p\n", framebuffer);

    fb_width = width;
    fb_height = height;
    fb_pitch = pitch;
    fb_bits_per_pixel = bits_per_pixel;

    // TODO: Support other font sizes
    cursor_x = 0;
    cursor_y = 0;
    max_x = width / 8;
    max_y = height / 16;

    // Prevent the framebuffer from being deleted if a process removes its handle
    framebuffer_vm_object->object.references++;

    debug_printf("Framebuffer at %#p is %d by %d pixels, %#zx bytes large\n", location, width, height, pitch * height);

    if (fb_bits_per_pixel != 32) {
        debug_printf("WARNING: Framebuffer not 32 bits per pixel\n");
    }
}

void framebuffer_fill_screen(unsigned char r, unsigned char g, unsigned char b) {
    if (!framebuffer) {
        debug_printf("WARNING: Attempting coloring without valid framebuffer!\n");
        return;
    }

    if (fb_bits_per_pixel == 32) {
        // TODO: Only works on little-endian CPUs
        uint32_t color = r << 16 | g << 8 | b;

        uint32_t* buffer = (uint32_t*)framebuffer;
        for (int y = 0; y < fb_height; y++) {
            for (int x = 0; x < fb_width; x++) {
                *buffer = color;
                buffer++;
            }
            buffer = (uint32_t*)(framebuffer + y * fb_pitch);
        }
    } else {
        // Assume 24 bpp
        char* buffer = (char*)framebuffer;
        for (int y = 0; y < fb_height; y++) {
            for (int x = 0; x < fb_width; x++) {
                buffer[0] = r;
                buffer[1] = g;
                buffer[2] = b;
                buffer += 3;
            }
            buffer = (char*)(framebuffer + y * fb_pitch);
        }
    }
}

void framebuffer_print(const char* string) {
    if (!framebuffer) {
        return;
    }

    const struct psf_font_header *font = (struct psf_font_header*)&FONT_START;

    const int bytes_per_pixel = fb_bits_per_pixel / 8;

    while (*string != '\0' && cursor_y <= max_y) {

        if (*string == '\n') {
            cursor_x = 0;
            cursor_y++;
        }
        else {
            // Find the character in the PSF glyph array
            char* glyph = (char*)((uintptr_t)font + font->header_size + (font->bytes_per_glyph * (*string)));

            int start_x = cursor_x * 8;
            int start_y = cursor_y * 16;
            // Every row of the glyph is a byte, and each bit in the byte represents a pixel
            for (int y = 0; y < 16; y++) {
                for (int x = 0; x < 8; x++) {
                    if ((glyph[y] >> (7-x)) & 1) {
                        // Draw a white pixel in the framebuffer
                        if (fb_bits_per_pixel == 32) {
                            // Optimized single write for pixel
                            *(uint32_t*)(framebuffer + ((y + start_y)*fb_pitch) + ((x + start_x)*bytes_per_pixel)) = 0x00ffffff;
                        } else {
                            // TODO: Assumes 24 bits per pixel
                            char* pixel = (char*)(framebuffer + ((y + start_y)*fb_pitch) + ((x + start_x)*bytes_per_pixel));
                            pixel[0] = 0xff; // R
                            pixel[1] = 0xff; // G
                            pixel[2] = 0xff; // B
                        }
                    }
                }
            }

            cursor_x++;
        }

        // Wrapping at end of line
        if (cursor_x >= max_x) {
            cursor_x = 0;
            cursor_y++;
        }

        string++;
    }
}

char buffer[2048];

void framebuffer_printf(const char * restrict format, ...) {
    if (!framebuffer) {
        return;
    }

    va_list args;
    va_start(args, format);
    vsprintf(buffer, format, args);
    va_end(args);

    framebuffer_print(buffer);
}

void framebuffer_set_cursor_pos(int x, int y) {
    cursor_x = x;
    cursor_y = y;
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
    *bits_per_pixel = fb_bits_per_pixel;
    return IR_OK;
}
