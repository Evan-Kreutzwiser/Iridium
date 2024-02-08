#include <stdlib.h>

long labs(long i) {
    long sign_mask = i >> (sizeof(long) - 1); // Bits are all 0s for positive numbers, all 1s for negative numbers
    return (i ^ sign_mask) + (sign_mask & 1);
}
