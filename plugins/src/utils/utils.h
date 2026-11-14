/*
 * Copyright 2021-2026 Paulo Mateus
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef UTILS_H
#define	UTILS_H

#include <stdbool.h>
#include <stddef.h>


/**
 * @return is @param a (sufficient) equals to zero?
 */
extern bool is_zero(float a);

/**
 * Transfor a bool 'vector' of 'size' into a mask where
 * the least significative bit corresponds to the first element of the 'vector'
 * and so on
 * 
 * @param vector Vector that will be transform in a mask
 * @param size   Size of vector
 * 
 * @return unsigned int Mask
 */
extern unsigned int make_mask(float* vector[], unsigned int size);

/**
 * Position of the least significant bit that is on (0 for the first bit)
 *
 * @param number bits to look at
 * @return position of its lowest bit on, or 0 when no bit is on
 */
extern unsigned int lowest_on_bit(unsigned int number);

/**
 * Duplicate a NUL-terminated string.
 * Remember to free it!
 *
 * @param source string that will be duplicated
 *
 * @return copy of source, or NULL if allocation fails
 */
extern char* duplicate_string(const char* source);

/**
 * Copies at most max_length characters from source,
 * replacing any character outside a-zA-Z+-:_. with '_'.
 * Remember to free it!
 *
 * Uses strnlen (instead of strlen) so a source that isn't
 * NUL-terminated within max_length can't cause an out-of-bounds read.
 *
 * @param source     string that will be sanitized
 * @param max_length maximum amount of characters copied from source
 *
 * @return sanitized copy of source, or NULL if allocation fails
 */
extern char* sanitize_label(const char* source, size_t max_length);

/**
 * Same as sanitize_label(), writing into @p destination, which must hold max_length + 1 characters.
 * Allocates nothing, so it can be used from run().
 */
extern void sanitize_label_to(char* destination, const char* source, size_t max_length);

/**
 * Builds a default label in the form "<prefix> <index+1>".
 * Remember to free it!
 *
 * @param prefix label prefix (e.g. "Footswitch", "Preset")
 * @param index  zero-based index that will be shown as index+1
 *
 * @return generated label, or NULL if allocation fails
 */
extern char* make_default_label(const char* prefix, unsigned int index);

/**
 * Same as make_default_label(), writing into @p destination (at most @p size bytes, NUL included)
 */
extern void format_default_label(char* destination, size_t size, const char* prefix, unsigned int index);

#endif