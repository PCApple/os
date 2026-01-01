#include "include/utils.h"

int ceiling(int numerator, int denominator) {
    if (numerator % denominator == 0) {
        return numerator / denominator;
    }
    else{
        return (numerator / denominator) + 1;
    }
}