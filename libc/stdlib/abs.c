#include <stdlib.h>

int abs(int i) {
    int sign_mask = i >> (sizeof(int) - 1); // Bits are all 0s for positive numbers, all 1s for negative numbers
    return (i ^ sign_mask) + (sign_mask & 1);
}
