/*
 * hsfft.h
 *
 *  Created on: Apr 14, 2013
 *      Author: Rafat Hussain
 */

#ifndef HSFFT_H_
#define HSFFT_H_

#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "nacomplex.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PI2 6.28318530717958647692528676655900577

typedef struct fft_set* fft_object;

fft_object fft_init(int N, int sgn);
void fft_exec(fft_object obj,NumArray_Complex *inp,NumArray_Complex *oup);
void free_fft(fft_object object);

#ifdef __cplusplus
}
#endif




#endif /* HSFFT_H_ */
