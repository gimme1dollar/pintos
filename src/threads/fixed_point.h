#ifndef FIXED_POINT_H
#define FIXED_POINT_H

#define P 17
#define Q 14
#define FRAC (1<<Q)

int int_to_fp (int n) 
{
    return n * FRAC;
}

int fp_to_int_zero (int x) 
{
    return x / FRAC;
}

int fp_to_int_nearest (int x) 
{
    return (x >= 0) ? (x + FRAC / 2) / (FRAC) : (x - FRAC / 2) / (FRAC);
}

int add_fp_fp (int x, int y) 
{
    return x + y;
}

int add_fp_int (int x, int n) 
{
    return x + n * FRAC;
}

int sub_fp_fp (int x, int y) 
{
    return x - y;
}

int sub_fp_int (int x, int n) 
{
    return x - n * FRAC;
}

int mult_fp_fp (int x, int y)
{
    return ((int64_t) x) * y / FRAC;
}

int mult_fp_int (int x, int n)
{
    return x * n;
}

int div_fp_fp (int x, int y)
{
    return ((int64_t) x) * FRAC / y;
}

int div_fp_int (int x, int n)
{
    return x / n;
}

#endif