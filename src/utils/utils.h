#ifndef UTILS_H
#define	UTILS_H

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
extern unsigned int make_mask(bool* vector[], unsigned int size);

/**
 * Returns the bit with the highest significative bit on
 * 
 * @param number 
 * @return unsigned int 
 */
extern unsigned int highest_on_bit(unsigned int number);

/**
 * Covert integer into string.
 * Remember to free it!
 * 
 * @param number that will be converted into string
 * 
 * @return string generated
 */
extern char* integer_to_string(unsigned int number);
#endif