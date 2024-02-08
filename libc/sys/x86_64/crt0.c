#include <stdlib.h>
#include <string.h>
#include <sys/channel.h>
#include <sys/x86_64/syscall.h>
#include <iridium/syscalls.h>
#include <iridium/errors.h>

void _start(void) {

    char **argv = malloc(sizeof(void*));
    argv[0] = NULL;
    int argc = 0;

    void *buffer = malloc(128);
    size_t buffer_length = 128;

    size_t handles;
    size_t message_length;

    ir_status_t status;
    while ((status = ir_channel_read(STARTUP_CHANNEL_HANDLE, buffer, buffer_length, &handles, &message_length)) != IR_ERROR_NOT_FOUND) {
        if (status == IR_ERROR_BUFFER_TOO_SMALL) {
            buffer_length = handles * sizeof(ir_handle_t) + message_length;
            free(buffer);
            buffer = malloc(buffer_length);
            continue;
        }

        // If the message has the magic string then the single handle provided will be the filesystem channel
        // If the parent process doesn't pass this handle then we cannot use any filesystem calls.
        char *char_data = &((char*)buffer)[handles * sizeof(ir_handle_t)];
        if (strncmp(char_data, "ir_fs", message_length) == 0) {
            _set_fs_channel(*(ir_handle_t*)buffer);
        }
        // Allow argument passing to main() by prefixing a string with "arg"
        else if (strncmp(char_data, "arg", 3)) {
            argc++;
            argv = realloc(argv, sizeof(void*) * argc);
            char *data = malloc(message_length);
            argv[argc] = NULL;
            argv[argc-1] = data;
        }
        _syscall_2(SYSCALL_SERIAL_OUT, (long)"status = %d\n", (long)status);
        if (status == IR_ERROR_WRONG_TYPE) {
            _syscall_1(SYSCALL_DEBUG_DUMP_HANDLES, 0);
            break;
        }
    }

    free(buffer);

    extern int main(int argc, char *argv[]);
    int code = main(argc, argv);

    exit(code);
    while (1);
}
