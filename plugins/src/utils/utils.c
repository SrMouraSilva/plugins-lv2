#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

static bool is_valid_label_char(char c);


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

char* duplicate_string(const char* source) {
    size_t length = strlen(source) + 1;
    char* copy = (char*) malloc(length);

    if (copy == NULL) {
        return NULL;
    }

    memcpy(copy, source, length);

    return copy;
}

static bool is_valid_label_char(char c) {
    return (c >= 'a' && c <= 'z')
        || (c >= 'A' && c <= 'Z')
        || (c >= '0' && c <= '9')
        || c == '+'
        || c == '-'
        || c == ':'
        || c == '_'
        || c == '.'
        || c == ' ';
}

char* sanitize_label(const char* source, size_t max_length) {
    size_t length = strnlen(source, max_length);
    char* sanitized = (char*) malloc(length + 1);

    if (sanitized == NULL) {
        return NULL;
    }

    for (size_t i = 0; i < length; i++) {
        sanitized[i] = is_valid_label_char(source[i]) ? source[i] : '_';
    }

    sanitized[length] = '\0';

    return sanitized;
}

char* make_default_label(const char* prefix, unsigned int index) {
    char label[32];
    snprintf(label, sizeof(label), "%s %u", prefix, index + 1);

    return duplicate_string(label);
}