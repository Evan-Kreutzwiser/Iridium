


void _start(void) {


    extern int main();
    int code = main();

    extern void exit(int);
    exit(code);
    while (1);
}
