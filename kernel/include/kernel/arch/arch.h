/// @brief Misc. architecture-specific functions

#ifndef KERNEL_ARCH_H_
#define KERNEL_ARCH_H_

#include <stdint.h>
#include <stdbool.h>
#include <stdnoreturn.h>

struct registers; // Defined in arch/registers.h
struct per_cpu_data; // Defined in kernel/cpu_locals.h
struct physical_region; // Defined in kernel/memory/pmm.h

/// @brief Set data accessible through `this_cpu` in kernel/process.h
/// @param cpu_local_data This cpu's local data struct
void arch_set_cpu_local_pointer(struct per_cpu_data *cpu_local_data);

/// @brief Check that the data addressed by a pointer is in user memory
/// @param pointer A pointer that a user process has given the kernel
/// @return Whether the pointer points to an address in user memory
bool arch_validate_user_pointer(void *pointer);

/// Check whether a the target of a pointer is in kernel space
bool arch_is_kernel_pointer(void *pointer);

/// Pause execution on the cpu
void arch_pause();

/// Prevent interrupts from firing while running important code
void arch_enter_critical();
/// Allow interrupts to fire again
void arch_exit_critical();

/// Add an interrupt handler to the platform's interrupt table
void arch_interrupt_set(int vector, int irq);
/// Remove an interrupt from the interrupt table
void arch_interrupt_remove(int irq);

/// Initialize a new thread's context with some basic register values
/// required to enter usermode and execute code
void arch_initialize_thread_context(struct registers *context);

/// Initialize a thread context for a kernel space idle thread
/// @note Target thread is assumed to ONLY run on the current cpu,
/// and that the cpu local data pointer has already been set
void arch_initialize_idle_thread_context(struct registers *context);

/// Set a thread's instruction pointer. For example, setting the entry
/// point before starting a process
void arch_set_instruction_pointer(struct registers *registers, uintptr_t pointer);
/// @brief Set a thread's stack pointer.
/// Used to initialize stacks
void arch_set_stack_pointer(struct registers *registers, uintptr_t pointer);
void arch_set_frame_pointer(struct registers *registers, uintptr_t pointer);
void arch_set_interrupt_stack(uintptr_t stack_top);

/// TODO: Do all 64 bit architectures use registers for parameters?
void arch_set_arg_0(struct registers *registers, uintptr_t value);

void arch_io_output(int port, long value, int word_size);
long arch_io_input(int port, int word_size);

/// @brief Resume executing a thread.
///
/// Assumeing the thread's address space is already loaded,
/// load its register values and enter usermode.
/// @note Does not return - executes a user process
noreturn void arch_enter_context(struct registers *context);

/// @brief Save cpu context to be resumed later
/// @param context The space to save register values to
void arch_save_context(struct registers *context);

/// @brief Task switching utility that unwinds a level of the stack before returning
void arch_leave_function(void);

/// @brief Print cpu register contents during a kernel panic
void arch_print_context_dump(struct registers *context);

/// @brief Print a stack trace during a kernel panic
void arch_print_stack_trace(struct registers *context);

#endif // ! KERNEL_ARCH_H_
