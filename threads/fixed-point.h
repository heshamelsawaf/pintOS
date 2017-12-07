#ifndef THREADS_FIXED_POINT_H
#define THREADS_FIXED_POINT_H

#include <stdint.h>

typedef int32_t                      fixed_float;
#define FIXED_POINT_REAL_FLOAT_BITS   14
#define FIXED_POINT_REAL_INT_BITS     (sizeof(Fixed_float)-FIXED_POINT_REAL_FLOAT_BITS)
#define FIXED_POINT_REAL_FLOAT_LIMIT  (1 << FIXED_POINT_REAL_FLOAT_BITS)

/* create and return new fixed-point real number */
#define new_fixed_point_real()   \
        0

/* create and return new fixed-point real number */
#define new_fixed_point_real_nom_dom(NOM,DOM)   \
        (div_real_by_real(convert_int_to_real(NOM),convert_int_to_real(DOM)))

/* convert int to real number */
#define convert_int_to_real(INT)   \
        ((INT)*(FIXED_POINT_REAL_FLOAT_LIMIT))

/* convert real to int number, IT WILL ROUND TOWARD ZERO */
#define convert_real_to_int(REAL)   \
        ((REAL)/(FIXED_POINT_REAL_FLOAT_LIMIT))

/* convert real to int number, IT WILL ROUND TOWARD NEAREST */
#define convert_real_to_int_nearest(REAL)   \
        ((REAL) >= 0 ?  \
        (((REAL) + ((FIXED_POINT_REAL_FLOAT_LIMIT) / 2))/FIXED_POINT_REAL_FLOAT_LIMIT) \
        : \
        (((REAL) - ((FIXED_POINT_REAL_FLOAT_LIMIT) / 2))/FIXED_POINT_REAL_FLOAT_LIMIT))

/* add int to real number */
#define add_int_to_real(INT,REAL)   \
        (((INT)*(FIXED_POINT_REAL_FLOAT_LIMIT))+(REAL))

/* add real to real number */
#define add_real_to_real(REAL1,REAL2)   \
        ((REAL1)+(REAL2))

/* sub REAL1 from REAL2 number */
#define sub_real_from_real(REAL1,REAL2)   \
        ((REAL2)-(REAL1))

/* sub int from real number */
#define sub_int_from_real(INT,REAL)   \
        ((REAL)-((INT)*(FIXED_POINT_REAL_FLOAT_LIMIT)))

/* mul REAL1 by REAL2 number */
#define mul_real_by_real(REAL1,REAL2)   \
        ((((int64_t) (REAL1)) * (REAL2)) / (FIXED_POINT_REAL_FLOAT_LIMIT))

/* mul int by real number */
#define mul_real_by_int(REAL,INT)   \
        ((REAL)*(INT))

/* div REAL1 by REAL2 number */
#define div_real_by_real(REAL1,REAL2)   \
        ((((int64_t) (REAL1)) * (FIXED_POINT_REAL_FLOAT_LIMIT)) / (REAL2))

/* div real by int number */
#define div_real_by_int(REAL,INT)   \
        ((REAL)/(INT))

#endif /* threads/fixed-point.h */