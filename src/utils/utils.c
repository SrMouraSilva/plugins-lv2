#include <stdio.h>
#include <stdlib.h>
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


// Based on https://stackoverflow.com/a/13361077/1524997
char* integer_to_string(unsigned int number) {
    //return number +'0';
    int len = number==0 ? 1 : floor(log10l(labs(number)))+1;
    //if (number<0) len++; // room for negative sign '-'

    char    *buf = calloc(sizeof(char), len+1); // +1 for null
    snprintf(buf, len+1, "%d", number);
    return   buf;
}