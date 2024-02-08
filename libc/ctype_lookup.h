
#ifndef _LIBC_CTYPE_LOOKUP_H_
#define _LIBC_CTYPE_LOOKUP_H_

#ifdef __cplusplus
extern "C" {
#endif

#define _L 0x01 // Lowercase characters
#define _U 0x02 // Uppercase characters
#define _N 0x04 // Numeric digits
#define _H 0x08 // Hexadecimal digits
#define _P 0x10 // Punctuation
#define _S 0x20 // Space characters (Space, tabs, newlines, etc)
#define _C 0x40 // Control characters
#define _B 0x80 // Blank characters (Spaces and tabs)

unsigned char ctype_lookup[256] = {
    /* 0x00 */ _C, _C, _C, _C, _C, _C, _C, _C,
    /* 0x08 */ _C, _C|_S, _C|_S, _C|_S, _C|_S, _C|_S, _C, _C,
    /* 0x10 */ _C, _C, _C, _C, _C, _C, _C, _C,
    /* 0x18 */ _C, _C, _C, _C, _C, _C, _C, _C,
    /* 0x20 */ _S|_B, _P, _P, _P, _P, _P, _P, _P,
    /* 0x28 */ _P, _P, _P, _P, _P, _P, _P, _P,
    /* 0x30 */ _N|_H, _N|_H, _N|_H, _N|_H, _N|_H, _N|_H, _N|_H, _N|_H,
    /* 0x38 */ _N|_H, _N|_H, _P, _P, _P, _P, _P, _P,
    /* 0x40 */ _P, _U|_H, _U|_H, _U|_H, _U|_H, _U|_H, _U|_H, _U,
    /* 0x48 */ _U, _U, _U, _U, _U, _U, _U, _U,
    /* 0x50 */ _U, _U, _U, _U, _U, _U, _U, _U,
    /* 0x58 */ _U, _U, _U, _P, _P, _P, _P, _P,
    /* 0x60 */ _P, _L|_H, _L|_H, _L|_H, _L|_H, _L|_H, _L|_H, _L,
    /* 0x68 */ _L, _L, _L, _L, _L, _L, _L, _L,
    /* 0x70 */ _L, _L, _L, _L, _L, _L, _L, _L,
    /* 0x78 */ _L, _L, _L, _P, _P, _P, _P, _C,

    /* 128-255 unsupported*/
};

#ifdef __cplusplus
}
#endif

#endif // _LIBC_CTYPE_LOOKUP_H_
