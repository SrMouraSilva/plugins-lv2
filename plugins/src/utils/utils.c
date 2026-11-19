/*
 * Copyright 2021-2026 Paulo Mateus
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

static bool is_valid_label_char(char c);


bool is_zero(float a) {
    return fabsf(a) <= 0.000001f;
}

unsigned int make_mask(float* vector[], unsigned int size) {
    unsigned int mask = 0b00000000;

    for (unsigned int i=0; i<size; i++) {
        bool binary = !is_zero(*vector[i]);
        mask |= binary << i;
    }

    return mask;
}

unsigned int lowest_on_bit(unsigned int number) {
    return number == 0 ? 0 : (unsigned int) __builtin_ctz(number);
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

void sanitize_label_to(char* destination, const char* source, size_t max_length) {
    size_t length = strnlen(source, max_length);

    for (size_t i = 0; i < length; i++) {
        destination[i] = is_valid_label_char(source[i]) ? source[i] : '_';
    }

    destination[length] = '\0';
}

char* sanitize_label(const char* source, size_t max_length) {
    char* sanitized = (char*) malloc(strnlen(source, max_length) + 1);

    if (sanitized == NULL) {
        return NULL;
    }

    sanitize_label_to(sanitized, source, max_length);

    return sanitized;
}

void format_default_label(char* destination, size_t size, const char* prefix, unsigned int index) {
    snprintf(destination, size, "%s %u", prefix, index + 1);
}

char* make_default_label(const char* prefix, unsigned int index) {
    char label[32];
    format_default_label(label, sizeof(label), prefix, index);

    return duplicate_string(label);
}