#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>


bool is_zero(float a) {
    return fabs(a) <= 0.000001f;
}

unsigned int make_mask(float* vector[], unsigned int size) {
    unsigned int mask = 0b00000000;

    for (unsigned int i=0; i<size; i++) {
        bool binary = !is_zero(*vector[i]);
        mask |= binary << i;
    }

    return mask;
}

unsigned int highest_on_bit(unsigned int number) {
    return log2(number & -number);
}


// Based on https://stackoverflow.com/a/13361077/1524997
char* integer_to_string(unsigned int number) {
    //int len = number==0 ? 1 : floor(log10l(labs(number)))+1;
    int len = number==0 ? 1 : floor(log10l(number))+1;

    char    *buf = calloc(sizeof(char), len+1); // +1 for null
    snprintf(buf, len+1, "%d", number);
    return   buf;
}