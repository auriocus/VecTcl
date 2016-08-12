#ifndef NACOMPLEX_H
#define NACOMPLEX_H
#include <tcl.h>
#include <math.h>

typedef struct {
	float re;
	float im;
} NumArray_ComplexFloat;

typedef struct {
	double re;
	double im;
} NumArray_Complex;


Tcl_Obj * NumArray_NewComplexObj(NumArray_Complex c);

int NumArray_GetComplexFromObj(Tcl_Interp *interp, Tcl_Obj *cplxobj, NumArray_Complex *result);

/* Conversion into string buffer */
#define NUMARRAY_COMPLEX_SPACE (2*TCL_DOUBLE_SPACE+2)

void NumArray_PrintComplex(NumArray_Complex c, char *buf);
int NumArray_ParseComplex(Tcl_Interp *interp, const char *buf, NumArray_Complex *dest);

/* Function to setup the complex value type */
int Complex_Init(Tcl_Interp *interp);

/* Simple constructor. Hopefully optimized out */
static inline NumArray_Complex NumArray_mkComplex(double re, double im) {
	NumArray_Complex result;
	result.re = re;
	result.im = im;
	return result;
}

static inline NumArray_ComplexFloat NumArray_mkComplexFloat(float re, float im) {
	NumArray_ComplexFloat result;
	result.re = re;
	result.im = im;
	return result;
}

/* arithmetic operations */
static inline NumArray_Complex NumArray_ComplexNeg(NumArray_Complex c) {
	NumArray_Complex result;
	result.re = -c.re;
	result.im = -c.im;
	return result;
}

static inline NumArray_Complex NumArray_ComplexConj(NumArray_Complex c) {
	NumArray_Complex result;
	result.re = c.re;
	result.im = -c.im;
	return result;
}

static inline NumArray_Complex NumArray_ComplexAdd(NumArray_Complex c1, NumArray_Complex c2) {
	NumArray_Complex result;
	result.re = c1.re + c2.re;
	result.im = c1.im + c2.im;
	return result;
}

static inline NumArray_Complex NumArray_ComplexSubtract(NumArray_Complex c1, NumArray_Complex c2) {
	NumArray_Complex result;
	result.re = c1.re - c2.re;
	result.im = c1.im - c2.im;
	return result;
}

static inline NumArray_Complex NumArray_ComplexMultiply(NumArray_Complex c1, NumArray_Complex c2) {
	NumArray_Complex result;
	result.re = c1.re*c2.re - c1.im*c2.im;
	result.im = c1.re*c2.im + c1.im*c2.re;
	return result;
}

static inline NumArray_Complex NumArray_ComplexDivide(NumArray_Complex c1, NumArray_Complex c2) {
	NumArray_Complex result;
	double denom=c2.re*c2.re + c2.im*c2.im;
	result.re= (c1.re*c2.re + c1.im*c2.im)/denom;
	result.im= (c1.im*c2.re - c1.re*c2.im)/denom;
	return result;
}

/* Multiply by real number */
static inline NumArray_Complex NumArray_ComplexScale(NumArray_Complex c, double s) {
	NumArray_Complex result;
	result.re= c.re*s;
	result.im= c.im*s;
	return result;
}

static inline double NumArray_ComplexAbs(NumArray_Complex c) {
	return hypot(c.re, c.im);
}

static inline double NumArray_ComplexArg(NumArray_Complex c) {
	return atan2(c.im, c.re);
}

static inline NumArray_Complex NumArray_ComplexSign(NumArray_Complex c) {
	NumArray_Complex result;
	double v=hypot(c.re, c.im);
	if (v==0.0) { return NumArray_mkComplex(0.0, 0.0); }
	result.re=c.re/v;
	result.im=c.im/v;
	return result;
}

static inline NumArray_Complex NumArray_ComplexSqrt(NumArray_Complex c) {
	NumArray_Complex result;
	double absval=hypot(c.re, c.im);
	result.re= sqrt((absval + c.re)/2.0);
	result.im= c.im < 0 ? -sqrt((absval - c.re)/2.0) : sqrt((absval - c.re)/2.0);
	return result;
}


NumArray_Complex NumArray_ComplexPow(NumArray_Complex c1, NumArray_Complex c2);
NumArray_Complex NumArray_ComplexSin(NumArray_Complex c);
NumArray_Complex NumArray_ComplexCos(NumArray_Complex c);
NumArray_Complex NumArray_ComplexTan(NumArray_Complex c);
NumArray_Complex NumArray_ComplexExp(NumArray_Complex c);
NumArray_Complex NumArray_ComplexLog(NumArray_Complex c);
NumArray_Complex NumArray_ComplexSinh(NumArray_Complex c);
NumArray_Complex NumArray_ComplexCosh(NumArray_Complex c);
NumArray_Complex NumArray_ComplexTanh(NumArray_Complex c);
NumArray_Complex NumArray_ComplexAsin(NumArray_Complex c);
NumArray_Complex NumArray_ComplexAcos(NumArray_Complex c);
NumArray_Complex NumArray_ComplexAtan(NumArray_Complex c);
NumArray_Complex NumArray_ComplexAsinh(NumArray_Complex c);
NumArray_Complex NumArray_ComplexAcosh(NumArray_Complex c);
NumArray_Complex NumArray_ComplexAtanh(NumArray_Complex c);


/* .... */

#endif
