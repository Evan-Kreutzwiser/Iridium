#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>

int atoi(const char *str) {
    bool negative = false;
    if (*str == '-') { str++; negative = true; }
    else if (*str == '+') { str++; }

    // Reading in the value as a negative number allows INT_MIN to be handled correctly without overflowing
    int negated_value = 0;
    while (isdigit(*str)) {
        negated_value *= 10;
        negated_value -= *str - '0';
    }

    return negative ? negated_value : negated_value * -1;
}

long atol(const char *str) {
    bool negative = false;
    if (*str == '-') { str++; negative = true; }
    else if (*str == '+') { str++; }

    // Reading in the value as a negative number allows LONG_MIN to be handled correctly without overflowing
    long negated_value = 0;
    while (isdigit(*str)) {
        negated_value *= 10;
        negated_value -= *str - '0';
    }

    return negative ? negated_value : negated_value * -1;
}
