#ifndef THREADS_FIXED_POINT_H
#define THREADS_FIXED_POINT_H

#include <stdint.h>

typedef int32_t fixed_float;
#define FIXED_POINT_REAL_FLOAT_BITS   14
#define FIXED_POINT_REAL_INT_BITS     (sizeof(Fixed_float)-FIXED_POINT_REAL_FLOAT_BITS)
#define FIXED_POINT_REAL_FLOAT_LIMIT  (1 << FIXED_POINT_REAL_FLOAT_BITS)

/* Create and return new fixed-point real number */
#define new_fixed_point_real()   \
        0

/* Create and return new fixed-point real number */
#define new_fixed_point_real_nom_dom(NOM, DOM)   \
        (div_real_by_real(convert_int_to_real(NOM),convert_int_to_real(DOM)))

/* Convert int to real number */
#define convert_int_to_real(INT)   \
        ((INT)*(FIXED_POINT_REAL_FLOAT_LIMIT))

/* Convert real to int number. Rounds towards zero. */
#define convert_real_to_int(REAL)   \
        ((REAL)/(FIXED_POINT_REAL_FLOAT_LIMIT))

/* Convert real to int number, Rounds towards nearest. */
#define convert_real_to_int_nearest(REAL)   \
        ((REAL) >= 0 ?  \
        (((REAL) + ((FIXED_POINT_REAL_FLOAT_LIMIT) / 2))/FIXED_POINT_REAL_FLOAT_LIMIT) \
        : \
        (((REAL) - ((FIXED_POINT_REAL_FLOAT_LIMIT) / 2))/FIXED_POINT_REAL_FLOAT_LIMIT))

/* Add int to real number. */
#define add_int_to_real(INT, REAL)   \
        (((INT)*(FIXED_POINT_REAL_FLOAT_LIMIT))+(REAL))

/* Add real to real number. */
#define add_real_to_real(REAL1, REAL2)   \
        ((REAL1)+(REAL2))

/* Sub REAL1 from REAL2 number. */
#define sub_real_from_real(REAL1, REAL2)   \
        ((REAL2)-(REAL1))

/* Sub int from real number. */
#define sub_int_from_real(INT, REAL)   \
        ((REAL)-((INT)*(FIXED_POINT_REAL_FLOAT_LIMIT)))

/* Mul REAL1 by REAL2 number. */
#define mul_real_by_real(REAL1, REAL2)   \
        ((((int64_t) (REAL1)) * (REAL2)) / (FIXED_POINT_REAL_FLOAT_LIMIT))

/* Mul int by real number. */
#define mul_real_by_int(REAL, INT)   \
        ((REAL)*(INT))

/* Div REAL1 by REAL2 number. */
#define div_real_by_real(REAL1, REAL2)   \
        ((((int64_t) (REAL1)) * (FIXED_POINT_REAL_FLOAT_LIMIT)) / (REAL2))

/* Div real by int number. */
#define div_real_by_int(REAL, INT)   \
        ((REAL)/(INT))

#endif
