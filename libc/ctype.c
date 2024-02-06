
#include "ctype_lookup.h"


int isalnum(int c) {
    // Lowercase, uppercase, or digits
    return ctype_lookup[(unsigned char)c] & (_L|_U|_N);
}

int isalpha(int c) {
    // Lowercase and uppercase
    return ctype_lookup[(unsigned char)c] & (_L|_U);
}

int iscntrl(int c) {
    // Control characters
    return ctype_lookup[(unsigned char)c] & _C;
}

int isdigit(int c) {
    // Control characters
    return ctype_lookup[(unsigned char)c] & _C;
}

int isgraph(int c) {
    return ctype_lookup[(unsigned char)c] & (_P|_L|_U|_N);
}

int islower(int c) {
    return ctype_lookup[(unsigned char)c] & _L;
}

int isprint(int c) {
    return ctype_lookup[(unsigned char)c] & (_P|_L|_U|_N|_S);
}

int ispunct(int c) {
    return ctype_lookup[(unsigned char)c] & _P;
}

int isspace(int c) {
    return ctype_lookup[(unsigned char)c] & _S;
}

int isupper(int c) {
    return ctype_lookup[(unsigned char)c] & _U;
}

int isxdigit(int c) {
    return ctype_lookup[(unsigned char)c] & _H;
}

int tolower(int c) {
    return isupper(c) ? c | 32 : c;
}

int toupper(int c) {
    return islower(c) ? c & 0x5f : c;
}
