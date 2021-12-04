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

/*
cc utils/utils.c
   utils/utils.h model/tetr4-switch.h lv2-hmi.h extension/lv2-hmi-extension.h
   -Wall -Wextra -pipe -Wno-unused-parameter -fPIC -DPIC -DNDEBUG -O3 -ffast-math -fvisibility=hidden -std=c99 -std=gnu99  -o utils/utils.o


cc utils/utils.c 
   utils/utils.h model/tetr4-switch.h lv2-hmi.h extension/lv2-hmi-extension.h
   -Wall -Wextra -pipe -Wno-unused-parameter -fPIC -DPIC -DNDEBUG -O3 -ffast-math -fvisibility=hidden -std=c99 -std=gnu99  -Wl,-O1 -Wl,--as-needed -Wl,--strip-all -Wl,--no-undefined  -lm -shared
   -o utils/utils.o

cc -c utils/utils.c -o utils/utils.o

*/