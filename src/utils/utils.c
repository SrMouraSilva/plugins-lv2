#include <stdbool.h>
#include <math.h>


unsigned int make_mask(bool* vector[], unsigned int size) {
    unsigned int mask = 0b00000000;

    for (unsigned int i=0; i<size; i++) {
        mask |= *vector[i] << i;
    }

    return mask;
}

unsigned int highest_on_bit(unsigned int number) {
    return log2(number & -number);
}
